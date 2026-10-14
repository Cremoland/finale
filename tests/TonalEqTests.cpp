#include "DSP/Biquad.h"
#include "DSP/TonalEq.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

using finale::dsp::TonalEq;

namespace
{
using Gains = std::array<float, 4>;

constexpr int kBlock = 512;

// Seno de `total` amostras; a amplitude e a frequência são as do teste.
std::vector<float> makeSine (double fs, double frequency, double amplitude, int total)
{
    std::vector<float> s (static_cast<std::size_t> (total));
    for (int n = 0; n < total; ++n)
        s[static_cast<std::size_t> (n)]
            = static_cast<float> (amplitude * std::sin (2.0 * std::numbers::pi * frequency * n / fs));
    return s;
}

// Processa o mesmo sinal nos dois canais. Os blocos seguem `blockSizes` (cíclicos) e nunca
// atravessam `changeAt`, onde os ganhos trocam para `after` (changeAt < 0: sem troca).
// Devolve o canal esquerdo.
std::vector<float> runEq (
    TonalEq& eq, const std::vector<float>& input, const std::vector<int>& blockSizes, int changeAt, const Gains& after)
{
    std::vector<float> left = input;
    std::vector<float> right = input;
    const int total = static_cast<int> (input.size());

    int pos = 0;
    std::size_t index = 0;
    while (pos < total)
    {
        if (pos == changeAt)
            eq.setGainsDecibels (after);

        int n = blockSizes[index % blockSizes.size()];
        ++index;
        n = std::min (n, total - pos);
        if (changeAt > pos)
            n = std::min (n, changeAt - pos);

        float* chans[2] = { left.data() + pos, right.data() + pos };
        eq.process (chans, 2, n);
        pos += n;
    }

    return left;
}

// Ganho em dB de um seno de 1 s (amplitude 0.25), medido no último 0.5 s para descartar o transitório.
double measureGainDb (const Gains& gains, double fs, double frequency)
{
    TonalEq eq;
    eq.setGainsDecibels (gains);
    eq.prepare (fs, 2);

    const int total = static_cast<int> (fs);
    const std::vector<float> in = makeSine (fs, frequency, 0.25, total);
    const std::vector<float> out = runEq (eq, in, { kBlock }, -1, Gains { });

    double inSquares = 0.0;
    double outSquares = 0.0;
    for (int n = total / 2; n < total; ++n)
    {
        const double x = in[static_cast<std::size_t> (n)];
        const double y = out[static_cast<std::size_t> (n)];
        inSquares += x * x;
        outSquares += y * y;
    }

    return 20.0 * std::log10 (std::sqrt (outSquares / inSquares));
}

// Maior curvatura |y[n] - 2y[n-1] + y[n-2]| no intervalo [from, to).
double maxCurvature (const std::vector<float>& y, int from, int to)
{
    double peak = 0.0;
    for (int n = std::max (from, 2); n < to; ++n)
    {
        const std::size_t i = static_cast<std::size_t> (n);
        const double c = static_cast<double> (y[i]) - 2.0 * y[i - 1] + y[i - 2];
        peak = std::max (peak, std::abs (c));
    }
    return peak;
}

// Maior diferença amostra a amostra entre duas saídas.
double maxDifference (const std::vector<float>& a, const std::vector<float>& b)
{
    double peak = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i)
        peak = std::max (peak, std::abs (static_cast<double> (a[i]) - b[i]));
    return peak;
}
} // namespace

TEST_CASE ("ganho 0 é identidade")
{
    TonalEq eq;
    eq.setGainsDecibels (Gains { 0.0f, 0.0f, 0.0f, 0.0f });
    eq.prepare (48000.0, 2);

    std::mt19937 rng (1);
    std::uniform_real_distribution<float> dist (-0.5f, 0.5f);
    std::vector<float> noise (48000);
    for (float& s : noise)
        s = dist (rng);

    const std::vector<float> out = runEq (eq, noise, { kBlock }, -1, Gains { });
    REQUIRE (maxDifference (out, noise) <= 1e-6);
}

TEST_CASE ("peak acerta o ganho no centro")
{
    CHECK (measureGainDb (Gains { 0.0f, 6.0f, 0.0f, 0.0f }, 48000.0, 300.0) == Catch::Approx (6.0).margin (0.1));
    CHECK (measureGainDb (Gains { 0.0f, 0.0f, -4.0f, 0.0f }, 48000.0, 3000.0) == Catch::Approx (-4.0).margin (0.1));
}

