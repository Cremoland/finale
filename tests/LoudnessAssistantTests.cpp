#include "DSP/LoudnessAssistant.h"
#include "DSP/LoudnessMeter.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <limits>

using finale::dsp::LoudnessAssistant;
using finale::dsp::LoudnessMeter;
using Mode = LoudnessAssistant::Mode;
using Catch::Approx;

namespace
{
constexpr double kFs = 48000.0;
constexpr int kStep = 480; // 10 ms a 48 kHz
constexpr int kDelaySteps = 150; // 1.5 s, atraso do short-term
constexpr float kFloor = LoudnessMeter::floorLufs;

// Planta simulada: medido = entrada + correção de 1.5 s atrás. No silêncio o medidor
// devolve o piso direto, como um medidor real faria.
struct Plant
{
    LoudnessAssistant assistant;
    std::array<float, kDelaySteps> queue { }; // correções de 1.5 s atrás, começa em 0
    int head = 0;

    Plant() { assistant.prepare (kFs); }

    // Roda um passo com a entrada dada e devolve a correção depois do passo.
    float step (float inputLufs)
    {
        const float delayed = queue[static_cast<std::size_t> (head)];
        const float measured = inputLufs == kFloor ? kFloor : inputLufs + delayed;

        assistant.update (measured, kStep);

        const float correction = assistant.getCorrectionDb();
        queue[static_cast<std::size_t> (head)] = correction;
        head = (head + 1) % kDelaySteps;
        return correction;
    }

    // Roda `seconds` com entrada constante; devolve a correção do último passo.
    float run (float inputLufs, double seconds)
    {
        const int steps = static_cast<int> (std::lround (seconds * kFs / kStep));
        float correction = assistant.getCorrectionDb();
        for (int s = 0; s < steps; ++s)
            correction = step (inputLufs);
        return correction;
    }
};
} // namespace

TEST_CASE ("off não corrige")
{
    Plant p;
    const int steps = 6000;
    for (int s = 0; s < steps; ++s)
        REQUIRE (p.step (-14.0f) == 0.0f);
}

TEST_CASE ("auto converge ao alvo sem bombear")
{
    Plant p;
    p.assistant.setMode (Mode::autoAdapt);

    float previous = 0.0f;
    float maxCorrection = 0.0f;
    const int steps = 18000; // 180 s
    for (int s = 0; s < steps; ++s)
    {
        const float correction = p.step (-14.0f);
        maxCorrection = std::max (maxCorrection, correction);
        if (s > 0)
            REQUIRE (correction >= previous - 0.05f);
        previous = correction;
    }

    REQUIRE (previous == Approx (5.0).margin (0.1));
    REQUIRE (maxCorrection <= 5.2f);
}

TEST_CASE ("auto ignora o silêncio")
{
    Plant p;
    p.assistant.setMode (Mode::autoAdapt);
    p.run (-14.0f, 60.0);

    const float atSilence = p.assistant.getCorrectionDb();
    for (int s = 0; s < 6000; ++s)
        REQUIRE (p.step (kFloor) == atSilence);
}

TEST_CASE ("analisar mede e fixa")
{
    Plant p;
    p.assistant.setMode (Mode::analyze);

    const float measured = p.run (-14.0f, 25.0);
    REQUIRE (measured == Approx (5.0).margin (0.3));

    const float fixed = p.assistant.getCorrectionDb();
    for (int s = 0; s < 6000; ++s)
        REQUIRE (p.step (-20.0f) == fixed);
}

TEST_CASE ("congelar segura a correção")
{
    Plant p;
    p.assistant.setMode (Mode::autoAdapt);
    p.run (-14.0f, 180.0);

    p.assistant.setMode (Mode::freeze);
    const float held = p.assistant.getCorrectionDb();
    for (int s = 0; s < 6000; ++s)
        REQUIRE (p.step (-20.0f) == held);
}

TEST_CASE ("mudar o alvo")
{
    Plant p;
    p.assistant.setMode (Mode::autoAdapt);
    p.run (-14.0f, 180.0);

    p.assistant.setTargetLufs (-6.0f);
    const float corrected = p.run (-14.0f, 240.0);
    REQUIRE (corrected == Approx (8.0).margin (0.1));
}

TEST_CASE ("correção limitada a ±12 dB")
{
    Plant p;
    p.assistant.setMode (Mode::autoAdapt);

    REQUIRE (p.run (-40.0f, 600.0) == 12.0f);
    REQUIRE (p.run (10.0f, 600.0) == -12.0f);
}

TEST_CASE ("reentrar em analisar refaz a análise")
{
    Plant p;
    p.assistant.setMode (Mode::analyze);
    p.run (-14.0f, 25.0);
    const float fixed = p.assistant.getCorrectionDb();

    p.assistant.setMode (Mode::autoAdapt);
    p.assistant.setMode (Mode::analyze);

    const float redone = p.run (-20.0f, 25.0);
    REQUIRE (redone != fixed);
    REQUIRE (redone == Approx (11.0).margin (0.5));
}

TEST_CASE ("assistente: entradas inválidas")
{
    Plant p;
    p.assistant.setMode (Mode::autoAdapt);
    p.run (-14.0f, 60.0);

    const float before = p.assistant.getCorrectionDb();
    p.assistant.setTargetLufs (std::numeric_limits<float>::quiet_NaN());

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (int s = 0; s < 1000; ++s)
    {
        const float correction = p.step (s % 2 == 0 ? nan : inf);
        REQUIRE (std::isfinite (correction));
        REQUIRE (correction == before);
    }
}
