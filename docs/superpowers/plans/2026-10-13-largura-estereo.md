# Largura estéreo com graves em mono — Plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Controle de largura estéreo (mid/side) entre a saturação e o limiter. Abrir a imagem alarga só os agudos e leva o grave para mono; fechar estreita tudo até virar mono. Em 100 % o sinal passa sem nenhuma alteração.

**Architecture:** Um bloco `StereoWidth` em `Source/DSP/`. M = (L+R)/2 e S = (L−R)/2. O side é separado em grave e agudo por um passa-alta Butterworth de 2ª ordem em 120 Hz, com o grave = S − agudo (a soma reconstrói S exatamente; usar o passa-alta no agudo dá 12 dB/oitava de rejeição do grave, contra só 6 dB/oitava do complemento de um passa-baixa). Cada parte recebe um ganho que depende da largura w, suavizada por amostra. Latência zero. Reaproveita o `Biquad.h` que já existe. Um PR, Issue #8.

**Tech Stack:** C++20, Catch2 v3.16.0, JUCE 9.0.3 (só em `Plugin/`).

**Spec:** `docs/decisoes.md` (cadeia … saturação → largura estéreo (graves em mono) → limiter; "Wide" = largura e brilho) + regras de `CLAUDE.md`.

## Global Constraints

- `Source/DSP/` usa só a biblioteca padrão; `finale::dsp`; header-only; nada de JUCE. Reaproveitar `BiquadCoefficients`, `makeHighPass` e `BiquadState` de `Source/DSP/Biquad.h`.
- Audio thread: nada de alocação, lock, I/O ou log em `process`. Qualquer sample rate e tamanho de bloco, inclusive 0 e 1. Saída sempre finita, e entrada não-finita vira 0.
- Latência zero. `setLatencySamples` não muda.
- Parâmetro novo `width` com ID permanente, padrão 100 %, que não muda o som de projetos existentes. IDs existentes intactos. A mudança de largura é suavizada, sem estalo.
- Implementação mínima: crossover fixo em 120 Hz e mapeamento linear, os dois marcados com `// shortcut: <limite>, <quando mudar>`.
- Identificadores em inglês; comentários e commits em português; cite a fonte (codificação mid/side; RBJ Cookbook para o passa-alta).
- `clang-format -i` (22.1.8). Não mexer no editor.

## Review Focus

1. `width` em 100 % → identidade bit a bit (projetos existentes não mudam). Teste na Task 1.
2. Mexer no `width` com a música tocando → sem estalo. Teste na Task 1.
3. Conteúdo mono (L = R) nunca muda, em qualquer largura. Teste na Task 1.
4. Layout mono (1 canal) → nada é tocado. Teste na Task 1.
5. Bloco variável e NaN. Testes na Task 1.

---

### Task 1: `StereoWidth`

