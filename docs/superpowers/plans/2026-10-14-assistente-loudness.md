# Assistente de loudness (Analisar / Auto / Congelar) — Plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Um assistente que mede o loudness da saída e corrige devagar o volume antes do limiter até chegar no `targetLufs`, em quatro modos: Off, Analyze (mede por um tempo e fixa), Auto (adapta sempre, devagar) e Freeze (trava a correção atual).

**Architecture:** Um bloco de lógica pura `LoudnessAssistant` em `Source/DSP/`, sem áudio, só um laço de controle. A cada pedaço processado ele recebe o short-term LUFS da saída e avança um integrador lento: correção += (alvo − medido) · dt / τ, com clamp de ±12 dB. A correção entra somada ao push que já existe antes do limiter, e o `Gain` já faz a rampa. A malha é fechada na saída, então o integrador compensa também o que o limiter come. Um PR, Issue #9 (escopo: só loudness; o tonal ficou na #20).

**Tech Stack:** C++20, Catch2 v3.16.0, JUCE 9.0.3 (só em `Plugin/`).

**Spec:** `docs/decisoes.md` ("Analisar: toca a música, o plugin mede e fixa os ajustes"; "Auto: adapta em tempo real, devagar (média de ~10–30 s), sem bombear"; "Congelar: fixa o estado atual do Auto"; "Regras determinísticas: medir → comparar com alvo → ajustar") + regras de `CLAUDE.md`.

## Global Constraints

- `Source/DSP/` usa só a biblioteca padrão; `finale::dsp`; header-only; nada de JUCE.
- Nada de alocação, lock, I/O ou log no caminho de áudio. Correção sempre finita e dentro de [−12, +12] dB.
- Silêncio (LUFS no piso ou não-finito) não mexe na correção: ela fica parada.
- Parâmetro novo `assistant` (choice, opções `Off`, `Analyze`, `Auto`, `Freeze`, nesta ordem permanente; padrão `Off`), com ID permanente. Com `Off` o som fica igual ao de antes. IDs existentes intactos.
- Implementação mínima: constantes fixas (τ do Auto = 20 s, τ do Analyze = 5 s, duração da análise = 20 s de sinal válido, clamp de ±12 dB), cada uma marcada com `// shortcut: <limite>, <quando mudar>`.
- Identificadores em inglês; comentários e commits em português. `clang-format -i` (22.1.8). Não mexer no editor.

## Review Focus

1. Auto não bombeia: converge sem oscilar, mesmo com o atraso de ~1.5 s do short-term. Teste com planta atrasada na Task 1.
2. Silêncio ou fim da música não fazem a correção disparar. Teste na Task 1.
3. Freeze e o fim da análise seguram a correção exatamente, mesmo com a música mudando. Teste na Task 1.
4. Correção limitada a ±12 dB e nunca NaN, com qualquer entrada. Teste na Task 1.
5. `Off` (o padrão) não muda o som de projetos existentes. Revisão da Task 2.

---

### Task 1: `LoudnessAssistant`

**Files:**
- Create: `Source/DSP/LoudnessAssistant.h`
- Create: `tests/LoudnessAssistantTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `LoudnessMeter::floorLufs` (Source/DSP/LoudnessMeter.h).
- Produces:
  ```cpp
  namespace finale::dsp {
  class LoudnessAssistant {
  public:
      enum class Mode { off = 0, analyze = 1, autoAdapt = 2, freeze = 3 };   // mesma ordem do parâmetro
      void prepare (double sampleRate);            // zera a correção e a análise
      void setMode (Mode mode);
      void setTargetLufs (float lufs);             // não-finito é ignorado; padrão -9
      void update (float measuredLufs, int numSamples);   // avança o laço por numSamples amostras
      float getCorrectionDb() const;
  };
  }
  ```

Regras de `update`, com dt = numSamples / sampleRate:

```
off:      correção = 0
freeze:   não muda nada
analyze:  se a análise já terminou, não muda nada. Senão, se a medida for válida:
            correção += (alvo − medido) * dt / 5;  tempoValido += dt
            quando tempoValido >= 20 s, a análise termina (a correção fica fixa)
