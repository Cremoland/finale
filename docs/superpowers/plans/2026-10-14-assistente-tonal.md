# Assistente tonal — Plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** O assistente (param `assistant`: Off/Analyze/Auto/Freeze) também corrige o equilíbrio tonal. Ele mede a forma espectral do original em 4 bandas e puxa devagar os ganhos do EQ tonal na direção de uma forma neutra de música comercial. A curva do gênero continua sendo aplicada por cima.

**Architecture:**
- **Medida.** O `BandAnalyzer` ganha memória exponencial opcional (`setDecaySeconds`); sem ela, o comportamento offline do `finale_curvegen` não muda. No plugin, ele mede o original (mono, antes do EQ) e só quando o short-term da entrada está acima de −50 LUFS.
- **Correção.** Um bloco de lógica pura, `TonalAssistant`, faz a correção de cada banda seguir `clamp (neutra − medida, ±6 dB)` com um passo de primeira ordem: τ de 5 s no Auto; τ de 2 s no Analyze, por 20 s de sinal válido, e depois fixa. A malha é aberta (mede antes do EQ), então não oscila.
- **Aplicação.** O processor soma `amount × eq × correção` aos ganhos do gênero. A correção vai no estado do projeto, como a do loudness.
- **Escopo.** Um PR, Issue #20, só a parte tonal. Dinâmica e estéreo ficam para depois.

**Tech Stack:** C++20, Catch2 v3.16.0, JUCE 9.0.3 (só em `Plugin/`).

**Spec:**
- Issue #20.
- `docs/decisoes.md`: "Análise mede: … equilíbrio tonal"; "Auto: adapta em tempo real, devagar"; "Congelar: fixa o estado atual".
- `docs/pesquisa/2026-10-14-parametros-por-genero.md`: não há curva tonal publicada por gênero, então o alvo vem de medição.
- Forma neutra: Adrian & Bitzer, DAGA 2014, fig. 2 (pop dos anos 1990: queda de ~6 dB/oitava em densidade entre 100 Hz e 10 kHz), convertida para energia por oitava nas 4 bandas → {+5, +1,5, −1,5, −5} dB (soma 0, lida na figura, ±2 dB).

## Global Constraints

- `Source/DSP/` usa só a biblioteca padrão; `finale::dsp`; header-only; nada de JUCE.
- Audio thread: nada de alocação, lock, I/O ou log. A correção é sempre finita e fica dentro de ±6 dB por banda.
- **Não muda o som com `assistant = Off`, que é o padrão.** Nenhum parâmetro novo e nenhum ID mudado. Projetos salvos com Analyze/Auto/Freeze passam a ter também a correção tonal; isso precisa ser mencionado no PR.
- `BandAnalyzer` sem `setDecaySeconds` precisa dar resultado **idêntico** ao de hoje. O teste do curvegen confere a regeneração byte a byte.
- Constantes fixas marcadas com `// shortcut: <limite>, <quando mudar>`.
- Identificadores em inglês; comentários, nomes de teste e commits em português. Rodar `clang-format -i` (22.1.8). Não mexer no editor.

## Review Focus

1. Off zera a correção e o padrão não muda o som.
2. Freeze, o fim da análise e o silêncio seguram a correção exatamente.
3. `BandAnalyzer` sem decaimento fica idêntico (os testes do curvegen e do analisador passam sem mudança).
4. O estado salvo traz a correção tonal de volta (Freeze), inclusive quando o projeto é reaberto e salvo de novo sem tocar áudio.

---

### Task 1: `BandAnalyzer::setDecaySeconds` + `TonalAssistant`

**Files:**
- Modify: `Source/DSP/BandAnalyzer.h`
- Create: `Source/DSP/TonalAssistant.h`
- Create: `tests/TonalAssistantTests.cpp`
- Modify: `tests/BandAnalyzerTests.cpp` (um caso novo no fim)
- Modify: `CMakeLists.txt` (adicionar `tests/TonalAssistantTests.cpp` à lista do `finale_tests`, depois de `tests/CurveGenTests.cpp`)

