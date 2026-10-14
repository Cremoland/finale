# A/B com volume igualado — Plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Um botão `compare` que troca o processado (B) pelo original (A) **no mesmo loudness**, para o usuário julgar o som e não o volume. A troca é suave, a latência é a mesma nos dois lados e o original igualado também respeita o teto de true peak.

**Architecture:** Um bloco `AbCompare` em `Source/DSP/` recebe o sinal original e o processado. O original passa por: atraso de `Saturator::latency` amostras → ganho de igualação suavizado → um `TruePeakLimiter` próprio (mesma latência do limiter da cadeia, então os dois lados ficam alinhados) → crossfade de 20 ms com o processado. O ganho de igualação = short-term LUFS da saída − short-term LUFS da entrada, medidos por um segundo `LoudnessMeter` na entrada. O `processBlock` passa a rodar em pedaços de no máximo `samplesPerBlock` amostras, para a cópia do original caber num buffer pré-alocado. Um PR, Issue #10.

**Tech Stack:** C++20, Catch2 v3.16.0, JUCE 9.0.3 (só em `Plugin/`).

**Spec:** `docs/decisoes.md` ("A/B com volume igualado + medidor de LUFS (atual vs alvo)") + regras de `CLAUDE.md` (nada de alocação no audio thread; saída nunca passa do teto; latência reportada).

## Global Constraints

- `Source/DSP/` usa só a biblioteca padrão; `finale::dsp`; header-only; nada de JUCE. Reaproveitar `TruePeakLimiter`, `LoudnessMeter` e `Saturator::latency`.
- Audio thread: nada de alocação, lock, I/O ou log. Os buffers são alocados em `prepare`/`prepareToPlay`. Qualquer tamanho de bloco: o plugin divide o bloco em pedaços que cabem no buffer.
- A saída nunca passa do teto, nem no lado A. Saída sempre finita. A latência total não muda (`setLatencySamples` fica igual).
- Parâmetro novo `compare` (bool, padrão desligado = ouvir o processado), com ID permanente. IDs existentes intactos.
- Implementação mínima: igualação pelo short-term (3 s), clamp de ±24 dB, suavização de 200 ms e crossfade de 20 ms, marcados com `// shortcut:` onde forem escolha fixa.
- Identificadores em inglês; comentários e commits em português. `clang-format -i` (22.1.8). Não mexer no editor.

## Review Focus

1. `compare` desligado → o processado sai idêntico, bit a bit. Teste na Task 1.
2. Original igualado mais alto que o teto → continua abaixo do teto. Teste na Task 1.
3. Ligar ou desligar o `compare` tocando → sem estalo. Teste na Task 1.
4. Silêncio, ou LUFS ainda no piso → o ganho de igualação não pula; mantém o último valor. Teste na Task 1 (ganho inválido) e na regra da Task 2.
5. Bloco maior que `samplesPerBlock` vindo do host → é processado em pedaços sem alocar. Revisão da Task 2.

---

### Task 1: `AbCompare`

