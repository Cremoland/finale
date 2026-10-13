#include "DSP/Saturator.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

using finale::dsp::Saturator;

namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr double fs = 48000.0;
constexpr std::size_t oneSecond = 48000;

std::vector<double> sine (double freq, double amplitude, double sampleRate, std::size_t numSamples)
{
    std::vector<double> x (numSamples);
    for (std::size_t n = 0; n < numSamples; ++n)
        x[n] = amplitude * std::sin (2.0 * pi * freq * static_cast<double> (n) / sampleRate);
    return x;
}

// Processa input[from, to) em blocos da sequência `pattern` (repetida). Cada bloco é
// entregue com o mesmo conteúdo em todos os canais, em float, como o host faz. O último
// bloco de cada trecho é cortado em `to`, então trechos consecutivos se encaixam exatamente.
void renderRange (Saturator& saturator, const std::vector<double>& input, int numChannels, std::size_t from,
    std::size_t to, const std::vector<std::size_t>& pattern, std::vector<double>& out)
{
    std::vector<std::vector<float>> channels (static_cast<std::size_t> (numChannels));
    std::vector<float*> pointers (static_cast<std::size_t> (numChannels));

    std::size_t pos = from;
    std::size_t k = 0;
    while (pos < to)
    {
        const std::size_t block = std::min (pattern[k % pattern.size()], to - pos);
        ++k;

        for (std::size_t ch = 0; ch < channels.size(); ++ch)
        {
            channels[ch].resize (block);
            for (std::size_t i = 0; i < block; ++i)
                channels[ch][i] = static_cast<float> (input[pos + i]);
            pointers[ch] = channels[ch].data();
        }

        saturator.process (pointers.data(), numChannels, static_cast<int> (block));

        for (std::size_t i = 0; i < block; ++i)
            out.push_back (static_cast<double> (channels[0][i]));
        pos += block;
    }
}

std::vector<double> render (
    Saturator& saturator, const std::vector<double>& input, int numChannels, const std::vector<std::size_t>& pattern)
{
    std::vector<double> out;
    out.reserve (input.size());
    renderRange (saturator, input, numChannels, 0, input.size(), pattern, out);
    return out;
}

// Magnitude de um bin de DFT: soma x[n] e^{-j 2 pi f n / fs} sobre [first, x.size()), sem janela.
double dftMagnitude (const std::vector<double>& x, std::size_t first, double freq, double sampleRate)
{
    double re = 0.0;
    double im = 0.0;
    for (std::size_t n = first; n < x.size(); ++n)
    {
        const double phase = 2.0 * pi * freq * static_cast<double> (n) / sampleRate;
        re += x[n] * std::cos (phase);
        im -= x[n] * std::sin (phase);
    }
    return std::sqrt (re * re + im * im);
}

// Nível em dB (20·log10 do RMS) de x em [first, last).
double levelDb (const std::vector<double>& x, std::size_t first, std::size_t last)
{
    double sum = 0.0;
    for (std::size_t n = first; n < last; ++n)
        sum += x[n] * x[n];
    return 20.0 * std::log10 (std::sqrt (sum / static_cast<double> (last - first)));
}

// Sequência de um fluxo de mix em 0 até 1 s e 1 depois, com os blocos indicados.
std::vector<double> mixStepRun (const std::vector<std::size_t>& pattern)
{
    const std::vector<double> input = sine (1000.0, 0.5, fs, 2 * oneSecond);

    Saturator saturator;
    saturator.prepare (fs, 2);
    saturator.setDriveDecibels (18.0f);
    saturator.setMix (0.0f);

    std::vector<double> out;
    renderRange (saturator, input, 2, 0, oneSecond, pattern, out);
    saturator.setMix (1.0f);
    renderRange (saturator, input, 2, oneSecond, 2 * oneSecond, pattern, out);
    return out;
}
} // namespace

TEST_CASE ("mix 0 é atraso puro")
{
    const std::vector<double> input = sine (1000.0, 0.9, fs, 2 * oneSecond);

    Saturator saturator;
    saturator.prepare (fs, 2);
    saturator.setDriveDecibels (18.0f);
    saturator.setMix (0.0f);

    const std::vector<double> output = render (saturator, input, 2, { 512 });

    for (std::size_t n = 0; n + Saturator::latency < output.size(); ++n)
        REQUIRE (output[n + Saturator::latency] == Catch::Approx (input[n]).margin (1e-6));
}

TEST_CASE ("sinal pequeno passa intacto")
{
    const std::vector<double> input = sine (1000.0, std::pow (10.0, -60.0 / 20.0), fs, 2 * oneSecond);

    Saturator saturator;
    saturator.prepare (fs, 2);
    saturator.setDriveDecibels (18.0f);
    saturator.setMix (1.0f);

    const std::vector<double> output = render (saturator, input, 2, { 512 });

    const double gainDb = levelDb (output, oneSecond, 2 * oneSecond) - levelDb (input, oneSecond, 2 * oneSecond);
    REQUIRE (gainDb == Catch::Approx (0.0).margin (0.1));
}