**Interfaces:**
- Consumes: `LoudnessAssistant::Mode` (Source/DSP/LoudnessAssistant.h).
- Produces:
  ```cpp
  namespace finale::dsp {
  class BandAnalyzer { public: void setDecaySeconds (double seconds); /* resto igual */ };
  class TonalAssistant {
  public:
      using Mode = LoudnessAssistant::Mode;
      static constexpr int numBands = 4;
      static constexpr std::array<float, 4> neutralShapeDb { 5.0f, 1.5f, -1.5f, -5.0f };
      static constexpr float maxCorrectionDb = 6.0f;
      static constexpr float inputGateLufs = -50.0f;
      static constexpr double analyzerMemorySeconds = 10.0;
      void prepare (double sampleRate);   // zera a análise; mantém correção e modo
      void setMode (Mode mode);
      void setCorrectionDb (const std::array<float, 4>& db);   // estado do projeto
      void update (const std::array<float, 4>& measuredShapeDb, bool valid, int numSamples);
      std::array<float, 4> getCorrectionDb() const;
  };
  }
  ```

- [ ] **Step 1: `BandAnalyzer` com memória.** Em `Source/DSP/BandAnalyzer.h`:
  - (a) Trocar o membro `std::int64_t count = 0;` por:
    ```cpp
    double weight = 0.0; // amostras somadas (com memória, a soma ponderada pelo decaimento)
    double decaySeconds = 0.0; // 0 = sem memória: soma tudo
    double decay = 1.0; // fator por amostra, calculado no prepare
    ```
    e tirar o `#include <cstdint>`, que deixa de ser usado.
  - (b) Adicionar este método público antes do `prepare`:
    ```cpp
    // Memória exponencial com constante de tempo `seconds`, para medir música tocando (o plugin).
    // 0 (o padrão) soma tudo, como na ferramenta offline. Vale a partir do próximo prepare.
    void setDecaySeconds (double seconds)
    {
        decaySeconds = (std::isfinite (seconds) && seconds > 0.0) ? seconds : 0.0;
    }
    ```
  - (c) No `prepare`, trocar `count = 0;` por:
    ```cpp
    decay = decaySeconds > 0.0 ? std::exp (-1.0 / (decaySeconds * sampleRate)) : 1.0;
    weight = 0.0;
    ```
  - (d) No `process`, trocar `band.sumSquares += y * y;` por `band.sumSquares = band.sumSquares * decay + y * y;` e `++count;` por `weight = weight * decay + 1.0;`. Com `decay == 1.0` a conta é exatamente a de hoje.
  - (e) No `getShapeDb`, trocar `if (count == 0)` por `if (weight <= 0.0)` e `(static_cast<double> (count) * bands[b].octaves)` por `(weight * bands[b].octaves)`. Atualizar o comentário do `process` ("Acumula a energia de cada banda", acrescentando "; com memória, a energia antiga decai").

- [ ] **Step 2: Teste do decaimento.** No fim de `tests/BandAnalyzerTests.cpp`, adicionar:

```cpp
TEST_CASE ("analisador: memória esquece o passado")
{
    // 5 s de seno em 300 Hz e depois 10 s em 3 kHz, mesma amplitude.
    std::vector<float> signal (static_cast<std::size_t> (15 * kFs));
    for (std::size_t n = 0; n < signal.size(); ++n)
    {
        const double f = n < static_cast<std::size_t> (5 * kFs) ? 300.0 : 3000.0;
        signal[n] = static_cast<float> (0.5 * std::sin (2.0 * std::numbers::pi * f * static_cast<double> (n) / kFs));
    }

    BandAnalyzer withMemory;
    withMemory.setDecaySeconds (1.0);
    withMemory.prepare (kFs);
    withMemory.process (signal.data(), static_cast<int> (signal.size()));
    const Shape recent = withMemory.getShapeDb();

    // Com memória de 1 s, os 300 Hz de 10 s atrás caíram ~43 dB: só os 3 kHz contam.
    CHECK (recent[2] - recent[1] > 20.0f);

    // Sem memória, os dois senos contam (10 s contra 5 s: ~3 dB de diferença).
    const Shape all = analyse (signal);
    CHECK (std::abs (all[2] - all[1]) < 6.0f);
}
```

