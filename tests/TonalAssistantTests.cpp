#include "DSP/TonalAssistant.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

using Catch::Approx;
using finale::dsp::TonalAssistant;
using Mode = TonalAssistant::Mode;
using Shape = std::array<float, 4>;

namespace
{
constexpr double kFs = 48000.0;
constexpr int kStep = 480; // 10 ms a 48 kHz

// Roda `seconds` com a mesma medida; devolve a correção no fim.
Shape run (TonalAssistant& assistant, const Shape& measured, double seconds, bool valid = true)
{
    const int steps = static_cast<int> (std::lround (seconds * kFs / kStep));
    for (int s = 0; s < steps; ++s)
        assistant.update (measured, valid, kStep);
    return assistant.getCorrectionDb();
}

TonalAssistant prepared (Mode mode)
{
    TonalAssistant assistant;
    assistant.prepare (kFs);
    assistant.setMode (mode);
    return assistant;
}

void checkShape (const Shape& actual, const Shape& expected, double margin)
{
    for (std::size_t b = 0; b < 4; ++b)
    {
        INFO ("banda " << b);
        CHECK (actual[b] == Approx (expected[b]).margin (margin));
    }
}

const Shape flat { 0.0f, 0.0f, 0.0f, 0.0f };
const Shape neutral = TonalAssistant::neutralShapeDb;
} // namespace

TEST_CASE ("tonal: off não corrige")
{
    auto assistant = prepared (Mode::off);
    checkShape (run (assistant, flat, 60.0), flat, 0.0);
}

TEST_CASE ("tonal: auto leva a mix para a forma neutra sem passar dela")
{
    auto assistant = prepared (Mode::autoAdapt);
    // Medida plana: a correção desejada é a própria forma neutra.
    Shape previous { };
    for (int s = 0; s < 6000; ++s) // 60 s
    {
        assistant.update (flat, true, kStep);
        const Shape now = assistant.getCorrectionDb();
        for (std::size_t b = 0; b < 4; ++b)
        {
            INFO ("banda " << b << ", passo " << s);
            // Bandas de alvo positivo sobem; as de alvo negativo descem. Nunca passam do alvo.
            if (neutral[b] >= 0.0f)
            {
                CHECK (now[b] >= previous[b]); // sobe sem oscilar
                CHECK (now[b] <= neutral[b]); // e não passa do alvo
            }
            else
            {
                CHECK (now[b] <= previous[b]); // desce sem oscilar
                CHECK (now[b] >= neutral[b]); // e não passa do alvo
            }
        }
        previous = now;
    }
    checkShape (assistant.getCorrectionDb(), neutral, 0.01);
}

TEST_CASE ("tonal: correção limitada a ±6 dB")
{
    auto assistant = prepared (Mode::autoAdapt);
    const Shape extreme { 20.0f, 0.0f, 0.0f, -20.0f };
    checkShape (run (assistant, extreme, 60.0), { -6.0f, 1.5f, -1.5f, 6.0f }, 0.01);
}

TEST_CASE ("tonal: silêncio e medida inválida seguram a correção")
{
    auto assistant = prepared (Mode::autoAdapt);
    const Shape converged = run (assistant, flat, 60.0);

    checkShape (run (assistant, { -9.0f, 3.0f, 3.0f, 3.0f }, 60.0, false), converged, 0.0);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    checkShape (run (assistant, { nan, 0.0f, 0.0f, 0.0f }, 60.0), converged, 0.0);
}

TEST_CASE ("tonal: analisar mede e fixa")
{
    auto assistant = prepared (Mode::analyze);
    const Shape fixed = run (assistant, flat, 25.0);
    checkShape (fixed, neutral, 0.01);

    checkShape (run (assistant, { -5.0f, 0.0f, 0.0f, 5.0f }, 60.0), fixed, 0.0);
}

TEST_CASE ("tonal: congelar segura a correção")
{
    auto assistant = prepared (Mode::autoAdapt);
    const Shape converged = run (assistant, flat, 60.0);
    assistant.setMode (Mode::freeze);
    checkShape (run (assistant, { -5.0f, 0.0f, 0.0f, 5.0f }, 60.0), converged, 0.0);
}

TEST_CASE ("tonal: reentrar em analisar refaz a análise")
{
    auto assistant = prepared (Mode::analyze);
    run (assistant, flat, 25.0);
    assistant.setMode (Mode::autoAdapt);
    assistant.setMode (Mode::analyze);
    // Desejado: neutra − medida = {10, 1.5, −1.5, −10}, limitado a ±6.
    checkShape (run (assistant, { -5.0f, 0.0f, 0.0f, 5.0f }, 25.0), { 6.0f, 1.5f, -1.5f, -6.0f }, 0.01);
}

TEST_CASE ("tonal: prepare mantém a correção")
{
    auto assistant = prepared (Mode::autoAdapt);
    const Shape converged = run (assistant, flat, 60.0);
    assistant.prepare (96000.0);
    checkShape (assistant.getCorrectionDb(), converged, 0.0);
}

TEST_CASE ("tonal: setCorrectionDb limita e ignora não-finito")
{
    TonalAssistant assistant;
    assistant.prepare (kFs);
    // Primeiro um valor conhecido, para o NaN provar que foi ignorado (e não só que o padrão é zero).
    assistant.setCorrectionDb ({ 1.0f, 2.0f, 3.0f, 4.0f });
    assistant.setCorrectionDb ({ 7.0f, std::numeric_limits<float>::quiet_NaN(), -2.0f, -9.0f });
    checkShape (assistant.getCorrectionDb(), { 6.0f, 2.0f, -2.0f, -6.0f }, 0.0);
}

TEST_CASE ("tonal: bloco de 0 e de 1 amostra")
{
    auto assistant = prepared (Mode::autoAdapt);
    assistant.update (flat, true, 0);
    checkShape (assistant.getCorrectionDb(), flat, 0.0);

    assistant.update (flat, true, 1);
    CHECK (assistant.getCorrectionDb()[0] > 0.0f);
    CHECK (assistant.getCorrectionDb()[0] < 0.01f);
}
