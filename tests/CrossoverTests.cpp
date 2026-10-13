#include "DSP/Biquad.h"
#include "DSP/Crossover.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numbers>

using finale::dsp::BiquadCoefficients;
using finale::dsp::BiquadState;
using finale::dsp::ThreeBandCrossover;

namespace
{
constexpr double kQ = std::numbers::sqrt2 / 2.0; // 1/√2

// Ganho em dB de um processo linear sobre um seno de amplitude 1 de 1 s,
// medido no último 0.5 s para descartar o transitório.
template <typename Process> double gainDb (double fs, double frequency, Process&& process)
{
    const int total = static_cast<int> (fs);
    const int tailStart = total / 2;
    double inSquares = 0.0;
    double outSquares = 0.0;

    for (int n = 0; n < total; ++n)
    {
        const double x = std::sin (2.0 * std::numbers::pi * frequency * n / fs);
        const double y = process (x);

        if (n >= tailStart)
        {
            inSquares += x * x;
            outSquares += y * y;
        }
    }

    return 20.0 * std::log10 (std::sqrt (outSquares / inSquares));
}

double biquadGainDb (double fs, double frequency, const BiquadCoefficients& c)
{
    BiquadState state;
    return gainDb (fs, frequency, [&] (double x) { return state.process (c, x); });
}

enum class Output
{
    Low,
    Mid,
    High,
    Sum
};

double crossoverGainDb (double fs, double frequency, double f1, double f2, Output output)
{
    ThreeBandCrossover crossover;
    crossover.prepare (fs, 1, f1, f2);

    return gainDb (fs, frequency,
        [&] (double x)
        {
            double low = 0.0;
            double mid = 0.0;
            double high = 0.0;
            crossover.process (0, x, low, mid, high);

            switch (output)
            {
            case Output::Low:
                return low;
            case Output::Mid:
                return mid;
            case Output::High:
                return high;
            case Output::Sum:
                break;
            }
            return low + mid + high;
        });
}
} // namespace

TEST_CASE ("Butterworth passa-baixa dá -3 dB em f0")
{
    const auto c = finale::dsp::makeLowPass (48000, 1000, kQ);
    REQUIRE (biquadGainDb (48000, 1000, c) == Catch::Approx (-3.01).margin (0.05));
}

TEST_CASE ("allpass tem magnitude 1")
{
    const auto c = finale::dsp::makeAllPass (48000, 2000, kQ);

    for (double f : { 100.0, 2000.0, 10000.0 })
        REQUIRE (biquadGainDb (48000, f, c) == Catch::Approx (0.0).margin (0.01));
}

TEST_CASE ("soma das bandas é plana")
{
    for (double f : { 50.0, 200.0, 1000.0, 2000.0, 8000.0 })
        REQUIRE (crossoverGainDb (48000, f, 200.0, 2000.0, Output::Sum) == Catch::Approx (0.0).margin (0.01));
}

TEST_CASE ("Linkwitz-Riley dá -6 dB no corte")
{
    REQUIRE (crossoverGainDb (48000, 200, 200.0, 2000.0, Output::Low) == Catch::Approx (-6.02).margin (0.2));
    REQUIRE (crossoverGainDb (48000, 200, 200.0, 2000.0, Output::Mid) == Catch::Approx (-6.02).margin (0.2));
}

TEST_CASE ("cada banda isola sua faixa")
{
    REQUIRE (crossoverGainDb (48000, 50, 200.0, 2000.0, Output::Low) >= -0.5);
    REQUIRE (crossoverGainDb (48000, 50, 200.0, 2000.0, Output::High) <= -30.0);

    REQUIRE (crossoverGainDb (48000, 10000, 200.0, 2000.0, Output::High) >= -0.5);
    REQUIRE (crossoverGainDb (48000, 10000, 200.0, 2000.0, Output::Low) <= -30.0);
}

TEST_CASE ("soma plana em outros sample rates")
{
    for (double fs : { 44100.0, 96000.0 })
    {
        for (double f : { 50.0, 200.0, 1000.0, 2000.0, 8000.0 })
            REQUIRE (crossoverGainDb (fs, f, 200.0, 2000.0, Output::Sum) == Catch::Approx (0.0).margin (0.01));
    }
}

TEST_CASE ("entrada não-finita")
{
    ThreeBandCrossover crossover;
    crossover.prepare (48000, 1, 200.0, 2000.0);

    const double bad[] { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() };

    for (double x : bad)
    {
        double low = 0.0;
        double mid = 0.0;
        double high = 0.0;
        crossover.process (0, x, low, mid, high);

        REQUIRE (std::isfinite (low));
        REQUIRE (std::isfinite (mid));
        REQUIRE (std::isfinite (high));
    }

    double low = 1.0;
    double mid = 1.0;
    double high = 1.0;
    for (int n = 0; n < 48000; ++n)
        crossover.process (0, 0.0, low, mid, high);

    REQUIRE (low == Catch::Approx (0.0).margin (1e-9));
    REQUIRE (mid == Catch::Approx (0.0).margin (1e-9));
    REQUIRE (high == Catch::Approx (0.0).margin (1e-9));
}
