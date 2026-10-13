#include "DSP/LoudnessMeter.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

using finale::dsp::LoudnessMeter;

namespace
{
constexpr double kFs = 48000.0;
constexpr float kFloor = LoudnessMeter::floorLufs;

// Seno de 1 kHz com pico 10^(dbfs/20), como nos casos do Tech 3341.
std::vector<float> tone (double fs, double freq, double dbfs, double seconds)
{
    const double amplitude = std::pow (10.0, dbfs / 20.0);
    const auto count = static_cast<std::size_t> (std::llround (fs * seconds));
    std::vector<float> out (count);

    for (std::size_t n = 0; n < count; ++n)
        out[n] = static_cast<float> (
            amplitude * std::sin (2.0 * 3.14159265358979323846 * freq * static_cast<double> (n) / fs));

    return out;
}

std::vector<float> silence (double fs, double seconds)
{
    return std::vector<float> (static_cast<std::size_t> (std::llround (fs * seconds)), 0.0f);
}

void append (std::vector<float>& dst, const std::vector<float>& src)
{
    dst.insert (dst.end(), src.begin(), src.end());
}

// Processa [from, to) do sinal (o mesmo vetor nos dois canais) em blocos de blockSize.
void feed (LoudnessMeter& meter, const std::vector<float>& signal, std::size_t& pos, std::size_t to, int blockSize,
    int numChannels = 2)
{
    while (pos < to)
    {
        const auto n = static_cast<int> (std::min<std::size_t> (static_cast<std::size_t> (blockSize), to - pos));
        const float* chans[2] = { signal.data() + pos, signal.data() + pos };
        meter.process (chans, numChannels, n);
        pos += static_cast<std::size_t> (n);
    }
}

void feedAll (LoudnessMeter& meter, const std::vector<float>& signal, int blockSize = 512, int numChannels = 2)
{
    std::size_t pos = 0;
    feed (meter, signal, pos, signal.size(), blockSize, numChannels);
}
} // namespace

TEST_CASE ("Tech 3341 caso 1", "[loudness]")
{
    LoudnessMeter meter;
    meter.prepare (kFs, 2);
    const auto signal = tone (kFs, 1000.0, -23.0, 20.0);
    feedAll (meter, signal);

    REQUIRE (meter.getMomentaryLufs() == Catch::Approx (-23.0).margin (0.1));
    REQUIRE (meter.getShortTermLufs() == Catch::Approx (-23.0).margin (0.1));
    REQUIRE (meter.getIntegratedLufs() == Catch::Approx (-23.0).margin (0.1));
}

TEST_CASE ("Tech 3341 caso 2", "[loudness]")
{
    LoudnessMeter meter;
    meter.prepare (kFs, 2);
    const auto signal = tone (kFs, 1000.0, -33.0, 20.0);
    feedAll (meter, signal);

    REQUIRE (meter.getMomentaryLufs() == Catch::Approx (-33.0).margin (0.1));
    REQUIRE (meter.getShortTermLufs() == Catch::Approx (-33.0).margin (0.1));
    REQUIRE (meter.getIntegratedLufs() == Catch::Approx (-33.0).margin (0.1));
}

TEST_CASE ("Tech 3341 caso 3 (gate relativo)", "[loudness]")
{
    LoudnessMeter meter;
    meter.prepare (kFs, 2);
    std::vector<float> signal;
    append (signal, tone (kFs, 1000.0, -36.0, 10.0));
    append (signal, tone (kFs, 1000.0, -23.0, 60.0));
    append (signal, tone (kFs, 1000.0, -36.0, 10.0));
    feedAll (meter, signal);

    REQUIRE (meter.getIntegratedLufs() == Catch::Approx (-23.0).margin (0.1));
}

TEST_CASE ("Tech 3341 caso 4 (gate absoluto)", "[loudness]")
{
    LoudnessMeter meter;
    meter.prepare (kFs, 2);
    std::vector<float> signal;
    append (signal, tone (kFs, 1000.0, -72.0, 10.0));
    append (signal, tone (kFs, 1000.0, -36.0, 10.0));
    append (signal, tone (kFs, 1000.0, -23.0, 60.0));
    append (signal, tone (kFs, 1000.0, -36.0, 10.0));
    append (signal, tone (kFs, 1000.0, -72.0, 10.0));
    feedAll (meter, signal);

    REQUIRE (meter.getIntegratedLufs() == Catch::Approx (-23.0).margin (0.1));
}

TEST_CASE ("janelas de 400 ms e 3 s", "[loudness]")
{
    LoudnessMeter meter;
    meter.prepare (kFs, 2);
    std::vector<float> signal;
    append (signal, silence (kFs, 5.0));
    append (signal, tone (kFs, 1000.0, -23.0, 4.0));

    const auto toneStart = static_cast<std::size_t> (5.0 * kFs);
    std::size_t pos = 0;

    // 0.2 s depois do início do tom: a janela de 400 ms tem metade de silêncio.
    feed (meter, signal, pos, toneStart + static_cast<std::size_t> (0.2 * kFs), 512);
    REQUIRE (meter.getMomentaryLufs() == Catch::Approx (-26.0).margin (0.2));

    // 1.5 s depois: a janela de 3 s tem metade de silêncio.
    feed (meter, signal, pos, toneStart + static_cast<std::size_t> (1.5 * kFs), 512);
    REQUIRE (meter.getShortTermLufs() == Catch::Approx (-26.0).margin (0.2));

    // 4 s depois: as duas janelas já estão cheias de tom.
    feed (meter, signal, pos, toneStart + static_cast<std::size_t> (4.0 * kFs), 512);
    REQUIRE (meter.getMomentaryLufs() == Catch::Approx (-23.0).margin (0.1));
    REQUIRE (meter.getShortTermLufs() == Catch::Approx (-23.0).margin (0.1));
}

