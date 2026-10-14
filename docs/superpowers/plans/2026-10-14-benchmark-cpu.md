# Benchmark de CPU — Plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Uma ferramenta `finale_benchmark` que mede o custo de CPU do `FinaleProcessor` inteiro em várias taxas, blocos e configurações; os números ficam registrados em `docs/decisoes.md`. Junto, dois minors adiados do #26.

**Architecture:** É um executável de console ligado ao `finale_plugin` do mesmo jeito que o `finale_plugin_tests`: copia os includes e as definições dele. Não entra no `ctest`, porque tempo de relógio varia de máquina para máquina e um teste de tempo seria instável no CI. Ele processa 10 s de ruído por configuração e imprime a CPU média e o p99 do tempo por bloco, em % do tempo real do bloco. O máximo não é impresso: no spike ele variou de 21 % a 153 % na mesma configuração, ou seja, é jitter do Windows e não custo do plugin. Um PR, Issue #27. Não muda o som.

**Tech Stack:** C++20, JUCE 9.0.3, Catch2 v3.16.0.

**Spec:** Issue #27.

## Global Constraints

- Não mexer em `Source/`.
- Resultado do spike (i7-9750H, Release, amount 100): 48 kHz/512 ≈ 3,7 % de um núcleo; 96 kHz ≈ 7,3 %. Custo por bloco (48 kHz/512, um núcleo): saturador ≈ 2,0 %, compressor ≈ 0,6 %, A/B ≈ 0,4 %, limiter ≈ 0,3 %, o resto < 0,1 % cada. Decisão: não otimizar agora.
- Comentários, textos impressos e commits em português; identificadores em inglês. `clang-format -i` (22.1.8) nos `.cpp` alterados.

## Review Focus

1. O benchmark mede só o `processBlock` (a geração do ruído fica fora do cronômetro).
2. Os dois minors do #26 fechados sem afrouxar nenhum teste existente.

---

### Task 1: `finale_benchmark` + minors do #26

**Files:**
- Create: `tools/benchmark/main.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/PluginProcessorTests.cpp`
- Modify: `README.md`
- Modify: `docs/decisoes.md`

**Interfaces:**
- Consumes: `finale::FinaleProcessor` (`parameters`, `prepareToPlay`, `processBlock`), `finale::ParamIDs`.
- Produces: alvo `finale_benchmark` (exe em `build\Release\finale_benchmark.exe`).

- [ ] **Step 1: CMake.** No `CMakeLists.txt`:
  - (a) Dentro do `if(FINALE_BUILD_PLUGIN)` existente, logo depois do bloco do `finale_curvegen` (antes do `endif()`), adicionar:

```cmake

    # Benchmark de CPU do processor (fora do ctest: tempo de relógio varia por máquina).
    add_executable (finale_benchmark tools/benchmark/main.cpp)
    target_link_libraries (finale_benchmark PRIVATE finale_plugin finale_dsp)
    target_include_directories (finale_benchmark PRIVATE $<TARGET_PROPERTY:finale_plugin,INCLUDE_DIRECTORIES>)
    target_compile_definitions (finale_benchmark PRIVATE $<TARGET_PROPERTY:finale_plugin,COMPILE_DEFINITIONS>)
```

  - (b) No bloco do `finale_plugin_tests` no fim do arquivo, trocar `target_link_libraries(finale_plugin_tests PRIVATE finale_plugin Catch2::Catch2WithMain)` por `target_link_libraries(finale_plugin_tests PRIVATE finale_plugin finale_dsp Catch2::Catch2WithMain)`. Isso torna explícito o include de `Source`, que hoje chega só por tabela, pelo `finale_plugin`.

- [ ] **Step 2: Benchmark.** Criar `tools/benchmark/main.cpp` com exatamente:

