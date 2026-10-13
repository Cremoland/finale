# Fundação do backend: parâmetros + limiter true peak — Plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publicar o contrato de parâmetros que a interface vai usar e colocar no fim da cadeia um limiter de true peak que garante o teto em dBTP.

**Architecture:** Dois PRs. PR 1 (Task 1) só adiciona parâmetros no `Plugin/`. PR 2 (Tasks 2–4) cria dois blocos em `Source/DSP/` sem JUCE (`TruePeakDetector`, `TruePeakLimiter`), testados com Catch2, e liga o limiter depois do ganho no `processBlock`.

**Tech Stack:** C++20, JUCE 9.0.3 (só em `Plugin/`), Catch2 v3.16.0, CMake ≥ 3.22, MSVC 2022.

**Spec:** `docs/decisoes.md` + regras de `CLAUDE.md`.

## Global Constraints

- `Source/DSP/` usa só a biblioteca padrão; namespace `finale::dsp`; nada de JUCE.
- Audio thread: nada de alocação, lock, I/O ou log em `process`/`processBlock`. Toda memória é alocada em `prepare`.
- Funciona em qualquer sample rate e tamanho de bloco, inclusive bloco de 1 e de 0 amostras.
- A saída nunca tem NaN/infinito e nunca passa do teto de true peak configurado.
- Latência adicionada é reportada com `setLatencySamples`.
- IDs de parâmetro são permanentes; o ID `gain` existente fica como está. A ordem das opções dos parâmetros de escolha também é permanente (novas opções só no fim).
- Nunca escrever "Finale" ou "Cremoland" no código.
- Identificadores em inglês; comentários e commits em português; comente o porquê.
- `clang-format -i` (22.1.8) em todo arquivo alterado antes do commit.
- Teto padrão: −1 dBTP.

## Review Focus

1. Host manda NaN/infinito na entrada → saída finita (amostra vira 0). Teste na Task 3.
2. Host varia o tamanho do bloco a cada chamada (0, 1, 7, 512…) → saída idêntica à de bloco fixo. Teste na Task 3.
3. Teto abaixado no meio da música (automação) → depois da latência a saída respeita o novo teto. Teste na Task 3.
4. `prepare` chamado de novo em outro sample rate → estado zerado, latência recalculada, nada da execução anterior vaza. Teste na Task 3.
5. Layout mono (1 canal) → funciona igual. Teste na Task 3.

---

### Task 1: Contrato de parâmetros (PR 1)

**Files:**
- Modify: `Source/Plugin/PluginProcessor.h` (namespace `ParamIDs`)
- Modify: `Source/Plugin/PluginProcessor.cpp` (`createParameterLayout`)

**Interfaces:**
- Produces (a UI e as Tasks seguintes usam exatamente estes IDs):

| ID (`ParamIDs::…`) | Tipo | Nome exibido | Faixa / opções | Padrão | Label |
|---|---|---|---|---|---|
| `amount` | Float | `Amount` | 0–100, passo 0.1 | 50 | `%` |
| `ceiling` | Float | `Ceiling` | −6.0–0.0, passo 0.1 | −1.0 | `dBTP` |
| `targetLufs` | Float | `Target LUFS` | −16.0–−5.0, passo 0.5 | −9.0 | `LUFS` |
| `genre` | Choice | `Genre` | `Trap`, `Rap/Boom bap`, `Lo-fi`, `Funk BR`, `House`, `EDM`, `Pop`, `Rock` | índice 6 (`Pop`) | — |
| `character` | Choice | `Character` | `Clean`, `Warm`, `Punch`, `Wide`, `DOOM` | índice 0 (`Clean`) | — |

Todos com `juce::ParameterID { id, 1 }`. O `gain` existente continua primeiro e intacto.

- [ ] **Step 1:** Adicionar as cinco constantes em `ParamIDs` (`inline constexpr auto amount = "amount";` etc.) e os parâmetros da tabela no `createParameterLayout`, depois do `gain`. Floats com `juce::AudioParameterFloatAttributes().withLabel (...)`; escolhas com `juce::AudioParameterChoice`. Comentário de uma linha acima de `genre`/`character`: a ordem das opções é permanente.
- [ ] **Step 2:** Compilar: `cmake -B build -G "Visual Studio 17 2022" -A x64` e `cmake --build build --config Release --parallel`. Esperado: compila sem warning novo.
- [ ] **Step 3:** `ctest --test-dir build -C Release --output-on-failure`. Esperado: testes existentes passam.
- [ ] **Step 4:** `clang-format -i Source/Plugin/PluginProcessor.h Source/Plugin/PluginProcessor.cpp` e commit: `git commit -m "Parâmetros base: amount, ceiling, targetLufs, genre, character"`.

Nesta task os parâmetros ainda não mudam o som (só `ceiling` passa a valer na Task 4). Isso vai no texto do PR.

---

### Task 2: `TruePeakDetector` (PR 2)

