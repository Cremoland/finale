#include "DSP/KWeighting.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numbers>

using finale::dsp::KWeighting;

namespace
{
// Ganho em dB de um seno de amplitude 1, medido sobre o último 0.5 s de 1 s
// para descartar o transitório do filtro.
double measureGainDb (double sampleRate, double frequency)
{
    KWeighting kw;
    kw.prepare (sampleRate, 1);

    const int total = static_cast<int> (sampleRate);
    const int tailStart = total / 2;
    double inSquares = 0.0;
    double outSquares = 0.0;

    for (int n = 0; n < total; ++n)
    {
        const double x = std::sin (2.0 * std::numbers::pi * frequency * n / sampleRate);
        const double y = kw.process (0, x);

        if (n >= tailStart)
        {
            inSquares += x * x;
            outSquares += y * y;
        }
    }

    return 20.0 * std::log10 (std::sqrt (outSquares / inSquares));
}
} // namespace

TEST_CASE ("coeficientes a 48 kHz batem com a norma")
{
    KWeighting kw;
    kw.prepare (48000, 1);

    const auto& s1 = kw.stage1();
    REQUIRE (s1.b0 == Catch::Approx (1.53512485958697).margin (1e-6));
    REQUIRE (s1.b1 == Catch::Approx (-2.69169618940638).margin (1e-6));
    REQUIRE (s1.b2 == Catch::Approx (1.19839281085285).margin (1e-6));
    REQUIRE (s1.a1 == Catch::Approx (-1.69065929318241).margin (1e-6));
    REQUIRE (s1.a2 == Catch::Approx (0.73248077421585).margin (1e-6));

    const auto& s2 = kw.stage2();
    REQUIRE (s2.b0 == Catch::Approx (1.0).margin (1e-6));
    REQUIRE (s2.b1 == Catch::Approx (-2.0).margin (1e-6));
    REQUIRE (s2.b2 == Catch::Approx (1.0).margin (1e-6));
    REQUIRE (s2.a1 == Catch::Approx (-1.99004745483398).margin (1e-6));
    REQUIRE (s2.a2 == Catch::Approx (0.99007225036621).margin (1e-6));
}

TEST_CASE ("ganho em 1 kHz compensa o -0.691")
{
    const double gainDb = measureGainDb (48000.0, 1000.0);

    REQUIRE (gainDb == Catch::Approx (0.691).margin (0.01));
}

TEST_CASE ("resposta igual em outros sample rates")
{
    const double frequencies[] { 100.0, 1000.0, 5000.0 };
    const double rates[] { 44100.0, 96000.0, 192000.0 };

    for (double f : frequencies)
    {
        const double reference = measureGainDb (48000.0, f);

        for (double rate : rates)
            REQUIRE (std::abs (measureGainDb (rate, f) - reference) <= 0.1);
    }
}

TEST_CASE ("entrada não-finita vira silêncio")
{
    KWeighting kw;
    kw.prepare (48000, 1);

    const double bad[] { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() };

    for (double x : bad)
        REQUIRE (std::isfinite (kw.process (0, x)));

    double last = 1.0;
    for (int n = 0; n < 48000; ++n)
    {
        last = kw.process (0, 0.0);
        REQUIRE (std::isfinite (last));
    }

    REQUIRE (last == Catch::Approx (0.0).margin (1e-9));
}