- [ ] **Step 3: Testes do assistente que falham.** Criar `tests/TonalAssistantTests.cpp` com exatamente:

```cpp
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
    float previous = 0.0f;
    for (int s = 0; s < 6000; ++s) // 60 s
    {
        assistant.update (flat, true, kStep);
        const float now = assistant.getCorrectionDb()[0];
        CHECK (now >= previous); // sobe sem oscilar
        CHECK (now <= neutral[0]); // e não passa do alvo
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
    assistant.setCorrectionDb ({ 7.0f, std::numeric_limits<float>::quiet_NaN(), -2.0f, -9.0f });
    checkShape (assistant.getCorrectionDb(), { 6.0f, 0.0f, -2.0f, -6.0f }, 0.0);
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
```

- [ ] **Step 4:** Adicionar `tests/TonalAssistantTests.cpp` ao `finale_tests` no `CMakeLists.txt` e rodar. Esperado: FALHA na compilação (o header não existe).
- [ ] **Step 5:** Criar `Source/DSP/TonalAssistant.h` com exatamente:

```cpp
#pragma once

#include "DSP/LoudnessAssistant.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace finale::dsp
{

// Assistente tonal: compara a forma espectral do original (4 bandas do BandAnalyzer, dB em relação à média)
// com a forma neutra de música comercial e faz a correção de cada banda seguir devagar
// clamp (neutra − medida, ±maxCorrectionDb). Só lógica, sem áudio: quem chama aplica a correção no EQ tonal.
// A medida é feita antes do EQ, então a correção não realimenta a medida (malha aberta, não oscila).
class TonalAssistant
{
public:
    using Mode = LoudnessAssistant::Mode;
    static constexpr int numBands = 4;

    // Forma neutra (energia por oitava em dB, relativa à média das 4 bandas de 90 Hz, 300 Hz, 3 kHz e 10 kHz):
    // espectro médio de pop dos anos 1990, Adrian & Bitzer, "Erstellung und Untersuchung von Langzeitspektren…",
    // DAGA 2014, fig. 2 (cai ~6 dB/oitava em densidade entre 100 Hz e 10 kHz; ruído rosa cai 3).
    // shortcut: uma forma só para todos os gêneros, lida na figura (±2 dB); trocar pela forma medida das
    // referências de cada gênero (finale_curvegen) quando houver.
    static constexpr std::array<float, numBands> neutralShapeDb { 5.0f, 1.5f, -1.5f, -5.0f };
    // shortcut: ±6 dB por banda antes da escala de amount e EQ; quando mudar: correções que soem fortes demais.
    static constexpr float maxCorrectionDb = 6.0f;
    // shortcut: abaixo de −50 LUFS de short-term na entrada a forma não é medida (silêncio, fade).
    static constexpr float inputGateLufs = -50.0f;
    // shortcut: o analisador do plugin esquece com constante de tempo de 10 s.
    static constexpr double analyzerMemorySeconds = 10.0;

    // Zera a análise. Mantém a correção (o projeto a guarda e o host pode preparar de novo) e o modo.
    void prepare (double newSampleRate)
    {
        sampleRate = (std::isfinite (newSampleRate) && newSampleRate > 0.0) ? newSampleRate : 48000.0;
        analysisTimeSec = 0.0;
        analysisDone = false;
    }

    void setMode (Mode newMode)
    {
        // Só recomeça a análise ao entrar nela; chamar com o mesmo modo a cada bloco não a reinicia.
        if (newMode == Mode::analyze && mode != Mode::analyze)
        {
            analysisTimeSec = 0.0;
            analysisDone = false;
        }
        mode = newMode;
    }

    // Correção vinda de fora (projeto salvo). Não-finito é ignorado; fora de ±maxCorrectionDb é limitado.
    void setCorrectionDb (const std::array<float, numBands>& db)
    {
        for (std::size_t b = 0; b < numBands; ++b)
            if (std::isfinite (db[b]))
                correctionDb[b] = std::clamp (static_cast<double> (db[b]), -limit, limit);
    }

    // valid: há música de verdade na entrada (acima do gate). Medida com algum valor não-finito é ignorada.
    void update (const std::array<float, numBands>& measuredShapeDb, bool valid, int numSamples)
    {
        if (numSamples <= 0)
            return;

        const double dt = static_cast<double> (numSamples) / sampleRate;
        bool usable = valid;
        for (const float v : measuredShapeDb)
            usable = usable && std::isfinite (v);

        switch (mode)
        {
        case Mode::off:
            correctionDb.fill (0.0);
            break;

        case Mode::freeze:
            break;

        case Mode::analyze:
            if (analysisDone || !usable)
                break;
            approach (measuredShapeDb, dt / analyzeTauSec);
            analysisTimeSec += dt;
            if (analysisTimeSec >= analysisSec)
                analysisDone = true;
            break;

        case Mode::autoAdapt:
            if (usable)
                approach (measuredShapeDb, dt / autoTauSec);
            break;
        }
    }

    std::array<float, numBands> getCorrectionDb() const
    {
        std::array<float, numBands> out { };
        for (std::size_t b = 0; b < numBands; ++b)
            out[b] = static_cast<float> (correctionDb[b]);
        return out;
    }

private:
    // shortcut: τ de 5 s no auto (a medida já tem memória de 10 s); quando mudar: correção lenta ou nervosa demais.
    static constexpr double autoTauSec = 5.0;
    // shortcut: τ de 2 s na análise, para chegar no alvo dentro dos 20 s.
    static constexpr double analyzeTauSec = 2.0;
    // shortcut: a análise dura 20 s de sinal válido e depois fixa a correção (igual ao assistente de loudness).
    static constexpr double analysisSec = 20.0;
    static constexpr double limit = static_cast<double> (maxCorrectionDb);

    // Passo de primeira ordem em direção ao desejado; o passo é limitado a 1, então nunca passa do alvo.
    void approach (const std::array<float, numBands>& measured, double step)
    {
        const double k = std::min (1.0, step);
        for (std::size_t b = 0; b < numBands; ++b)
        {
            const double desired = std::clamp (
                static_cast<double> (neutralShapeDb[b]) - static_cast<double> (measured[b]), -limit, limit);
            correctionDb[b] += (desired - correctionDb[b]) * k;
        }
    }

    double sampleRate = 48000.0;
    Mode mode = Mode::off;
    std::array<double, numBands> correctionDb { };
    double analysisTimeSec = 0.0;
    bool analysisDone = false;
};

} // namespace finale::dsp
```

