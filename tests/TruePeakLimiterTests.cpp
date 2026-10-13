#include "DSP/TruePeakDetector.h"
#include "DSP/TruePeakLimiter.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <random>
#include <vector>

using finale::dsp::TruePeakDetector;
using finale::dsp::TruePeakLimiter;

namespace
{
constexpr double sampleRate = 48000.0;
constexpr double pi = 3.14159265358979323846;
constexpr float ceilingLin = 0.8913f; // -1 dBTP
constexpr float measureTolerance = 1.001f; // 0.1% de tolerância de medição

// Pico verdadeiro medido por um detector novo, considerando só os intervalos que
// começam em `start` ou depois. O sinal inteiro é empurrado para o detector: cortar
// a fatia criaria um degrau artificial no início, que o detector frio leria como
// overshoot.
float truePeakFrom (const std::vector<float>& signal, std::size_t start)
{
    TruePeakDetector detector;
    detector.prepare (1);

    const auto latency = static_cast<std::ptrdiff_t> (TruePeakDetector::latency);
    const auto total = static_cast<std::ptrdiff_t> (signal.size()) + latency + 6;

    float peak = 0.0f;
    for (std::ptrdiff_t i = 0; i < total; ++i)
    {
        const float sample
            = i < static_cast<std::ptrdiff_t> (signal.size()) ? signal[static_cast<std::size_t> (i)] : 0.0f;
        const float value = detector.push (0, sample);

        // O resultado do push i é o intervalo que começa em i - latency.
        const std::ptrdiff_t interval = i - latency;
        if (interval >= 0 && static_cast<std::size_t> (interval) >= start)
            peak = std::max (peak, value);
    }

    return peak;
}

float truePeak (const std::vector<float>& signal)
{
    return truePeakFrom (signal, 0);
}

std::vector<float> sine (double frequency, float amplitude, std::size_t numSamples, double fs)
{
    std::vector<float> out (numSamples);
    for (std::size_t n = 0; n < numSamples; ++n)
        out[n] = amplitude * static_cast<float> (std::sin (2.0 * pi * frequency * static_cast<double> (n) / fs));
    return out;
}

// Ruído uniforme em [-2, 2] com semente fixa: o mesmo ruído em todo teste.
std::vector<float> noise (std::size_t numSamples)
{
    std::mt19937 generator { 42 };
    std::uniform_real_distribution<float> distribution (-2.0f, 2.0f);

    std::vector<float> out (numSamples);
    for (float& s : out)
        s = distribution (generator);
    return out;
}

// Processa todos os canais em blocos cujos tamanhos se repetem em ciclo.
void processInBlocks (
    TruePeakLimiter& limiter, std::vector<std::vector<float>>& channels, const std::vector<int>& blockSizes)
{
    const std::size_t total = channels.front().size();
    std::vector<float*> pointers (channels.size());
    std::size_t offset = 0;
    std::size_t k = 0;

    while (offset < total)
    {
        const std::size_t size
            = std::min (static_cast<std::size_t> (blockSizes[k % blockSizes.size()]), total - offset);

        for (std::size_t ch = 0; ch < channels.size(); ++ch)
            pointers[ch] = channels[ch].data() + offset;

        limiter.process (pointers.data(), static_cast<int> (channels.size()), static_cast<int> (size));
        offset += size;
        ++k;
    }
}

// Seno de 997 Hz com +12 dBFS, limitado a -1 dBTP: o caso central do limiter.
void requireLoudSineUnderCeiling (double fs, int numChannels)
{
    TruePeakLimiter limiter;
    limiter.prepare (fs, numChannels);
    limiter.setCeilingDecibels (-1.0f);

    const std::vector<float> input = sine (997.0, 4.0f, static_cast<std::size_t> (fs), fs);
    std::vector<std::vector<float>> channels (static_cast<std::size_t> (numChannels), input);
    processInBlocks (limiter, channels, { 512 });

    for (const auto& channel : channels)
    {
        for (float s : channel)
            REQUIRE (std::abs (s) <= ceilingLin);

        REQUIRE (truePeak (channel) <= ceilingLin * measureTolerance);
    }
}
} // namespace

TEST_CASE ("sinal alto fica abaixo do teto")
{
    requireLoudSineUnderCeiling (sampleRate, 2);
}

TEST_CASE ("sinal baixo passa intacto")
{
    TruePeakLimiter limiter;
    limiter.prepare (sampleRate, 1);
    limiter.setCeilingDecibels (-1.0f);

    const std::vector<float> input = sine (997.0, 0.1f, 24000, sampleRate);
    std::vector<std::vector<float>> channels (1, input);
    processInBlocks (limiter, channels, { 512 });

    const auto latency = static_cast<std::size_t> (limiter.getLatencySamples());
    for (std::size_t n = 0; n + latency < input.size(); ++n)
        REQUIRE (channels[0][n + latency] == Catch::Approx (input[n]).margin (1e-6));
}

