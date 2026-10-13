# Saturação com oversampling 2x — Plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Saturação suave (tanh) com oversampling 2x entre o compressor e o limiter, controlada por um parâmetro `saturation`. O caráter DOOM usa um drive bem maior (crunch); o teto de true peak continua garantido pelo limiter, que vem depois.

**Architecture:** Um `HalfbandOversampler` (FIR de fase linear com 63 taps, sinc com janela Blackman calculada no `prepare`) e um `Saturator`. O saturator faz upsample, aplica `tanh(g·x)/g`, faz downsample e mistura com o seco atrasado. Drive e mix são suavizados por amostra. O plugin soma a latência do oversampling à do limiter. Um PR, Issue #7.

**Tech Stack:** C++20, Catch2 v3.16.0, JUCE 9.0.3 (só em `Plugin/`).

**Spec:** `docs/decisoes.md` (cadeia EQ → compressão → saturação → largura → limiter; oversampling 2x na saturação; DOOM = soft clip agressivo, harmônico, respeitando o teto) + regras de `CLAUDE.md`.

## Global Constraints

- `Source/DSP/` usa só a biblioteca padrão; namespace `finale::dsp`; header-only; nada de JUCE.
- Audio thread: nada de alocação, lock, I/O ou log em `process`. Memória alocada em `prepare`.
- Qualquer sample rate e tamanho de bloco, inclusive 0 e 1. Saída sempre finita. Entrada não-finita vira 0.
- Latência nova reportada com `setLatencySamples`: o total passa a ser a do limiter mais a da saturação.
- Parâmetro novo `saturation` com ID permanente; IDs existentes intactos. Mudança de parâmetro é suavizada, sem estalo.
- Implementação mínima: drive máximo fixo (18 dB normal, 30 dB no DOOM), curva única (tanh). Marque cada valor fixo com `// shortcut: <limite>, <quando mudar>`.
- Identificadores em inglês; comentários e commits em português; cite a fonte.
- `clang-format -i` (22.1.8). Não mexer no editor.

## Review Focus

1. Mexer no `saturation` ou trocar o caráter para DOOM com a música tocando → sem estalo. Teste na Task 2.
2. `saturation` em 0 % → atraso puro, sem colorir o som. Teste na Task 2.
3. Aliasing: harmônico dobrando para dentro da banda audível. Teste na Task 2.
4. Bloco variável e NaN. Testes nas Tasks 1 e 2.
5. Latência reportada = atraso real. Testes nas Tasks 1 e 2 (atraso puro = `latency`).

---

### Task 1: `HalfbandOversampler`

