# Knob `amount` na cadeia inteira — Plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** O knob principal `amount` controla a cadeia inteira. Em 0 % o sinal sai praticamente sem processamento; em 100 % sai a "master completa" do caráter escolhido: compressão, saturação, largura e empurrão para o limiter. Os parâmetros avançados `compression`, `saturation` e `width` passam a ser *quantidades* que escalam o que o `amount` define, como diz o `decisoes.md`.

**Architecture:** Uma função pura `mapAmount` em `Source/DSP/Macro.h`, que recebe amount, caráter e quantidades e devolve os alvos de cada bloco a partir de uma tabela fixa por caráter. O plugin chama essa função uma vez por bloco e repassa os alvos aos blocos que já existem. Um novo `dsp::Gain` ("push"), antes do limiter, dá o volume da master. Toda a suavização já existe nos blocos. Um PR, Issue #11.

**Tech Stack:** C++20, Catch2 v3.16.0, JUCE 9.0.3 (só em `Plugin/`).

**Spec:** `docs/decisoes.md` ("Knob principal: 0% ≈ sem processamento, 100% = master completa no alvo do preset. Não passa de 100%"; "Configurações avançadas: … quantidade de EQ/compressão/saturação/largura"; caracteres Clean/Warm/Punch/Wide/DOOM) + regras de `CLAUDE.md`.

## Global Constraints

