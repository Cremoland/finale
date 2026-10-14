#include "DSP/StereoWidth.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <random>
#include <vector>

using finale::dsp::StereoWidth;

namespace
{
constexpr double pi = 3.14159265358979323846;
constexpr double fs = 48000.0;
constexpr std::size_t oneSecond = 48000;

const std::vector<std::size_t> fixedBlocks { 512 };
const std::vector<std::size_t> variableBlocks { 0, 1, 7, 512, 3 };

std::vector<float> sine (double freq, double amplitude, double sampleRate, std::size_t numSamples)
{
    std::vector<float> x (numSamples);
    for (std::size_t n = 0; n < numSamples; ++n)
        x[n] = static_cast<float> (amplitude * std::sin (2.0 * pi * freq * static_cast<double> (n) / sampleRate));
    return x;
}

// Ruído estéreo: dois geradores com seeds diferentes, uniforme em [−0.5, 0.5].
void stereoNoise (std::size_t numSamples, std::vector<float>& left, std::vector<float>& right)
{
    std::mt19937 genL (1);
    std::mt19937 genR (2);
    std::uniform_real_distribution<double> dist (-0.5, 0.5);

    left.resize (numSamples);
    right.resize (numSamples);
    for (std::size_t n = 0; n < numSamples; ++n)
    {
        left[n] = static_cast<float> (dist (genL));
        right[n] = static_cast<float> (dist (genR));
    }
}

// Processa [from, to) in place em blocos da sequência `pattern` (repetida). O último bloco de
// cada trecho é cortado em `to`, então trechos consecutivos se encaixam exatamente.
void processRange (StereoWidth& sw, std::vector<float>& left, std::vector<float>& right, std::size_t from,
    std::size_t to, const std::vector<std::size_t>& pattern)
{
    std::size_t pos = from;
    std::size_t k = 0;
    while (pos < to)
    {
        const std::size_t block = std::min (pattern[k % pattern.size()], to - pos);
        ++k;

        float* ptrs[2] = { left.data() + pos, right.data() + pos };
        sw.process (ptrs, 2, static_cast<int> (block));
        pos += block;
    }
}

// Nível do side em dB: 20·log10(RMS de (L−R)/2) no trecho [from, to).
double sideLevelDb (const std::vector<float>& left, const std::vector<float>& right, std::size_t from, std::size_t to)
{
    double sum = 0.0;
    for (std::size_t n = from; n < to; ++n)
    {
        const double s = (static_cast<double> (left[n]) - static_cast<double> (right[n])) / 2.0;
        sum += s * s;
    }
    return 20.0 * std::log10 (std::sqrt (sum / static_cast<double> (to - from)));
}

double maxAbsDiff (const std::vector<float>& a, const std::vector<float>& b)
{
    double worst = 0.0;
    for (std::size_t n = 0; n < a.size(); ++n)
        worst = std::max (worst, std::abs (static_cast<double> (a[n]) - static_cast<double> (b[n])));
    return worst;
}

// Side puro (L = x, R = −x) de `freq` por 1 s, com largura `width`. Devolve o nível do side
// de saída menos o de entrada, medidos no último 0.5 s.
double sideChangeDb (double sampleRate, double freq, double amplitude, float width)
{
    const std::size_t n = static_cast<std::size_t> (sampleRate);
    std::vector<float> left = sine (freq, amplitude, sampleRate, n);
    std::vector<float> right (n);
    for (std::size_t i = 0; i < n; ++i)
        right[i] = -left[i];

    const std::size_t from = n / 2;
    const double inputDb = sideLevelDb (left, right, from, n);

    StereoWidth sw;
    sw.setWidth (width);
    sw.prepare (sampleRate);
    processRange (sw, left, right, 0, n, fixedBlocks);

    return sideLevelDb (left, right, from, n) - inputDb;
}

// Máximo de c[n] = |y[n] − 2y[n−1] + y[n−2]| em [from, to): mede saltos/quinas na saída.
double maxCurvature (const std::vector<float>& y, std::size_t from, std::size_t to)
{
    double worst = 0.0;
    for (std::size_t n = std::max<std::size_t> (from, 2); n < to; ++n)
    {
        const double c
            = static_cast<double> (y[n]) - 2.0 * static_cast<double> (y[n - 1]) + static_cast<double> (y[n - 2]);
        worst = std::max (worst, std::abs (c));
    }
    return worst;
}

// Side puro de 1 kHz, 2 s: largura 1 até 1.0 s e 2 depois. Processa em `pattern`.
void renderSwitchSignal (const std::vector<std::size_t>& pattern, std::vector<float>& left, std::vector<float>& right)
{
    const std::size_t n = 2 * oneSecond;
    left = sine (1000.0, 0.25, fs, n);
    right.resize (n);
    for (std::size_t i = 0; i < n; ++i)
        right[i] = -left[i];

    StereoWidth sw;
    sw.setWidth (1.0f);
    sw.prepare (fs);
    processRange (sw, left, right, 0, oneSecond, pattern);

    sw.setWidth (2.0f);
    processRange (sw, left, right, oneSecond, n, pattern);
}
} // namespace

TEST_CASE ("largura 100 % é identidade")
{
    std::vector<float> left;
    std::vector<float> right;
    stereoNoise (oneSecond, left, right);
    const std::vector<float> inL = left;
    const std::vector<float> inR = right;

    StereoWidth sw;
    sw.setWidth (1.0f);
    sw.prepare (fs);
    processRange (sw, left, right, 0, oneSecond, fixedBlocks);

    std::size_t differing = 0;
    for (std::size_t n = 0; n < oneSecond; ++n)
    {
        if (left[n] != inL[n])
            ++differing;
        if (right[n] != inR[n])
            ++differing;
    }
    REQUIRE (differing == 0);
}

