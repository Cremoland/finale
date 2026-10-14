# Alvo de LUFS por gênero — Plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** O alvo do assistente de loudness passa a vir do gênero, com um ajuste relativo (`lufsOffset`). Um interruptor `manualTarget` volta a usar o `targetLufs` absoluto. Projetos antigos abrem em modo manual e mantêm o alvo que tinham. Junto, a correção de loudness passa a zerar ao abrir um projeto sem ela numa instância reaproveitada (pendência do #21).

**Architecture:**
- **DSP.** Uma tabela `genreTargetLufsTable` e a função `genreTargetLufs (genre, offset)` em `Source/DSP/GenreCurves.h`. Não vão no `GenreCurveTable.h`, que é gerado pelo `finale_curvegen`.
- **Processor.** Escolhe o alvo a cada bloco: manual ou gênero + ajuste. Expõe o alvo atual num getter para a UI. Migra projetos antigos no `setStateInformation`.
- Um PR, Issue #29.

**Tech Stack:** C++20, Catch2 v3.16.0, JUCE 9.0.3 (só em `Plugin/`).

**Spec:**
- Issue #29 e o comentário de decisão nela.
- Valores: medianas de `docs/pesquisa/2026-10-14-parametros-por-genero.md` ([17]): House −9,1; EDM −8,7; Pop −9,5; Rock −9,2. Arredondadas para o passo de 0,5 do `targetLufs`.
- Gêneros sem dado em −9 (decisão do usuário).

## Global Constraints

- IDs existentes intactos. Parâmetros novos no fim do layout, com `ParameterID { id, 1 }`:
  - `lufsOffset`: float, −6 a +6, passo 0,5, padrão 0, label `LU`, nome `LUFS Offset`.
  - `manualTarget`: bool, padrão false, nome `Manual Target`.
- Alvo = `genreTargetLufs (genre, lufsOffset)`, limitado a [−16, −5] (a faixa do `targetLufs`), ou `targetLufs` quando `manualTarget` está ligado.
- **Com o assistente em Off (o padrão), o som não muda.** Instâncias novas com o assistente ligado no gênero padrão (Pop) passam de alvo −9 para −9,5. Projetos antigos mantêm o alvo. Mencionar no PR.
- Audio thread: nada de alocação, lock, I/O ou log.
- Identificadores em inglês; comentários, nomes de teste e commits em português. Rodar `clang-format -i` (22.1.8). Não mexer no editor.

## Review Focus

1. A migração: projeto sem `manualTarget` abre com ele ligado e o alvo fica idêntico ao `targetLufs` salvo. Projeto novo salvo com `manualTarget` desligado continua desligado.
2. Assistente em Off sem mudança de som. Nenhum ID existente mexido.
3. A pendência do #21: estado sem `assistantCorrectionDb` zera a correção mesmo numa instância já usada.

---

### Task 1: `genreTargetLufs`

**Files:**
- Modify: `Source/DSP/GenreCurves.h`
- Modify: `tests/GenreCurvesTests.cpp`

**Interfaces:**
- Produces: `inline constexpr float genreTargetLufsTable[8]` e `inline float genreTargetLufs (int genre, float offsetLu)` em `finale::dsp`.

- [ ] **Step 1: Teste que falha.** No fim de `tests/GenreCurvesTests.cpp`, adicionar:

```cpp
TEST_CASE ("alvo de LUFS por gênero")
{
    using finale::dsp::genreTargetLufs;
    // Trap, Rap/Boom bap, Lo-fi, Funk BR, House, EDM, Pop, Rock.
    const float expected[] { -9.0f, -9.0f, -9.0f, -9.0f, -9.0f, -8.5f, -9.5f, -9.0f };
    for (int g = 0; g < finale::dsp::numGenres; ++g)
    {
        INFO ("gênero " << g);
        CHECK (genreTargetLufs (g, 0.0f) == expected[g]);
    }

    CHECK (genreTargetLufs (6, -3.0f) == -12.5f); // ajuste soma ao alvo
    CHECK (genreTargetLufs (5, 6.0f) == -5.0f); // −2,5 limitado ao teto da faixa
    CHECK (genreTargetLufs (6, -10.0f) == -16.0f); // limitado ao piso da faixa
    CHECK (genreTargetLufs (6, std::numeric_limits<float>::quiet_NaN()) == -9.5f);
    CHECK (genreTargetLufs (-1, 0.0f) == -9.0f); // gênero inválido usa −9
    CHECK (genreTargetLufs (99, 1.0f) == -8.0f);
}
```