- `Source/DSP/` usa só a biblioteca padrão; `finale::dsp`; header-only; nada de JUCE.
- Audio thread: nada de alocação, lock, I/O ou log. A mudança de qualquer parâmetro é suavizada; os blocos já suavizam, e o push usa o `dsp::Gain`, que tem rampa.
- IDs de parâmetro permanentes, nenhum renomeado ou removido. Os padrões de `compression` e `saturation` passam de 0 para 100 (a quantidade cheia). Essa mudança de significado altera o som de projetos salvos e vai descrita no PR.
- A tabela por caráter é um palpite inicial, para ajustar por ouvido. Ela fica num único lugar, marcada com `// shortcut: valores por ouvido; presets por gênero (#6) e o Auto (#9) vão refinar`.
- Fora do escopo: alvo de LUFS adaptativo (o `targetLufs` fica para o #9), gênero e EQ.
- Identificadores em inglês; comentários e commits em português. `clang-format -i` (22.1.8). Não mexer no editor.

## Review Focus

1. `amount` em 0 % → todos os blocos neutros, com qualquer caráter e qualquer quantidade. Teste na Task 1.
2. Quantidade de um bloco em 0 → só aquele bloco fica neutro. Teste na Task 1.
3. Entrada inválida (NaN, caráter fora da faixa, quantidade negativa) → alvos válidos e finitos. Teste na Task 1.
4. Girar o `amount` com a música tocando → sem estalo. Os blocos já suavizam; a Task 2 só repassa alvos.
5. Os alvos sempre ficam dentro da faixa que cada bloco aceita (razão ≥ 1, mix e largura nos limites). Teste na Task 1.

---

### Task 1: `mapAmount`

**Files:**
- Create: `Source/DSP/Macro.h`
- Create: `tests/MacroTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces:
  ```cpp
  namespace finale::dsp {
  struct MacroTargets { float ratio; float saturationMix; float saturationDriveDb; float width; float pushDb; };
  // amount, compressionQty e saturationQty em [0, 1]; widthQty em [0, 2]; character = índice do parâmetro (0 Clean … 4 DOOM).
  MacroTargets mapAmount (float amount, int character, float compressionQty, float saturationQty, float widthQty);
  }
  ```

Tabela "master completa" por caráter (amount = 1 e quantidades = 1):

| caráter | ratio | saturationMix | saturationDriveDb | width | pushDb |
|---|---|---|---|---|---|
| 0 Clean | 2.0 | 0.15 | 6 | 1.0 | 4 |
| 1 Warm | 2.0 | 0.50 | 6 | 1.0 | 5 |
| 2 Punch | 3.0 | 0.20 | 6 | 1.0 | 6 |
| 3 Wide | 2.0 | 0.15 | 6 | 1.5 | 4 |
| 4 DOOM | 4.0 | 1.00 | 18 | 1.1 | 8 |

Regras, com a = amount:

```
entradas não-finitas -> 0 (amount e quantidades); widthQty não-finito -> 1
clamp: a, compressionQty, saturationQty em [0, 1]; widthQty em [0, 2]
character fora de 0..4 -> 0 (Clean)
ratio             = 1 + a * (row.ratio - 1) * compressionQty
saturationMix     = a * row.saturationMix * saturationQty
saturationDriveDb = row.saturationDriveDb                    // só o caráter decide; com mix 0 não soa
width             = clamp(1 + a * (row.width - 1) * widthQty, 0, 2)
pushDb            = a * row.pushDb
```

- [ ] **Step 1: Testes que falham**, em `tests/MacroTests.cpp`:
  - `"amount 0 é neutro"`: para cada caráter 0..4 e quantidades (1, 1, 1) e (1, 1, 2) → ratio == 1, saturationMix == 0, width == 1, pushDb == 0.
  - `"amount 100 % dá a tabela"`: para cada caráter, com quantidades (1, 1, 1) → os cinco campos == a linha da tabela (`Approx`, margem 1e-6).
  - `"metade do amount é metade do caminho"`: Punch com amount 0.5 → ratio == 2.0, saturationMix == 0.1, pushDb == 3.
  - `"quantidade 0 desliga só aquele bloco"`: DOOM, amount 1. Com compressionQty 0 → ratio == 1, e mix e push ficam iguais aos da tabela. Com saturationQty 0 → mix == 0, ratio da tabela. Com widthQty 0 → width == 1.
  - `"quantidade de largura vai até 200 %"`: Wide, amount 1, widthQty 2 → width == 2.0.
  - `"entradas inválidas"`: amount NaN → saída igual à de amount 0; character −1 e 7 → mesma saída do Clean; compressionQty −0.5 → ratio == 1; amount 1.5 → mesma saída de amount 1. Todos os campos `std::isfinite`.
- [ ] **Step 2:** Rodar os testes. Esperado: FALHA (o header não existe).
- [ ] **Step 3:** Implementar `Source/DSP/Macro.h`, com a tabela como `constexpr` dentro do header.
- [ ] **Step 4:** Rodar os testes. Esperado: PASSAM.
- [ ] **Step 5:** `clang-format -i` e commit: `"DSP: mapeamento do knob amount por caráter"`.

---

### Task 2: Ligar o `amount` no plugin

**Files:**
- Modify: `Source/Plugin/PluginProcessor.h`
- Modify: `Source/Plugin/PluginProcessor.cpp`

**Interfaces:**
- Consumes: `mapAmount` e `MacroTargets` (Task 1); os blocos que já existem; `dsp::Gain` (`prepare (double)`, `setGainDecibels (float)`, `process (float* const*, int, int)`).

- [ ] **Step 1:** Header: `#include "DSP/Macro.h"`, o membro `std::atomic<float>* amountParam` (iniciado no construtor com `ParamIDs::amount`) e `dsp::Gain push;`.
- [ ] **Step 2:** `createParameterLayout`: o padrão de `compression` e de `saturation` passa de 0.0f para 100.0f. Nada mais muda: IDs, nomes e faixas ficam iguais. Comentário de uma linha acima dos dois: viraram quantidade que escala o amount.
- [ ] **Step 3:** `prepareToPlay`: `push.setGainDecibels (0.0f); push.prepare (sampleRate);`, junto dos outros prepares.
- [ ] **Step 4:** `processBlock`: no começo, depois do `gain.process`, calcule uma vez:
  ```cpp
  const auto targets = dsp::mapAmount (amountParam->load() / 100.0f, juce::roundToInt (characterParam->load()),
      compressionParam->load() / 100.0f, saturationParam->load() / 100.0f, widthParam->load() / 100.0f);
  ```
  e troque as chamadas atuais por `compressor.setRatio (targets.ratio)`, `saturator.setDriveDecibels (targets.saturationDriveDb)`, `saturator.setMix (targets.saturationMix)` e `stereoWidth.setWidth (targets.width)`. Logo antes do limiter: `push.setGainDecibels (targets.pushDb); push.process (...)`. Remova os dois comentários `// shortcut:` antigos e a constante `characterDoom`, porque o mapeamento agora mora na `Macro.h`. No `prepareToPlay`, `stereoWidth.setWidth (widthParam->load() / 100.0f)` vira `stereoWidth.setWidth (1.0f)`: a largura real chega no primeiro bloco, com suavização.
- [ ] **Step 5:** Compilar tudo, rodar o `ctest` e o `clang-format -i`. Esperado: build limpo, testes passando.
- [ ] **Step 6:** Commit: `"Plugin: knob amount controla a cadeia; avançados viram quantidade"`.

---

## Fora deste plano

Volume no alvo de LUFS (`targetLufs`), feito pelo Auto (#9) · tempos de attack por caráter (Punch mais rápido) · presets por gênero (#6) · curva não linear do knob · A/B com volume igualado (#10).
