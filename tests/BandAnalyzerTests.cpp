#include "DSP/BandAnalyzer.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

using finale::dsp::BandAnalyzer;

namespace
{
constexpr double kFs = 48000.0;
constexpr int kTotal = 10 * 48000;

using Shape = std::array<float, 4>;

// Soma de senos de mesma amplitude (1/N, então a soma fica dentro de ±1) com fase aleatória fixa
// (std::mt19937 {7}). Cada seno sai de uma recorrência de fase em double, que é bem mais barata que
// chamar sin() 2000 × 480000 vezes; o erro acumulado fica abaixo de 1e-10.
std::vector<float> sumOfSines (const std::vector<double>& frequencies)
{
    std::mt19937 rng { 7 };
    std::uniform_real_distribution<double> phaseDist (0.0, 2.0 * std::numbers::pi);
    const double amplitude = 1.0 / static_cast<double> (frequencies.size());

    std::vector<double> acc (static_cast<std::size_t> (kTotal), 0.0);
    for (const double frequency : frequencies)
    {
        const double phase = phaseDist (rng);
        double re = std::cos (phase);
        double im = std::sin (phase);
        const double w = 2.0 * std::numbers::pi * frequency / kFs;
        const double cw = std::cos (w);
        const double sw = std::sin (w);

        for (std::size_t n = 0; n < acc.size(); ++n)
        {
            acc[n] += amplitude * re;
            const double nextRe = re * cw - im * sw;
            im = re * sw + im * cw;
            re = nextRe;
        }
    }

    std::vector<float> out (acc.size());
    for (std::size_t n = 0; n < acc.size(); ++n)
        out[n] = static_cast<float> (acc[n]);
    return out;
}

// 40 senos por oitava, de 20 Hz até 20 kHz, em escala log: energia igual em cada oitava.
std::vector<double> pinkFrequencies()
{
    std::vector<double> frequencies;
    for (int k = 0;; ++k)
    {
        const double hz = 20.0 * std::exp2 (k / 40.0);
        if (hz > 20000.0)
            break;
        frequencies.push_back (hz);
    }
    return frequencies;
}

// 2000 senos espaçados linearmente de 20 Hz a 20 kHz.
std::vector<double> whiteFrequencies()
{
    constexpr int count = 2000;
    std::vector<double> frequencies;
    for (int k = 0; k < count; ++k)
        frequencies.push_back (20.0 + (20000.0 - 20.0) * k / (count - 1));
    return frequencies;
}

// O sinal rosa é o mesmo em todos os testes; gerá-lo uma vez só.
const std::vector<float>& pinkSignal()
{
    static const std::vector<float> signal = sumOfSines (pinkFrequencies());
    return signal;
}

Shape analyse (const std::vector<float>& signal)
{
    BandAnalyzer analyzer;
    analyzer.prepare (kFs);
    analyzer.process (signal.data(), static_cast<int> (signal.size()));
    return analyzer.getShapeDb();
}
} // namespace

TEST_CASE ("rosa dá forma plana")
{
    const Shape shape = analyse (pinkSignal());
    for (std::size_t b = 0; b < shape.size(); ++b)
        CHECK (shape[b] == Catch::Approx (0.0).margin (0.75));
}

TEST_CASE ("branco sobe com a frequência")
{
    const Shape shape = analyse (sumOfSines (whiteFrequencies()));
    // Energia proporcional à largura da banda: 10·log10(10000 / 90) dB.
    CHECK (shape[3] - shape[0] == Catch::Approx (10.0 * std::log10 (10000.0 / 90.0)).margin (1.5));
}

TEST_CASE ("volume não muda a forma")
{
    std::vector<float> quiet = pinkSignal();
    for (float& sample : quiet)
        sample *= 0.01f;

    const Shape loud = analyse (pinkSignal());
    const Shape soft = analyse (quiet);
    for (std::size_t b = 0; b < loud.size(); ++b)
        CHECK (soft[b] == Catch::Approx (loud[b]).margin (1e-3));
}

TEST_CASE ("seno cai na banda certa")
{
    std::vector<float> sine (static_cast<std::size_t> (kTotal));
    for (std::size_t n = 0; n < sine.size(); ++n)
        sine[n] = static_cast<float> (0.5 * std::sin (2.0 * std::numbers::pi * 300.0 * static_cast<double> (n) / kFs));

    const Shape shape = analyse (sine);
    for (std::size_t b = 0; b < shape.size(); ++b)
        if (b != 1)
            CHECK (shape[1] - shape[b] > 20.0f);
}

TEST_CASE ("analisador: sem sinal")
{
    BandAnalyzer analyzer;
    analyzer.prepare (kFs);

    const Shape none = analyzer.getShapeDb();
    for (const float value : none)
        CHECK (value == 0.0f);

    const std::vector<float> zeros (static_cast<std::size_t> (kTotal), 0.0f);
    analyzer.process (zeros.data(), static_cast<int> (zeros.size()));
    const Shape silent = analyzer.getShapeDb();
    for (const float value : silent)
        CHECK (value == 0.0f);
}

TEST_CASE ("analisador: entrada não-finita")
{
    std::vector<float> dirty = pinkSignal();
    dirty[240000] = std::numeric_limits<float>::quiet_NaN();
    dirty[240001] = std::numeric_limits<float>::infinity();

    const Shape clean = analyse (pinkSignal());
    const Shape shape = analyse (dirty);
    for (std::size_t b = 0; b < shape.size(); ++b)
    {
        CHECK (std::isfinite (shape[b]));
        CHECK (shape[b] == Catch::Approx (clean[b]).margin (0.1));
    }
}
