# Compressão 3 bandas — Plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Separar o sinal em 3 bandas com crossovers Linkwitz-Riley de latência zero, comprimir cada banda e somar de volta, controlado por um parâmetro de quantidade de compressão.

**Architecture:** Um biquad compartilhado (`Biquad.h`, fórmulas do RBJ Audio EQ Cookbook), extraído do `KWeighting` sem mudar o comportamento dele. Um `ThreeBandCrossover` LR4 (200 Hz / 2 kHz) com allpass de compensação, para a soma sair plana. Um `MultibandCompressor` feedforward com detector de pico linkado entre canais e suavização no domínio de dB (Giannoulis, Massberg e Reiss, JAES 2012, "smooth branching"). O plugin ganha o parâmetro `compression` (0–100 %, padrão 0) e põe o compressor antes do limiter. Um PR, Issue #5.

**Tech Stack:** C++20, Catch2 v3.16.0, JUCE 9.0.3 (só em `Plugin/`).

**Spec:** `docs/decisoes.md` (cadeia EQ → compressão 3 bandas → saturação → largura → limiter; crossovers Linkwitz-Riley de latência zero; "quantidade de compressão" nas configurações avançadas) + regras de `CLAUDE.md`.

## Global Constraints

- `Source/DSP/` usa só a biblioteca padrão; namespace `finale::dsp`; header-only; nada de JUCE.
- Audio thread: nada de alocação, lock, I/O ou log em `process`. Toda memória é alocada em `prepare`.
- Qualquer sample rate e tamanho de bloco, inclusive 0 e 1. Saída sempre finita.
- Latência zero: nada de lookahead, nada de FIR. `setLatencySamples` não muda.
- Parâmetro novo `compression` com ID permanente. Os IDs existentes ficam intactos.
- Implementação mínima: crossovers, limiares e tempos fixos no código. Presets, caráter e o Auto (#9) ajustam depois. Marque cada valor fixo com `// shortcut: <limite>, <quando mudar>`.
- Identificadores em inglês; comentários e commits em português; cite a fonte (RBJ Cookbook, Giannoulis 2012).
- `clang-format -i` (22.1.8). Não mexer no editor.

## Review Focus

1. Automação do `compression` com a música tocando → sem estalo. Teste "mudança de razão sem estalo" na Task 2.
2. `compression` em 0 % → volume igual (só a fase do allpass muda). Teste "razão 1 não muda o volume" na Task 2.
3. Bloco variável → saída idêntica. Teste na Task 2.
4. Entrada NaN/infinita → saída finita, que se recupera depois. Teste na Task 2.
5. Sample rates diferentes de 48 kHz → mesma resposta. Testes nas Tasks 1 e 2.

---

### Task 1: `Biquad` + `ThreeBandCrossover`

**Files:**
- Create: `Source/DSP/Biquad.h`
- Create: `Source/DSP/Crossover.h`
- Modify: `Source/DSP/KWeighting.h` (passa a usar o `Biquad.h`; mesmo comportamento)
- Create: `tests/CrossoverTests.cpp`
- Modify: `CMakeLists.txt` (adicionar o teste)

**Interfaces:**
- Produces:
  - `struct finale::dsp::BiquadCoefficients { double b0, b1, b2, a1, a2; };`
  - `BiquadCoefficients makeLowPass (double fs, double f0, double q)`, `makeHighPass (...)` e `makeAllPass (...)`: funções livres, fórmulas LPF/HPF/APF do RBJ Cookbook normalizadas por a0.
  - `struct finale::dsp::BiquadState { double x1, x2, y1, y2; double process (const BiquadCoefficients&, double x); };`, com y = b0·x + b1·x1 + b2·x2 − a1·y1 − a2·y2.
  - `KWeighting`: `using Coefficients = BiquadCoefficients;`. A API pública e os testes continuam iguais.
  - `class finale::dsp::ThreeBandCrossover`
    - `void prepare (double sampleRate, int numChannels, double lowMidHz, double midHighHz)`
    - `void process (int channel, double x, double& low, double& mid, double& high)`

Topologia (cada LR4 = dois Butterworth idênticos de 2ª ordem em cascata, Q = 1/√2):

```
low  = AP(f2)( LP(f1)·LP(f1)(x) )       // allpass de 2ª ordem em f2 (Q = 1/√2) = fase de LP4+HP4 em f2
rest = HP(f1)·HP(f1)(x)
mid  = LP(f2)·LP(f2)(rest)
high = HP(f2)·HP(f2)(rest)
low + mid + high = allpass  ->  magnitude plana
```

Entrada não-finita vira 0 antes de entrar no estado.

- [ ] **Step 1: Testes que falham**, em `tests/CrossoverTests.cpp`. O ganho em f é medido com um seno de amplitude 1 por 1 s, como 20·log10(RMS de saída / RMS de entrada) no último 0.5 s. Crossovers em 200 Hz e 2000 Hz, a 48 kHz, salvo quando indicado:
  - `"Butterworth passa-baixa dá -3 dB em f0"`: `makeLowPass (48000, 1000, 1/√2)` aplicado a um seno de 1 kHz → `Approx (-3.01).margin (0.05)`.
  - `"allpass tem magnitude 1"`: `makeAllPass (48000, 2000, 1/√2)` em 100, 2000 e 10000 Hz → `Approx (0.0).margin (0.01)`.
  - `"soma das bandas é plana"`: low + mid + high em 50, 200, 1000, 2000 e 8000 Hz → `Approx (0.0).margin (0.01)` dB.
  - `"Linkwitz-Riley dá -6 dB no corte"`: em 200 Hz, low e mid ficam cada um em `Approx (-6.02).margin (0.2)`.
  - `"cada banda isola sua faixa"`: em 50 Hz, low ≥ −0.5 dB e high ≤ −30 dB; em 10 kHz, high ≥ −0.5 dB e low ≤ −30 dB.
  - `"soma plana em outros sample rates"`: o teste da soma a 44100 e a 96000.
  - `"entrada não-finita"`: NaN e +inf → bandas finitas, e depois de 1 s de zeros as bandas valem ~0 (`margin (1e-9)`).
- [ ] **Step 2:** Rodar os testes. Esperado: FALHA (os headers não existem).
- [ ] **Step 3:** Implementar `Biquad.h` e `Crossover.h`, e trocar o biquad privado do `KWeighting` pelo do `Biquad.h`.
- [ ] **Step 4:** Rodar os testes. Esperado: todos PASSAM, inclusive os de K-weighting e LUFS (refactor sem mudança de comportamento).
- [ ] **Step 5:** `clang-format -i` e commit: `"DSP: biquad compartilhado e crossover Linkwitz-Riley de 3 bandas"`.

---

### Task 2: `MultibandCompressor`

**Files:**
- Create: `Source/DSP/MultibandCompressor.h`
- Create: `tests/MultibandCompressorTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `ThreeBandCrossover` (Task 1).
- Produces:
  - `class finale::dsp::MultibandCompressor`
  - `void prepare (double sampleRate, int numChannels)`: crossover em 200/2000 Hz, aloca e zera tudo.
  - `void setRatio (float ratio)`: vale para as 3 bandas, com clamp em ≥ 1.
  - `void process (float* const* channels, int numChannels, int numSamples)`: canais acima do `prepare` não são tocados.

Valores fixos (cada um com `// shortcut:` apontando para presets e Auto): limiar −18 dBFS nas 3 bandas; attack/release de 30/200 ms na low, 10/120 ms na mid e 5/80 ms na high.

Por amostra (Giannoulis et al. 2012, detector de pico no domínio de dB com suavização "branching"):

```
para cada canal: crossover.process(ch, x) -> b[ch][0..2]        // não-finito já vira 0 no crossover
para cada banda k:
  level = max_ch |b[ch][k]|                                      // linkado entre canais
  xdb   = 20 * log10(max(level, 1e-10))
  over  = xdb - thresholdDb
  gc    = over > 0 ? -over * (1 - 1 / ratio) : 0                  // curva estática, joelho duro
  gs    = gc < gs ? aA * gs + (1 - aA) * gc : aR * gs + (1 - aR) * gc   // a = exp(-1 / (tau * fs))
  gain_k = 10^(gs / 20)
y[ch] = sum_k b[ch][k] * gain_k
```

O estado `gs` começa em 0 dB. Os buffers de banda por canal são alocados no `prepare`. Mudança de `ratio` muda `gc` na hora, mas `gs` suaviza a transição, e é isso que evita o estalo.

- [ ] **Step 1: Testes que falham**, em `tests/MultibandCompressorTests.cpp`. Senos são estéreo (o mesmo nos dois canais), a 48 kHz e em blocos de 512, salvo quando indicado. "Nível" = 20·log10(RMS) da saída no último 1 s de um sinal de 2 s:
  - `"razão 1 não muda o volume"`: ratio 1; seno de −6 dBFS de pico em 50, 1000 e 8000 Hz → nível de saída − nível de entrada = `Approx (0.0).margin (0.01)` dB.
  - `"abaixo do limiar não muda"`: ratio 4; seno de 1 kHz a −30 dBFS → diferença `Approx (0.0).margin (0.01)` dB.
  - `"acima do limiar comprime pela razão"`: ratio 4; seno de 1 kHz a −6 dBFS de pico → nível de saída = `Approx (-18.1).margin (0.5)` dBFS RMS. (A banda mid a ~−6.3 dB passa ~11.7 dB do limiar; 4:1 tira ~8.8 dB, o pico vai a ~−15.1 e o RMS a ~−18.1.)
  - `"bandas são independentes"`: ratio 4; 50 Hz a −6 dBFS somado a 8 kHz a −30 dBFS. Num `ThreeBandCrossover` novo, aplicado à entrada e à saída, o RMS da banda high muda no máximo 0.2 dB, enquanto o da banda low cai mais de 3 dB.
  - `"mudança de razão sem estalo"`: 1 kHz a −6 dBFS por 2 s, ratio 1 até 1.0 s e 4 depois disso → no trecho [1.0 s, 1.1 s], max |y[n] − y[n−1]| ≤ 0.07 (num seno de amplitude 0.5 a 1 kHz, a diferença máxima natural é ~0.065).
  - `"bloco variável dá o mesmo resultado"`: o sinal do teste acima, processado em blocos de 512 e numa sequência que repete {0, 1, 7, 512, 3} → saídas iguais (`margin (1e-6)`).
  - `"entrada não-finita"`: um NaN e um +inf no meio de um seno → saída sempre `std::isfinite`, e o nível do último 0.5 s igual ao de uma execução sem os valores ruins (`margin (0.1)` dB).
  - `"mono funciona"`: `prepare (48000, 1)` e o teste "acima do limiar" com 1 canal → `Approx (-18.1).margin (0.5)`.
  - `"outros sample rates"`: o teste "acima do limiar" a 44100 e a 96000.
- [ ] **Step 2:** Rodar os testes. Esperado: FALHA (o header não existe).
- [ ] **Step 3:** Implementar `Source/DSP/MultibandCompressor.h`.
- [ ] **Step 4:** Rodar os testes. Esperado: todos PASSAM.
- [ ] **Step 5:** `clang-format -i` e commit: `"DSP: compressor de 3 bandas (Giannoulis 2012)"`.

---

### Task 3: Parâmetro `compression` e ligação no plugin

**Files:**
- Modify: `Source/Plugin/PluginProcessor.h`
- Modify: `Source/Plugin/PluginProcessor.cpp`

**Interfaces:**
- Consumes: `MultibandCompressor` (Task 2).
- Produces (para a UI): `ParamIDs::compression = "compression"`: Float, nome exibido `Compression`, 0–100, passo 0.1, padrão 0, label `%`, `ParameterID { id, 1 }`. Entra no fim do layout.

- [ ] **Step 1:** Adicionar o ID e o parâmetro. Adicionar o membro `std::atomic<float>* compressionParam` (iniciado no construtor) e `dsp::MultibandCompressor compressor;`.
- [ ] **Step 2:** No `prepareToPlay`: `compressor.prepare (sampleRate, std::max (1, getTotalNumOutputChannels()));`.
- [ ] **Step 3:** No `processBlock`, entre o `gain.process` e o limiter: `compressor.setRatio (1.0f + 3.0f * compressionParam->load() / 100.0f);` e depois `compressor.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());`. Comentário com `// shortcut: 0–100 % vira razão 1:1–4:1; o knob principal (#11) e os presets vão mapear isso depois`.
- [ ] **Step 4:** Compilar tudo, rodar o `ctest` e o `clang-format -i`. Esperado: build limpo, testes passando.
- [ ] **Step 5:** Commit: `"Plugin: compressão 3 bandas antes do limiter, parâmetro compression"`.

---

## Fora deste plano

Joelho suave · makeup gain (o loudness é do limiter e do Auto) · crossovers, limiares e tempos por preset ou caráter (Punch = attack rápido) · Auto ajustando limiares por banda (#9) · ligar no knob `amount` (#11) · medidor de redução de ganho na UI.