TEST_CASE ("sinal alto é saturado")
{
    const std::vector<double> input = sine (1000.0, 1.0, fs, 2 * oneSecond);

    Saturator saturator;
    saturator.prepare (fs, 2);
    saturator.setDriveDecibels (18.0f);
    saturator.setMix (1.0f);

    const std::vector<double> output = render (saturator, input, 2, { 512 });

    double peak = 0.0;
    for (std::size_t n = oneSecond; n < output.size(); ++n)
        peak = std::max (peak, std::abs (output[n]));
    REQUIRE (peak <= 0.15);

    const double h1 = dftMagnitude (output, oneSecond, 1000.0, fs);
    const double h3 = dftMagnitude (output, oneSecond, 3000.0, fs);
    REQUIRE (20.0 * std::log10 (h3 / h1) >= -30.0);
}

TEST_CASE ("aliasing rejeitado")
{
    const std::vector<double> input = sine (10000.0, 1.0, fs, 2 * oneSecond);

    Saturator saturator;
    saturator.prepare (fs, 2);
    saturator.setDriveDecibels (6.0f);
    saturator.setMix (1.0f);

    const std::vector<double> output = render (saturator, input, 2, { 512 });

    const double h10k = dftMagnitude (output, oneSecond, 10000.0, fs);
    const double h18k = dftMagnitude (output, oneSecond, 18000.0, fs);
    REQUIRE (20.0 * std::log10 (h18k / h10k) <= -50.0);
}

TEST_CASE ("mudança de mix sem estalo")
{
    const std::vector<double> output = mixStepRun ({ 512 });

    double maxStep = 0.0;
    for (std::size_t n = oneSecond + Saturator::latency; n < oneSecond + oneSecond / 10; ++n)
        maxStep = std::max (maxStep, std::abs (output[n] - output[n - 1]));
    REQUIRE (maxStep <= 0.07);
}

TEST_CASE ("mudança de drive sem estalo")
{
    const std::vector<double> input = sine (1000.0, 0.5, fs, 2 * oneSecond);

    Saturator saturator;
    saturator.prepare (fs, 2);
    saturator.setDriveDecibels (18.0f);
    saturator.setMix (1.0f);

    std::vector<double> output;
    renderRange (saturator, input, 2, 0, oneSecond, { 512 }, output);
    saturator.setDriveDecibels (30.0f);
    renderRange (saturator, input, 2, oneSecond, 2 * oneSecond, { 512 }, output);

    double maxStep = 0.0;
    for (std::size_t n = oneSecond + Saturator::latency; n < oneSecond + oneSecond / 10; ++n)
        maxStep = std::max (maxStep, std::abs (output[n] - output[n - 1]));
    REQUIRE (maxStep <= 0.07);
}

TEST_CASE ("saturação: bloco variável dá o mesmo resultado")
{
    const std::vector<double> regular = mixStepRun ({ 512 });
    const std::vector<double> irregular = mixStepRun ({ 0, 1, 7, 512, 3 });

    REQUIRE (regular.size() == irregular.size());
    for (std::size_t n = 0; n < regular.size(); ++n)
        REQUIRE (irregular[n] == Catch::Approx (regular[n]).margin (1e-6));
}

TEST_CASE ("saturação: entrada não-finita")
{
    std::vector<double> input = sine (1000.0, 0.5, fs, 2 * oneSecond);
    const std::vector<double> clean = input;
    input[40000] = std::numeric_limits<double>::quiet_NaN();
    input[60000] = std::numeric_limits<double>::infinity();

    Saturator dirty;
    dirty.prepare (fs, 2);
    dirty.setDriveDecibels (18.0f);
    dirty.setMix (1.0f);
    Saturator reference;
    reference.prepare (fs, 2);
    reference.setDriveDecibels (18.0f);
    reference.setMix (1.0f);

    const std::vector<double> out = render (dirty, input, 2, { 512 });
    const std::vector<double> ref = render (reference, clean, 2, { 512 });

    std::size_t nonFinite = 0;
    for (double y : out)
        nonFinite += std::isfinite (y) ? 0 : 1;
    REQUIRE (nonFinite == 0);

    const std::size_t last = 2 * oneSecond;
    REQUIRE (levelDb (out, last - oneSecond / 2, last)
        == Catch::Approx (levelDb (ref, last - oneSecond / 2, last)).margin (0.1));
}

TEST_CASE ("mono e outros sample rates")
{
    struct Config
    {
        double sampleRate;
        int numChannels;
    };

    for (const Config config : { Config { 48000.0, 1 }, Config { 44100.0, 2 }, Config { 96000.0, 2 } })
    {
        const auto rate = static_cast<std::size_t> (config.sampleRate);
        const std::vector<double> input = sine (1000.0, std::pow (10.0, -60.0 / 20.0), config.sampleRate, 2 * rate);

        Saturator saturator;
        saturator.prepare (config.sampleRate, config.numChannels);
        saturator.setDriveDecibels (18.0f);
        saturator.setMix (1.0f);

        const std::vector<double> output = render (saturator, input, config.numChannels, { 512 });

        const double gainDb = levelDb (output, rate, 2 * rate) - levelDb (input, rate, 2 * rate);
        REQUIRE (gainDb == Catch::Approx (0.0).margin (0.1));
    }
}