TEST_CASE ("low shelf sobe o grave")
{
    const Gains boost { 6.0f, 0.0f, 0.0f, 0.0f };
    CHECK (measureGainDb (boost, 48000.0, 20.0) == Catch::Approx (6.0).margin (0.3));
    CHECK (measureGainDb (boost, 48000.0, 2000.0) == Catch::Approx (0.0).margin (0.2));
}

TEST_CASE ("high shelf sobe o agudo")
{
    const Gains boost { 0.0f, 0.0f, 0.0f, 6.0f };
    CHECK (measureGainDb (boost, 48000.0, 20000.0) == Catch::Approx (6.0).margin (0.3));
    CHECK (measureGainDb (boost, 48000.0, 500.0) == Catch::Approx (0.0).margin (0.2));
}

TEST_CASE ("ganhos limitados")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const std::vector<float> in = makeSine (48000.0, 1000.0, 0.25, 48000);

    TonalEq wild;
    wild.setGainsDecibels (Gains { 40.0f, nan, -40.0f, inf });
    wild.prepare (48000.0, 2);

    TonalEq clamped;
    clamped.setGainsDecibels (Gains { 12.0f, 0.0f, -12.0f, 0.0f });
    clamped.prepare (48000.0, 2);

    const std::vector<float> a = runEq (wild, in, { kBlock }, -1, Gains { });
    const std::vector<float> b = runEq (clamped, in, { kBlock }, -1, Gains { });
    REQUIRE (maxDifference (a, b) <= 1e-6);
}

TEST_CASE ("troca de curva sem estalo")
{
    const double fs = 48000.0;
    const std::vector<float> in = makeSine (fs, 1000.0, 0.25, 2 * 48000);

    TonalEq eq;
    eq.setGainsDecibels (Gains { 0.0f, 0.0f, 0.0f, 0.0f });
    eq.prepare (fs, 2);

    const std::vector<float> out = runEq (eq, in, { kBlock }, 48000, Gains { 6.0f, -6.0f, 6.0f, 6.0f });

    const double transient = maxCurvature (out, 48000, 52800); // [1.0 s, 1.1 s]
    const double steady = maxCurvature (out, 72000, 96000); // [1.5 s, 2.0 s]
    REQUIRE (transient <= 1.5 * steady);
}

TEST_CASE ("eq: bloco variável dá o mesmo resultado")
{
    const std::vector<float> in = makeSine (48000.0, 1000.0, 0.25, 2 * 48000);
    const Gains after { 6.0f, -6.0f, 6.0f, 6.0f };

    TonalEq fixed;
    fixed.setGainsDecibels (Gains { 0.0f, 0.0f, 0.0f, 0.0f });
    fixed.prepare (48000.0, 2);

    TonalEq variable;
    variable.setGainsDecibels (Gains { 0.0f, 0.0f, 0.0f, 0.0f });
    variable.prepare (48000.0, 2);

    const std::vector<float> a = runEq (fixed, in, { kBlock }, 48000, after);
    const std::vector<float> b = runEq (variable, in, { 0, 1, 7, 512, 3 }, 48000, after);
    REQUIRE (maxDifference (a, b) <= 1e-6);
}

TEST_CASE ("eq: entrada não-finita")
{
    std::vector<float> in = makeSine (48000.0, 1000.0, 0.25, 48000);
    in[24000] = std::numeric_limits<float>::quiet_NaN();
    in[24001] = std::numeric_limits<float>::infinity();

    TonalEq eq;
    eq.setGainsDecibels (Gains { 6.0f, 6.0f, 6.0f, 6.0f });
    eq.prepare (48000.0, 2);

    const std::vector<float> out = runEq (eq, in, { kBlock }, -1, Gains { });
    const auto finite = std::count_if (out.begin(), out.end(), [] (float v) { return !std::isfinite (v); });
    REQUIRE (finite == 0);
}

TEST_CASE ("eq: outros sample rates")
{
    CHECK (measureGainDb (Gains { 0.0f, 6.0f, 0.0f, 0.0f }, 44100.0, 300.0) == Catch::Approx (6.0).margin (0.1));
    CHECK (measureGainDb (Gains { 0.0f, 6.0f, 0.0f, 0.0f }, 96000.0, 300.0) == Catch::Approx (6.0).margin (0.1));
}