**Files:**
- Create: `Source/DSP/TruePeakDetector.h`
- Create: `tests/TruePeakDetectorTests.cpp`
- Modify: `CMakeLists.txt` (adicionar o arquivo de teste em `add_executable(finale_tests ...)`)

**Interfaces:**
- Produces:
  - `class finale::dsp::TruePeakDetector`
  - `void prepare (int numChannels)`: aloca o histórico e zera o estado.
  - `void reset()`: zera o histórico sem alocar.
  - `float push (int channel, float sample)`: recebe uma amostra e devolve o maior |valor| entre os 4 pontos interpolados do intervalo que começa `latency` amostras atrás.
  - `static constexpr int latency = 6;`

Algoritmo (a fonte é o interpolador 4x da ITU-R BS.1770-4, Anexo 2; os coeficientes são gerados em vez de copiados):

```
// 48 taps no grid 4x, centro em 24: a fase 0 reproduz a amostra exata.
h[k] = sinc((k - 24) / 4.0) * blackman(k),  k = 0..47
sinc(x) = x == 0 ? 1 : sin(pi x) / (pi x)
blackman(k) = 0.42 - 0.5 cos(2 pi k / 48) + 0.08 cos(4 pi k / 48)
// normalize cada fase p (taps 4j+p, j = 0..11) para somar 1: DC passa intacto
y_p[n] = sum_{j=0..11} x[n - j] * h[4j + p],   p = 0..3
push() devolve max_p |y_p[n]|   // pontos em n-6, n-5.75, n-5.5, n-5.25
```

Os coeficientes são calculados uma vez (construtor ou `prepare`), nunca no `push`.

- [ ] **Step 1: Testes que falham**, em `tests/TruePeakDetectorTests.cpp`:
  - `"silêncio dá pico zero"`: 256 amostras de 0 → todo `push` devolve exatamente `0.0f`.
  - `"DC passa intacto"`: 256 amostras de 0.5 → depois das primeiras 12, `push` devolve `Approx (0.5f).epsilon (1e-4)`.
  - `"acha o pico entre amostras"`: seno em fs/4 com fase π/4, amplitude 1 (`x[n] = sin(pi/2 * n + pi/4)`; as amostras valem ±0.7071). Depois de 12 amostras, o maior valor devolvido fica entre 0.98 e 1.02.
  - `"canais são independentes"`: canal 0 recebe 1.0 constante e canal 1 recebe 0 → depois de 12 amostras, o canal 1 devolve 0.
- [ ] **Step 2:** Compilar e rodar `ctest --test-dir build -C Release --output-on-failure`. Esperado: FALHA (o header não existe).
- [ ] **Step 3:** Implementar `Source/DSP/TruePeakDetector.h` (header-only, como `Gain.h`), com histórico circular de 12 amostras por canal em `std::vector` alocado no `prepare`.
- [ ] **Step 4:** Rodar os testes. Esperado: PASSA.
- [ ] **Step 5:** `clang-format -i` e commit: `"DSP: detector de true peak (interpolação 4x, BS.1770)"`.

---

### Task 3: `TruePeakLimiter` (PR 2)

**Files:**
- Create: `Source/DSP/TruePeakLimiter.h`
- Create: `tests/TruePeakLimiterTests.cpp`
- Modify: `CMakeLists.txt` (adicionar o arquivo de teste)

**Interfaces:**
- Consumes: `TruePeakDetector` (Task 2): `prepare`, `reset`, `push`, `latency`.
- Produces:
  - `class finale::dsp::TruePeakLimiter`
  - `void prepare (double sampleRate, int numChannels)`: aloca tudo, zera o estado, calcula a latência.
  - `void setCeilingDecibels (float db)`
  - `void process (float* const* channels, int numChannels, int numSamples)`: `numChannels` ≤ o valor do `prepare`; canais acima disso não são tocados.
  - `int getLatencySamples() const`

Valores fixos: lookahead `L = max (1, round (0.002 * sampleRate))` amostras; release de 100 ms (`releaseCoef = 1 - exp (-1 / (0.1 * sampleRate))`).

Algoritmo, por amostra e com ganho linkado entre os canais (fonte: o limiter de lookahead do Signalsmith Audio, "Designing a straightforward limiter"):

```
x_ch = isfinite(x_ch) ? x_ch : 0                    // entrada não-finita do host vira silêncio
peak = max_ch detector.push(ch, x_ch)              // atrasado TruePeakDetector::latency (Dd)
required = peak > ceilingLin ? ceilingLin / peak : 1
held = slidingMin(required, janela = L + 1)        // +1: o pico entre k e k+1 protege as duas amostras
released = held < prev ? held : prev + (held - prev) * releaseCoef   // sempre <= held
gain = movingAverage(released, janela = L)         // soma em double, para não acumular erro
y_ch = clamp(delay(x_ch, Dd + L - 1) * gain, -ceilingLin, ceilingLin)   // clamp: rede de segurança
latência = Dd + L - 1
```

