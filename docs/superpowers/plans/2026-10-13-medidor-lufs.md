# Medidor de LUFS — Plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Medir loudness momentary (400 ms), short-term (3 s) e integrado (com gate) na saída do plugin, conforme ITU-R BS.1770-5 / EBU Tech 3341, e publicar os valores para a interface.

**Architecture:** Dois blocos em `Source/DSP/` sem JUCE: `KWeighting` (os dois biquads, coeficientes calculados para qualquer sample rate) e `LoudnessMeter` (energia em sub-blocos de 100 ms, janelas de 4 e 30 sub-blocos, histograma pré-alocado para o integrado). O processor mede a saída depois do limiter e publica três `std::atomic<float>` que o editor lê. Um PR, Issue #4.

**Tech Stack:** C++20, Catch2 v3.16.0, JUCE 9.0.3 (só em `Plugin/`).

**Spec:** `docs/decisoes.md` (medidor de LUFS atual vs alvo; o Auto usa médias de 10–30 s) + `docs/pesquisa/2026-10-13-medidor-lufs.md` (algoritmo, fontes, decisão de fazer do zero) + regras de `CLAUDE.md`.

## Global Constraints

- `Source/DSP/` usa só a biblioteca padrão; namespace `finale::dsp`; header-only; nada de JUCE.
- Audio thread: nada de alocação, lock, I/O ou log em `process`. Toda memória é alocada em `prepare`.
- Funciona em qualquer sample rate e tamanho de bloco, inclusive bloco de 1 e de 0 amostras.
- Sem dependência nova (nada de libebur128, nem nos testes). A verificação são os casos da EBU Tech 3341 gerados no próprio teste.
- Tolerância de conformidade: ±0.1 LU (Tech 3341).
- Pesos de canal G = 1.0 para L e R (BS.1770-5, Tabela 3). Mono soma um canal só, então um seno mono lê 3 LU abaixo do mesmo seno em estéreo. É o comportamento da norma.
- Leitura sem dado (silêncio, ou antes do primeiro bloco acima do gate) devolve `LoudnessMeter::floorLufs = -100.0f`. Nunca NaN ou infinito.
- Identificadores em inglês; comentários e commits em português; comente o porquê e cite a fonte (BS.1770-5 Anexo 1, Tech 3341).
- `clang-format -i` (22.1.8) em todo arquivo alterado. Não mexer no editor (`PluginEditor.*`): a UI é do outro desenvolvedor.

## Review Focus

1. Sessão longa (o Auto roda por horas) → memória fixa e integrado correto. Teste "sessão longa" na Task 2.
2. Bloco variável (0, 1, 7, 512, 3) → leituras idênticas às de bloco fixo. Teste na Task 2.
3. Entrada NaN/infinita → leituras finitas, e o medidor volta a ler certo depois. Teste na Task 2.
4. Sinal acima de 0 dBFS (antes do limiter, ou no A/B depois) → integrado correto acima do topo do histograma. Teste "sinal alto" na Task 2.
5. Sample rates diferentes de 48 kHz → mesma leitura. Testes nas Tasks 1 e 2.

---

### Task 1: `KWeighting`

**Files:**
- Create: `Source/DSP/KWeighting.h`
- Create: `tests/KWeightingTests.cpp`
- Modify: `CMakeLists.txt` (adicionar o teste em `add_executable(finale_tests ...)`)

**Interfaces:**
- Produces:
  - `class finale::dsp::KWeighting`
  - `void prepare (double sampleRate, int numChannels)`: calcula os coeficientes, aloca e zera o estado.
  - `double process (int channel, double sample)`: aplica os dois estágios em cascata e devolve a amostra filtrada. Entrada não-finita vira 0.
  - `struct Coefficients { double b0, b1, b2, a1, a2; };` e `const Coefficients& stage1() const`, `const Coefficients& stage2() const`, para o teste conferir a Tabela 1/2.

Coeficientes, para qualquer `fs`, a partir dos parâmetros analógicos que reproduzem as Tabelas 1 e 2 do BS.1770-5. Os parâmetros são os do libebur128 (MIT, Jan Kokemüller), com crédito em comentário; o código é nosso:

```
// Estágio 1: shelving de cabeça
f0 = 1681.974450955533; G = 3.999843853973347 (dB); Q = 0.7071752369554196
K = tan(pi * f0 / fs); Vh = 10^(G / 20); Vb = Vh^0.4996667741545416
a0 = 1 + K / Q + K^2
b0 = (Vh + Vb * K / Q + K^2) / a0
b1 = 2 * (K^2 - Vh) / a0
b2 = (Vh - Vb * K / Q + K^2) / a0
a1 = 2 * (K^2 - 1) / a0
a2 = (1 - K / Q + K^2) / a0

// Estágio 2: high-pass RLB
f0 = 38.13547087602444; Q = 0.5003270373238773
K = tan(pi * f0 / fs); a0 = 1 + K / Q + K^2
b0 = 1; b1 = -2; b2 = 1
a1 = 2 * (K^2 - 1) / a0
a2 = (1 - K / Q + K^2) / a0
```

