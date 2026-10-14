#pragma once

#include "Biquad.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace finale::dsp
{

// Analisador de equilíbrio tonal offline: energia em 4 bandas e a forma espectral resultante,
// em dB em relação à média das 4 bandas. Cada banda é um passa-altas e um passa-baixas
// Butterworth em cascata (dois biquads de cada, ~24 dB/oitava de cada lado), como no TonalEq.
class BandAnalyzer
{
public:
    // Bordas de cada banda em Hz. As pontas cobrem o que os shelves do TonalEq afetam: o shelf de 90 Hz
    // mexe em tudo abaixo dele (inclusive os 808 de 35–60 Hz) e o de 10 kHz, em tudo acima dele.
    // As bandas do meio são oitavas em torno de 300 Hz e 3 kHz, como no TonalEq.
    static constexpr std::array<double, 4> lowEdgeHz { 25.0, 300.0 / std::numbers::sqrt2, 3000.0 / std::numbers::sqrt2,
        10000.0 / std::numbers::sqrt2 };
    static constexpr std::array<double, 4> highEdgeHz { 90.0 * std::numbers::sqrt2, 300.0 * std::numbers::sqrt2,
        3000.0 * std::numbers::sqrt2, 20000.0 };

    // Monta os filtros para sampleRate e zera as somas.
    void prepare (double sampleRate)
    {
        // Q = 1/√2 é o Butterworth de 2ª ordem (RBJ Audio EQ Cookbook, Robert Bristow-Johnson). Como cada
        // lado tem duas seções em cascata, a borda cai 6 dB, não 3 dB.
        const double q = 1.0 / std::numbers::sqrt2;

        for (std::size_t b = 0; b < bands.size(); ++b)
        {
            // A borda alta é limitada a 0.45·fs para não ultrapassar Nyquist.
            const double low = lowEdgeHz[b];
            const double high = std::min (highEdgeHz[b], 0.45 * sampleRate);

            Band band;
            band.highPass = makeHighPass (sampleRate, low, q);
            band.lowPass = makeLowPass (sampleRate, high, q);
            band.octaves = effectiveOctaves (band, sampleRate);
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

    // Nível de cada banda em dB, como densidade por oitava: 10·log10 (energia / amostras / larguraEmOitavas),
    // menos a média dos 4 níveis. A densidade é o que deixa o ruído rosa plano mesmo com bandas de larguras
    // diferentes (energia proporcional à largura em oitavas). Sem energia finita e positiva em alguma banda
    // (ou sem amostras), devolve {0, 0, 0, 0}.
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
            if (!std::isfinite (energy) || energy <= 0.0)
                return { };

            levels[b] = 10.0 * std::log10 (energy / (static_cast<double> (count) * bands[b].octaves));
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
        double octaves = 1.0; // largura efetiva da banda em oitavas (ver effectiveOctaves)
    };

    // |H|² de um biquad em ω (rad/amostra): H(z) = (b0 + b1 z⁻¹ + b2 z⁻²) / (1 + a1 z⁻¹ + a2 z⁻²), z = e^{jω}.
    static double magnitudeSquared (const BiquadCoefficients& c, double w)
    {
        const double cos1 = std::cos (w);
        const double sin1 = std::sin (w);
        const double cos2 = std::cos (2.0 * w);
        const double sin2 = std::sin (2.0 * w);

        const double numRe = c.b0 + c.b1 * cos1 + c.b2 * cos2;
        const double numIm = -(c.b1 * sin1 + c.b2 * sin2);
        const double denRe = 1.0 + c.a1 * cos1 + c.a2 * cos2;
        const double denIm = -(c.a1 * sin1 + c.a2 * sin2);

        return (numRe * numRe + numIm * numIm) / (denRe * denRe + denIm * denIm);
    }

    // Largura efetiva da banda em oitavas: ∫ |H(f)|² d(log2 f), com |H|² o produto das 4 biquads
    // (passa-altas e passa-baixas em cascata), de 5 Hz até 0.499·fs. Os filtros não são retangulares:
    // as bordas caem devagar (dois biquads por lado, −6 dB na borda nominal), então a largura real
    // é menor que (bordaAlta / bordaBaixa) em oitavas. Com a largura nominal, bandas de larguras
    // diferentes saem com níveis tortos mesmo com ruído rosa. Ruído rosa tem a mesma energia por oitava,
    // então dividir pela largura efetiva deixa a forma plana. Integração trapezoidal em log2 f.
    static double effectiveOctaves (const Band& band, double sampleRate)
    {
        constexpr int points = 4000;
        const double x0 = std::log2 (5.0);
        const double x1 = std::log2 (0.499 * sampleRate);
        const double dx = (x1 - x0) / static_cast<double> (points - 1);

        double sum = 0.0;
        for (int k = 0; k < points; ++k)
        {
            const double w = 2.0 * std::numbers::pi * std::exp2 (x0 + dx * static_cast<double> (k)) / sampleRate;
            const double hp = magnitudeSquared (band.highPass, w);
            const double lp = magnitudeSquared (band.lowPass, w);
            // Duas biquads de cada lado com os mesmos coeficientes: |H|² de cada lado é o quadrado.
            const double gain = hp * hp * lp * lp;
            sum += (k == 0 || k == points - 1) ? 0.5 * gain : gain;
        }
        return sum * dx;
    }

    std::array<Band, 4> bands { };
    std::int64_t count = 0;
};

} // namespace finale::dsp
