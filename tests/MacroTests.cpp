#include "DSP/Macro.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

using Catch::Approx;
using finale::dsp::MacroTargets;
using finale::dsp::mapAmount;

namespace
{
struct Row
{
    float ratio, saturationMix, saturationDriveDb, width, pushDb;
};

// Tabela "master completa" (amount = 1 e quantidades = 1), na ordem Clean..DOOM.
constexpr Row table[5] = {
    { 2.0f, 0.15f, 6.0f, 1.0f, 4.0f }, // Clean
    { 2.0f, 0.50f, 6.0f, 1.0f, 5.0f }, // Warm
    { 3.0f, 0.20f, 6.0f, 1.0f, 6.0f }, // Punch
    { 2.0f, 0.15f, 6.0f, 1.5f, 4.0f }, // Wide
    { 4.0f, 1.00f, 18.0f, 1.1f, 8.0f }, // DOOM
};

void requireNeutral (const MacroTargets& t)
{
    REQUIRE (t.ratio == Approx (1.0f));
    REQUIRE (t.saturationMix == Approx (0.0f));
    REQUIRE (t.width == Approx (1.0f));
    REQUIRE (t.pushDb == Approx (0.0f));
}

void requireRow (const MacroTargets& t, const Row& r)
{
    REQUIRE (t.ratio == Approx (r.ratio).margin (1e-6));
    REQUIRE (t.saturationMix == Approx (r.saturationMix).margin (1e-6));
    REQUIRE (t.saturationDriveDb == Approx (r.saturationDriveDb).margin (1e-6));
    REQUIRE (t.width == Approx (r.width).margin (1e-6));
    REQUIRE (t.pushDb == Approx (r.pushDb).margin (1e-6));
}

void requireSame (const MacroTargets& a, const MacroTargets& b)
{
    REQUIRE (a.ratio == Approx (b.ratio).margin (1e-6));
    REQUIRE (a.saturationMix == Approx (b.saturationMix).margin (1e-6));
    REQUIRE (a.saturationDriveDb == Approx (b.saturationDriveDb).margin (1e-6));
    REQUIRE (a.width == Approx (b.width).margin (1e-6));
    REQUIRE (a.pushDb == Approx (b.pushDb).margin (1e-6));
}

void requireFinite (const MacroTargets& t)
{
    REQUIRE (std::isfinite (t.ratio));
    REQUIRE (std::isfinite (t.saturationMix));
    REQUIRE (std::isfinite (t.saturationDriveDb));
    REQUIRE (std::isfinite (t.width));
    REQUIRE (std::isfinite (t.pushDb));
}
} // namespace

TEST_CASE ("amount 0 é neutro")
{
    for (int c = 0; c <= 4; ++c)
    {
        requireNeutral (mapAmount (0.0f, c, 1.0f, 1.0f, 1.0f));
        requireNeutral (mapAmount (0.0f, c, 1.0f, 1.0f, 2.0f));
    }
}

TEST_CASE ("amount 100 % dá a tabela")
{
    for (int c = 0; c <= 4; ++c)
        requireRow (mapAmount (1.0f, c, 1.0f, 1.0f, 1.0f), table[c]);
}

TEST_CASE ("metade do amount é metade do caminho")
{
    const auto t = mapAmount (0.5f, 2, 1.0f, 1.0f, 1.0f); // Punch
    REQUIRE (t.ratio == Approx (2.0f));
    REQUIRE (t.saturationMix == Approx (0.1f));
    REQUIRE (t.pushDb == Approx (3.0f));
}

TEST_CASE ("quantidade 0 desliga só aquele bloco")
{
    const Row& doom = table[4];

    SECTION ("compressionQty 0 zera a razão")
    {
        const auto t = mapAmount (1.0f, 4, 0.0f, 1.0f, 1.0f);
        REQUIRE (t.ratio == Approx (1.0f));
        REQUIRE (t.saturationMix == Approx (doom.saturationMix));
        REQUIRE (t.pushDb == Approx (doom.pushDb));
    }

    SECTION ("saturationQty 0 zera a saturação")
    {
        const auto t = mapAmount (1.0f, 4, 1.0f, 0.0f, 1.0f);
        REQUIRE (t.saturationMix == Approx (0.0f));
        REQUIRE (t.ratio == Approx (doom.ratio));
    }

    SECTION ("widthQty 0 zera a largura")
    {
        const auto t = mapAmount (1.0f, 4, 1.0f, 1.0f, 0.0f);
        REQUIRE (t.width == Approx (1.0f));
    }
}

TEST_CASE ("quantidade de largura vai até 200 %")
{
    const auto t = mapAmount (1.0f, 3, 1.0f, 1.0f, 2.0f); // Wide
    REQUIRE (t.width == Approx (2.0f));
}

TEST_CASE ("entradas inválidas")
{
    SECTION ("amount NaN sai igual ao amount 0")
    {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        requireSame (mapAmount (nan, 2, 1.0f, 1.0f, 1.0f), mapAmount (0.0f, 2, 1.0f, 1.0f, 1.0f));
    }

    SECTION ("character fora de 0..4 sai igual ao Clean")
    {
        const auto clean = mapAmount (1.0f, 0, 1.0f, 1.0f, 1.0f);
        requireSame (mapAmount (1.0f, -1, 1.0f, 1.0f, 1.0f), clean);
        requireSame (mapAmount (1.0f, 7, 1.0f, 1.0f, 1.0f), clean);
    }

    SECTION ("compressionQty negativo vira 0")
    {
        const auto t = mapAmount (1.0f, 4, -0.5f, 1.0f, 1.0f);
        REQUIRE (t.ratio == Approx (1.0f));
    }

    SECTION ("amount acima de 1 satura em 1")
    {
        requireSame (mapAmount (1.5f, 1, 1.0f, 1.0f, 1.0f), mapAmount (1.0f, 1, 1.0f, 1.0f, 1.0f));
    }

    SECTION ("todos os campos são finitos")
    {
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        const auto inf = std::numeric_limits<float>::infinity();
        for (int c = -1; c <= 7; ++c)
        {
            requireFinite (mapAmount (nan, c, nan, inf, -inf));
            requireFinite (mapAmount (inf, c, -inf, nan, inf));
        }
    }
}
