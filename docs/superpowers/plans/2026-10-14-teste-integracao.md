# Teste de integração do FinaleProcessor — Plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Um executável Catch2 `finale_plugin_tests`, ligado ao `finale_plugin`, que testa a ligação do DSP no plugin (parâmetros, latência, processBlock com qualquer bloco, estado, assistente). Ele roda no `ctest` e, por isso, também no CI.

**Architecture:** O `finale_plugin` do `juce_add_plugin` é uma biblioteca estática com o processor e os módulos JUCE já compilados. O teste liga nela e copia os includes e as definições dela (`$<TARGET_PROPERTY:...>`), para enxergar `JucePlugin_Name` e os headers do JUCE sem compilar os módulos de novo. Isso foi testado num spike: compila e roda em ~0,5 s. Um PR, Issue #25. Não muda o som.

**Tech Stack:** C++20, Catch2 v3.16.0, JUCE 9.0.3.

**Spec:** Issue #25 + regras do `CLAUDE.md` (audio thread: qualquer bloco, inclusive 0 e 1; saída finita e abaixo do teto; latência reportada; IDs permanentes).

## Global Constraints

- Não mexer em `Source/` (nem no processor, nem no editor). Este PR só adiciona teste, CMake e uma linha de doc.
- Valores medidos no spike (48 kHz, padrões): latência 132 amostras; ruído gaussiano 0,5 → pico true peak da saída −0,999 dB com teto −1; Auto com ruído 0,05 → correção 8,6 dB e short-term −9,6 LUFS aos 50 s.
- Identificadores em inglês; comentários, nomes de teste e commits em português. `clang-format -i` (22.1.8) no arquivo de teste.

## Review Focus

1. Os testes afirmam algo real (nada de teste que passa sempre).
2. O alvo de teste não recompila os módulos JUCE nem duplica símbolos (build sem warnings novos).
3. `ctest` roda os dois testes (`dsp` e `plugin`).

---

### Task 1: `finale_plugin_tests`

**Files:**
- Create: `tests/PluginProcessorTests.cpp`
- Modify: `CMakeLists.txt` (no fim do arquivo)
- Modify: `CLAUDE.md` (linha de `tests/` na seção Estrutura)

**Interfaces:**
- Consumes: `finale::FinaleProcessor` (`parameters`, `prepareToPlay`, `processBlock`, `getLatencySamples`, `isBusesLayoutSupported`, `setBusesLayout`, `getStateInformation`, `setStateInformation`, `getOutputShortTermLufs`, `getAssistantCorrectionDb`), `finale::ParamIDs`, `dsp::TruePeakDetector` (`prepare (int channels)`, `float push (int channel, float sample)`), `dsp::TruePeakLimiter` (`prepare (double, int)`, `getLatencySamples()`), `dsp::Saturator::latency`.
- Produces: alvo `finale_plugin_tests` e teste `plugin` no ctest.

- [ ] **Step 1: CMake.** No fim do `CMakeLists.txt`, adicionar exatamente:

```cmake

# Teste de integração do processor: liga na biblioteca do plugin (módulos JUCE já compilados nela)
# e copia os includes e as definições dela, para enxergar JucePlugin_Name e os headers do JUCE.
if(FINALE_BUILD_PLUGIN AND FINALE_BUILD_TESTS)
    add_executable(finale_plugin_tests tests/PluginProcessorTests.cpp)
    target_link_libraries(finale_plugin_tests PRIVATE finale_plugin Catch2::Catch2WithMain)
    target_include_directories(finale_plugin_tests PRIVATE $<TARGET_PROPERTY:finale_plugin,INCLUDE_DIRECTORIES>)
    target_compile_definitions(finale_plugin_tests PRIVATE $<TARGET_PROPERTY:finale_plugin,COMPILE_DEFINITIONS>)
    add_test(NAME plugin COMMAND finale_plugin_tests)
endif()
```

- [ ] **Step 2: Testes.** Criar `tests/PluginProcessorTests.cpp` com exatamente:

```cpp
#include "DSP/Saturator.h"
#include "DSP/TruePeakDetector.h"
#include "DSP/TruePeakLimiter.h"
#include "Plugin/PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <random>
#include <vector>

using Catch::Approx;
using finale::FinaleProcessor;

namespace
{

void setParam (FinaleProcessor& p, const char* id, float value)
{
    auto* param = p.parameters.getParameter (id);
    REQUIRE (param != nullptr);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

float rawParam (FinaleProcessor& p, const char* id)
{
    return p.parameters.getRawParameterValue (id)->load();
}

float toDb (float linear)
{
    return 20.0f * std::log10 (std::max (linear, 1.0e-12f));
}

struct RunResult
{
    bool finite = true;
    float maxTruePeakDb = -240.0f;
};

// Ruído gaussiano em blocos com os tamanhos dados (em ciclo), por `seconds` segundos.
// O processor já precisa estar preparado. Mede o true peak da saída com um detector próprio.
RunResult run (FinaleProcessor& p, int channels, double sampleRate, float amplitude, double seconds,
    std::initializer_list<int> blockSizes, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> noise (0.0f, amplitude);
    finale::dsp::TruePeakDetector detector;
    detector.prepare (channels);
    juce::MidiBuffer midi;
    RunResult result;
    float maxPeak = 0.0f;

    const auto total = static_cast<long long> (seconds * sampleRate);
    long long done = 0;
    while (done < total)
    {
        for (int size : blockSizes)
        {
            juce::AudioBuffer<float> buffer (channels, size);
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < size; ++i)
                    buffer.setSample (ch, i, noise (rng));

            p.processBlock (buffer, midi);

            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < size; ++i)
                {
                    const float s = buffer.getSample (ch, i);
                    result.finite = result.finite && std::isfinite (s);
                    maxPeak = std::max (maxPeak, detector.push (ch, s));
                }
            done += size;
        }
    }
    result.maxTruePeakDb = toDb (maxPeak);
    return result;
}

} // namespace

TEST_CASE ("parâmetros: IDs e padrões permanentes")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor p;

    struct Expected
    {
        const char* id;
        float value;
    };
    // Projetos salvos dependem destes IDs e padrões: mudar um deles quebra o projeto do usuário.
    const std::vector<Expected> expected { { finale::ParamIDs::gain, 0.0f }, { finale::ParamIDs::amount, 50.0f },
        { finale::ParamIDs::ceiling, -1.0f }, { finale::ParamIDs::targetLufs, -9.0f }, { finale::ParamIDs::genre, 6.0f },
        { finale::ParamIDs::character, 0.0f }, { finale::ParamIDs::compression, 100.0f },
        { finale::ParamIDs::saturation, 100.0f }, { finale::ParamIDs::width, 100.0f },
        { finale::ParamIDs::compare, 0.0f }, { finale::ParamIDs::assistant, 0.0f }, { finale::ParamIDs::eq, 100.0f } };

    CHECK (p.getParameters().size() == static_cast<int> (expected.size()));
    for (const auto& e : expected)
    {
        INFO (e.id);
        REQUIRE (p.parameters.getParameter (e.id) != nullptr);
        CHECK (rawParam (p, e.id) == Approx (e.value));
    }
}

TEST_CASE ("latência reportada = limiter + saturação")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor p;

    for (double sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        INFO (sampleRate);
        p.prepareToPlay (sampleRate, 512);
        finale::dsp::TruePeakLimiter reference;
        reference.prepare (sampleRate, 2);
        CHECK (p.getLatencySamples() == reference.getLatencySamples() + finale::dsp::Saturator::latency);
    }

    p.prepareToPlay (48000.0, 512);
    CHECK (p.getLatencySamples() == 132);
}

TEST_CASE ("saída finita e abaixo do teto com qualquer bloco, caráter e A/B")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    for (float ceiling : { -1.0f, -3.0f })
        for (int character = 0; character < 5; ++character)
            for (bool compare : { false, true })
            {
                INFO ("teto " << ceiling << " caráter " << character << " A/B " << compare);
                FinaleProcessor p;
                setParam (p, finale::ParamIDs::gain, 12.0f); // entrada bem quente: o limiter tem trabalho
                setParam (p, finale::ParamIDs::amount, 100.0f);
                setParam (p, finale::ParamIDs::ceiling, ceiling);
                setParam (p, finale::ParamIDs::character, static_cast<float> (character));
                setParam (p, finale::ParamIDs::compare, compare ? 1.0f : 0.0f);
                p.prepareToPlay (48000.0, 512);

                // 0 e 1 amostra, maior que o preparado (o host pode mandar) e um tamanho irregular.
                const auto result = run (p, 2, 48000.0, 0.5f, 2.0, { 512, 1, 0, 2048, 37 });
                CHECK (result.finite);
                CHECK (result.maxTruePeakDb <= ceiling + 0.1f);
            }
}

TEST_CASE ("processBlock sem prepareToPlay não mexe no áudio")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor p;
    juce::AudioBuffer<float> buffer (2, 64);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 64; ++i)
            buffer.setSample (ch, i, 0.5f);
    juce::MidiBuffer midi;

    p.processBlock (buffer, midi);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 64; ++i)
            CHECK (buffer.getSample (ch, i) == 0.5f);
}

TEST_CASE ("layouts: mono e estéreo, entrada igual à saída")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor p;

    const auto layout = [] (const juce::AudioChannelSet& in, const juce::AudioChannelSet& out)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (in);
        l.outputBuses.add (out);
        return l;
    };
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();

    CHECK (p.isBusesLayoutSupported (layout (stereo, stereo)));
    CHECK (p.isBusesLayoutSupported (layout (mono, mono)));
    CHECK_FALSE (p.isBusesLayoutSupported (layout (mono, stereo)));
    CHECK_FALSE (p.isBusesLayoutSupported (layout (stereo, mono)));

    // Em mono a cadeia inteira também precisa respeitar o teto.
    REQUIRE (p.setBusesLayout (layout (mono, mono)));
    setParam (p, finale::ParamIDs::gain, 12.0f);
    p.prepareToPlay (48000.0, 512);
    const auto result = run (p, 1, 48000.0, 0.5f, 2.0, { 512, 1, 37 });
    CHECK (result.finite);
    CHECK (result.maxTruePeakDb <= -1.0f + 0.1f);
}

TEST_CASE ("estado: parâmetros salvos e restaurados")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor a;
    setParam (a, finale::ParamIDs::amount, 80.0f);
    setParam (a, finale::ParamIDs::genre, 2.0f);
    setParam (a, finale::ParamIDs::character, 4.0f);
    setParam (a, finale::ParamIDs::assistant, 3.0f);
    setParam (a, finale::ParamIDs::targetLufs, -12.0f);
    setParam (a, finale::ParamIDs::ceiling, -2.5f);
    setParam (a, finale::ParamIDs::width, 150.0f);
    setParam (a, finale::ParamIDs::compare, 1.0f);

    juce::MemoryBlock state;
    a.getStateInformation (state);
    FinaleProcessor b;
    b.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    for (const char* id : { finale::ParamIDs::gain, finale::ParamIDs::amount, finale::ParamIDs::ceiling,
             finale::ParamIDs::targetLufs, finale::ParamIDs::genre, finale::ParamIDs::character,
             finale::ParamIDs::compression, finale::ParamIDs::saturation, finale::ParamIDs::width,
             finale::ParamIDs::compare, finale::ParamIDs::assistant, finale::ParamIDs::eq })
    {
        INFO (id);
        CHECK (rawParam (b, id) == Approx (rawParam (a, id)).margin (1.0e-4));
    }
}

TEST_CASE ("assistente Auto puxa o loudness para o alvo")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor p;
    setParam (p, finale::ParamIDs::assistant, 2.0f); // Auto
    p.prepareToPlay (48000.0, 512);

    // Ruído 0,05 sai em ~−17 LUFS sem correção (medido no spike); o alvo padrão é −9.
    const auto result = run (p, 2, 48000.0, 0.05f, 60.0, { 512 });

    CHECK (result.finite);
    CHECK (p.getAssistantCorrectionDb() > 5.0f);
    CHECK (p.getOutputShortTermLufs() == Approx (-9.0f).margin (1.5f));
}

TEST_CASE ("estado: correção do assistente sobrevive ao salvar e abrir o projeto")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor a;
    setParam (a, finale::ParamIDs::assistant, 2.0f); // Auto
    a.prepareToPlay (48000.0, 512);
    run (a, 2, 48000.0, 0.05f, 30.0, { 512 });
    const float correction = a.getAssistantCorrectionDb();
    REQUIRE (correction > 2.0f);

    // Congelado: ao abrir o projeto, a correção salva tem que valer sem reanalisar.
    setParam (a, finale::ParamIDs::assistant, 3.0f);
    juce::MemoryBlock state;
    a.getStateInformation (state);

    FinaleProcessor b;
    b.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    // Projeto aberto e salvo de novo antes de tocar áudio: a correção pendente é a que vai.
    juce::MemoryBlock resaved;
    b.getStateInformation (resaved);

    for (auto* saved : { &state, &resaved })
    {
        FinaleProcessor c;
        c.setStateInformation (saved->getData(), static_cast<int> (saved->getSize()));
        c.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> silence (2, 512);
        silence.clear();
        juce::MidiBuffer midi;
        c.processBlock (silence, midi);
        CHECK (c.getAssistantCorrectionDb() == Approx (correction).margin (1.0e-4));
    }
}
```

- [ ] **Step 3: Doc.** Em `CLAUDE.md`, seção Estrutura, trocar a linha `- \`tests/\`: testes Catch2 do DSP.` por `- \`tests/\`: testes Catch2 do DSP e, em \`PluginProcessorTests.cpp\`, da integração no processor.`
- [ ] **Step 4:** Configurar, compilar tudo (`cmake -B build -G "Visual Studio 17 2022" -A x64`, `cmake --build build --config Release --parallel`) e rodar `ctest --test-dir build -C Release --output-on-failure`. Esperado: build sem warnings novos e os dois testes (`dsp` e `plugin`) passando. Se algum valor esperado falhar, **não afrouxe o teste**: pare com BLOCKED e mostre o valor medido.
- [ ] **Step 5:** `clang-format -i tests/PluginProcessorTests.cpp` e commit: `"Testes: integração do FinaleProcessor (parâmetros, latência, blocos, estado, assistente)"`.

---

## Fora deste plano

Benchmark de CPU · teste do editor (é da UI) · teste de impulso para conferir a latência de verdade (a cadeia não é identidade em nenhum ajuste).
