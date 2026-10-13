#include "DSP/TruePeakDetector.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

using finale::dsp::TruePeakDetector;

namespace
{
constexpr int warmup = 12; // tamanho do histórico por canal
constexpr double pi = 3.14159265358979323846;
} // namespace

TEST_CASE ("silêncio dá pico zero")
{
    TruePeakDetector detector;
    detector.prepare (1);

    for (int i = 0; i < 256; ++i)
        REQUIRE (detector.push (0, 0.0f) == 0.0f);
}

TEST_CASE ("DC passa intacto")
{
    TruePeakDetector detector;
    detector.prepare (1);

    for (int i = 0; i < 256; ++i)
    {
        const float peak = detector.push (0, 0.5f);
        if (i >= warmup)
            REQUIRE (peak == Catch::Approx (0.5f).epsilon (1e-4));
    }
}

TEST_CASE ("acha o pico entre amostras")
{
    // Seno em fs/4 com fase pi/4: as amostras valem +-0.7071, o pico real é 1.0
    // entre elas.
    TruePeakDetector detector;
    detector.prepare (1);

    float maxPeak = 0.0f;
    for (int n = 0; n < 256; ++n)
    {
        const float sample = static_cast<float> (std::sin (pi / 2.0 * n + pi / 4.0));
        const float peak = detector.push (0, sample);
        if (n >= warmup)
            maxPeak = std::max (maxPeak, peak);
    }

    REQUIRE (maxPeak >= 0.98f);
    REQUIRE (maxPeak <= 1.02f);
}

TEST_CASE ("entrada não-finita não contamina a saída")
{
    TruePeakDetector detector;
    detector.prepare (1);

    const std::vector<float> input = []
    {
        std::vector<float> v (20, 0.5f);
        v.push_back (std::numeric_limits<float>::quiet_NaN());
        v.push_back (std::numeric_limits<float>::infinity());
        v.push_back (-std::numeric_limits<float>::infinity());
        v.insert (v.end(), 20, 0.5f);
        return v;
    }();

    std::vector<float> peaks;
    for (float sample : input)
    {
        const float peak = detector.push (0, sample);
        REQUIRE (std::isfinite (peak));
        REQUIRE (peak <= 1.0f); // overshoot limitado: nada explode
        peaks.push_back (peak);
    }

    // Os 3 valores ruins saem do histórico de 12 amostras até o índice 34;
    // do índice 35 em diante (últimas 8) a saída volta a ser o DC de 0.5.
    for (std::size_t i = peaks.size() - 8; i < peaks.size(); ++i)
        REQUIRE (peaks[i] == Catch::Approx (0.5f).epsilon (1e-4));
}

TEST_CASE ("canais são independentes")
{
    TruePeakDetector detector;
    detector.prepare (2);

    for (int i = 0; i < 256; ++i)
    {
        const float loud = detector.push (0, 1.0f);
        const float silent = detector.push (1, 0.0f);

        if (i >= warmup)
        {
            REQUIRE (loud == Catch::Approx (1.0f).epsilon (1e-4));
            REQUIRE (silent == 0.0f);
        }
    }
}