- [ ] **Step 6:** Compilar e rodar `ctest --test-dir build -C Release --output-on-failure`. Esperado: tudo passa, inclusive os testes antigos do `BandAnalyzer` e do `CurveGen` sem nenhuma mudança neles. Se um valor esperado falhar, **não afrouxe**: pare com BLOCKED e mostre o valor medido.
- [ ] **Step 7:** `clang-format -i` nos arquivos alterados e commit: `"DSP: assistente tonal e memória no BandAnalyzer"`.

---

### Task 2: Ligar o assistente tonal no plugin

**Files:**
- Modify: `Source/Plugin/PluginProcessor.h`
- Modify: `Source/Plugin/PluginProcessor.cpp`
- Modify: `tests/PluginProcessorTests.cpp`
- Modify: `docs/decisoes.md`

**Interfaces:**
- Consumes: `TonalAssistant`, `BandAnalyzer::setDecaySeconds` (Task 1); `inputMeter`, `dryBuffers`, `assistantParam` (já existem).
- Produces (para a UI): `std::array<float, 4> getTonalCorrectionDb() const` (lê 4 `std::atomic<float>`, padrão 0). A UI lê de outra thread.

- [ ] **Step 1: Header.** Em `PluginProcessor.h`:
  - `#include "DSP/BandAnalyzer.h"` e `#include "DSP/TonalAssistant.h"` (ordem alfabética dos includes de DSP), e `#include <array>`.
  - Getter público, logo depois de `getAssistantCorrectionDb`:
    ```cpp
    // Correção tonal atual do assistente em dB por banda (90 Hz, 300 Hz, 3 kHz, 10 kHz), antes da escala de
    // amount e EQ. O editor lê de outra thread, por isso atomic.
    std::array<float, 4> getTonalCorrectionDb() const
    {
        std::array<float, 4> out { };
        for (std::size_t b = 0; b < out.size(); ++b)
            out[b] = tonalCorrectionDb[b].load (std::memory_order_relaxed);
        return out;
    }
    ```
  - Membros privados, logo depois de `pendingCorrectionDb`:
    ```cpp
    // Assistente tonal: mede o original (mono, antes do EQ) e corrige os ganhos do EQ tonal.
    dsp::BandAnalyzer tonalAnalyzer;
    dsp::TonalAssistant tonalAssistant;
    std::array<std::atomic<float>, 4> tonalCorrectionDb { };
    // Correção tonal lida do projeto, esperando o audio thread (vale quando hasPendingTonal é true).
    std::array<std::atomic<float>, 4> pendingTonalDb { };
    std::atomic<bool> hasPendingTonal { false };
    std::vector<float> dryMono; // mono do original por pedaço, pré-alocado
    ```