**Files:**
- Create: `Source/DSP/StereoWidth.h`
- Create: `tests/StereoWidthTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `Biquad.h` (`makeHighPass (fs, f0, q)`, `BiquadState::process`).
- Produces:
  - `class finale::dsp::StereoWidth`
  - `void setWidth (float width)`: largura em [0, 2], com clamp; não-finito vira 1. Pode ser chamado antes do `prepare`.
  - `void prepare (double sampleRate)`: passa-alta de 120 Hz com Q = 1/√2, zera o estado e põe o valor suavizado igual ao alvo atual (padrão 1).
  - `void process (float* const* channels, int numChannels, int numSamples)`: in place. Só age com `numChannels >= 2` (canais 0 e 1); com 1 canal não toca em nada.

Por amostra (w suavizado por um one-pole de 20 ms):

```
l, r = não-finito -> 0
m = (l + r) / 2;  s = (l - r) / 2
sHigh = highPass.process(s);  sLow = s - sHigh     // soma exata: sLow + sHigh == s
lowGain  = w <= 1 ? w : 2 - w                          // 0 % -> mono; 100 % -> 1; 200 % -> grave mono
highGain = w
s2 = lowGain * sLow + highGain * sHigh
l = m + s2;  r = m - s2
```

Em w = 1, s2 = sLow + sHigh = s, e a saída é igual à entrada.

- [ ] **Step 1: Testes que falham**, em `tests/StereoWidthTests.cpp`. 48 kHz, blocos de 512, com `setWidth` chamado antes do `prepare`, salvo quando indicado. "Nível do side" = 20·log10(RMS de (L−R)/2) no último 0.5 s de 1 s. Ruído estéreo = dois `std::mt19937` (seeds 1 e 2), uniforme em [−0.5, 0.5]:
  - `"largura 100 % é identidade"`: ruído estéreo, 1 s → saída == entrada (`margin (1e-6)`) nos dois canais.
  - `"largura 0 % vira mono"`: ruído estéreo → L == R == (L_in + R_in)/2 (`margin (1e-6)`).
  - `"mid nunca muda"`: L = R = seno de 1 kHz com amplitude 0.5, para w em {0, 0.5, 1.5, 2} → saída == entrada (`margin (1e-6)`).
  - `"200 % dobra o side nos agudos"`: L = seno de 5 kHz e R = −L (side puro), amplitude 0.25 → nível do side da saída − da entrada = `Approx (6.02).margin (0.1)` dB.
  - `"200 % leva o grave para mono"`: side puro de 30 Hz → nível do side da saída − da entrada ≤ −12 dB.
  - `"mudança de largura sem estalo"`: side puro de 1 kHz com amplitude 0.25 por 2 s, w = 1 até 1.0 s e 2 depois disso. Com c[n] = |y[n] − 2y[n−1] + y[n−2]| no canal L, max c em [1.0 s, 1.1 s] ≤ 1.5 · max c em [1.5 s, 2.0 s].
  - `"bloco variável dá o mesmo resultado"`: o sinal do teste acima, em blocos de 512 e numa sequência que repete {0, 1, 7, 512, 3} → saídas iguais (`margin (1e-6)`).
  - `"entrada não-finita"`: um NaN e um +inf no meio do ruído, com w = 2 → saída sempre `std::isfinite`.
  - `"mono não é tocado"`: 1 canal com ruído e w = 2 → saída == entrada exatamente.
  - `"outros sample rates"`: o teste "200 % dobra o side nos agudos" a 44100 e a 96000.
- [ ] **Step 2:** Rodar os testes. Esperado: FALHA (o header não existe).
- [ ] **Step 3:** Implementar `Source/DSP/StereoWidth.h`.
- [ ] **Step 4:** Rodar os testes. Esperado: PASSAM.
- [ ] **Step 5:** `clang-format -i` e commit: `"DSP: largura estéreo mid/side com grave em mono"`.

---

### Task 2: Parâmetro `width` e ligação no plugin

**Files:**
- Modify: `Source/Plugin/PluginProcessor.h`
- Modify: `Source/Plugin/PluginProcessor.cpp`

**Interfaces:**
- Consumes: `StereoWidth` (Task 1).
- Produces (para a UI): `ParamIDs::width = "width"`: Float, nome exibido `Width`, 0–200, passo 0.1, padrão 100, label `%`, `ParameterID { id, 1 }`, no fim do layout.

- [ ] **Step 1:** Adicionar o ID, o parâmetro, `std::atomic<float>* widthParam` (iniciado no construtor) e `dsp::StereoWidth stereoWidth;`.
- [ ] **Step 2:** No `prepareToPlay`: `stereoWidth.setWidth (widthParam->load() / 100.0f);` e depois `stereoWidth.prepare (sampleRate);`.
- [ ] **Step 3:** No `processBlock`, entre o `saturator.process` e o limiter: `stereoWidth.setWidth (widthParam->load() / 100.0f);` e `stereoWidth.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());`.
- [ ] **Step 4:** Compilar tudo, rodar o `ctest` e o `clang-format -i`. Esperado: build limpo, testes passando.
- [ ] **Step 5:** Commit: `"Plugin: largura estéreo antes do limiter, parâmetro width"`.

---

## Fora deste plano

Crossover do grave ajustável · curva não linear · correlação/medidor de fase na UI · caráter "Wide" mudando a largura (presets) · ligar no knob `amount` (#11).