autoAdapt: se a medida for válida: correção += (alvo − medido) * dt / 20
medida válida = finita e > LoudnessMeter::floorLufs
depois de qualquer mudança: clamp em [−12, 12]
setMode: entrar em analyze vindo de outro modo zera tempoValido e marca a análise como não terminada (a correção atual é o ponto de partida)
```

- [ ] **Step 1: Testes que falham**, em `tests/LoudnessAssistantTests.cpp`. Use uma planta simulada: a cada passo de 480 amostras (10 ms a 48 kHz), medido = loudnessDeEntrada + correção de 1.5 s atrás (uma fila fixa de 150 passos, para imitar o atraso do short-term). Alvo −9, a menos que o teste diga outra coisa:
  - `"off não corrige"`: modo off, entrada −14, 60 s → correção == 0 o tempo todo.
  - `"auto converge ao alvo sem bombear"`: auto, entrada −14, 180 s → correção final = `Approx (5.0).margin (0.1)`; a correção máxima no caminho ≤ 5.2 (sem overshoot relevante) e nunca diminui por mais de 0.05 dB entre passos (não oscila).
  - `"auto ignora o silêncio"`: auto, entrada −14 por 60 s, depois medido = `LoudnessMeter::floorLufs` por 60 s → durante o silêncio a correção não muda (igual exata ao valor do início do silêncio).
  - `"analisar mede e fixa"`: analyze, entrada −14 → depois de 25 s a correção = `Approx (5.0).margin (0.3)`; então a entrada muda para −20 e roda mais 60 s → a correção é exatamente a mesma.
  - `"congelar segura a correção"`: auto até convergir (180 s), setMode freeze, entrada muda para −20, mais 60 s → correção exatamente igual.
  - `"mudar o alvo"`: auto convergido em −9, setTargetLufs (−6), mais 240 s → correção = `Approx (8.0).margin (0.1)`.
  - `"correção limitada a ±12 dB"`: auto, entrada −40, 600 s → correção == 12; entrada +10, 600 s → correção == −12.
  - `"reentrar em analisar refaz a análise"`: analyze até fixar (25 s, entrada −14); setMode auto; setMode analyze; entrada −20 por 25 s → a correção sai do valor fixo e chega a `Approx (11.0).margin (0.5)`.
  - `"entradas inválidas"`: NaN e +inf no medido e NaN no alvo → a correção continua finita e igual à de antes.
- [ ] **Step 2:** Rodar os testes. Esperado: FALHA (o header não existe).
- [ ] **Step 3:** Implementar `Source/DSP/LoudnessAssistant.h`.
- [ ] **Step 4:** Rodar os testes. Esperado: PASSAM.
- [ ] **Step 5:** `clang-format -i` e commit: `"DSP: assistente de loudness (analisar/auto/congelar)"`.

---

### Task 2: Ligar o assistente no plugin

**Files:**
- Modify: `Source/Plugin/PluginProcessor.h`
- Modify: `Source/Plugin/PluginProcessor.cpp`

**Interfaces:**
- Consumes: `LoudnessAssistant` (Task 1); `ParamIDs::targetLufs` (já existe); o push e o `outputMeter` que já existem.
- Produces (para a UI): `ParamIDs::assistant = "assistant"`, `juce::AudioParameterChoice` com `ParameterID { id, 1 }`, nome exibido `Assistant`, opções `{ "Off", "Analyze", "Auto", "Freeze" }` (ordem permanente), padrão 0, no fim do layout. Getter `float getAssistantCorrectionDb() const`, lendo um `std::atomic<float>` (padrão 0).

- [ ] **Step 1:** Header: `#include "DSP/LoudnessAssistant.h"`; os membros `std::atomic<float>* assistantParam` e `targetLufsParam` (iniciados no construtor), `dsp::LoudnessAssistant assistant;`, `std::atomic<float> assistantCorrectionDb { 0.0f };` e o getter público. Comentário: a UI lê de outra thread.
- [ ] **Step 2:** `prepareToPlay`: `assistant.prepare (sampleRate);`.
- [ ] **Step 3:** `processBlock`, antes do laço de pedaços: `assistant.setMode (static_cast<dsp::LoudnessAssistant::Mode> (std::clamp (juce::roundToInt (assistantParam->load()), 0, 3)));` e `assistant.setTargetLufs (targetLufsParam->load());`. Dentro do laço: o push passa a usar `push.setGainDecibels (targets.pushDb + assistant.getCorrectionDb());`. Depois do `outputMeter.process` do pedaço: `assistant.update (outputMeter.getShortTermLufs(), n);` e `assistantCorrectionDb.store (assistant.getCorrectionDb(), std::memory_order_relaxed);`. Comentário de uma linha: a malha é fechada na saída, então a correção também compensa o limiter.
- [ ] **Step 4:** Compilar tudo, rodar o `ctest` e o `clang-format -i`. Esperado: build limpo, testes passando.
- [ ] **Step 5:** Commit: `"Plugin: assistente de loudness no push, parâmetro assistant"`.

---

## Fora deste plano

Tonal, dinâmica e imagem estéreo (#20) · reset do integrado do medidor ao começar a análise · indicar na UI quando a análise terminou · τ e duração configuráveis.
