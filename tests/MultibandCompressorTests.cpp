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

// Compara a saída com o modelo estático (ratio 4, joelho em −18 dBFS) montado com o mesmo crossover, para o vazamento
// entre bandas entrar dos dois lados. Low e high ficam em ganho 1 (abaixo do limiar); a mid recebe o ganho estático.
// Últimos 1 s de um sinal de 2 s.
void requireMatchesStaticModel (const std::vector<float>& input, const std::vector<float>& output, double fs)
{
    const auto total = static_cast<std::size_t> (2.0 * fs);
    const auto tail = static_cast<std::size_t> (fs);

    ThreeBandCrossover crossover;
    crossover.prepare (fs, 1, 200.0, 2000.0);
    std::vector<double> low (total);
    std::vector<double> mid (total);
    std::vector<double> high (total);
    for (std::size_t n = 0; n < total; ++n)
        crossover.process (0, input[n], low[n], mid[n], high[n]);

    const double thresholdLin = std::pow (10.0, -18.0 / 20.0);
    double maxLow = 0.0;
    double maxMid = 0.0;
    double maxHigh = 0.0;
    for (std::size_t n = total - tail; n < total; ++n)
    {
        maxLow = std::max (maxLow, std::abs (low[n]));
        maxMid = std::max (maxMid, std::abs (mid[n]));
        maxHigh = std::max (maxHigh, std::abs (high[n]));
    }
    REQUIRE (maxLow < thresholdLin);
    REQUIRE (maxHigh < thresholdLin);

    const double overDb = 20.0 * std::log10 (maxMid) + 18.0;
    const double gMid = std::pow (10.0, -overDb * (1.0 - 1.0 / 4.0) / 20.0);

    double modelPow = 0.0;
    double outPow = 0.0;
    for (std::size_t n = total - tail; n < total; ++n)
    {
        const double model = low[n] + gMid * mid[n] + high[n];
        modelPow += model * model;
        outPow += static_cast<double> (output[n]) * output[n];
    }
    REQUIRE (10.0 * std::log10 (outPow / tail) == Catch::Approx (10.0 * std::log10 (modelPow / tail)).margin (0.15));
}

// Seno de 1 kHz a −6 dBFS, 2 s, ratio 4, comparado com o modelo estático.
void requireAboveThresholdCompresses (double fs, int numChannels)
{
    const auto total = static_cast<std::size_t> (2.0 * fs);
    const Stereo in = sine (fs, 2.0, 1000.0, -6.0);

    MultibandCompressor comp;
    comp.prepare (fs, numChannels);
    comp.setRatio (4.0f);
    Stereo s = in;
    processInBlocks (comp, s, 0, total, kBlock, numChannels);

    requireMatchesStaticModel (in.l, s.l, fs);
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
    const Stereo in = sine (48000, 2.0, 1000.0, -6.0);

    MultibandCompressor comp;
    comp.prepare (48000, 2);
    comp.setRatio (4.0f);
    Stereo s = in;
    processInBlocks (comp, s, 0, 96000);
    requireMatchesStaticModel (in.l, s.l, 48000);

    MultibandCompressor unity;
    unity.prepare (48000, 2);
    unity.setRatio (1.0f);
    Stereo u = in;
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

    const Stereo in = sine (48000, 2.0, 1000.0, -6.0);
    Stereo s = in;
    processInBlocks (comp, s, 0, 96000, kBlock, 1);
    requireMatchesStaticModel (in.l, s.l, 48000);
}

TEST_CASE ("detecção linkada entre canais")
{
    // L a −6 dBFS e R a −30 dBFS: R não passa do limiar sozinho, mas tem que levar a mesma redução de L.
    Stereo s = sine (48000, 2.0, 1000.0, -6.0);
    s.r = sine (48000, 2.0, 1000.0, -30.0).l;

    MultibandCompressor comp;
    comp.prepare (48000, 2);
    comp.setRatio (4.0f);
    Stereo c = s;
    processInBlocks (comp, c, 0, 96000);

    MultibandCompressor unity;
    unity.prepare (48000, 2);
    unity.setRatio (1.0f);
    Stereo u = s;
    processInBlocks (unity, u, 0, 96000);

    const double dropL = levelDb (c.l, 48000, 96000) - levelDb (u.l, 48000, 96000);
    const double dropR = levelDb (c.r, 48000, 96000) - levelDb (u.r, 48000, 96000);

    REQUIRE (dropR == Catch::Approx (dropL).margin (0.2));
    REQUIRE (-dropL > 5.0);
}

TEST_CASE ("attack e release têm os tempos certos")
{
    // 0.5 s de silêncio e depois 1 kHz a −6 dBFS. A redução em t é medida na janela de 1 ms que termina em t,
    // comparando ratio 4 com ratio 1 (1 ms = 1 período de 1 kHz a 48 kHz, então o máximo da janela é o pico do tom).
    const std::size_t onset = 24000;
    const std::size_t total = onset + 48000;
    const std::size_t win = 48;

    Stereo s;
    s.l.assign (total, 0.0f);
    const Stereo tone = sine (48000, 1.0, 1000.0, -6.0);
    std::copy (tone.l.begin(), tone.l.end(), s.l.begin() + static_cast<std::ptrdiff_t> (onset));
    s.r = s.l;

    MultibandCompressor comp;
    comp.prepare (48000, 2);
    comp.setRatio (4.0f);
    Stereo c = s;
    processInBlocks (comp, c, 0, total);

    MultibandCompressor unity;
    unity.prepare (48000, 2);
    unity.setRatio (1.0f);
    Stereo u = s;
    processInBlocks (unity, u, 0, total);

    // Redução em dB (positiva) na janela [end − 1 ms, end).
    auto reductionAt = [&] (std::size_t end)
    {
        double peakC = 0.0;
        double peakU = 0.0;
        for (std::size_t n = end - win; n < end; ++n)
        {
            peakC = std::max (peakC, std::abs (static_cast<double> (c.l[n])));
            peakU = std::max (peakU, std::abs (static_cast<double> (u.l[n])));
        }
        return -20.0 * std::log10 (peakC / peakU);
    };

    double rFinal = 0.0;
    for (std::size_t end = total - 9600; end < total; ++end)
        rFinal += reductionAt (end);
    rFinal /= 9600.0;

    REQUIRE (rFinal > 5.0);
    REQUIRE (reductionAt (onset + 48) <= 0.5 * rFinal);
    REQUIRE (reductionAt (onset + 1440) >= 0.8 * rFinal);
}

TEST_CASE ("compressor: outros sample rates")
{
    requireAboveThresholdCompresses (44100, 2);
    requireAboveThresholdCompresses (96000, 2);
}