**Files:**
- Create: `Source/DSP/Oversampler.h`
- Create: `tests/OversamplerTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:
  - `class finale::dsp::HalfbandOversampler`
  - `static constexpr int taps = 63;` e `static constexpr int latency = 31;` (ida e volta, em amostras da taxa base: (63 − 1) / 2 amostras na taxa 2x por filtro, dois filtros)
  - `void prepare (int numChannels)`: calcula os coeficientes uma vez e aloca e zera o histórico.
  - `void upsample (int channel, double x, double& y0, double& y1)`: uma amostra base vira duas amostras 2x.
  - `double downsample (int channel, double y0, double y1)`: duas amostras 2x viram uma amostra base.

```
h[k] = 0.5 * sinc((k - 31) / 2) * blackman(k),   k = 0..62     // corte em fs_base/2 na taxa 2x
sinc(x) = x == 0 ? 1 : sin(pi x)/(pi x);  blackman(k) = 0.42 - 0.5 cos(2 pi k/62) + 0.08 cos(4 pi k/62)
normalize h para soma 1 (ganho DC 1 na taxa 2x)
up:   insere zero entre amostras e filtra com 2·h (o 2 compensa o zero inserido)
down: filtra com h na taxa 2x e fica com uma amostra a cada duas
```

Use um histórico circular por canal para cada filtro. Implementar em polifase é opcional; o resultado tem que ser o mesmo. Entrada não-finita no `upsample` vira 0.

- [ ] **Step 1: Testes que falham**, em `tests/OversamplerTests.cpp`. Ida e volta = `downsample (ch, up0, up1)` com as duas saídas do `upsample`:
  - `"ida e volta é atraso puro"`: seno de 1 kHz com amplitude 0.5 a 48 kHz, 1 s → `saída[n + latency] == Approx (entrada[n]).margin (1e-3)`, para n ≥ 200.
  - `"resposta plana na banda útil"`: ganho da ida e volta (RMS do último 0.5 s de 1 s) em 100, 1000, 10000 e 15000 Hz = `Approx (0.0).margin (0.1)` dB; em 20000 Hz, ≥ −3 dB.
  - `"rejeita imagem acima de fs/2"`: upsample de um seno de 10 kHz. A amostra 2x passa por um DFT de um bin em 38 kHz (a imagem), sobre o último 0.5 s a 96 kHz, e fica pelo menos 60 dB abaixo do bin de 10 kHz.
  - `"entrada não-finita"`: NaN e +inf → saídas finitas, e depois de 200 zeros a saída é `Approx (0.0).margin (1e-9)`.
- [ ] **Step 2:** Rodar os testes. Esperado: FALHA (o header não existe).
- [ ] **Step 3:** Implementar `Source/DSP/Oversampler.h`.
- [ ] **Step 4:** Rodar os testes. Esperado: PASSAM.
- [ ] **Step 5:** `clang-format -i` e commit: `"DSP: oversampler 2x half-band (FIR de fase linear)"`.

---

### Task 2: `Saturator`

**Files:**
- Create: `Source/DSP/Saturator.h`
- Create: `tests/SaturatorTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `HalfbandOversampler` (Task 1).
- Produces:
  - `class finale::dsp::Saturator`
  - `static constexpr int latency = HalfbandOversampler::latency;`
  - `void prepare (double sampleRate, int numChannels)`: aloca o oversampler e a linha de atraso do sinal seco (`latency` amostras por canal). Zera tudo, e os suavizadores começam no alvo atual. Os alvos padrão são drive 0 dB e mix 0, então `prepare` seguido de `setMix (0)` não gera rampa.
  - `void setDriveDecibels (float db)`: clamp em ≥ 0.
  - `void setMix (float mix)`: clamp em [0, 1].
  - `void process (float* const* channels, int numChannels, int numSamples)`: in place; canais acima do `prepare` não são tocados.

Por amostra base, por canal, com `driveDb` e `mix` suavizados por um one-pole de 20 ms na taxa base (comuns aos canais):

```
g = 10^(driveDb / 20)
upsample(x) -> u0, u1;   w_i = tanh(g * u_i) / g          // tanh(gx)/g: sinal pequeno passa com ganho 1
wet = downsample(w0, w1)
dry = x atrasado de `latency` amostras                     // alinha seco e molhado
y = dry + mix * (wet - dry)
```