- [ ] **Step 2: Construtor e `prepareToPlay`.**
  - No fim do construtor: `tonalAnalyzer.setDecaySeconds (dsp::TonalAssistant::analyzerMemorySeconds);`.
  - No `prepareToPlay`, ao lado de `dryRight.assign (...)`: `dryMono.assign (static_cast<std::size_t> (maxChunk), 0.0f);`. No fim, depois de `assistant.prepare (sampleRate);`: `tonalAnalyzer.prepare (sampleRate);` e `tonalAssistant.prepare (sampleRate);`.
- [ ] **Step 3: `processBlock`.**
  - (a) Logo depois do bloco que aplica `pendingCorrectionDb`:
    ```cpp
    if (hasPendingTonal.exchange (false, std::memory_order_acquire))
    {
        std::array<float, 4> pendingTonal { };
        for (std::size_t b = 0; b < pendingTonal.size(); ++b)
            pendingTonal[b] = pendingTonalDb[b].load (std::memory_order_relaxed);
        tonalAssistant.setCorrectionDb (pendingTonal);
    }
    ```
  - (b) Trocar a chamada atual `tonalEq.setGainsDecibels (dsp::genreEqGains (...));`, de antes do laço, por:
    ```cpp
    const auto genreGains = dsp::genreEqGains (
        amountParam->load() / 100.0f, juce::roundToInt (genreParam->load()), eqParam->load() / 100.0f);
    // A correção tonal segue o mesmo amount × EQ da curva do gênero (0 % de EQ desliga as duas).
    const auto unit = [] (float v) { return std::isfinite (v) ? std::clamp (v, 0.0f, 1.0f) : 0.0f; };
    const float tonalScale = unit (amountParam->load() / 100.0f) * unit (eqParam->load() / 100.0f);
    ```
    Depois da linha `assistant.setTargetLufs (...)`, adicionar `tonalAssistant.setMode (mode);` reaproveitando o modo: trocar a chamada atual `assistant.setMode (static_cast<...> (...));` por
    ```cpp
    const auto mode = static_cast<dsp::LoudnessAssistant::Mode> (
        std::clamp (juce::roundToInt (assistantParam->load()), 0, 3));
    assistant.setMode (mode);
    ```
    e `tonalAssistant.setMode (mode);` logo em seguida.
  - (c) Dentro do laço, logo depois de `inputMeter.process (dry, channels, n);` e antes do `gain`:
    ```cpp
    // Forma tonal do original, antes do EQ: a correção não realimenta a medida.
    const bool tonalValid = channels > 0 && inputMeter.getShortTermLufs() > dsp::TonalAssistant::inputGateLufs;
    if (tonalValid)
    {
        for (int i = 0; i < n; ++i)
            dryMono[static_cast<std::size_t> (i)] = channels == 2 ? 0.5f * (dry[0][i] + dry[1][i]) : dry[0][i];
        tonalAnalyzer.process (dryMono.data(), n);
    }
    tonalAssistant.update (tonalAnalyzer.getShapeDb(), tonalValid, n);

    const auto tonal = tonalAssistant.getCorrectionDb();
    auto eqGains = genreGains;
    for (std::size_t b = 0; b < eqGains.size(); ++b)
    {
        eqGains[b] += tonalScale * tonal[b];
        tonalCorrectionDb[b].store (tonal[b], std::memory_order_relaxed);
    }
    tonalEq.setGainsDecibels (eqGains);
    ```
