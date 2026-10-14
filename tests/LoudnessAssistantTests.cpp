#include "DSP/LoudnessAssistant.h"
#include "DSP/LoudnessMeter.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

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

    // −25 e não −40: abaixo de alvo − 20 o gate de entrada baixa não deixa subir.
    REQUIRE (p.run (-25.0f, 600.0) == 12.0f);
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

TEST_CASE ("assistente: prepare mantém a correção")
{
    Plant p;
    p.assistant.setMode (Mode::autoAdapt);
    p.run (-14.0f, 180.0);
    const float before = p.assistant.getCorrectionDb();

    p.assistant.prepare (kFs);
    REQUIRE (p.assistant.getCorrectionDb() == before);

    p.assistant.setMode (Mode::freeze);
    for (int s = 0; s < 6000; ++s)
        REQUIRE (p.step (-20.0f) == before);
}

TEST_CASE ("assistente: setCorrectionDb")
{
    LoudnessAssistant assistant;
    assistant.prepare (kFs);

    assistant.setCorrectionDb (7.0f);
    REQUIRE (assistant.getCorrectionDb() == 7.0f);

    assistant.setCorrectionDb (std::numeric_limits<float>::quiet_NaN());
    REQUIRE (assistant.getCorrectionDb() == 7.0f);

    assistant.setCorrectionDb (40.0f);
    REQUIRE (assistant.getCorrectionDb() == 12.0f);
}

TEST_CASE ("assistente: não sobe com o limiter saturado")
{
    // Planta com limiter de teto −10 dBFS: pré = entrada + c; medido = min(pré, −10);
    // redução = max(0, pré + 10). Com entrada −14, a redução é c − 4.
    constexpr float ceilingDb = -10.0f;
    Plant p;
    p.assistant.setMode (Mode::autoAdapt);

    for (int s = 0; s < 30000; ++s) // 300 s
    {
        const float pre = -14.0f + p.queue[static_cast<std::size_t> (p.head)];
        const float measured = std::min (pre, ceilingDb);
        const float reduction = std::max (0.0f, pre - ceilingDb);

        p.assistant.update (measured, kStep, reduction);
        p.queue[static_cast<std::size_t> (p.head)] = p.assistant.getCorrectionDb();
        p.head = (p.head + 1) % kDelaySteps;
    }

    // Sem o gate de redução a correção iria a 12; com o gate de 6 dB ela para em ~10.
    REQUIRE (p.assistant.getCorrectionDb() <= 10.5f);
    REQUIRE (p.assistant.getCorrectionDb() >= 9.5f);
}

TEST_CASE ("assistente: intro baixa não puxa a correção")
{
    Plant p;
    p.assistant.setMode (Mode::autoAdapt);

    p.run (-30.0f, 30.0);
    REQUIRE (p.assistant.getCorrectionDb() == 0.0f);

    p.run (-14.0f, 180.0);
    REQUIRE (p.assistant.getCorrectionDb() == Approx (5.0).margin (0.1));
}

TEST_CASE ("assistente: fim da música não puxa a correção")
{
    Plant p;
    p.assistant.setMode (Mode::autoAdapt);
    p.run (-14.0f, 180.0);
    const float converged = p.assistant.getCorrectionDb();

    // Medido cai linearmente de −9 a −70 em 3 s e depois fica no piso por 30 s.
    float maxCorrection = converged;
    constexpr int fallSteps = 300;
    for (int s = 1; s <= fallSteps; ++s)
    {
        p.assistant.update (-9.0f - 61.0f * static_cast<float> (s) / fallSteps, kStep);
        maxCorrection = std::max (maxCorrection, p.assistant.getCorrectionDb());
    }
    for (int s = 0; s < 3000; ++s)
    {
        p.assistant.update (kFloor, kStep);
        maxCorrection = std::max (maxCorrection, p.assistant.getCorrectionDb());
    }

    REQUIRE (maxCorrection - converged <= 0.5f);
}

TEST_CASE ("assistente: bloco de 0 e de 1 amostra")
{
    LoudnessAssistant assistant;
    assistant.prepare (kFs);
    assistant.setMode (Mode::autoAdapt);

    // Bloco de 0 amostras não muda nada, nem com entrada inválida.
    assistant.update (-14.0f, 0);
    assistant.update (std::numeric_limits<float>::quiet_NaN(), 0, 20.0f);
    REQUIRE (assistant.getCorrectionDb() == 0.0f);

    // Mesma planta de 1.5 s de atraso, agora com passos de 1 amostra por 180 s.
    constexpr int delaySamples = 72000;
    std::vector<float> queue (delaySamples, 0.0f);
    int head = 0;
    for (int s = 0; s < 180 * 48000; ++s)
    {
        assistant.update (-14.0f + queue[static_cast<std::size_t> (head)], 1);
        queue[static_cast<std::size_t> (head)] = assistant.getCorrectionDb();
        head = (head + 1) % delaySamples;
    }

    REQUIRE (assistant.getCorrectionDb() == Approx (5.0).margin (0.1));
}