O mínimo deslizante usa uma deque monotônica num buffer circular pré-alocado (O(1) amortizado, sem alocar). Por que funciona: a média de L valores que são todos ≤ `required[k]` também é ≤ `required[k]` no instante em que a amostra k sai.

- [ ] **Step 1: Testes que falham**, em `tests/TruePeakLimiterTests.cpp`. Copie para o arquivo de teste a função `truePeak(const std::vector<float>&)`, que mede o true peak passando o sinal por um `TruePeakDetector` novo. Todos os testes rodam a 48 kHz e teto de −1 dB, salvo quando indicado:
  - `"sinal alto fica abaixo do teto"`: seno de 997 Hz com amplitude 4.0 (+12 dBFS), 1 s → `truePeak(saída) <= 0.8913f * 1.001f` (−1 dBTP, 0.1% de tolerância de medição), e todo `|y| <= 0.8913f`.
  - `"sinal baixo passa intacto"`: seno de 997 Hz com amplitude 0.1, 0.5 s → `saída[n + latência] == Approx (entrada[n]).margin (1e-6)`.
  - `"latência bate com o atraso real"`: impulso de 0.5 em n = 100 → o maior |valor| da saída está em n = `100 + getLatencySamples()`.
  - `"bloco variável dá o mesmo resultado"`: o mesmo ruído com amplitude 2.0 (gerador fixo: `std::mt19937 { 42 }`), processado em blocos de 512 e numa sequência que repete {0, 1, 7, 512, 3} → saídas iguais amostra a amostra (`margin (1e-6)`).
  - `"nunca gera NaN"`: entrada com `NaN`, `+inf`, `1e6f` e `-1e6f` no meio de um seno → toda saída é `std::isfinite` e ≤ teto.
  - `"teto abaixado no meio"`: seno com amplitude 2.0, teto 0 dB por 0.25 s e depois −6 dB → depois de `getLatencySamples()` amostras da mudança, todo `|y| <= 0.5012f` e o true peak ≤ `0.5012f * 1.001f`.
  - `"prepare de novo zera o estado"`: prepara a 48 kHz e processa ruído; prepara a 96 kHz e processa 1000 zeros → saída toda 0, e `getLatencySamples()` == `6 + 192 - 1`.
  - `"mono funciona"`: `prepare (48000, 1)` e o teste do sinal alto com 1 canal.
  - `"funciona em vários sample rates"`: para 44100, 88200 e 192000, o teste do sinal alto passa.
- [ ] **Step 2:** Rodar os testes. Esperado: FALHA (o header não existe).
- [ ] **Step 3:** Implementar `Source/DSP/TruePeakLimiter.h` (header-only) seguindo o algoritmo acima.
- [ ] **Step 4:** Rodar os testes. Esperado: todos PASSAM, inclusive os de `Gain` e `TruePeakDetector`.
- [ ] **Step 5:** `clang-format -i` e commit: `"DSP: limiter de true peak com lookahead"`.

---

### Task 4: Ligar o limiter no plugin (PR 2)

**Files:**
- Modify: `Source/Plugin/PluginProcessor.h`
- Modify: `Source/Plugin/PluginProcessor.cpp`

**Interfaces:**
- Consumes: `TruePeakLimiter` (Task 3); `ParamIDs::ceiling` (Task 1).

- [ ] **Step 1:** No header: `#include "DSP/TruePeakLimiter.h"`, membros `std::atomic<float>* ceilingParam = nullptr;` e `dsp::TruePeakLimiter limiter;`. No construtor: `ceilingParam = parameters.getRawParameterValue (ParamIDs::ceiling);`.
- [ ] **Step 2:** No `prepareToPlay`: `limiter.prepare (sampleRate, std::max (1, getTotalNumOutputChannels()));`, `limiter.setCeilingDecibels (ceilingParam->load());` e `setLatencySamples (limiter.getLatencySamples());`.
- [ ] **Step 3:** No `processBlock`, depois do `gain.process`: `limiter.setCeilingDecibels (ceilingParam->load());` e depois `limiter.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());`.
- [ ] **Step 4:** Compilar, rodar o `ctest` e rodar o pluginval localmente, se ele estiver instalado (senão o CI cobre). Esperado: build limpo, testes passando.
- [ ] **Step 5:** Teste manual no FL Studio: ganho em +12 dB num loop alto → o medidor da master fica abaixo de −1 dBTP, sem estalo ao mexer no teto.
- [ ] **Step 6:** `clang-format -i` e commit: `"Plugin: limiter de true peak no fim da cadeia, teto pelo parâmetro ceiling"`.

---

## Fora deste plano (Issues próprias, plano próprio quando chegar a vez)

Medidor de LUFS · crossover Linkwitz-Riley + compressão 3 bandas · EQ tonal · saturação (soft clip do DOOM, oversampling 2x) · largura estéreo com graves em mono · Analisar/Auto/Congelar · A/B com volume igualado · ligar o `amount` na cadeia inteira.