- [ ] **Step 4: Estado.**
  - Em `getStateInformation`, depois do `state.setProperty ("assistantCorrectionDb", ...)`:
    ```cpp
    // Correção tonal: uma propriedade por banda. A pendente (projeto aberto sem tocar áudio) vale mais.
    const bool tonalPending = hasPendingTonal.load (std::memory_order_acquire);
    for (std::size_t b = 0; b < tonalCorrectionDb.size(); ++b)
    {
        const float value = tonalPending ? pendingTonalDb[b].load (std::memory_order_relaxed)
                                         : tonalCorrectionDb[b].load (std::memory_order_relaxed);
        state.setProperty ("tonalCorrectionDb" + juce::String (static_cast<int> (b)), value, nullptr);
    }
    ```
  - Em `setStateInformation`, depois do bloco do `assistantCorrectionDb`:
    ```cpp
    // Projetos de antes do assistente tonal não têm as propriedades: a correção tonal fica em 0.
    bool hasTonal = true;
    for (int b = 0; b < 4; ++b)
        hasTonal = hasTonal && state.hasProperty ("tonalCorrectionDb" + juce::String (b));
    if (hasTonal)
    {
        for (std::size_t b = 0; b < pendingTonalDb.size(); ++b)
            pendingTonalDb[b].store (static_cast<float> (static_cast<double> (
                                         state.getProperty ("tonalCorrectionDb" + juce::String (static_cast<int> (b))))),
                std::memory_order_relaxed);
        hasPendingTonal.store (true, std::memory_order_release);
    }
    ```
- [ ] **Step 5: Testes de integração.** No fim de `tests/PluginProcessorTests.cpp`, adicionar:

