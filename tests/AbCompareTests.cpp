#include "DSP/AbCompare.h"
#include "DSP/TruePeakLimiter.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <random>
#include <vector>

using finale::dsp::AbCompare;
using finale::dsp::TruePeakLimiter;

namespace
{
constexpr double sampleRate = 48000.0;
constexpr double pi = 3.14159265358979323846;
constexpr int dryDelay = 31;
constexpr std::size_t blockSize = 512;
constexpr std::size_t oneSecond = 48000;

using Channels = std::vector<std::vector<float>>;

Channels stereo (const std::vector<float>& mono)
{
    return { mono, mono };
}

std::vector<float> tone (double frequency, float amplitude, double phase, std::size_t numSamples)
{
    std::vector<float> out (numSamples);
    for (std::size_t i = 0; i < numSamples; ++i)
        out[i] = static_cast<float> (
            amplitude * std::cos (2.0 * pi * frequency * static_cast<double> (i) / sampleRate + phase));
    return out;
}

// Seno: cosseno defasado em -pi/2.
std::vector<float> sine (double frequency, float amplitude, std::size_t numSamples)
{
    return tone (frequency, amplitude, -pi / 2.0, numSamples);
}

std::vector<float> noise (unsigned seed, std::size_t numSamples)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> dist (-0.5f, 0.5f);
    std::vector<float> out (numSamples);
    for (auto& x : out)
        x = dist (rng);
    return out;
}

std::vector<float> zeros (std::size_t numSamples)
{
    return std::vector<float> (numSamples, 0.0f);
}

// Nível RMS em dB no intervalo [from, to).
double levelDb (const std::vector<float>& x, std::size_t from, std::size_t to)
{
    double sum = 0.0;
    for (std::size_t i = from; i < to; ++i)
        sum += static_cast<double> (x[i]) * static_cast<double> (x[i]);
    return 20.0 * std::log10 (std::sqrt (sum / static_cast<double> (to - from)));
}

// Maior |segunda diferença| em [from, to): mede saltos e cliques.
double maxSecondDifference (const std::vector<float>& y, std::size_t from, std::size_t to)
{
    double peak = 0.0;
    for (std::size_t i = from; i < to; ++i)
        peak = std::max (peak, std::abs (static_cast<double> (y[i]) - 2.0 * y[i - 1] + y[i - 2]));
    return peak;
}

// Processa out[from, to) com blocos cujos tamanhos seguem `pattern` ciclicamente.
// out entra com o processado (wet) e sai com o resultado do A/B.
void runRange (AbCompare& ab, const Channels& dry, Channels& out, std::size_t from, std::size_t to,
    const std::vector<std::size_t>& pattern)
{
    std::vector<const float*> dryPtr (out.size());
    std::vector<float*> wetPtr (out.size());

    for (std::size_t k = 0, pos = from; pos < to; ++k)
    {
        const std::size_t len = std::min (pattern[k % pattern.size()], to - pos);
        for (std::size_t ch = 0; ch < out.size(); ++ch)
        {
            dryPtr[ch] = dry[ch].data() + pos;
            wetPtr[ch] = out[ch].data() + pos;
        }
        ab.process (dryPtr.data(), wetPtr.data(), static_cast<int> (out.size()), static_cast<int> (len));
        pos += len;
    }
}

int limiterLatency()
{
    TruePeakLimiter limiter;
    limiter.prepare (sampleRate, 2);
    return limiter.getLatencySamples();
}

// Com compare ligado e wet zerado, a saida tem que ser o dry atrasado pela cadeia inteira.
// Os blocos de processamento (blockSize) podem ser maiores que o maxBlockSize do prepare.
void requireAlignedOriginal (int numChannels, int maxBlockSize = static_cast<int> (blockSize))
{
    AbCompare ab;
    ab.setCompare (true);
    ab.prepare (sampleRate, numChannels, dryDelay, maxBlockSize);

    const std::size_t lat = static_cast<std::size_t> (dryDelay + limiterLatency());
    const std::size_t n = 4096;

    const Channels dry (static_cast<std::size_t> (numChannels), sine (1000.0, 0.25f, n));
    Channels out (static_cast<std::size_t> (numChannels), zeros (n));
    runRange (ab, dry, out, 0, n, { blockSize });

    for (std::size_t ch = 0; ch < out.size(); ++ch)
        for (std::size_t i = 200; i + lat < n; ++i)
            REQUIRE (out[ch][i + lat] == Catch::Approx (dry[ch][i]).margin (1e-5));
}
} // namespace

TEST_CASE ("compare desligado não mexe no processado")
{
    AbCompare ab;
    ab.prepare (sampleRate, 2, dryDelay, static_cast<int> (blockSize));

    const std::size_t n = 4096;
    const Channels dry = stereo (noise (1, n));
    const Channels wet = stereo (noise (2, n));

    Channels out = wet;
    runRange (ab, dry, out, 0, n, { blockSize });

    int diffs = 0;
    for (std::size_t ch = 0; ch < out.size(); ++ch)
        for (std::size_t i = 0; i < n; ++i)
            diffs += out[ch][i] != wet[ch][i] ? 1 : 0;

    REQUIRE (diffs == 0);
}

TEST_CASE ("compare ligado toca o original alinhado")
{
    requireAlignedOriginal (2);
}

TEST_CASE ("ganho de igualação é aplicado")
{
    AbCompare ab;
    ab.setCompare (true);
    ab.setMatchGainDecibels (6.0f);
    ab.prepare (sampleRate, 2, dryDelay, static_cast<int> (blockSize));

    const std::size_t n = 2 * oneSecond;
    const Channels dry = stereo (sine (1000.0, 0.1f, n));
    Channels out = stereo (zeros (n));
    runRange (ab, dry, out, 0, n, { blockSize });

    const double gainDb = levelDb (out[0], n - oneSecond, n) - levelDb (dry[0], n - oneSecond, n);
    REQUIRE (gainDb == Catch::Approx (6.02).margin (0.05));
}