O estado e os coeficientes são `double`. O polo do high-pass de 38 Hz fica muito perto de 1 em 192 kHz, e `float` perde precisão. Use a forma direta I ou a forma direta II transposta, com y = b0·x + b1·x1 + b2·x2 − a1·y1 − a2·y2.

- [ ] **Step 1: Testes que falham**, em `tests/KWeightingTests.cpp`:
  - `"coeficientes a 48 kHz batem com a norma"`: `prepare (48000, 1)` → `stage1()` = {1.53512485958697, −2.69169618940638, 1.19839281085285, −1.69065929318241, 0.73248077421585} e `stage2()` = {1.0, −2.0, 1.0, −1.99004745483398, 0.99007225036621}, cada um com `Approx (...).margin (1e-6)`.
  - `"ganho em 1 kHz compensa o -0.691"`: um seno de 1 kHz com amplitude 1 a 48 kHz, 1 s. O RMS da saída no último 0.5 s, dividido pelo RMS da entrada, em dB, dá `Approx (0.691).margin (0.01)`.
  - `"resposta igual em outros sample rates"`: para fs em {44100, 96000, 192000} e f em {100, 1000, 5000} Hz, o ganho em dB medido como acima difere do ganho a 48 kHz na mesma f em no máximo 0.1 dB.
  - `"entrada não-finita vira silêncio"`: NaN e +inf entram e a saída é `std::isfinite`. Depois de 1 s de zeros, a saída é `Approx (0.0).margin (1e-9)`.
- [ ] **Step 2:** Compilar o alvo `finale_tests` e rodar o `ctest`. Esperado: FALHA (o header não existe).
- [ ] **Step 3:** Implementar `Source/DSP/KWeighting.h` com o estado por canal em `std::vector<double>`, alocado no `prepare`.
- [ ] **Step 4:** Rodar os testes. Esperado: PASSAM.
- [ ] **Step 5:** `clang-format -i` e commit: `"DSP: filtro K-weighting do BS.1770 para qualquer sample rate"`.

---

### Task 2: `LoudnessMeter`

**Files:**
- Create: `Source/DSP/LoudnessMeter.h`
- Create: `tests/LoudnessMeterTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `KWeighting` (Task 1): `prepare (double, int)`, `double process (int, double)`.
- Produces:
  - `class finale::dsp::LoudnessMeter`
  - `static constexpr float floorLufs = -100.0f;`
  - `void prepare (double sampleRate, int numChannels)`: aloca e zera tudo, inclusive o integrado.
  - `void process (const float* const* channels, int numChannels, int numSamples)`: `numChannels` ≤ o valor do `prepare`, e canais acima disso são ignorados.
  - `float getMomentaryLufs() const`, `float getShortTermLufs() const`, `float getIntegratedLufs() const`

Algoritmo (BS.1770-5 Anexo 1, eq. 1–7; Tech 3341 §2.2–2.3):

```
subBlockLength = max(1, round(0.1 * fs))          // passo de 100 ms (overlap de 75% dos blocos de 400 ms)
por amostra: e += sum_ch KWeighting.process(ch, x)^2      // G = 1 para L e R
ao fechar um sub-bloco: subEnergy = e / subBlockLength; põe num ring de 30; e = 0; subCount++
  momentaryEnergy = média dos últimos 4 sub-blocos  (sub-blocos ainda não vistos contam como 0)
  shortTermEnergy = média dos últimos 30
  se subCount >= 4: bloco de gate com energia momentaryEnergy -> histograma (abaixo)
