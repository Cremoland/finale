#pragma once

#include "DSP/GenreCurves.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>

namespace finale::tools
{

using CurveTable = std::array<std::array<float, 4>, 8>;

struct CurveResult
{
    CurveTable table;
    bool changed;
    std::string message;
};

// Pastas dos gêneros de referência (ordem igual à da tabela).
inline constexpr std::array<const char*, 8> genreFolders { "Trap", "Rap-Boom bap", "Lo-fi", "Funk BR", "House", "EDM",
    "Pop", "Rock" };

// Nomes das opções do parâmetro genre: é o que aparece nos comentários da tabela gerada.
inline constexpr std::array<const char*, 8> genreNames { "Trap", "Rap/Boom bap", "Lo-fi", "Funk BR", "House", "EDM",
    "Pop", "Rock" };

static_assert (dsp::numGenres == 8, "a ferramenta assume 8 gêneros");

inline CurveTable currentTable()
{
    CurveTable table { };
    for (std::size_t g = 0; g < table.size(); ++g)
        for (std::size_t b = 0; b < 4; ++b)
            table[g][b] = dsp::genreEqTable[g][b];
    return table;
}

// Arredonda para 0.1 dB; -0.0 vira 0.0 para não sair "-0.0f" no cabeçalho.
inline float round1 (float v)
{
    const float r = std::round (v * 10.0f) / 10.0f;
    return r == 0.0f ? 0.0f : r;
}

// Com 2 ou mais gêneros medidos, cada curva medida vira o viés comum da tabela atual
// mais o desvio da forma em relação à média das formas medidas, limitado a ±6 dB.
inline CurveResult computeCurves (
    const CurveTable& current, const std::array<std::optional<std::array<float, 4>>, 8>& measuredShapes)
{
    CurveResult result { current, false, { } };

    int measured = 0;
    for (const auto& shape : measuredShapes)
        if (shape)
            ++measured;

    if (measured < 2)
    {
        result.message = "São necessários pelo menos 2 gêneros com referência (encontrados: "
            + std::to_string (measured) + "). Tabela mantida.";
        return result;
    }

    std::array<float, 4> currentMean { };
    std::array<float, 4> shapeMean { };
    for (std::size_t g = 0; g < 8; ++g)
    {
        for (std::size_t b = 0; b < 4; ++b)
            currentMean[b] += current[g][b] / 8.0f;
        if (measuredShapes[g])
            for (std::size_t b = 0; b < 4; ++b)
                shapeMean[b] += (*measuredShapes[g])[b] / static_cast<float> (measured);
    }

    std::string changedNames;
    int changedCount = 0;
    for (std::size_t g = 0; g < 8; ++g)
    {
        if (!measuredShapes[g])
            continue;

        const auto& shape = *measuredShapes[g];
        bool genreChanged = false;
        for (std::size_t b = 0; b < 4; ++b)
        {
            const float deviation = std::clamp (shape[b] - shapeMean[b], -6.0f, 6.0f);
            result.table[g][b] = round1 (currentMean[b] + deviation);
            if (result.table[g][b] != current[g][b])
                genreChanged = true;
        }

        if (genreChanged)
        {
            changedNames += (changedCount++ == 0 ? "" : ", ");
            changedNames += genreNames[g];
        }
    }

    result.changed = changedCount > 0;
    result.message = result.changed ? "Curvas atualizadas: " + changedNames + "."
                                    : "Nenhuma curva mudou (" + std::to_string (measured) + " gêneros medidos).";
    return result;
}

// Texto exato de Source/DSP/GenreCurveTable.h: LF, uma linha por gênero, uma casa decimal com sufixo f.
inline std::string writeCurveTableHeader (const CurveTable& table)
{
    const auto formatValue = [] (float v)
    {
        char buffer[32];
        std::snprintf (buffer, sizeof buffer, "%.1ff", static_cast<double> (round1 (v)));
        return std::string (buffer);
    };

    std::string out;
    out += "// Gerado por tools/curvegen (finale_curvegen); edite à mão só para ajuste por ouvido.\n";
    out += "#pragma once\n";
    out += "\n";
    out += "namespace finale::dsp\n";
    out += "{\n";
    out += "\n";
    out += "// shortcut: valores iniciais à mão; regenere com finale_curvegen (README) e ajuste o viés comum por "
           "ouvido\n";
    out += "// Ordem das linhas = ordem permanente das opções do parâmetro genre (Trap, Rap/Boom bap, Lo-fi, Funk BR, "
           "House, EDM,\n";
    out += "// Pop, Rock). Ordem das colunas (bandas do TonalEq): low shelf 90 Hz, peak 300 Hz, peak 3 kHz, high shelf "
           "10 kHz.\n";
    out += "// Valores em dB.\n";
    out += "inline constexpr float genreEqTable[8][4] = {\n";
    for (std::size_t g = 0; g < 8; ++g)
    {
        out += "    { " + formatValue (table[g][0]) + ", " + formatValue (table[g][1]) + ", "
            + formatValue (table[g][2]) + ", " + formatValue (table[g][3]) + " }, // " + genreNames[g] + "\n";
    }
    out += "};\n";
    out += "\n";
    out += "} // namespace finale::dsp\n";
    return out;
}

} // namespace finale::tools
