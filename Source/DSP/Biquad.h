#pragma once

#include <cmath>
#include <numbers>

namespace finale::dsp
{

// Coeficientes de um biquad já normalizados por a0 (forma direta I).
struct BiquadCoefficients
{
    double b0, b1, b2, a1, a2;
};

// Filtros de 2ª ordem do RBJ Audio EQ Cookbook (Robert Bristow-Johnson):
// w0 = 2π f0/fs, alpha = sin(w0)/(2Q), e divide tudo por a0 = 1 + alpha.
inline BiquadCoefficients makeLowPass (double fs, double f0, double q)
{
    const double w0 = 2.0 * std::numbers::pi * f0 / fs;
    const double cosW0 = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;

    return { (1.0 - cosW0) / 2.0 / a0, (1.0 - cosW0) / a0, (1.0 - cosW0) / 2.0 / a0, -2.0 * cosW0 / a0,
        (1.0 - alpha) / a0 };
}

inline BiquadCoefficients makeHighPass (double fs, double f0, double q)
{
    const double w0 = 2.0 * std::numbers::pi * f0 / fs;
    const double cosW0 = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;

    return { (1.0 + cosW0) / 2.0 / a0, -(1.0 + cosW0) / a0, (1.0 + cosW0) / 2.0 / a0, -2.0 * cosW0 / a0,
        (1.0 - alpha) / a0 };
}

// Passa-tudo: magnitude 1 em todas as frequências, só muda a fase em torno de f0.
inline BiquadCoefficients makeAllPass (double fs, double f0, double q)
{
    const double w0 = 2.0 * std::numbers::pi * f0 / fs;
    const double cosW0 = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);
    const double a0 = 1.0 + alpha;

    return { (1.0 - alpha) / a0, -2.0 * cosW0 / a0, (1.0 + alpha) / a0, -2.0 * cosW0 / a0, (1.0 - alpha) / a0 };
}

// Peaking EQ do RBJ Audio EQ Cookbook: ganho gainDb em f0, unitário longe de f0.
// A = 10^(gainDb/40); com gainDb = 0 os coeficientes dão exatamente a identidade.
inline BiquadCoefficients makePeak (double fs, double f0, double q, double gainDb)
{
    const double a = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * std::numbers::pi * f0 / fs;
    const double cosW0 = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);
    const double a0 = 1.0 + alpha / a;

    return { (1.0 + alpha * a) / a0, -2.0 * cosW0 / a0, (1.0 - alpha * a) / a0, -2.0 * cosW0 / a0,
        (1.0 - alpha / a) / a0 };
}

// Shelf de graves do RBJ Audio EQ Cookbook: ganho gainDb abaixo de f0, unitário acima.
inline BiquadCoefficients makeLowShelf (double fs, double f0, double q, double gainDb)
{
    const double a = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * std::numbers::pi * f0 / fs;
    const double cosW0 = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);
    const double twoSqrtAAlpha = 2.0 * std::sqrt (a) * alpha;
    const double a0 = (a + 1.0) + (a - 1.0) * cosW0 + twoSqrtAAlpha;

    return { a * ((a + 1.0) - (a - 1.0) * cosW0 + twoSqrtAAlpha) / a0, 2.0 * a * ((a - 1.0) - (a + 1.0) * cosW0) / a0,
        a * ((a + 1.0) - (a - 1.0) * cosW0 - twoSqrtAAlpha) / a0, -2.0 * ((a - 1.0) + (a + 1.0) * cosW0) / a0,
        ((a + 1.0) + (a - 1.0) * cosW0 - twoSqrtAAlpha) / a0 };
}

// Shelf de agudos do RBJ Audio EQ Cookbook: ganho gainDb acima de f0, unitário abaixo.
inline BiquadCoefficients makeHighShelf (double fs, double f0, double q, double gainDb)
{
    const double a = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * std::numbers::pi * f0 / fs;
    const double cosW0 = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);
    const double twoSqrtAAlpha = 2.0 * std::sqrt (a) * alpha;
    const double a0 = (a + 1.0) - (a - 1.0) * cosW0 + twoSqrtAAlpha;

    return { a * ((a + 1.0) + (a - 1.0) * cosW0 + twoSqrtAAlpha) / a0, -2.0 * a * ((a - 1.0) + (a + 1.0) * cosW0) / a0,
        a * ((a + 1.0) + (a - 1.0) * cosW0 - twoSqrtAAlpha) / a0, 2.0 * ((a - 1.0) - (a + 1.0) * cosW0) / a0,
        ((a + 1.0) - (a - 1.0) * cosW0 - twoSqrtAAlpha) / a0 };
}

// Estado de um biquad: as duas últimas entradas e saídas.
struct BiquadState
{
    double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;

    double process (const BiquadCoefficients& c, double x)
    {
        const double y = c.b0 * x + c.b1 * x1 + c.b2 * x2 - c.a1 * y1 - c.a2 * y2;

        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;

        return y;
    }
};

} // namespace finale::dsp
