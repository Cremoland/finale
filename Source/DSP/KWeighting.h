#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

namespace finale::dsp
{

// Filtro K-weighting do ITU-R BS.1770-5 (base do LUFS): shelving de cabeça
// seguido de high-pass RLB, em cascata. Os parâmetros analógicos abaixo são os
// do libebur128 (MIT, Jan Kokemüller), que reproduzem as Tabelas 1 e 2 do
// Anexo 1 do BS.1770-5; a discretização bilinear para qualquer sample rate é nossa.
// Coeficientes e estado são double: o polo do high-pass de 38 Hz fica muito
// perto de 1 em 192 kHz, e float perderia precisão.
class KWeighting
{
public:
    struct Coefficients
    {
        double b0, b1, b2, a1, a2;
    };

    // Calcula os coeficientes e aloca/zera o estado. Única parte que aloca.
    void prepare (double sampleRate, int numChannels)
    {
        shelfCoefficients = makeHeadShelf (sampleRate);
        highPassCoefficients = makeHighPass (sampleRate);

        shelfStates.assign (static_cast<std::size_t> (numChannels), BiquadState { });
        highPassStates.assign (static_cast<std::size_t> (numChannels), BiquadState { });
    }

    // Entrada não-finita vira silêncio, para um NaN nunca entrar no estado do filtro.
    double process (int channel, double sample)
    {
        if (!std::isfinite (sample))
            sample = 0.0;

        const double shelved = runBiquad (shelfCoefficients, shelfStates[static_cast<std::size_t> (channel)], sample);
        return runBiquad (highPassCoefficients, highPassStates[static_cast<std::size_t> (channel)], shelved);
    }

    const Coefficients& stage1() const { return shelfCoefficients; }
    const Coefficients& stage2() const { return highPassCoefficients; }

private:
    struct BiquadState
    {
        double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
    };

    static double runBiquad (const Coefficients& c, BiquadState& s, double x)
    {
        const double y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;

        s.x2 = s.x1;
        s.x1 = x;
        s.y2 = s.y1;
        s.y1 = y;

        return y;
    }

    static Coefficients makeHeadShelf (double fs)
    {
        const double f0 = 1681.974450955533;
        const double gainDb = 3.999843853973347;
        const double q = 0.7071752369554196;

        const double k = std::tan (std::numbers::pi * f0 / fs);
        const double vh = std::pow (10.0, gainDb / 20.0);
        const double vb = std::pow (vh, 0.4996667741545416);
        const double a0 = 1.0 + k / q + k * k;

        return { (vh + vb * k / q + k * k) / a0, 2.0 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0,
            2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0 };
    }

    static Coefficients makeHighPass (double fs)
    {
        const double f0 = 38.13547087602444;
        const double q = 0.5003270373238773;

        const double k = std::tan (std::numbers::pi * f0 / fs);
        const double a0 = 1.0 + k / q + k * k;

        return { 1.0, -2.0, 1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0 };
    }

    Coefficients shelfCoefficients { 1.0, 0.0, 0.0, 0.0, 0.0 };
    Coefficients highPassCoefficients { 1.0, 0.0, 0.0, 0.0, 0.0 };
    std::vector<BiquadState> shelfStates;
    std::vector<BiquadState> highPassStates;
};

} // namespace finale::dsp