- [ ] **Step 1: Testes que falham**, em `tests/SaturatorTests.cpp`. 48 kHz, estéreo (o mesmo sinal nos dois canais), blocos de 512, salvo quando indicado. DFT de um bin sobre o último 1 s de 2 s (bins inteiros de Hz, sem janela):
  - `"mix 0 é atraso puro"`: drive 18 dB, mix 0; seno de 1 kHz com amplitude 0.9 → `saída[n + latency] == Approx (entrada[n]).margin (1e-6)` (n ≥ 0; é o seco atrasado).
  - `"sinal pequeno passa intacto"`: drive 18 dB, mix 1, seno de 1 kHz a −60 dBFS → nível de saída − entrada = `Approx (0.0).margin (0.1)` dB.
  - `"sinal alto é saturado"`: drive 18 dB, mix 1, seno de 1 kHz com amplitude 1.0 → pico da saída no último 1 s ≤ 0.15 (tanh(7.94)/7.94 ≈ 0.126, mais o overshoot do filtro), e o bin de 3 kHz ≥ −30 dB em relação ao de 1 kHz (há harmônico).
  - `"aliasing rejeitado"`: drive 6 dB, mix 1, seno de 10 kHz com amplitude 1.0 → o bin de 18 kHz (onde cairia o 3º harmônico de 30 kHz sem oversampling) fica ≤ −50 dB em relação ao de 10 kHz.
  - `"mudança de mix sem estalo"`: drive 18 dB, seno de 1 kHz com amplitude 0.5 por 2 s, mix 0 até 1.0 s e 1 depois disso → no trecho [1.0 s + latency, 1.1 s], max |y[n] − y[n−1]| ≤ 0.07.
  - `"mudança de drive sem estalo"`: mix 1, o mesmo seno, drive 18 dB até 1.0 s e 30 dB depois disso → mesmo critério.
  - `"bloco variável dá o mesmo resultado"`: o sinal do teste de mix, em blocos de 512 e numa sequência que repete {0, 1, 7, 512, 3} → saídas iguais (`margin (1e-6)`).
  - `"entrada não-finita"`: um NaN e um +inf no meio de um seno (drive 18, mix 1) → saída sempre finita, e o nível do último 0.5 s igual ao de uma execução limpa (`margin (0.1)` dB).
  - `"mono e outros sample rates"`: `prepare (48000, 1)`, `prepare (44100, 2)` e `prepare (96000, 2)` com o teste do sinal pequeno.
- [ ] **Step 2:** Rodar os testes. Esperado: FALHA (o header não existe).
- [ ] **Step 3:** Implementar `Source/DSP/Saturator.h`.
- [ ] **Step 4:** Rodar os testes. Esperado: PASSAM.
- [ ] **Step 5:** `clang-format -i` e commit: `"DSP: saturação tanh com oversampling 2x"`.

---

### Task 3: Parâmetro `saturation` e ligação no plugin

**Files:**
- Modify: `Source/Plugin/PluginProcessor.h`
- Modify: `Source/Plugin/PluginProcessor.cpp`

**Interfaces:**
- Consumes: `Saturator` (Task 2); `ParamIDs::character` (choice existente: Clean = 0, Warm = 1, Punch = 2, Wide = 3, DOOM = 4).
- Produces (para a UI): `ParamIDs::saturation = "saturation"`, Float, nome exibido `Saturation`, 0–100, passo 0.1, padrão 0, label `%`, `ParameterID { id, 1 }`, no fim do layout.

- [ ] **Step 1:** Adicionar o ID e o parâmetro. Adicionar os membros `std::atomic<float>* saturationParam` e `characterParam` (iniciados no construtor) e `dsp::Saturator saturator;`.
- [ ] **Step 2:** No `prepareToPlay`: `saturator.prepare (sampleRate, std::max (1, getTotalNumOutputChannels()));` e `setLatencySamples (limiter.getLatencySamples() + dsp::Saturator::latency);`, no lugar da linha atual.
- [ ] **Step 3:** No `processBlock`, entre o compressor e o limiter:
  ```
  const bool doom = juce::roundToInt (characterParam->load()) == 4;
  saturator.setDriveDecibels (doom ? 30.0f : 18.0f);   // shortcut: drive fixo; presets/caráter (Warm) vão ajustar depois
  saturator.setMix (saturationParam->load() / 100.0f);
  saturator.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
  ```
- [ ] **Step 4:** Compilar tudo, rodar o `ctest` e o `clang-format -i`. Esperado: build limpo, testes passando.
- [ ] **Step 5:** Commit: `"Plugin: saturação com oversampling antes do limiter, parâmetro saturation"`.

---

## Fora deste plano

Curvas diferentes por caráter (Warm assimétrica, DOOM hard/soft) · drive por preset · compensação de loudness · oversampling 4x · ligar no knob `amount` (#11).
