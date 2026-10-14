#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>

#include "GenreCurveTable.h"

namespace finale::dsp
{

// Número de gêneros da tabela; o PluginProcessor confere com o número de opções do parâmetro genre.
inline constexpr int numGenres = static_cast<int> (std::size (genreEqTable));

// Ganho em dB de cada banda do EQ tonal para o gênero, já escalado por amount e eqQty.
// Entradas inválidas viram neutras: não-finito vira 0 e depois tudo é limitado a [0, 1].
// Gênero fora de 0..numGenres-1 dá a curva plana.
inline std::array<float, 4> genreEqGains (float amount, int genre, float eqQty)
{
    const auto sanitize = [] (float v) { return std::isfinite (v) ? std::clamp (v, 0.0f, 1.0f) : 0.0f; };

    std::array<float, 4> gains { 0.0f, 0.0f, 0.0f, 0.0f };
    if (genre < 0 || genre >= numGenres)
        return gains;

    const float scale = sanitize (amount) * sanitize (eqQty);
    for (std::size_t band = 0; band < 4; ++band)
        gains[band] = scale * genreEqTable[genre][band];

    return gains;
}

// Alvo de loudness integrado por gênero (LUFS), na mesma ordem do parâmetro genre. Medianas medidas por terceiros
// (docs/pesquisa/2026-10-14-parametros-por-genero.md, [17]): House −9,1, EDM −8,7, Pop −9,5, Rock −9,2,
// arredondadas para o passo de 0,5 do targetLufs.
// shortcut: Trap, Rap/Boom bap, Lo-fi e Funk BR em −9 por falta de dado; medir referências e trocar.
inline constexpr float genreTargetLufsTable[] = { -9.0f, -9.0f, -9.0f, -9.0f, -9.0f, -8.5f, -9.5f, -9.0f };
static_assert (std::size (genreTargetLufsTable) == std::size (genreEqTable),
    "genreTargetLufsTable deve ter uma entrada por gênero");

// Alvo do gênero mais o ajuste, limitado a [−16, −5] (a faixa do targetLufs). Ajuste não-finito vira 0;
// gênero fora da tabela usa −9.
inline float genreTargetLufs (int genre, float offsetLu)
{
    const float base = (genre >= 0 && genre < numGenres) ? genreTargetLufsTable[genre] : -9.0f;
    const float offset = std::isfinite (offsetLu) ? offsetLu : 0.0f;
    return std::clamp (base + offset, -16.0f, -5.0f);
}

} // namespace finale::dsp