TEST_CASE ("silencio le o piso", "[loudness]")
{
    LoudnessMeter meter;
    meter.prepare (kFs, 2);
    feedAll (meter, silence (kFs, 5.0));

    REQUIRE (meter.getMomentaryLufs() == kFloor);
    REQUIRE (meter.getShortTermLufs() == kFloor);
    REQUIRE (meter.getIntegratedLufs() == kFloor);
}

TEST_CASE ("mono le 3 LU abaixo", "[loudness]")
{
    LoudnessMeter meter;
    meter.prepare (kFs, 1);
    feedAll (meter, tone (kFs, 1000.0, -23.0, 20.0), 512, 1);

    REQUIRE (meter.getIntegratedLufs() == Catch::Approx (-26.0).margin (0.1));
}

TEST_CASE ("sinal alto", "[loudness]")
{
    LoudnessMeter meter;
    meter.prepare (kFs, 2);
    feedAll (meter, tone (kFs, 1000.0, 12.0, 20.0));

    REQUIRE (meter.getIntegratedLufs() == Catch::Approx (12.0).margin (0.1));
}

TEST_CASE ("sessao longa", "[loudness]")
{
    LoudnessMeter meter;
    meter.prepare (kFs, 2);
    feedAll (meter, tone (kFs, 1000.0, -23.0, 600.0));

    REQUIRE (meter.getIntegratedLufs() == Catch::Approx (-23.0).margin (0.1));
}

TEST_CASE ("outros sample rates", "[loudness]")
{
    for (const double fs : { 44100.0, 96000.0 })
    {
        LoudnessMeter meter;
        meter.prepare (fs, 2);
        feedAll (meter, tone (fs, 1000.0, -23.0, 20.0));

        REQUIRE (meter.getIntegratedLufs() == Catch::Approx (-23.0).margin (0.1));
    }
}

TEST_CASE ("bloco variavel da o mesmo resultado", "[loudness]")
{
    std::vector<float> signal;
    append (signal, tone (kFs, 1000.0, -36.0, 10.0));
    append (signal, tone (kFs, 1000.0, -23.0, 60.0));
    append (signal, tone (kFs, 1000.0, -36.0, 10.0));

    LoudnessMeter reference;
    reference.prepare (kFs, 2);
    feedAll (reference, signal, 512);

    LoudnessMeter meter;
    meter.prepare (kFs, 2);
    const std::size_t sequence[] = { 0, 1, 7, 512, 3 };
    std::size_t pos = 0;
    std::size_t step = 0;
    while (pos < signal.size())
    {
        const std::size_t n = std::min (signal.size() - pos, sequence[step % 5]);
        const float* chans[2] = { signal.data() + pos, signal.data() + pos };
        meter.process (chans, 2, static_cast<int> (n)); // n == 0 tambem e chamada valida
        pos += n;
        ++step;
    }

    REQUIRE (meter.getMomentaryLufs() == Catch::Approx (reference.getMomentaryLufs()).margin (1e-4));
    REQUIRE (meter.getShortTermLufs() == Catch::Approx (reference.getShortTermLufs()).margin (1e-4));
    REQUIRE (meter.getIntegratedLufs() == Catch::Approx (reference.getIntegratedLufs()).margin (1e-4));
}

TEST_CASE ("entrada nao-finita", "[loudness]")
{
    auto signal = tone (kFs, 1000.0, -23.0, 20.0);
    const auto at = static_cast<std::size_t> (10.0 * kFs);
    signal[at] = std::numeric_limits<float>::quiet_NaN();
    signal[at + 1] = std::numeric_limits<float>::infinity();

    LoudnessMeter meter;
    meter.prepare (kFs, 2);

    std::size_t pos = 0;
    while (pos < signal.size())
    {
        feed (meter, signal, pos, std::min (signal.size(), pos + 512), 512);
        REQUIRE (std::isfinite (meter.getMomentaryLufs()));
        REQUIRE (std::isfinite (meter.getShortTermLufs()));
        REQUIRE (std::isfinite (meter.getIntegratedLufs()));
    }

    REQUIRE (meter.getIntegratedLufs() == Catch::Approx (-23.0).margin (0.2));
}

TEST_CASE ("prepare de novo zera o integrado", "[loudness]")
{
    LoudnessMeter meter;
    meter.prepare (kFs, 2);
    feedAll (meter, tone (kFs, 1000.0, -23.0, 20.0));
    REQUIRE (meter.getIntegratedLufs() == Catch::Approx (-23.0).margin (0.1));

    meter.prepare (kFs, 2);
    feedAll (meter, silence (kFs, 5.0));
    REQUIRE (meter.getIntegratedLufs() == kFloor);
}
