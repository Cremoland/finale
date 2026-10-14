#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>

namespace finale::dsp
{

// shortcut: curvas à mão; a ferramenta de referências (#22) vai regenerar esta tabela
// Ordem das linhas = ordem permanente das opções do parâmetro genre (Trap, Rap/Boom bap, Lo-fi, Funk BR, House, EDM,
// Pop, Rock). Ordem das colunas (bandas do TonalEq): low shelf 90 Hz, peak 300 Hz, peak 3 kHz, high shelf 10 kHz.
// Valores em dB.
inline constexpr float genreEqTable[8][4] = {
    { 2.5f, -1.0f, 0.5f, 1.5f }, // Trap
    { 2.0f, -0.5f, 1.0f, 0.5f }, // Rap/Boom bap
    { 1.5f, 0.5f, -1.0f, -3.0f }, // Lo-fi
    { 3.0f, -1.5f, 1.0f, 1.0f }, // Funk BR
    { 2.0f, -1.0f, 0.5f, 1.5f }, // House
    { 2.0f, -1.5f, 1.0f, 2.5f }, // EDM
    { 1.0f, -1.0f, 1.5f, 2.0f }, // Pop
    { 1.0f, 0.0f, 1.5f, 1.0f }, // Rock
};

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