TEST_CASE ("original igualado respeita o teto")
{
    AbCompare ab;
    ab.setCompare (true);
    ab.setMatchGainDecibels (24.0f);
    ab.setCeilingDecibels (-1.0f);
    ab.prepare (sampleRate, 2, dryDelay, static_cast<int> (blockSize));

    const std::size_t n = 4096;
    const Channels dry = stereo (sine (1000.0, 0.5f, n));
    Channels out = stereo (zeros (n));
    runRange (ab, dry, out, 0, n, { blockSize });

    float peak = 0.0f;
    for (std::size_t ch = 0; ch < out.size(); ++ch)
        for (std::size_t i = 0; i < n; ++i)
        {
            REQUIRE (std::abs (out[ch][i]) <= 0.891251f);
            peak = std::max (peak, std::abs (out[ch][i]));
        }

    // Sanidade: o teste só vale se o limiter de fato esta atuando.
    REQUIRE (peak > 0.8f);
}

TEST_CASE ("troca sem estalo")
{
    AbCompare ab;
    ab.prepare (sampleRate, 2, dryDelay, static_cast<int> (blockSize));

    const std::size_t n = 2 * oneSecond;
    const Channels dry = stereo (tone (1000.0, 0.25f, 0.0, n)); // cosseno
    Channels out = stereo (sine (1000.0, 0.25f, n));

    runRange (ab, dry, out, 0, oneSecond, { blockSize });
    ab.setCompare (true);
    runRange (ab, dry, out, oneSecond, n, { blockSize });

    const double early = maxSecondDifference (out[0], oneSecond, oneSecond + oneSecond / 10);
    const double late = maxSecondDifference (out[0], oneSecond + oneSecond / 2, n);
    REQUIRE (early <= 1.5 * late);
}

TEST_CASE ("ganho inválido mantém o anterior")
{
    AbCompare ab;
    ab.setCompare (true);
    ab.setMatchGainDecibels (6.0f);
    ab.prepare (sampleRate, 2, dryDelay, static_cast<int> (blockSize));

    const std::size_t n = 2 * oneSecond;
    const Channels dry = stereo (sine (1000.0, 0.1f, n));
    Channels out = stereo (zeros (n));

    runRange (ab, dry, out, 0, oneSecond, { blockSize });
    ab.setMatchGainDecibels (std::numeric_limits<float>::quiet_NaN());
    runRange (ab, dry, out, oneSecond, oneSecond + oneSecond / 2, { blockSize });
    ab.setMatchGainDecibels (std::numeric_limits<float>::infinity());
    runRange (ab, dry, out, oneSecond + oneSecond / 2, n, { blockSize });

    const std::size_t from = n - oneSecond / 2;
    const double gainDb = levelDb (out[0], from, n) - levelDb (dry[0], from, n);
    REQUIRE (gainDb == Catch::Approx (6.02).margin (0.05));
}

TEST_CASE ("ab: bloco variável dá o mesmo resultado")
{
    const std::size_t n = 2 * oneSecond;
    const Channels dry = stereo (tone (1000.0, 0.25f, 0.0, n)); // cosseno
    const Channels wet = stereo (sine (1000.0, 0.25f, n));

    // Mesma troca em dois esquemas de bloco. A troca cai na amostra 48000 nos dois.
    auto render = [&] (const std::vector<std::size_t>& pattern)
    {
        AbCompare ab;
        ab.prepare (sampleRate, 2, dryDelay, static_cast<int> (blockSize));
        Channels out = wet;
        runRange (ab, dry, out, 0, oneSecond, pattern);
        ab.setCompare (true);
        runRange (ab, dry, out, oneSecond, n, pattern);
        return out;
    };

    const Channels reference = render ({ blockSize });
    const Channels variable = render ({ 0, 1, 7, blockSize, 3 });

    int mismatches = 0;
    for (std::size_t ch = 0; ch < 2; ++ch)
        for (std::size_t i = 0; i < n; ++i)
            mismatches += std::abs (variable[ch][i] - reference[ch][i]) > 1e-6f ? 1 : 0;

    REQUIRE (mismatches == 0);
}

TEST_CASE ("ab: entrada não-finita")
{
    AbCompare ab;
    ab.setCompare (true);
    ab.prepare (sampleRate, 2, dryDelay, static_cast<int> (blockSize));

    const std::size_t n = 4096;
    Channels dry = stereo (sine (1000.0, 0.25f, n));
    dry[0][100] = std::numeric_limits<float>::quiet_NaN();
    dry[1][200] = std::numeric_limits<float>::infinity();

    Channels out = stereo (sine (440.0, 0.25f, n));
    out[0][50] = std::numeric_limits<float>::quiet_NaN();
    out[1][60] = std::numeric_limits<float>::infinity();

    runRange (ab, dry, out, 0, n, { blockSize });

    for (std::size_t ch = 0; ch < out.size(); ++ch)
        for (std::size_t i = 0; i < n; ++i)
            REQUIRE (std::isfinite (out[ch][i]));
}

TEST_CASE ("ab: mono")
{
    requireAlignedOriginal (1);
}

TEST_CASE ("ab: bloco maior que o buffer")
{
    // Prepara com 64 amostras e processa blocos de 512: o alinhamento tem que se manter.
    requireAlignedOriginal (2, 64);
}
