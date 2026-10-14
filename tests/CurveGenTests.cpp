#include "curvegen/CurveGen.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <optional>
#include <sstream>
#include <string>

using Catch::Approx;
using finale::tools::computeCurves;
using finale::tools::currentTable;
using finale::tools::CurveTable;
using finale::tools::writeCurveTableHeader;

namespace
{
using Shapes = std::array<std::optional<std::array<float, 4>>, 8>;

constexpr int trap = 0;
constexpr int pop = 6;

CurveTable zeros()
{
    return { };
}

// Só o teste normaliza fim de linha: o gerador sempre escreve LF.
std::string normaliseEol (const std::string& s)
{
    std::string out;
    out.reserve (s.size());
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '\r' && i + 1 < s.size() && s[i + 1] == '\n')
            continue;
        out.push_back (s[i]);
    }
    return out;
}
} // namespace

TEST_CASE ("regenerar sem mudança é idêntico")
{
    const std::string generated = writeCurveTableHeader (currentTable());

    std::ifstream in (std::string (FINALE_SOURCE_DIR) + "/Source/DSP/GenreCurveTable.h", std::ios::binary);
    REQUIRE (in.good());
    std::stringstream buffer;
    buffer << in.rdbuf();

    REQUIRE (normaliseEol (generated) == normaliseEol (buffer.str()));
}

TEST_CASE ("um gênero só não muda nada")
{
    const auto current = currentTable();
    Shapes shapes;
    shapes[trap] = std::array<float, 4> { 2.0f, 0.0f, 0.0f, -2.0f };

    const auto result = computeCurves (current, shapes);

    REQUIRE_FALSE (result.changed);
    REQUIRE (result.table == current);
}

TEST_CASE ("diferença entre gêneros vira curva")
{
    const auto current = zeros();
    Shapes shapes;
    shapes[trap] = std::array<float, 4> { 2.0f, 0.0f, 0.0f, -2.0f };
    shapes[pop] = std::array<float, 4> { -2.0f, 0.0f, 0.0f, 2.0f };

    const auto result = computeCurves (current, shapes);

    REQUIRE (result.changed);
    const std::array<float, 4> expectedTrap { 2.0f, 0.0f, 0.0f, -2.0f };
    const std::array<float, 4> expectedPop { -2.0f, 0.0f, 0.0f, 2.0f };
    for (std::size_t b = 0; b < 4; ++b)
    {
        REQUIRE (result.table[trap][b] == Approx (expectedTrap[b]).margin (1e-6));
        REQUIRE (result.table[pop][b] == Approx (expectedPop[b]).margin (1e-6));
    }
    for (std::size_t g = 0; g < 8; ++g)
    {
        if (g == static_cast<std::size_t> (trap) || g == static_cast<std::size_t> (pop))
            continue;
        for (std::size_t b = 0; b < 4; ++b)
            REQUIRE (result.table[g][b] == 0.0f);
    }
}

TEST_CASE ("viés comum vem da tabela atual")
{
    auto current = zeros();
    for (auto& row : current)
        row[0] = 1.0f;
    Shapes shapes;
    shapes[trap] = std::array<float, 4> { 2.0f, 0.0f, 0.0f, -2.0f };
    shapes[pop] = std::array<float, 4> { -2.0f, 0.0f, 0.0f, 2.0f };

    const auto result = computeCurves (current, shapes);

    REQUIRE (result.table[trap][0] == 3.0f);
    REQUIRE (result.table[pop][0] == -1.0f);
}

TEST_CASE ("diferença limitada a 6 dB")
{
    const auto current = zeros();
    Shapes shapes;
    shapes[trap] = std::array<float, 4> { 20.0f, 0.0f, 0.0f, 0.0f };
    shapes[pop] = std::array<float, 4> { -20.0f, 0.0f, 0.0f, 0.0f };

    const auto result = computeCurves (current, shapes);

    REQUIRE (result.table[trap][0] == 6.0f);
    REQUIRE (result.table[pop][0] == -6.0f);
}

TEST_CASE ("arredonda para 0.1 dB")
{
    const auto current = zeros();
    Shapes shapes;
    shapes[trap] = std::array<float, 4> { 0.123f, 0.0f, 0.0f, 0.0f };
    shapes[pop] = std::array<float, 4> { -0.123f, 0.0f, 0.0f, 0.0f };

    const auto result = computeCurves (current, shapes);

    REQUIRE (result.table[trap][0] == 0.1f);
    REQUIRE (result.table[pop][0] == -0.1f);
}

TEST_CASE ("curvas: gênero não medido não interfere")
{
    // Viés = média da tabela atual só dos gêneros medidos (Trap e Pop: (4 + 0) / 2 = 2), não dos 8.
    auto current = zeros();
    current[trap][0] = 4.0f;
    current[pop][0] = 0.0f;
    current[7][0] = 10.0f; // Rock, sem referência
    Shapes shapes;
    shapes[trap] = std::array<float, 4> { 1.0f, 0.0f, 0.0f, 0.0f };
    shapes[pop] = std::array<float, 4> { -1.0f, 0.0f, 0.0f, 0.0f };

    const auto result = computeCurves (current, shapes);

    REQUIRE (result.table[trap][0] == 3.0f);
    REQUIRE (result.table[pop][0] == 1.0f);
    REQUIRE (result.table[7][0] == 10.0f);
}
