#pragma once

#include "Crossover.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace finale::dsp
{

// Compressor feedforward de 3 bandas. Cada canal é dividido pelo ThreeBandCrossover, e
// cada banda tem um ganho próprio, linkado entre canais (o maior nível manda).
//
// Curva estática com joelho duro e detector de pico no domínio de dB, com suavização
// "branching" (attack quando o alvo está abaixo do estado, release quando está acima):
// Giannoulis, Massberg & Reiss, "Digital Dynamic Range Compressor Design: A Tutorial and
// Analysis", JAES 2012. Os coeficientes são polos exponenciais de primeira ordem.
class MultibandCompressor
{
public:
    // Calcula crossover e coeficientes e zera todo o estado. Única parte que aloca.
    void prepare (double sampleRate, int numChannels)
    {
        crossover.prepare (sampleRate, numChannels, 200.0, 2000.0);
        bands.assign (static_cast<std::size_t> (std::max (numChannels, 0)), { });

        for (std::size_t k = 0; k < 3; ++k)
        {
            attackCoeff[k] = std::exp (-1.0 / (attackMs[k] / 1000.0 * sampleRate));
            releaseCoeff[k] = std::exp (-1.0 / (releaseMs[k] / 1000.0 * sampleRate));
            gainDb[k] = 0.0;
        }
    }

    // Razão de compressão para as 3 bandas. Abaixo de 1 vira 1 (sem compressão).
    void setRatio (float value) { ratio = std::max (1.0, static_cast<double> (value)); }

    // Processa in place. Canais além dos preparados ficam intactos.
    void process (float* const* channels, int numChannels, int numSamples)
    {
        const int active = std::min (numChannels, static_cast<int> (bands.size()));

        for (int n = 0; n < numSamples; ++n)
        {
            for (int ch = 0; ch < active; ++ch)
            {
                double low = 0.0;
                double mid = 0.0;
                double high = 0.0;
                crossover.process (ch, static_cast<double> (channels[ch][n]), low, mid, high);
                bands[static_cast<std::size_t> (ch)] = { low, mid, high };
            }

            std::array<double, 3> gain { };
            for (std::size_t k = 0; k < 3; ++k)
            {
                double level = 0.0;
                for (int ch = 0; ch < active; ++ch)
                    level = std::max (level, std::abs (bands[static_cast<std::size_t> (ch)][k]));

                // shortcut: limiar fixo de −18 dBFS nas 3 bandas; presets e Auto vão escolher.
                const double thresholdDb = -18.0;
                const double overDb = 20.0 * std::log10 (std::max (level, 1e-10)) - thresholdDb;
                const double targetDb = overDb > 0.0 ? -overDb * (1.0 - 1.0 / ratio) : 0.0;

                const double a = targetDb < gainDb[k] ? attackCoeff[k] : releaseCoeff[k];
                gainDb[k] = a * gainDb[k] + (1.0 - a) * targetDb;
                gain[k] = std::pow (10.0, gainDb[k] / 20.0);
            }

            for (int ch = 0; ch < active; ++ch)
            {
                const auto& b = bands[static_cast<std::size_t> (ch)];
                channels[ch][n] = static_cast<float> (b[0] * gain[0] + b[1] * gain[1] + b[2] * gain[2]);
            }
        }
    }

private:
    // shortcut: attack de 30 ms na low, 10 ms na mid e 5 ms na high; presets e Auto vão escolher.
    static constexpr std::array<double, 3> attackMs { 30.0, 10.0, 5.0 };
    // shortcut: release de 200 ms na low, 120 ms na mid e 80 ms na high; presets e Auto vão escolher.
    static constexpr std::array<double, 3> releaseMs { 200.0, 120.0, 80.0 };

    ThreeBandCrossover crossover;
    std::vector<std::array<double, 3>> bands;
    std::array<double, 3> attackCoeff { };
    std::array<double, 3> releaseCoeff { };
    std::array<double, 3> gainDb { };
    double ratio = 1.0;
};

} // namespace finale::dsp