- [ ] **Step 2:** Compilar e rodar. Esperado: FALHA na compilação (`genreTargetLufs` não existe).
- [ ] **Step 3: Implementar.** Em `Source/DSP/GenreCurves.h`, depois da função `genreEqGains`, adicionar:

```cpp
// Alvo de loudness integrado por gênero (LUFS), na mesma ordem do parâmetro genre. Medianas medidas por terceiros
// (docs/pesquisa/2026-10-14-parametros-por-genero.md, [17]): House −9,1, EDM −8,7, Pop −9,5, Rock −9,2,
// arredondadas para o passo de 0,5 do targetLufs.
// shortcut: Trap, Rap/Boom bap, Lo-fi e Funk BR em −9 por falta de dado; medir referências e trocar.
inline constexpr float genreTargetLufsTable[] = { -9.0f, -9.0f, -9.0f, -9.0f, -9.0f, -8.5f, -9.5f, -9.0f };
static_assert (std::size (genreTargetLufsTable) == std::size (genreEqTable),
    "genreTargetLufsTable deve ter uma entrada por gênero");

// Alvo do gênero mais o ajuste, limitado a [−16, −5] (a faixa do targetLufs). Ajuste não-finito vira 0;
// gênero fora da tabela usa −9.
inline float genreTargetLufs (int genre, float offsetLu)
{
    const float base = (genre >= 0 && genre < numGenres) ? genreTargetLufsTable[genre] : -9.0f;
    const float offset = std::isfinite (offsetLu) ? offsetLu : 0.0f;
    return std::clamp (base + offset, -16.0f, -5.0f);
}
```

- [ ] **Step 4:** Rodar `ctest --test-dir build -C Release --output-on-failure`. Esperado: tudo passa.
- [ ] **Step 5:** `clang-format -i` e commit: `"DSP: alvo de LUFS por gênero"`.

---

### Task 2: Alvo no plugin, migração e pendência do #21

**Files:**
- Modify: `Source/Plugin/PluginProcessor.h`
- Modify: `Source/Plugin/PluginProcessor.cpp`
- Modify: `tests/PluginProcessorTests.cpp`
- Modify: `docs/decisoes.md`

**Interfaces:**
- Consumes: `dsp::genreTargetLufs` (Task 1).
- Produces (para a UI): `ParamIDs::lufsOffset = "lufsOffset"`, `ParamIDs::manualTarget = "manualTarget"`; getter `float getLoudnessTargetLufs() const`, que lê um `std::atomic<float>` (padrão −9) com o alvo em uso.

- [ ] **Step 1: Header.**
  - Em `ParamIDs`, depois de `eq`, adicionar `inline constexpr auto lufsOffset = "lufsOffset";` e `inline constexpr auto manualTarget = "manualTarget";`.
  - Getter público, depois de `getAssistantCorrectionDb`:
    ```cpp
    // Alvo de loudness em uso (gênero + ajuste, ou o manual), para a interface. O editor lê de outra thread.
    float getLoudnessTargetLufs() const { return loudnessTargetLufs.load (std::memory_order_relaxed); }
    ```
  - Membros privados, depois de `targetLufsParam`: `std::atomic<float>* lufsOffsetParam = nullptr;`, `std::atomic<float>* manualTargetParam = nullptr;` e `std::atomic<float> loudnessTargetLufs { -9.0f };`.
- [ ] **Step 2: Layout e construtor.**
  - No fim da lista do `createParameterLayout`, depois do parâmetro `eq`:
    ```cpp
    // Ajuste em cima do alvo do gênero. Com Manual Target ligado, vale o Target LUFS absoluto.
    std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamIDs::lufsOffset, 1 }, "LUFS Offset",
        juce::NormalisableRange<float> (-6.0f, 6.0f, 0.5f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("LU")),
    std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::manualTarget, 1 }, "Manual Target", false),
    ```
  - No construtor: `lufsOffsetParam = parameters.getRawParameterValue (ParamIDs::lufsOffset);` e `manualTargetParam = parameters.getRawParameterValue (ParamIDs::manualTarget);`.
