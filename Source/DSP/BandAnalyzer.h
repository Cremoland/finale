#pragma once

#include "Biquad.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace finale::dsp
{

// Analisador de equilíbrio tonal offline: energia em 4 oitavas e a forma espectral resultante,
// em dB em relação à média das 4 bandas. Cada banda é um passa-altas e um passa-baixas
// Butterworth em cascata (dois biquads de cada, ~24 dB/oitava de cada lado), como no TonalEq.
class BandAnalyzer
{
public:
    static constexpr std::array<double, 4> centersHz { 90.0, 300.0, 3000.0, 10000.0 };

    // Monta os filtros para sampleRate e zera as somas.
    void prepare (double sampleRate)
    {
        // Butterworth de 2ª ordem: Q = 1/√2 é o que deixa a cascata plana na banda.
        const double q = 1.0 / std::numbers::sqrt2;

        for (std::size_t b = 0; b < bands.size(); ++b)
        {
            Band band;
            band.highPass = makeHighPass (sampleRate, centersHz[b] / std::numbers::sqrt2, q);
            // A borda alta é limitada a 0.45·fs para não ultrapassar Nyquist.
            const double highEdge = std::min (centersHz[b] * std::numbers::sqrt2, 0.45 * sampleRate);
            band.lowPass = makeLowPass (sampleRate, highEdge, q);
            bands[b] = band;
        }
        count = 0;
    }

    // Acumula a energia de cada banda. Amostra não-finita conta como 0, mas ainda entra na contagem.
    void process (const float* mono, int numSamples)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            const float raw = mono[n];
            const double x = std::isfinite (raw) ? static_cast<double> (raw) : 0.0;

            for (Band& band : bands)
            {
                double y = band.hp1.process (band.highPass, x);
                y = band.hp2.process (band.highPass, y);
                y = band.lp1.process (band.lowPass, y);
                y = band.lp2.process (band.lowPass, y);
                band.sumSquares += y * y;
            }
            ++count;
        }
    }

    // Nível de cada banda em dB (10·log10 da média do quadrado) menos a média dos 4 níveis.
    // Sem energia em alguma banda (ou sem amostras), devolve {0, 0, 0, 0}.
    std::array<float, 4> getShapeDb() const
    {
        std::array<float, 4> shape { };
        if (count == 0)
            return shape;

        std::array<double, 4> levels { };
        double mean = 0.0;
        for (std::size_t b = 0; b < bands.size(); ++b)
        {
            const double energy = bands[b].sumSquares;
            if (energy <= 0.0)
                return { };

            levels[b] = 10.0 * std::log10 (energy / static_cast<double> (count));
            mean += levels[b] / static_cast<double> (bands.size());
        }

        for (std::size_t b = 0; b < bands.size(); ++b)
            shape[b] = static_cast<float> (levels[b] - mean);
        return shape;
    }

private:
    struct Band
    {
        BiquadCoefficients highPass { };
        BiquadCoefficients lowPass { };
        BiquadState hp1, hp2, lp1, lp2;
        double sumSquares = 0.0;
    };

    std::array<Band, 4> bands { };
    std::int64_t count = 0;
};

} // namespace finale::dsp
