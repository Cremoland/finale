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

} // namespace finale::dsp