TEST_CASE ("latência bate com o atraso real")
{
    TruePeakLimiter limiter;
    limiter.prepare (sampleRate, 1);
    limiter.setCeilingDecibels (-1.0f);

    std::vector<float> impulse (1000, 0.0f);
    impulse[100] = 0.5f;
    std::vector<std::vector<float>> channels (1, impulse);
    processInBlocks (limiter, channels, { 512 });

    const auto& out = channels[0];
    const auto loudest
        = std::max_element (out.begin(), out.end(), [] (float a, float b) { return std::abs (a) < std::abs (b); });
    const auto peakIndex = std::distance (out.begin(), loudest);

    REQUIRE (peakIndex == 100 + limiter.getLatencySamples());
}

TEST_CASE ("bloco variável dá o mesmo resultado")
{
    const std::vector<float> input = noise (48000);
    std::vector<std::vector<float>> fixedBlocks (1, input);
    std::vector<std::vector<float>> variableBlocks (1, input);

    TruePeakLimiter fixed;
    fixed.prepare (sampleRate, 1);
    fixed.setCeilingDecibels (-1.0f);

    TruePeakLimiter variable;
    variable.prepare (sampleRate, 1);
    variable.setCeilingDecibels (-1.0f);

    processInBlocks (fixed, fixedBlocks, { 512 });
    processInBlocks (variable, variableBlocks, { 0, 1, 7, 512, 3 });

    for (std::size_t n = 0; n < input.size(); ++n)
        REQUIRE (variableBlocks[0][n] == Catch::Approx (fixedBlocks[0][n]).margin (1e-6));
}

TEST_CASE ("nunca gera NaN")
{
    std::vector<float> input = sine (997.0, 2.0f, 4800, sampleRate);
    input[1000] = std::numeric_limits<float>::quiet_NaN();
    input[1001] = std::numeric_limits<float>::infinity();
    input[1002] = 1e6f;
    input[1003] = -1e6f;

    TruePeakLimiter limiter;
    limiter.prepare (sampleRate, 1);
    limiter.setCeilingDecibels (-1.0f);

    std::vector<std::vector<float>> channels (1, input);
    processInBlocks (limiter, channels, { 512 });

    for (float s : channels[0])
    {
        REQUIRE (std::isfinite (s));
        REQUIRE (std::abs (s) <= ceilingLin);
    }
}

TEST_CASE ("teto abaixado no meio")
{
    constexpr std::size_t total = 48000;
    constexpr std::size_t switchAt = 12000; // 0.25 s

    std::vector<std::vector<float>> channels (1, sine (997.0, 2.0f, total, sampleRate));

    TruePeakLimiter limiter;
    limiter.prepare (sampleRate, 1);
    limiter.setCeilingDecibels (0.0f);

    float* first[1] = { channels[0].data() };
    limiter.process (first, 1, static_cast<int> (switchAt));

    limiter.setCeilingDecibels (-6.0f);
    float* second[1] = { channels[0].data() + switchAt };
    limiter.process (second, 1, static_cast<int> (total - switchAt));

    const std::size_t from = switchAt + static_cast<std::size_t> (limiter.getLatencySamples());
    const std::vector<float>& out = channels[0];

    for (std::size_t n = from; n < total; ++n)
        REQUIRE (std::abs (out[n]) <= 0.5012f);

    REQUIRE (truePeakFrom (out, from) <= 0.5012f * measureTolerance);
}

TEST_CASE ("prepare de novo zera o estado")
{
    TruePeakLimiter limiter;
    limiter.prepare (sampleRate, 1);

    std::vector<std::vector<float>> loud (1, noise (4800));
    processInBlocks (limiter, loud, { 512 });

    limiter.prepare (96000.0, 1);
    REQUIRE (limiter.getLatencySamples() == TruePeakDetector::latency + 192 - 1);

    std::vector<std::vector<float>> silence (1, std::vector<float> (1000, 0.0f));
    processInBlocks (limiter, silence, { 512 });

    for (float s : silence[0])
        REQUIRE (s == 0.0f);
}

TEST_CASE ("mono funciona")
{
    requireLoudSineUnderCeiling (sampleRate, 1);
}

TEST_CASE ("funciona em vários sample rates")
{
    for (double fs : { 44100.0, 88200.0, 192000.0 })
        requireLoudSineUnderCeiling (fs, 2);
}
