#include "DSP/Oversampler.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

using finale::dsp::HalfbandOversampler;

namespace
{
constexpr double pi = 3.14159265358979323846;

// Passa um sinal pela ida e volta (upsample + downsample), uma amostra base por vez.
std::vector<double> roundTrip (const std::vector<double>& input)
{
    HalfbandOversampler oversampler;
    oversampler.prepare (1);

    std::vector<double> output;
    output.reserve (input.size());
    for (double x : input)
    {
        double y0 = 0.0;
        double y1 = 0.0;
        oversampler.upsample (0, x, y0, y1);
        output.push_back (oversampler.downsample (0, y0, y1));
    }
    return output;
}

// Magnitude de um bin de DFT: soma x[n] e^{-j 2 pi f n / fs} sobre a janela, sem janela.
double dftMagnitude (const std::vector<double>& x, std::size_t first, double freq, double fs)
{
    double re = 0.0;
    double im = 0.0;
    for (std::size_t n = first; n < x.size(); ++n)
    {
        const double phase = 2.0 * pi * freq * static_cast<double> (n) / fs;
        re += x[n] * std::cos (phase);
        im -= x[n] * std::sin (phase);
    }
    return std::sqrt (re * re + im * im);
}

double rms (const std::vector<double>& x, std::size_t first)
{
    double sum = 0.0;
    for (std::size_t n = first; n < x.size(); ++n)
        sum += x[n] * x[n];
    return std::sqrt (sum / static_cast<double> (x.size() - first));
}
} // namespace

TEST_CASE ("ida e volta é atraso puro")
{
    constexpr double fs = 48000.0;
    constexpr int numSamples = 48000;

    std::vector<double> input (numSamples);
    for (int n = 0; n < numSamples; ++n)
        input[static_cast<std::size_t> (n)] = 0.5 * std::sin (2.0 * pi * 1000.0 * n / fs);

    const std::vector<double> output = roundTrip (input);

    for (int n = 200; n + HalfbandOversampler::latency < numSamples; ++n)
    {
        const auto i = static_cast<std::size_t> (n);
        const auto o = static_cast<std::size_t> (n + HalfbandOversampler::latency);
        REQUIRE (output[o] == Catch::Approx (input[i]).margin (1e-3));
    }
}

TEST_CASE ("resposta plana na banda útil")
{
    constexpr double fs = 48000.0;
    constexpr int numSamples = 48000; // 1 s
    constexpr std::size_t lastHalf = 24000; // últimos 0.5 s

    const double freqs[] = { 100.0, 1000.0, 10000.0, 15000.0 };
    for (double freq : freqs)
    {
        std::vector<double> input (numSamples);
        for (int n = 0; n < numSamples; ++n)
            input[static_cast<std::size_t> (n)] = std::sin (2.0 * pi * freq * n / fs);

        const std::vector<double> output = roundTrip (input);
        const double gainDb
            = 20.0 * std::log10 (rms (output, numSamples - lastHalf) / rms (input, numSamples - lastHalf));
        REQUIRE (gainDb == Catch::Approx (0.0).margin (0.1));
    }

    std::vector<double> input (numSamples);
    for (int n = 0; n < numSamples; ++n)
        input[static_cast<std::size_t> (n)] = std::sin (2.0 * pi * 20000.0 * n / fs);

    const std::vector<double> output = roundTrip (input);
    const double gainDb = 20.0 * std::log10 (rms (output, numSamples - lastHalf) / rms (input, numSamples - lastHalf));
    REQUIRE (gainDb >= -3.0);
}

TEST_CASE ("rejeita imagem acima de fs/2")
{
    constexpr double fs = 48000.0;
    constexpr double fs2 = 2.0 * fs;
    constexpr int numSamples = 48000; // 1 s na taxa base

    HalfbandOversampler oversampler;
    oversampler.prepare (1);

    std::vector<double> upsampled;
    upsampled.reserve (2 * static_cast<std::size_t> (numSamples));
    for (int n = 0; n < numSamples; ++n)
    {
        const double x = std::sin (2.0 * pi * 10000.0 * n / fs);
        double y0 = 0.0;
        double y1 = 0.0;
        oversampler.upsample (0, x, y0, y1);
        upsampled.push_back (y0);
        upsampled.push_back (y1);
    }

    // Último 0.5 s a 96 kHz.
    const std::size_t first = upsampled.size() - 48000;
    const double tone = dftMagnitude (upsampled, first, 10000.0, fs2);
    const double image = dftMagnitude (upsampled, first, 38000.0, fs2);

    REQUIRE (tone > 0.0);
    REQUIRE (image <= tone * 1e-3); // pelo menos 60 dB abaixo
}

TEST_CASE ("oversampler: entrada não-finita")
{
    HalfbandOversampler oversampler;
    oversampler.prepare (1);

    const double bad[] = { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() };
    for (double x : bad)
    {
        double y0 = 0.0;
        double y1 = 0.0;
        oversampler.upsample (0, x, y0, y1);
        REQUIRE (std::isfinite (y0));
        REQUIRE (std::isfinite (y1));
        REQUIRE (std::isfinite (oversampler.downsample (0, y0, y1)));
    }

    double last = 1.0;
    for (int i = 0; i < 200; ++i)
    {
        double y0 = 0.0;
        double y1 = 0.0;
        oversampler.upsample (0, 0.0, y0, y1);
        last = oversampler.downsample (0, y0, y1);
    }

    REQUIRE (last == Catch::Approx (0.0).margin (1e-9));
}
