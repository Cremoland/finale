#pragma once

#include <algorithm>
#include <cmath>

namespace finale::dsp
{

struct MacroTargets
{
    float ratio;
    float saturationMix;
    float saturationDriveDb;
    float width;
    float pushDb;
};

// shortcut: valores por ouvido; presets por gênero (#6) e o Auto (#9) vão refinar
// Ordem das linhas = ordem permanente das opções do parâmetro character (Clean, Warm, Punch, Wide, DOOM).
constexpr MacroTargets characterTable[5] = {
    { 2.0f, 0.15f, 6.0f, 1.0f, 4.0f }, // Clean
    { 2.0f, 0.50f, 6.0f, 1.0f, 5.0f }, // Warm
    { 3.0f, 0.20f, 6.0f, 1.0f, 6.0f }, // Punch
    { 2.0f, 0.15f, 6.0f, 1.5f, 4.0f }, // Wide
    { 4.0f, 1.00f, 18.0f, 1.1f, 8.0f }, // DOOM
};

// Converte o knob amount, o caráter e as três quantidades em alvos por bloco.
// Entradas inválidas viram valores neutros, para o áudio nunca receber NaN ou infinito.
inline MacroTargets mapAmount (float amount, int character, float compressionQty, float saturationQty, float widthQty)
{
    const auto sanitize = [] (float v, float fallback) { return std::isfinite (v) ? v : fallback; };

    const float a = std::clamp (sanitize (amount, 0.0f), 0.0f, 1.0f);
    const float comp = std::clamp (sanitize (compressionQty, 0.0f), 0.0f, 1.0f);
    const float sat = std::clamp (sanitize (saturationQty, 0.0f), 0.0f, 1.0f);
    const float wid = std::clamp (sanitize (widthQty, 1.0f), 0.0f, 2.0f);

    const MacroTargets& row = characterTable[(character >= 0 && character <= 4) ? character : 0];

    MacroTargets t;
    t.ratio = 1.0f + a * (row.ratio - 1.0f) * comp;
    t.saturationMix = a * row.saturationMix * sat;
    t.saturationDriveDb = row.saturationDriveDb; // só o caráter decide; com mix 0 não soa
    t.width = std::clamp (1.0f + a * (row.width - 1.0f) * wid, 0.0f, 2.0f);
    t.pushDb = a * row.pushDb;
    return t;
}

} // namespace finale::dsp