loudness(E) = E > 0 ? -0.691 + 10 * log10(E) : -inf  ->  max(loudness, floorLufs)
```

Integrado sem alocação, num histograma pré-alocado no `prepare`. Os bins vão de −70 a +5 LUFS em passos de 0.01 LU (7500 bins). Cada bin guarda `count` (uint64) e `energySum` (double). Bloco com loudness < −70 é descartado (gate absoluto). Bloco acima de +5 entra no último bin, e o integrado continua exato porque a soma de energia é exata. A cada bloco novo:

```
E_abs = soma(energySum) / soma(count) sobre todos os bins          // já estão acima de -70
limiarRelativo = loudness(E_abs) - 10
integrado = loudness( soma(energySum) / soma(count) sobre os bins cujo limite inferior >= limiarRelativo )
```

Os valores finais ficam em três `float` membros, atualizados quando um sub-bloco fecha. Os getters só leem.

- [ ] **Step 1: Testes que falham**, em `tests/LoudnessMeterTests.cpp`. Use um helper `std::vector<float> tone (double fs, double freq, double dbfs, double seconds)`, com amplitude de pico `10^(dbfs/20)`. Os sinais estéreo põem o mesmo seno nos dois canais. Tudo a 48 kHz, estéreo e bloco de 512, salvo quando indicado; tolerância `margin (0.1)`:
  - `"Tech 3341 caso 1"`: 1 kHz a −23 dBFS por 20 s → M, S e I = −23.0.
  - `"Tech 3341 caso 2"`: 1 kHz a −33 dBFS por 20 s → M, S e I = −33.0.
  - `"Tech 3341 caso 3 (gate relativo)"`: 10 s a −36, 60 s a −23 e 10 s a −36 dBFS → I = −23.0.
  - `"Tech 3341 caso 4 (gate absoluto)"`: 10 s a −72, 10 s a −36, 60 s a −23, 10 s a −36 e 10 s a −72 dBFS → I = −23.0.
  - `"janelas de 400 ms e 3 s"`: 5 s de silêncio e depois 1 kHz a −23 dBFS. 0.2 s depois do início do tom, M = −26.0 (`margin (0.2)`). 1.5 s depois do início do tom, S = −26.0 (`margin (0.2)`). 4 s depois do início do tom, M e S = −23.0.
  - `"silêncio lê o piso"`: 5 s de zeros → M, S e I == `floorLufs`.
  - `"mono lê 3 LU abaixo"`: `prepare (48000, 1)`, 1 kHz a −23 dBFS por 20 s → I = −26.0.
  - `"sinal alto"`: 1 kHz com amplitude 4.0 (+12 dBFS) por 20 s → I = +12.0.
  - `"sessão longa"`: 600 s de 1 kHz a −23 dBFS → I = −23.0.
  - `"outros sample rates"`: o caso 1 a 44100 e a 96000 → I = −23.0.
  - `"bloco variável dá o mesmo resultado"`: os 80 s do caso 3, processados em blocos de 512 e numa sequência que repete {0, 1, 7, 512, 3} → M, S e I iguais (`margin (1e-4)`) ao fim.
  - `"entrada não-finita"`: caso 1 com um NaN e um +inf em t = 10 s → leituras sempre `std::isfinite`, e I ao fim = −23.0 (`margin (0.2)`).
  - `"prepare de novo zera o integrado"`: o caso 1, depois `prepare` e 5 s de zeros → I == `floorLufs`.
- [ ] **Step 2:** Rodar os testes. Esperado: FALHA (o header não existe).
- [ ] **Step 3:** Implementar `Source/DSP/LoudnessMeter.h` seguindo o algoritmo acima.
- [ ] **Step 4:** Rodar os testes. Esperado: todos PASSAM, inclusive os anteriores.
- [ ] **Step 5:** `clang-format -i` e commit: `"DSP: medidor de LUFS momentary/short-term/integrado (BS.1770, Tech 3341)"`.

---

### Task 3: Medir a saída no plugin

**Files:**
- Modify: `Source/Plugin/PluginProcessor.h`
- Modify: `Source/Plugin/PluginProcessor.cpp`

**Interfaces:**
- Consumes: `LoudnessMeter` (Task 2).
- Produces (para a UI): `float getOutputMomentaryLufs() const`, `float getOutputShortTermLufs() const` e `float getOutputIntegratedLufs() const` em `FinaleProcessor`, cada um lendo um `std::atomic<float>` (padrão `dsp::LoudnessMeter::floorLufs`).

- [ ] **Step 1:** No header: `#include "DSP/LoudnessMeter.h"`, o membro `dsp::LoudnessMeter outputMeter;`, os três `std::atomic<float>` iniciados com `floorLufs` e os três getters públicos (`load (std::memory_order_relaxed)`). Comentário: o editor lê de outra thread, por isso atomic.
- [ ] **Step 2:** No `prepareToPlay`: `outputMeter.prepare (sampleRate, std::max (1, getTotalNumOutputChannels()));`.
- [ ] **Step 3:** No `processBlock`, no fim, depois do limiter: `outputMeter.process (buffer.getArrayOfReadPointers(), buffer.getNumChannels(), buffer.getNumSamples());`, e depois os três `store (..., std::memory_order_relaxed)`.
- [ ] **Step 4:** Compilar tudo, rodar o `ctest` e o `clang-format -i`. Esperado: build limpo, testes passando.
- [ ] **Step 5:** Commit: `"Plugin: mede LUFS da saída e publica para a interface"`.

---

## Fora deste plano

LRA (Tech 3342) · medidor da entrada para o A/B (vem com a Issue #10) · médias de 10–30 s do Auto (vêm com a Issue #9) · reset do integrado pelo botão Analisar (vem com a Issue #9) · exibir na UI (outro desenvolvedor, que lê os três getters).
