#include "DSP/Crossover.h"
#include "DSP/MultibandCompressor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <numbers>
#include <vector>

using finale::dsp::MultibandCompressor;
using finale::dsp::ThreeBandCrossover;

namespace
{
constexpr std::size_t kBlock = 512;

// Sinal estéreo: o mesmo seno nos dois canais.
struct Stereo
{
    std::vector<float> l, r;
};

// Seno de `frequency` com pico `peakDbfs`, de `seconds` segundos.
Stereo sine (double fs, double seconds, double frequency, double peakDbfs)
{
    const double amplitude = std::pow (10.0, peakDbfs / 20.0);
    const auto count = static_cast<std::size_t> (fs * seconds);

    Stereo s;
    s.l.resize (count);
    for (std::size_t n = 0; n < count; ++n)
        s.l[n] = static_cast<float> (amplitude * std::sin (2.0 * std::numbers::pi * frequency * n / fs));
    s.r = s.l;
    return s;
}

// Processa `count` amostras a partir de `start`, com `numChannels` canais.
void processChunk (MultibandCompressor& comp, Stereo& s, std::size_t start, std::size_t count, int numChannels)
{
    float* channels[2] { s.l.data() + start, s.r.data() + start };
    comp.process (channels, numChannels, static_cast<int> (count));
}

// Processa [start, end) em blocos de `block` amostras.
void processInBlocks (MultibandCompressor& comp, Stereo& s, std::size_t start, std::size_t end,
    std::size_t block = kBlock, int numChannels = 2)
{
    for (std::size_t n = start; n < end; n += block)
        processChunk (comp, s, n, std::min (block, end - n), numChannels);
}

// Nível = 20·log10(RMS) de x em [from, to).
double levelDb (const std::vector<float>& x, std::size_t from, std::size_t to)
{
    double sum = 0.0;
    for (std::size_t n = from; n < to; ++n)
        sum += static_cast<double> (x[n]) * x[n];

    return 20.0 * std::log10 (std::sqrt (sum / static_cast<double> (to - from)));
}

// Nível RMS da banda `band` (0 = low, 2 = high) de x, no último 1 s de um sinal de 2 s.
double bandLevelDb (const std::vector<float>& x, double fs, int band)
{
    ThreeBandCrossover crossover;
    crossover.prepare (fs, 1, 200.0, 2000.0);

    const auto total = static_cast<std::size_t> (2.0 * fs);
    const auto tail = static_cast<std::size_t> (fs);
    double sum = 0.0;

    for (std::size_t n = 0; n < total; ++n)
    {
        double low = 0.0;
        double mid = 0.0;
        double high = 0.0;
        crossover.process (0, x[n], low, mid, high);

        if (n >= total - tail)
        {
            const double v = band == 0 ? low : high;
            sum += v * v;
        }
    }

    return 20.0 * std::log10 (std::sqrt (sum / static_cast<double> (tail)));
}

// Seno de 1 kHz a −6 dBFS, 2 s, ratio 4: a saída deve dar −18.1 dBFS RMS.
void requireAboveThresholdCompresses (double fs, int numChannels)
{
    MultibandCompressor comp;
    comp.prepare (fs, numChannels);
    comp.setRatio (4.0f);

    const auto total = static_cast<std::size_t> (2.0 * fs);
    Stereo s = sine (fs, 2.0, 1000.0, -6.0);
    processInBlocks (comp, s, 0, total, kBlock, numChannels);

    REQUIRE (levelDb (s.l, total / 2, total) == Catch::Approx (-15.9).margin (0.5));
}
} // namespace

TEST_CASE ("razão 1 não muda o volume")
{
    for (double f : { 50.0, 1000.0, 8000.0 })
    {
        MultibandCompressor comp;
        comp.prepare (48000, 2);
        comp.setRatio (1.0f);

        Stereo s = sine (48000, 2.0, f, -6.0);
        const double in = levelDb (s.l, 48000, 96000);
        processInBlocks (comp, s, 0, 96000);

        REQUIRE (levelDb (s.l, 48000, 96000) - in == Catch::Approx (0.0).margin (0.01));
    }
}

TEST_CASE ("abaixo do limiar não muda")
{
    MultibandCompressor comp;
    comp.prepare (48000, 2);
    comp.setRatio (4.0f);

    Stereo s = sine (48000, 2.0, 1000.0, -30.0);
    const double in = levelDb (s.l, 48000, 96000);
    processInBlocks (comp, s, 0, 96000);

    REQUIRE (levelDb (s.l, 48000, 96000) - in == Catch::Approx (0.0).margin (0.01));
}