**Files:**
- Create: `Source/DSP/AbCompare.h`
- Create: `tests/AbCompareTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `TruePeakLimiter` (`prepare (double, int)`, `setCeilingDecibels (float)`, `process (float* const*, int, int)`, `getLatencySamples()`).
- Produces:
  - `class finale::dsp::AbCompare`
  - `void setCompare (bool original)`: `true` toca o original igualado. Pode ser chamado antes do `prepare`.
  - `void setMatchGainDecibels (float db)`: clamp em [−24, 24]; não-finito é ignorado (mantém o alvo anterior). O padrão é 0.
  - `void setCeilingDecibels (float db)`: repassa ao limiter do lado A.
  - `void prepare (double sampleRate, int numChannels, int dryDelaySamples, int maxBlockSize)`: aloca as linhas de atraso do original, um buffer de trabalho de `maxBlockSize` por canal e o limiter do lado A. Zera tudo e põe os suavizadores nos alvos atuais.
  - `void process (const float* const* dry, float* const* wet, int numChannels, int numSamples)`: `numSamples` ≤ `maxBlockSize` (o plugin garante); `wet` entra processado e sai com o resultado do A/B.

Por bloco (`a` = posição do crossfade suavizada em 20 ms, 0 = processado e 1 = original; `g` = ganho de igualação suavizado em 200 ms, no domínio de dB):

```
para cada amostra e canal: scratch[ch][n] = delay(dry[ch][n], dryDelaySamples) * 10^(g/20)   // não-finito -> 0
limiterA.process(scratch)                                    // mesma latência do limiter da cadeia: A e B alinhados
para cada amostra: wet[ch][n] = wet[ch][n] + a * (scratch[ch][n] - wet[ch][n])
```

O limiter do lado A roda sempre, inclusive com `compare` desligado, para não ter transitório quando o usuário liga.

- [ ] **Step 1: Testes que falham**, em `tests/AbCompareTests.cpp`. 48 kHz, estéreo (o mesmo sinal nos dois canais), `dryDelaySamples = 31`, `maxBlockSize = 512`, blocos de 512 e teto de 0 dB, salvo quando indicado. `lat = 31 + limiterLatency`, onde `limiterLatency` vem de um `TruePeakLimiter` preparado a 48 kHz.
  - `"compare desligado não mexe no processado"`: dry = ruído (seed 1, [−0.5, 0.5]), wet = outro ruído (seed 2) → wet de saída == wet de entrada, exatamente (conte as diferenças e faça um REQUIRE == 0).
  - `"compare ligado toca o original alinhado"`: `setCompare (true)` antes do `prepare`, ganho 0 dB, dry = seno de 1 kHz com amplitude 0.25, wet = zeros → `saída[n + lat] == Approx (dry[n]).margin (1e-5)`, para n ≥ 200.
  - `"ganho de igualação é aplicado"`: compare ligado, `setMatchGainDecibels (6)` antes do `prepare`, dry = seno de 1 kHz com amplitude 0.1, 2 s → nível da saída − nível do dry (último 1 s) = `Approx (6.02).margin (0.05)` dB.
  - `"original igualado respeita o teto"`: compare ligado, ganho +24 dB, teto −1 dB, dry = seno de 1 kHz com amplitude 0.5 → todo |y| ≤ 0.891251.
  - `"troca sem estalo"`: wet = seno de 1 kHz com amplitude 0.25, dry = cosseno de 1 kHz com amplitude 0.25, 2 s; compare desligado até 1.0 s e ligado depois disso. Com c[n] = |y[n] − 2y[n−1] + y[n−2]|, max c em [1.0 s, 1.1 s] ≤ 1.5 · max c em [1.5 s, 2.0 s].
  - `"ganho inválido mantém o anterior"`: compare ligado, ganho +6 dB, 1 s; depois `setMatchGainDecibels` com NaN e com +inf, mais 1 s → nível do último 0.5 s − dry = `Approx (6.02).margin (0.05)` dB.
  - `"ab: bloco variável dá o mesmo resultado"`: o sinal do teste de troca, em blocos de 512 e numa sequência que repete {0, 1, 7, 512, 3} → saídas iguais (`margin (1e-6)`).
  - `"ab: entrada não-finita"`: um NaN e um +inf no dry e no wet, com compare ligado → saída sempre finita.
  - `"ab: mono"`: `prepare (48000, 1, 31, 512)` e o teste do original alinhado com 1 canal.
- [ ] **Step 2:** Rodar os testes. Esperado: FALHA (o header não existe).
- [ ] **Step 3:** Implementar `Source/DSP/AbCompare.h`.
- [ ] **Step 4:** Rodar os testes. Esperado: PASSAM.
- [ ] **Step 5:** `clang-format -i` e commit: `"DSP: A/B com o original igualado em loudness"`.

---

### Task 2: Ligar o A/B no plugin

**Files:**
- Modify: `Source/Plugin/PluginProcessor.h`
- Modify: `Source/Plugin/PluginProcessor.cpp`

**Interfaces:**
- Consumes: `AbCompare` (Task 1); `LoudnessMeter` (`prepare`, `process`, `getShortTermLufs`, `floorLufs`); `dsp::Saturator::latency`.
- Produces (para a UI): `ParamIDs::compare = "compare"`, `juce::AudioParameterBool` com `ParameterID { id, 1 }`, nome exibido `Compare`, padrão `false`, no fim do layout.

- [ ] **Step 1:** Header: `#include "DSP/AbCompare.h"`; membros `std::atomic<float>* compareParam`, `dsp::LoudnessMeter inputMeter;`, `dsp::AbCompare abCompare;`, `std::vector<float> dryLeft, dryRight;` e `int maxChunk = 0;`.
- [ ] **Step 2:** `prepareToPlay (double sampleRate, int samplesPerBlock)`: `maxChunk = std::max (1, samplesPerBlock)`; `dryLeft` e `dryRight` com `assign (maxChunk, 0.0f)`; `inputMeter.prepare (sampleRate, channels)`; `abCompare.setCompare (compareParam->load() > 0.5f)`; `abCompare.prepare (sampleRate, channels, dsp::Saturator::latency, maxChunk)`, com `channels = std::max (1, getTotalNumOutputChannels())`.
- [ ] **Step 3:** `processBlock`: guarde `ScopedNoDenormals` e os cálculos que valem para o bloco todo (o `mapAmount` e os parâmetros) fora do laço. Depois rode a cadeia num laço de pedaços: `for (int start = 0; start < numSamples; start += maxChunk)`, com `n = std::min (maxChunk, numSamples - start)` e um array local `float* chunk[2]` apontando para `buffer.getWritePointer (ch, start)` (só os canais existentes, no máximo 2). Em cada pedaço:
  1. copie a entrada para `dryLeft`/`dryRight` (std::copy_n) e monte `const float* dry[2]`;
  2. `inputMeter.process (dry, channels, n)`;
  3. a cadeia atual inteira (gain → … → limiter → outputMeter), agora sobre `chunk` e `n`;
  4. igualação: `const float in = inputMeter.getShortTermLufs(), out = outputMeter.getShortTermLufs();` e, só se os dois forem `> dsp::LoudnessMeter::floorLufs`, `abCompare.setMatchGainDecibels (out - in);`;
  5. `abCompare.setCompare (compareParam->load() > 0.5f); abCompare.setCeilingDecibels (ceilingParam->load()); abCompare.process (dry, chunk, channels, n);`.
  
  Os três `store` dos atomics de LUFS continuam no fim de cada pedaço. A ordem e os parâmetros da cadeia não mudam. Comentário de uma linha no laço: os pedaços existem para a cópia do original caber num buffer pré-alocado.
- [ ] **Step 4:** Compilar tudo, rodar o `ctest` e o `clang-format -i`. Esperado: build limpo, testes passando.
- [ ] **Step 5:** Commit: `"Plugin: A/B com volume igualado, parâmetro compare"`.

---

## Fora deste plano

Medidor de LUFS da entrada exposto na UI · igualação pelo integrado ou por janela configurável · A/B entre presets · tecla de atalho para o A/B (a UI decide).