TEST_CASE ("largura 0 % vira mono")
{
    std::vector<float> left;
    std::vector<float> right;
    stereoNoise (oneSecond, left, right);
    const std::vector<float> inL = left;
    const std::vector<float> inR = right;

    StereoWidth sw;
    sw.setWidth (0.0f);
    sw.prepare (fs);
    processRange (sw, left, right, 0, oneSecond, fixedBlocks);

    std::vector<float> mid (oneSecond);
    for (std::size_t n = 0; n < oneSecond; ++n)
        mid[n] = static_cast<float> ((static_cast<double> (inL[n]) + static_cast<double> (inR[n])) / 2.0);

    REQUIRE (maxAbsDiff (left, mid) <= 1e-6);
    REQUIRE (maxAbsDiff (right, mid) <= 1e-6);
}

TEST_CASE ("mid nunca muda")
{
    const std::vector<float> tone = sine (1000.0, 0.5, fs, oneSecond);

    for (const float width : { 0.0f, 0.5f, 1.5f, 2.0f })
    {
        std::vector<float> left = tone;
        std::vector<float> right = tone;

        StereoWidth sw;
        sw.setWidth (width);
        sw.prepare (fs);
        processRange (sw, left, right, 0, oneSecond, fixedBlocks);

        REQUIRE (maxAbsDiff (left, tone) <= 1e-6);
        REQUIRE (maxAbsDiff (right, tone) <= 1e-6);
    }
}

TEST_CASE ("mid nunca muda, também com side não nulo")
{
    const std::vector<float> mid = sine (1000.0, 0.3, fs, oneSecond);
    std::mt19937 gen (3);
    std::uniform_real_distribution<double> dist (-0.2, 0.2);

    std::vector<float> inL (oneSecond);
    std::vector<float> inR (oneSecond);
    for (std::size_t n = 0; n < oneSecond; ++n)
    {
        const double side = dist (gen);
        inL[n] = static_cast<float> (static_cast<double> (mid[n]) + side);
        inR[n] = static_cast<float> (static_cast<double> (mid[n]) - side);
    }

    for (const float width : { 0.0f, 0.5f, 1.5f, 2.0f })
    {
        std::vector<float> left = inL;
        std::vector<float> right = inR;

        StereoWidth sw;
        sw.setWidth (width);
        sw.prepare (fs);
        processRange (sw, left, right, 0, oneSecond, fixedBlocks);

        std::vector<float> outMid (oneSecond);
        for (std::size_t n = 0; n < oneSecond; ++n)
            outMid[n] = static_cast<float> ((static_cast<double> (left[n]) + static_cast<double> (right[n])) / 2.0);

        REQUIRE (maxAbsDiff (outMid, mid) <= 1e-6);
    }
}

TEST_CASE ("200 % dobra o side nos agudos")
{
    REQUIRE (sideChangeDb (fs, 5000.0, 0.25, 2.0f) == Catch::Approx (6.02).margin (0.1));
}

TEST_CASE ("200 % leva o grave para mono")
{
    REQUIRE (sideChangeDb (fs, 30.0, 0.25, 2.0f) <= -12.0);
    REQUIRE (sideChangeDb (fs, 60.0, 0.25, 2.0f) <= -12.0);
    REQUIRE (sideChangeDb (fs, 150.0, 0.25, 2.0f) > 0.0);
}

TEST_CASE ("mudança de largura sem estalo")
{
    std::vector<float> left;
    std::vector<float> right;
    renderSwitchSignal (fixedBlocks, left, right);

    const double before = maxCurvature (left, oneSecond, oneSecond + oneSecond / 10);
    const double after = maxCurvature (left, oneSecond + oneSecond / 2, 2 * oneSecond);

    REQUIRE (before <= 1.5 * after);
}

TEST_CASE ("largura: bloco variável dá o mesmo resultado")
{
    std::vector<float> leftA;
    std::vector<float> rightA;
    renderSwitchSignal (fixedBlocks, leftA, rightA);

    std::vector<float> leftB;
    std::vector<float> rightB;
    renderSwitchSignal (variableBlocks, leftB, rightB);

    REQUIRE (maxAbsDiff (leftB, leftA) <= 1e-6);
    REQUIRE (maxAbsDiff (rightB, rightA) <= 1e-6);
}

TEST_CASE ("largura: entrada não-finita")
{
    std::vector<float> left;
    std::vector<float> right;
    stereoNoise (oneSecond, left, right);
    left[1000] = std::numeric_limits<float>::quiet_NaN();
    right[2000] = std::numeric_limits<float>::infinity();

    StereoWidth sw;
    sw.setWidth (2.0f);
    sw.prepare (fs);
    processRange (sw, left, right, 0, oneSecond, fixedBlocks);

    bool allFinite = true;
    for (std::size_t n = 0; n < oneSecond; ++n)
        allFinite = allFinite && std::isfinite (left[n]) && std::isfinite (right[n]);

    REQUIRE (allFinite);
}

TEST_CASE ("mono não é tocado")
{
    std::vector<float> channel;
    std::vector<float> unused;
    stereoNoise (oneSecond, channel, unused);
    const std::vector<float> original = channel;

    StereoWidth sw;
    sw.setWidth (2.0f);
    sw.prepare (fs);
    float* ptrs[1] = { channel.data() };
    sw.process (ptrs, 1, static_cast<int> (channel.size()));

    REQUIRE (channel == original);
}

TEST_CASE ("largura: outros sample rates")
{
    for (const double rate : { 44100.0, 96000.0 })
        REQUIRE (sideChangeDb (rate, 5000.0, 0.25, 2.0f) == Catch::Approx (6.02).margin (0.1));
}