TEST_CASE ("acima do limiar comprime pela razão")
{
    // o detector de pico com release de 120 ms solta um pouco entre os picos do seno, então a redução estabiliza em
    // ~7.6 dB e não nos 8.8 dB da curva estática (conferido com uma implementação independente).
    MultibandCompressor comp;
    comp.prepare (48000, 2);
    comp.setRatio (4.0f);
    Stereo s = sine (48000, 2.0, 1000.0, -6.0);
    processInBlocks (comp, s, 0, 96000);
    REQUIRE (levelDb (s.l, 48000, 96000) == Catch::Approx (-15.9).margin (0.5));

    MultibandCompressor unity;
    unity.prepare (48000, 2);
    unity.setRatio (1.0f);
    Stereo u = sine (48000, 2.0, 1000.0, -6.0);
    processInBlocks (unity, u, 0, 96000);
    REQUIRE (levelDb (u.l, 48000, 96000) - levelDb (s.l, 48000, 96000) >= 5.0);
}

TEST_CASE ("bandas são independentes")
{
    MultibandCompressor comp;
    comp.prepare (48000, 2);
    comp.setRatio (4.0f);

    Stereo s = sine (48000, 2.0, 50.0, -6.0);
    const Stereo hf = sine (48000, 2.0, 8000.0, -30.0);
    for (std::size_t n = 0; n < s.l.size(); ++n)
        s.l[n] += hf.l[n];
    s.r = s.l;

    const std::vector<float> input = s.l;
    processInBlocks (comp, s, 0, s.l.size());

    REQUIRE (std::abs (bandLevelDb (s.l, 48000, 2) - bandLevelDb (input, 48000, 2)) <= 0.2);
    REQUIRE (bandLevelDb (input, 48000, 0) - bandLevelDb (s.l, 48000, 0) > 3.0);
}

TEST_CASE ("mudança de razão sem estalo")
{
    MultibandCompressor comp;
    comp.prepare (48000, 2);
    comp.setRatio (1.0f);

    Stereo s = sine (48000, 2.0, 1000.0, -6.0);
    processInBlocks (comp, s, 0, 48000);

    comp.setRatio (4.0f);
    processInBlocks (comp, s, 48000, 96000);

    double maxStep = 0.0;
    for (std::size_t n = 48000; n < 52800; ++n)
        maxStep = std::max (maxStep, std::abs (static_cast<double> (s.l[n]) - s.l[n - 1]));

    REQUIRE (maxStep <= 0.07);
}

TEST_CASE ("compressor: bloco variável dá o mesmo resultado")
{
    const std::size_t total = 96000;
    const std::size_t pattern[] { 0, 1, 7, 512, 3 };

    MultibandCompressor fixed;
    fixed.prepare (48000, 2);
    fixed.setRatio (4.0f);
    Stereo a = sine (48000, 2.0, 1000.0, -6.0);
    processInBlocks (fixed, a, 0, total);

    MultibandCompressor variable;
    variable.prepare (48000, 2);
    variable.setRatio (4.0f);
    Stereo b = sine (48000, 2.0, 1000.0, -6.0);
    std::size_t pos = 0;
    std::size_t i = 0;
    while (pos < total)
    {
        const std::size_t count = std::min (pattern[i % 5], total - pos);
        processChunk (variable, b, pos, count, 2);
        pos += count;
        ++i;
    }

    double maxDiff = 0.0;
    for (std::size_t n = 0; n < total; ++n)
        maxDiff = std::max (maxDiff, std::abs (static_cast<double> (a.l[n]) - b.l[n]));

    REQUIRE (maxDiff == Catch::Approx (0.0).margin (1e-6));
}

TEST_CASE ("compressor: entrada não-finita")
{
    MultibandCompressor comp;
    comp.prepare (48000, 2);
    comp.setRatio (4.0f);

    Stereo clean = sine (48000, 2.0, 1000.0, -6.0);
    Stereo bad = clean;
    for (std::vector<float>* ch : { &bad.l, &bad.r })
    {
        (*ch)[10000] = std::numeric_limits<float>::quiet_NaN();
        (*ch)[20000] = std::numeric_limits<float>::infinity();
    }

    MultibandCompressor reference;
    reference.prepare (48000, 2);
    reference.setRatio (4.0f);
    processInBlocks (reference, clean, 0, 96000);
    processInBlocks (comp, bad, 0, 96000);

    for (float v : bad.l)
        REQUIRE (std::isfinite (v));

    REQUIRE (levelDb (bad.l, 72000, 96000) == Catch::Approx (levelDb (clean.l, 72000, 96000)).margin (0.1));
}

TEST_CASE ("compressor: mono funciona")
{
    MultibandCompressor comp;
    comp.prepare (48000, 1);
    comp.setRatio (4.0f);

    Stereo s = sine (48000, 2.0, 1000.0, -6.0);
    processInBlocks (comp, s, 0, 96000, kBlock, 1);

    REQUIRE (levelDb (s.l, 48000, 96000) == Catch::Approx (-15.9).margin (0.5));
}

TEST_CASE ("compressor: outros sample rates")
{
    requireAboveThresholdCompresses (44100, 2);
    requireAboveThresholdCompresses (96000, 2);
}