- [ ] **Step 3: Alvo no `processBlock`.** Trocar a linha `assistant.setTargetLufs (targetLufsParam->load());` por:
    ```cpp
    // Alvo do gênero + ajuste; com o alvo manual ligado, vale o Target LUFS absoluto (como antes do #29).
    const float targetLufs = manualTargetParam->load() > 0.5f
                                 ? targetLufsParam->load()
                                 : dsp::genreTargetLufs (juce::roundToInt (genreParam->load()), lufsOffsetParam->load());
    assistant.setTargetLufs (targetLufs);
    loudnessTargetLufs.store (targetLufs, std::memory_order_relaxed);
    ```
- [ ] **Step 4: Estado.** Em `setStateInformation`:
  - (a) Trocar
    ```cpp
    if (state.hasProperty ("assistantCorrectionDb"))
        pendingCorrectionDb.store (...);
    ```
    por (mantendo a conversão atual dentro do `if`):
    ```cpp
    if (state.hasProperty ("assistantCorrectionDb"))
        pendingCorrectionDb.store (...); // a mesma expressão de hoje
    else
        pendingCorrectionDb.store (0.0f); // projeto sem correção salva: volta a 0, mesmo numa instância já usada
    ```
  - (b) Logo depois de `parameters.replaceState (state);`:
    ```cpp
    // Projetos de antes do #29 não têm o manualTarget: abrem com ele ligado e mantêm o alvo absoluto que tinham.
    if (! state.getChildWithProperty ("id", ParamIDs::manualTarget).isValid())
        if (auto* manual = parameters.getParameter (ParamIDs::manualTarget))
            manual->setValueNotifyingHost (1.0f);
    ```
- [ ] **Step 5: Testes.** Em `tests/PluginProcessorTests.cpp`:
  - (a) No `TEST_CASE ("parâmetros: IDs e padrões permanentes")`, acrescentar ao fim da lista `expected`: `{ finale::ParamIDs::lufsOffset, 0.0f }, { finale::ParamIDs::manualTarget, 0.0f }`.
  - (b) No `TEST_CASE ("estado: parâmetros salvos e restaurados")`, acrescentar `setParam (a, finale::ParamIDs::lufsOffset, -2.0f);` e `setParam (a, finale::ParamIDs::manualTarget, 1.0f);` aos `setParam`, e `finale::ParamIDs::lufsOffset, finale::ParamIDs::manualTarget` ao fim da lista de IDs conferidos.
  - (c) No `TEST_CASE ("assistente Auto puxa o loudness para o alvo")`: trocar o comentário `o alvo padrão é −9` por `o alvo padrão (Pop) é −9,5` e a conferência por `CHECK (p.getOutputShortTermLufs() == Approx (-9.5f).margin (1.5f));`.
  - (d) No fim do arquivo, adicionar:

```cpp
namespace
{

// Processa um bloco curto de silêncio e devolve o alvo de loudness em uso.
float targetAfterBlock (FinaleProcessor& p)
{
    juce::AudioBuffer<float> silence (2, 64);
    silence.clear();
    juce::MidiBuffer midi;
    p.processBlock (silence, midi);
    return p.getLoudnessTargetLufs();
}

// Estado salvo sem os filhos PARAM dos IDs dados nem as propriedades dadas: imita um projeto antigo.
juce::MemoryBlock withoutParts (const juce::MemoryBlock& state, std::initializer_list<const char*> paramIds,
    std::initializer_list<const char*> properties)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));
    REQUIRE (xml != nullptr);
    for (const char* id : paramIds)
        if (auto* child = xml->getChildByAttribute ("id", id))
            xml->removeChildElement (child, true);
    for (const char* property : properties)
        xml->removeAttribute (property);
    juce::MemoryBlock out;
    juce::AudioProcessor::copyXmlToBinary (*xml, out);
    return out;
}

} // namespace

TEST_CASE ("alvo de LUFS: gênero, ajuste e manual")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor p;
    p.prepareToPlay (48000.0, 512);

    CHECK (targetAfterBlock (p) == Approx (-9.5f).margin (1.0e-4)); // Pop, o padrão
    setParam (p, finale::ParamIDs::genre, 5.0f); // EDM
    CHECK (targetAfterBlock (p) == Approx (-8.5f).margin (1.0e-4));
    setParam (p, finale::ParamIDs::lufsOffset, -3.0f);
    CHECK (targetAfterBlock (p) == Approx (-11.5f).margin (1.0e-4));
    setParam (p, finale::ParamIDs::manualTarget, 1.0f);
    setParam (p, finale::ParamIDs::targetLufs, -14.0f);
    CHECK (targetAfterBlock (p) == Approx (-14.0f).margin (1.0e-4));
}

TEST_CASE ("estado: projeto antigo mantém o alvo absoluto")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor a;
    setParam (a, finale::ParamIDs::targetLufs, -12.0f);
    juce::MemoryBlock state;
    a.getStateInformation (state);

    // Projeto de antes do #29: sem lufsOffset nem manualTarget.
    const auto oldState = withoutParts (state, { finale::ParamIDs::lufsOffset, finale::ParamIDs::manualTarget }, { });
    FinaleProcessor b;
    b.setStateInformation (oldState.getData(), static_cast<int> (oldState.getSize()));
    CHECK (rawParam (b, finale::ParamIDs::manualTarget) == Approx (1.0f));
    b.prepareToPlay (48000.0, 512);
    CHECK (targetAfterBlock (b) == Approx (-12.0f).margin (1.0e-4));

    // Projeto novo, salvo com o alvo do gênero: continua no alvo do gênero.
    FinaleProcessor c;
    c.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    CHECK (rawParam (c, finale::ParamIDs::manualTarget) == Approx (0.0f));
    c.prepareToPlay (48000.0, 512);
    CHECK (targetAfterBlock (c) == Approx (-9.5f).margin (1.0e-4));
}

TEST_CASE ("estado: projeto sem correção de loudness zera a correção")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor a;
    setParam (a, finale::ParamIDs::assistant, 2.0f); // Auto
    a.prepareToPlay (48000.0, 512);
    run (a, 2, 48000.0, 0.05f, 30.0, { 512 });
    REQUIRE (a.getAssistantCorrectionDb() > 2.0f);

    // Congelar antes de salvar: o silêncio não move a correção, então um zero depois prova o reset.
    setParam (a, finale::ParamIDs::assistant, 3.0f);
    juce::MemoryBlock state;
    a.getStateInformation (state);
    const auto oldState = withoutParts (state, { }, { "assistantCorrectionDb" });

    // Mesma instância: a correção de antes não pode sobrar.
    a.setStateInformation (oldState.getData(), static_cast<int> (oldState.getSize()));
    targetAfterBlock (a);
    CHECK (a.getAssistantCorrectionDb() == 0.0f);
}
```

- [ ] **Step 6: decisoes.md.** Em `docs/decisoes.md`, trocar a linha que começa com `- **Loudness:** alvo por gênero (ex: Lo-fi −12` por:

```markdown
- **Loudness:**
  - O alvo vem do gênero mais um ajuste (`lufsOffset`, ±6 LU). Também dá para usar um alvo manual absoluto (`manualTarget` + `targetLufs`). Teto −1 dBTP.
  - Alvos (decidido 14/10, a partir das medianas medidas por terceiros em `docs/pesquisa/2026-10-14-parametros-por-genero.md`): EDM −8,5, Pop −9,5, House e Rock −9. Trap, Rap/Boom bap, Lo-fi e Funk BR ficam em −9 até medir referências.
  - Projetos de antes disso abrem em alvo manual e mantêm o alvo que tinham.
```

- [ ] **Step 7:** Compilar tudo, rodar o `ctest` e o `clang-format -i`. Esperado: build sem warnings novos e os 2 testes passando. Se um valor esperado falhar, **não afrouxe**: pare com BLOCKED e mostre o valor medido.
- [ ] **Step 8:** Commit: `"Plugin: alvo de LUFS por gênero, alvo manual e migração de projetos antigos"`.

---

## Fora deste plano

UI do ajuste e do alvo manual (é do amigo; os parâmetros e o getter já existem) · alvos medidos por gênero com o `finale_curvegen` · alerta de loudness acima do alvo de streaming.
