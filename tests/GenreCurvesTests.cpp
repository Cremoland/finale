#include "DSP/GenreCurves.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

using Catch::Approx;
using finale::dsp::genreEqGains;

namespace
{
// Mesma tabela de GenreCurves.h, escrita à mão: o teste não deve depender da implementação.
constexpr float table[8][4] = {
    { +2.5f, -1.0f, +0.5f, +1.5f }, // Trap
    { +2.0f, -0.5f, +1.0f, +0.5f }, // Rap/Boom bap
    { +1.5f, +0.5f, -1.0f, -3.0f }, // Lo-fi
    { +3.0f, -1.5f, +1.0f, +1.0f }, // Funk BR
    { +2.0f, -1.0f, +0.5f, +1.5f }, // House
    { +2.0f, -1.5f, +1.0f, +2.5f }, // EDM
    { +1.0f, -1.0f, +1.5f, +2.0f }, // Pop
    { +1.0f, 0.0f, +1.5f, +1.0f }, // Rock
};

void requireFlat (const std::array<float, 4>& g)
{
    for (float v : g)
        REQUIRE (v == Approx (0.0f).margin (1e-6));
}

void requireEqual (const std::array<float, 4>& a, const std::array<float, 4>& b)
{
    for (size_t i = 0; i < 4; ++i)
        REQUIRE (a[i] == Approx (b[i]).margin (1e-6));
}
} // namespace

TEST_CASE ("amount 0 é plano")
{
    for (int g = 0; g <= 7; ++g)
        requireFlat (genreEqGains (0.0f, g, 1.0f));
}

TEST_CASE ("amount 100 % dá a curva do gênero")
{
    for (int g = 0; g <= 7; ++g)
    {
        const auto gains = genreEqGains (1.0f, g, 1.0f);
        for (size_t b = 0; b < 4; ++b)
            REQUIRE (gains[b] == Approx (table[g][b]).margin (1e-6));
    }
}

TEST_CASE ("metade do caminho")
{
    const std::array<float, 4> expected { 1.5f, -0.75f, 0.5f, 0.5f };
    requireEqual (genreEqGains (0.5f, 3, 1.0f), expected); // Funk BR
    requireEqual (genreEqGains (1.0f, 3, 0.5f), expected);
}

TEST_CASE ("curvas: entradas inválidas")
{
    const auto nan = std::numeric_limits<float>::quiet_NaN();

    SECTION ("gênero fora de 0..7 é plano")
    {
        requireFlat (genreEqGains (1.0f, -1, 1.0f));
        requireFlat (genreEqGains (1.0f, 8, 1.0f));
    }

    SECTION ("amount NaN é plano")
    {
        requireFlat (genreEqGains (nan, 3, 1.0f));
    }

    SECTION ("qty acima de 1 satura em 1")
    {
        requireEqual (genreEqGains (1.0f, 3, 2.0f), genreEqGains (1.0f, 3, 1.0f));
    }

    SECTION ("amount negativo é plano")
    {
        requireFlat (genreEqGains (-1.0f, 3, 1.0f));
    }
}
