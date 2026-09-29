#include "DSP/Gain.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using finale::dsp::Gain;

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;

struct StereoBuffer
{
    explicit StereoBuffer (float value) :
        left (blockSize, value),
        right (blockSize, value)
    {
    }

    float* const* data()
    {
        ptrs[0] = left.data();
        ptrs[1] = right.data();
        return ptrs;
    }

    std::vector<float> left, right;
    float* ptrs[2] { };
};
} // namespace

TEST_CASE ("silêncio entra, silêncio sai")
{
    Gain gain;
    gain.prepare (sampleRate);
    gain.setGainDecibels (12.0f);

    StereoBuffer buffer (0.0f);
    gain.process (buffer.data(), 2, blockSize);

    for (float s : buffer.left)
        REQUIRE (s == 0.0f);
}

TEST_CASE ("0 dB não altera o sinal")
{
    Gain gain;
    gain.prepare (sampleRate);
    gain.setGainDecibels (0.0f);

    StereoBuffer buffer (0.5f);
    gain.process (buffer.data(), 2, blockSize);

    for (float s : buffer.right)
        REQUIRE (s == Catch::Approx (0.5f));
}

TEST_CASE ("mudança de ganho é suave e chega no alvo")
{
    Gain gain;
    gain.prepare (sampleRate, 0.001); // 48 amostras de rampa
    gain.setGainDecibels (-6.0f);

    StereoBuffer buffer (1.0f);
    gain.process (buffer.data(), 2, blockSize);

    for (int i = 1; i < blockSize; ++i)
        REQUIRE (buffer.left[i] <= buffer.left[i - 1]);

    REQUIRE (buffer.left.back() == Catch::Approx (0.501187f).epsilon (1e-4));
}

TEST_CASE ("nunca gera NaN nem infinito nos extremos")
{
    Gain gain;
    gain.prepare (sampleRate);

    for (float db : { -100.0f, 0.0f, 24.0f })
    {
        gain.setGainDecibels (db);
        StereoBuffer buffer (1.0f);
        gain.process (buffer.data(), 2, blockSize);

        for (float s : buffer.left)
            REQUIRE (std::isfinite (s));
    }
}
