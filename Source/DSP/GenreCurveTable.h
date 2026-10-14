// Gerado por tools/curvegen (finale_curvegen); edite à mão só para ajuste por ouvido.
#pragma once

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

} // namespace finale::dsp
