#pragma once

#include "DSP/KWeighting.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace finale::dsp
{

// Medidor de loudness do ITU-R BS.1770-5 (Anexo 1, eq. 1–7) com as janelas da
// EBU Tech 3341 §2.2–2.3: momentary (400 ms), short-term (3 s) e integrado com
// gate absoluto (−70 LUFS) e relativo (−10 LU).
//
// O sinal é somado em sub-blocos de 100 ms. Um bloco de 400 ms é a média dos 4
// últimos sub-blocos (overlap de 75%, como manda o BS.1770 para o gate) e a janela
// de 3 s é a média dos 30 últimos. Sub-blocos ainda não vistos contam como
// silêncio, por isso a leitura sobe gradualmente no início do sinal.
class LoudnessMeter
{
public:
    static constexpr float floorLufs = -100.0f;

    // Aloca e zera tudo, inclusive o histograma do integrado.
    void prepare (double sampleRate, int numChannels)
    {
        preparedChannels = std::max (0, numChannels);
        kWeighting.prepare (sampleRate, preparedChannels);

        subBlockLength = std::max (1, static_cast<int> (std::lround (0.1 * sampleRate)));
        samplesInSubBlock = 0;
        subBlockSum = 0.0;
        subBlockCount = 0;
        ring.fill (0.0);
        ringHead = 0;

        histogram.assign (static_cast<std::size_t> (kBins), Bin { });

        momentaryLufs = floorLufs;
        shortTermLufs = floorLufs;
        integratedLufs = floorLufs;
        prepared = true;
    }

    // numChannels deve ser no máximo o valor de prepare; canais acima disso são ignorados.
    // Não aloca: só usa o estado pré-alocado em prepare.
    void process (const float* const* channels, int numChannels, int numSamples)
    {
        if (!prepared || numSamples <= 0)
            return;

        const int activeChannels = std::min (numChannels, preparedChannels);

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < activeChannels; ++ch)
            {
                // G = 1 para L e R (BS.1770-5 Anexo 1, eq. 2).
                const double weighted = kWeighting.process (ch, static_cast<double> (channels[ch][i]));
                subBlockSum += weighted * weighted;
            }

            if (++samplesInSubBlock == subBlockLength)
                closeSubBlock();
        }
    }

    float getMomentaryLufs() const { return momentaryLufs; }
    float getShortTermLufs() const { return shortTermLufs; }
    float getIntegratedLufs() const { return integratedLufs; }

private:
    static constexpr int kBins = 7500;
    static constexpr double kBinBottom = -70.0;
    static constexpr double kBinWidth = 0.01;
    static constexpr double kAbsoluteGate = -70.0;
    static constexpr double kRelativeGateLu = 10.0;
    static constexpr std::size_t kRingSize = 30;
    static constexpr std::size_t kMomentarySubBlocks = 4;

    struct Bin
    {
        std::uint64_t count = 0;
        double energySum = 0.0;
    };

    void closeSubBlock()
    {
        // Energia média por amostra do sub-bloco, somada sobre os canais (eq. 2).
        const double subEnergy = subBlockSum / static_cast<double> (subBlockLength);
        subBlockSum = 0.0;
        samplesInSubBlock = 0;

        ring[ringHead] = subEnergy;
        ringHead = (ringHead + 1) % kRingSize;
        ++subBlockCount;

        double momentaryEnergy = 0.0;
        for (std::size_t k = 1; k <= kMomentarySubBlocks; ++k)
            momentaryEnergy += ring[(ringHead + kRingSize - k) % kRingSize];
        momentaryEnergy /= static_cast<double> (kMomentarySubBlocks);

        double shortTermEnergy = 0.0;
        for (const double energy : ring)
            shortTermEnergy += energy;
        shortTermEnergy /= static_cast<double> (kRingSize);

        momentaryLufs = energyToLufs (momentaryEnergy);
        shortTermLufs = energyToLufs (shortTermEnergy);

        // Gating block (Tech 3341 §2.3): um a cada sub-bloco, a partir do quarto.
        if (subBlockCount >= kMomentarySubBlocks)
            addGatingBlock (momentaryEnergy);
    }

    void addGatingBlock (double energy)
    {
        const double loudness = energyToLufs (energy);

        // Gate absoluto: blocos abaixo de −70 LUFS não entram no integrado.
        if (loudness < kAbsoluteGate)
            return;

        // Blocos acima de +5 LUFS caem no último bin; a soma de energia continua exata.
        const double position
            = std::clamp (std::floor ((loudness - kBinBottom) / kBinWidth), 0.0, static_cast<double> (kBins - 1));
        Bin& bin = histogram[static_cast<std::size_t> (position)];
        ++bin.count;
        bin.energySum += energy;

        integratedLufs = computeIntegrated();
    }

    // BS.1770-5 eq. 3 (gate absoluto) e eq. 5 (gate relativo, −10 LU), sobre o histograma.
    float computeIntegrated() const
    {
        std::uint64_t totalCount = 0;
        double totalEnergy = 0.0;
        for (const Bin& bin : histogram)
        {
            totalCount += bin.count;
            totalEnergy += bin.energySum;
        }

        if (totalCount == 0)
            return floorLufs;

        const double absoluteMean = totalEnergy / static_cast<double> (totalCount);
        const double relativeThreshold = energyToLufs (absoluteMean) - kRelativeGateLu;

        std::uint64_t gatedCount = 0;
        double gatedEnergy = 0.0;
        for (int i = 0; i < kBins; ++i)
        {
            const Bin& bin = histogram[static_cast<std::size_t> (i)];
            const double lowerEdge = kBinBottom + static_cast<double> (i) * kBinWidth;

            if (bin.count > 0 && lowerEdge >= relativeThreshold)
            {
                gatedCount += bin.count;
                gatedEnergy += bin.energySum;
            }
        }

        if (gatedCount == 0)
            return floorLufs;

        return energyToLufs (gatedEnergy / static_cast<double> (gatedCount));
    }

    // BS.1770-5 eq. 1 (loudness de um bloco) e eq. 4 (−0.691 é a compensação de canal).
    static float energyToLufs (double energy)
    {
        if (!(energy > 0.0) || !std::isfinite (energy))
            return floorLufs;

        const double loudness = -0.691 + 10.0 * std::log10 (energy);
        return static_cast<float> (std::max (loudness, static_cast<double> (floorLufs)));
    }

    KWeighting kWeighting;
    bool prepared = false;
    int preparedChannels = 0;
    int subBlockLength = 1;
    int samplesInSubBlock = 0;
    double subBlockSum = 0.0;
    std::uint64_t subBlockCount = 0;
    std::array<double, kRingSize> ring { };
    std::size_t ringHead = 0;
    std::vector<Bin> histogram;

    float momentaryLufs = floorLufs;
    float shortTermLufs = floorLufs;
    float integratedLufs = floorLufs;
};

} // namespace finale::dsp