```cpp
namespace
{

// Ruído "abafado": gaussiano passando por dois passa-baixas de um polo em 300 Hz (sobra pouco agudo).
// Processa `seconds` em blocos de 512 a 48 kHz.
void runDull (FinaleProcessor& p, double seconds)
{
    std::mt19937 rng (3);
    std::normal_distribution<float> noise (0.0f, 0.5f);
    const float a = std::exp (-2.0f * 3.14159265f * 300.0f / 48000.0f);
    float s1 = 0.0f, s2 = 0.0f;
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    const int blocks = static_cast<int> (seconds * 48000.0 / 512.0);
    for (int blk = 0; blk < blocks; ++blk)
    {
        for (int i = 0; i < 512; ++i)
        {
            s1 = (1.0f - a) * noise (rng) + a * s1;
            s2 = (1.0f - a) * s1 + a * s2;
            buffer.setSample (0, i, s2);
            buffer.setSample (1, i, s2);
        }
        p.processBlock (buffer, midi);
    }
}

} // namespace

TEST_CASE ("assistente tonal: Off não corrige")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor p;
    p.prepareToPlay (48000.0, 512);
    runDull (p, 10.0);
    for (float c : p.getTonalCorrectionDb())
        CHECK (c == 0.0f);
}

TEST_CASE ("assistente tonal: Auto abre o agudo de uma mix abafada")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor p;
    setParam (p, finale::ParamIDs::assistant, 2.0f); // Auto
    p.prepareToPlay (48000.0, 512);
    runDull (p, 30.0);

    const auto correction = p.getTonalCorrectionDb();
    CHECK (correction[3] > 3.0f); // mais agudo
    CHECK (correction[0] < -3.0f); // menos grave
    for (float c : correction)
        CHECK (std::abs (c) <= finale::dsp::TonalAssistant::maxCorrectionDb);
}

TEST_CASE ("estado: correção tonal sobrevive ao salvar e abrir o projeto")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor a;
    setParam (a, finale::ParamIDs::assistant, 2.0f); // Auto
    a.prepareToPlay (48000.0, 512);
    runDull (a, 30.0);
    const auto correction = a.getTonalCorrectionDb();
    REQUIRE (correction[3] > 3.0f);

    setParam (a, finale::ParamIDs::assistant, 3.0f); // Congelar
    juce::MemoryBlock state;
    a.getStateInformation (state);

    FinaleProcessor b;
    b.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
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

        const auto restored = c.getTonalCorrectionDb();
        for (std::size_t band = 0; band < 4; ++band)
            CHECK (restored[band] == Approx (correction[band]).margin (1.0e-4));
    }
}
```
  Também adicionar `#include "DSP/TonalAssistant.h"` aos includes do topo.
- [ ] **Step 6: decisoes.md.** Em `docs/decisoes.md`, seção `## Som`, logo depois da linha que começa com `- **Curvas de referência:**`, adicionar:

```markdown
- **Assistente tonal (14/10):**
  - Usa o mesmo seletor Analisar/Auto/Congelar do loudness, e Off desliga os dois.
  - Mede a forma espectral do original (4 bandas, antes do EQ, só acima de −50 LUFS short-term, memória de 10 s) e puxa os ganhos do EQ tonal na direção de uma forma neutra, até ±6 dB por banda, escalados por amount × EQ. A curva do gênero continua por cima.
  - Forma neutra: espectro médio de pop dos anos 1990 (Adrian & Bitzer, DAGA 2014), +5 / +1,5 / −1,5 / −5 dB em 90 Hz / 300 Hz / 3 kHz / 10 kHz em relação à média. Não há curva tonal publicada por gênero (`docs/pesquisa/2026-10-14-parametros-por-genero.md`).
  - Próximo passo: alvo por gênero medido nas referências pelo `finale_curvegen`.
```

- [ ] **Step 7:** Compilar tudo, rodar o `ctest` e o `clang-format -i` nos arquivos alterados. Esperado: build sem warnings novos e os 2 testes passando. Se um valor esperado falhar, **não afrouxe**: pare com BLOCKED e mostre o valor medido.
- [ ] **Step 8:** Commit: `"Plugin: assistente tonal no EQ, correção salva no projeto"`.

---

## Fora deste plano

Alvo tonal por gênero medido pelo `finale_curvegen` · dinâmica (crest/LRA) e estéreo do assistente · alvo de LUFS por gênero (#29) · UI mostrando a correção tonal (é do amigo; o getter já existe).