```cpp
#include "Plugin/PluginProcessor.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

// Mede o custo do processBlock inteiro, em % do tempo real de cada bloco (100 % = um núcleo inteiro).
// Média = custo contínuo; p99 = blocos lentos. O máximo não é impresso: no Windows ele é dominado
// por preempção do sistema e muda muito de uma execução para outra.

namespace
{

void setParam (finale::FinaleProcessor& p, const char* id, float value)
{
    auto* param = p.parameters.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

struct Config
{
    const char* name;
    int character;
    bool compare;
    int assistant;
};

} // namespace

int main()
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    const Config configs[] { { "Clean", 0, false, 0 }, { "DOOM", 4, false, 0 }, { "Clean + A/B", 0, true, 0 },
        { "Clean + Auto", 0, false, 2 }, { "DOOM + A/B + Auto", 4, true, 2 } };
    constexpr double seconds = 10.0;
    constexpr int sourceBlocks = 64; // ruído pré-gerado: gerar fora do cronômetro

    std::printf ("taxa      bloco  configuração        média   p99   (%% do tempo real do bloco)\n");

    for (double sampleRate : { 44100.0, 48000.0, 96000.0 })
        for (int blockSize : { 64, 512 })
            for (const auto& config : configs)
            {
                finale::FinaleProcessor p;
                setParam (p, finale::ParamIDs::amount, 100.0f);
                setParam (p, finale::ParamIDs::character, static_cast<float> (config.character));
                setParam (p, finale::ParamIDs::compare, config.compare ? 1.0f : 0.0f);
                setParam (p, finale::ParamIDs::assistant, static_cast<float> (config.assistant));
                p.prepareToPlay (sampleRate, blockSize);

                std::mt19937 rng (1);
                std::normal_distribution<float> noise (0.0f, 0.2f);
                std::vector<float> source (static_cast<std::size_t> (2 * blockSize * sourceBlocks));
                for (auto& s : source)
                    s = noise (rng);

                juce::AudioBuffer<float> buffer (2, blockSize);
                juce::MidiBuffer midi;
                const int blocks = static_cast<int> (seconds * sampleRate / blockSize);
                std::vector<double> times;
                times.reserve (static_cast<std::size_t> (blocks));

                for (int b = 0; b < blocks; ++b)
                {
                    for (int ch = 0; ch < 2; ++ch)
                        std::copy_n (source.data() + ((b % sourceBlocks) * 2 + ch) * blockSize, blockSize,
                            buffer.getWritePointer (ch));

                    const auto start = std::chrono::steady_clock::now();
                    p.processBlock (buffer, midi);
                    const auto end = std::chrono::steady_clock::now();
                    times.push_back (std::chrono::duration<double> (end - start).count());
                }

                double total = 0.0;
                for (double t : times)
                    total += t;
                std::sort (times.begin(), times.end());

                const double blockSeconds = blockSize / sampleRate;
                const double mean = 100.0 * total / (blocks * blockSeconds);
                const double p99 = 100.0 * times[static_cast<std::size_t> (blocks * 0.99)] / blockSeconds;
                std::printf (
                    "%6.0f Hz  %4d   %-18s %5.2f %%  %5.1f %%\n", sampleRate, blockSize, config.name, mean, p99);
            }

    return 0;
}
```

- [ ] **Step 3: Minor do teto.** Em `tests/PluginProcessorTests.cpp`, logo depois do `TEST_CASE ("saída finita e abaixo do teto com qualquer bloco, caráter e A/B")` (antes do `TEST_CASE ("processBlock sem prepareToPlay não mexe no áudio")`), adicionar:

```cpp
TEST_CASE ("saída abaixo do teto em 44,1 e 96 kHz")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    for (double sampleRate : { 44100.0, 96000.0 })
    {
        INFO ("taxa " << sampleRate);
        FinaleProcessor p;
        setParam (p, finale::ParamIDs::gain, 12.0f);
        setParam (p, finale::ParamIDs::amount, 100.0f);
        setParam (p, finale::ParamIDs::character, 4.0f); // DOOM: o caso mais quente
        setParam (p, finale::ParamIDs::compare, 1.0f);
        p.prepareToPlay (sampleRate, 512);

        const auto result = run (p, 2, sampleRate, 0.5f, 2.0, { 512, 1, 37 });
        CHECK (result.finite);
        CHECK (result.maxTruePeakDb <= -1.0f + 0.1f);
    }
}
```

- [ ] **Step 4: README.** Em `README.md`, logo antes do parágrafo que começa com `Pra testar na DAW, copie`, adicionar:

````markdown
Para medir a CPU do plugin (cadeia inteira, por taxa, bloco e configuração), a partir da raiz do repo:

```
build\Release\finale_benchmark.exe
```

Os números de referência e a análise estão em `docs/decisoes.md` (Tecnologia).

````

- [ ] **Step 5: decisoes.md.** Em `docs/decisoes.md`, seção `## Tecnologia`, depois da linha do `**CLAUDE.md**`, adicionar:

```markdown
- **CPU (14/10/2026, `finale_benchmark`, i7-9750H, Release, amount 100):** cadeia inteira ≈ 3,7 % de um núcleo a 48 kHz e ≈ 7,3 % a 96 kHz, parecido em qualquer caráter, com A/B e com assistente (o A/B e o saturador rodam sempre). p99 por bloco: ≤ 7 % a 48 kHz e ≤ 12 % a 96 kHz. Metade do custo é o saturador (oversampling 2x com FIR de 63 taps, três convoluções por amostra), seguido do compressor. Decidido: não otimizar agora. Se precisar, o primeiro passo é o oversampler pular os taps nulos da meia banda e os zeros inseridos no upsample, o que corta cerca de 2/3 das multiplicações dele (de 189 para ~64 por amostra e canal).
```

- [ ] **Step 6:** Configurar, compilar tudo (`cmake -B build -G "Visual Studio 17 2022" -A x64`, `cmake --build build --config Release --parallel`), rodar `ctest --test-dir build -C Release --output-on-failure` e `build/Release/finale_benchmark.exe`. Esperado:
  - build sem warnings novos e `ctest` 2/2;
  - o benchmark imprime 30 linhas, com médias perto das do spike (≈ 3–5 % a 44,1/48 kHz e ≈ 6–9 % a 96 kHz, variando com a máquina).
  - Cole a saída do benchmark no relatório.
  - Se o teste novo do teto falhar, **não afrouxe**: pare com BLOCKED e mostre o valor medido.
- [ ] **Step 7:** `clang-format -i tools/benchmark/main.cpp tests/PluginProcessorTests.cpp` e commit: `"Tools: finale_benchmark (CPU do processor) e teto testado em 44,1/96 kHz"`.

---

## Fora deste plano

Otimizar o oversampler · benchmark no CI · medir a CPU dentro da DAW (o usuário pode ver no medidor do FL).
