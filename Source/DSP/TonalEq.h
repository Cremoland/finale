#pragma once

#include "Biquad.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

namespace finale::dsp
{

// EQ tonal de 4 bandas em cascata: shelf de graves, peak 300 Hz, peak 3 kHz e shelf de agudos.
// Cada banda é um biquad do RBJ Audio EQ Cookbook (ver Biquad.h).
class TonalEq
{
public:
    static constexpr int numBands = 4;

    // Aloca e zera o estado. Os ganhos suavizados começam iguais aos alvos atuais.
    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        // shortcut: 20 ms de suavização fixos; não é parâmetro.
        smoothingAlpha = 1.0 - std::exp (-static_cast<double> (kSubBlock) / (kSmoothingSeconds * fs));

        smoothedDb = targetDb;
        for (int b = 0; b < numBands; ++b)
        {
            coeffs[static_cast<std::size_t> (b)] = designBand (b, smoothedDb[static_cast<std::size_t> (b)]);
            computedDb[static_cast<std::size_t> (b)] = smoothedDb[static_cast<std::size_t> (b)];
        }

        states.assign (static_cast<std::size_t> (numChannels), ChannelStates { });
        preparedChannels = numChannels;
        phase = 0;
    }

    // Não-finito vira 0 dB; o resto é limitado a [-12, 12] dB. Pode ser chamado antes de prepare.
    void setGainsDecibels (const std::array<float, numBands>& gainsDb)
    {
        for (std::size_t b = 0; b < numBands; ++b)
        {
            const double g = gainsDb[b];
            // shortcut: limite de ±12 dB por banda; fixo para o EQ tonal.
            targetDb[b] = std::isfinite (g) ? std::clamp (g, -kMaxDb, kMaxDb) : 0.0;
        }
    }

    // Processa in place. Canais além dos preparados não são tocados; 0 amostras não faz nada.
    void process (float* const* channels, int numChannels, int numSamples)
    {
        const int count = std::min (numChannels, preparedChannels);
        if (count <= 0 || numSamples <= 0)
            return;

        int pos = 0;
        while (pos < numSamples)
        {
            // Os sub-blocos são alinhados à posição absoluta (múltiplos de 32), então o resultado
            // não depende de como o host divide os blocos.
            if (phase == 0)
                updateBands();

            const int chunk = std::min (numSamples - pos, kSubBlock - phase);

            for (int ch = 0; ch < count; ++ch)
            {
                float* const data = channels[ch] + pos;
                ChannelStates& s = states[static_cast<std::size_t> (ch)];

                for (int i = 0; i < chunk; ++i)
                {
                    double x = std::isfinite (data[i]) ? static_cast<double> (data[i]) : 0.0;
                    for (std::size_t b = 0; b < numBands; ++b)
                        x = s.bands[b].process (coeffs[b], x);
                    data[i] = static_cast<float> (x);
                }
            }

            phase = (phase + chunk) % kSubBlock;
            pos += chunk;
        }
    }

private:
    // shortcut: 32 amostras por sub-bloco; recalcular a cada amostra seria caro demais.
    static constexpr int kSubBlock = 32;
    // shortcut: suavização de 20 ms por one-pole.
    static constexpr double kSmoothingSeconds = 0.020;
    // shortcut: só recalcula os coeficientes se o ganho mudou mais que isto (dB).
    static constexpr double kRecomputeThresholdDb = 1e-4;
    static constexpr double kMaxDb = 12.0;

    struct ChannelStates
    {
        std::array<BiquadState, numBands> bands { };
    };

    // Frequências e Q fixos de cada banda (cascata nessa ordem). Os shelves usam Q = 1/√2.
    // shortcut: frequências e Q fixos; o plano do EQ tonal os define por gênero.
    BiquadCoefficients designBand (int band, double gainDb) const
    {
        const double qShelf = std::numbers::sqrt2 / 2.0;
        switch (band)
        {
        case 0:
            return makeLowShelf (fs, 90.0, qShelf, gainDb);
        case 1:
            return makePeak (fs, 300.0, 1.0, gainDb);
        case 2:
            return makePeak (fs, 3000.0, 1.0, gainDb);
        default:
            return makeHighShelf (fs, 10000.0, qShelf, gainDb);
        }
    }

    // Avança o suavizador por um sub-bloco e recalcula as bandas que mudaram.
    void updateBands()
    {
        for (std::size_t b = 0; b < numBands; ++b)
        {
            smoothedDb[b] += (targetDb[b] - smoothedDb[b]) * smoothingAlpha;

            if (std::abs (smoothedDb[b] - computedDb[b]) > kRecomputeThresholdDb)
            {
                coeffs[b] = designBand (static_cast<int> (b), smoothedDb[b]);
                computedDb[b] = smoothedDb[b];
            }
        }
    }

    double fs = 48000.0;
    double smoothingAlpha = 0.0;
    int preparedChannels = 0;
    int phase = 0;

    std::array<double, numBands> targetDb { };
    std::array<double, numBands> smoothedDb { };
    std::array<double, numBands> computedDb { };
    std::array<BiquadCoefficients, numBands> coeffs { };
    std::vector<ChannelStates> states;
};

} // namespace finale::dsp
