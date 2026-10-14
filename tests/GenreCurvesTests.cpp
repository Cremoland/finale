#include "DSP/GenreCurves.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

using Catch::Approx;
using finale::dsp::genreEqGains;
using finale::dsp::genreEqTable;

namespace
{
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
    for (int g = 0; g < finale::dsp::numGenres; ++g)
        requireFlat (genreEqGains (0.0f, g, 1.0f));
}

TEST_CASE ("amount 100 % dá a curva do gênero")
{
    for (int g = 0; g < finale::dsp::numGenres; ++g)
    {
        const auto gains = genreEqGains (1.0f, g, 1.0f);
        for (size_t b = 0; b < 4; ++b)
            REQUIRE (gains[b] == Approx (genreEqTable[g][b]).margin (1e-6));
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

    SECTION ("eqQty NaN é plano")
    {
        requireFlat (genreEqGains (1.0f, 3, nan));
    }

    SECTION ("eqQty +inf é plano")
    {
        requireFlat (genreEqGains (1.0f, 3, std::numeric_limits<float>::infinity()));
    }

    SECTION ("eqQty negativo é plano")
    {
        requireFlat (genreEqGains (1.0f, 3, -1.0f));
    }
}

TEST_CASE ("alvo de LUFS por gênero")
{
    using finale::dsp::genreTargetLufs;
    // Trap, Rap/Boom bap, Lo-fi, Funk BR, House, EDM, Pop, Rock.
    const float expected[] { -9.0f, -9.0f, -9.0f, -9.0f, -9.0f, -8.5f, -9.5f, -9.0f };
    for (int g = 0; g < finale::dsp::numGenres; ++g)
    {
        INFO ("gênero " << g);
        CHECK (genreTargetLufs (g, 0.0f) == expected[g]);
    }

    CHECK (genreTargetLufs (6, -3.0f) == -12.5f); // ajuste soma ao alvo
    CHECK (genreTargetLufs (5, 6.0f) == -5.0f); // −2,5 limitado ao teto da faixa
    CHECK (genreTargetLufs (6, -10.0f) == -16.0f); // limitado ao piso da faixa
    CHECK (genreTargetLufs (6, std::numeric_limits<float>::quiet_NaN()) == -9.5f);
    CHECK (genreTargetLufs (-1, 0.0f) == -9.0f); // gênero inválido usa −9
    CHECK (genreTargetLufs (99, 1.0f) == -8.0f);
}
