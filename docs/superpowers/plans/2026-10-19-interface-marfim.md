# Interface Marfim no PluginEditor — Plano

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Trocar o editor provisório (um knob de ganho) pela interface escolhida, `docs/ui/prototipos/v9-marfim-final.html`, com a fonte **Sora**. A referência visual é a captura `docs/ui/prototipos/previas/v9-marfim-final-sora.png`.

**Architecture:**
- **Tema:** cores e fontes em `Source/Plugin/Theme.h`. As fontes vão embutidas por `juce_add_binary_data`: Sora 400/600/700/800 e IBM Plex Mono 400/500, todas OFL, já em `Source/Plugin/Fonts/`.
- **Peças reutilizáveis** em `Source/Plugin/Widgets.h`, sem `LookAndFeel`. Cada peça é um `juce::Component` que desenha a si mesmo e liga num parâmetro por `juce::ParameterAttachment`:
  - knob;
  - seletor de opções (chips ou segmentado);
  - mini-controle de arrastar.
- **Box de loudness:** `Source/Plugin/LoudnessPanel.h`. Lê os getters do processor; quem chama é o editor.
- **Editor:** monta tudo numa janela fixa de 820 × 500 e roda um `Timer` de 30 Hz, que atualiza o box de loudness e o status do assistente.
- **Processor:** ganha dois getters para a UI, o true peak da saída e a redução do limiter.

Issue #34. Branch `ui/34-interface-marfim`, que sai de `ui/32-prototipos` (PR #33) porque precisa do protótipo como referência.

**Tech Stack:** C++20, JUCE 9.0.3, Catch2 v3.16.0.

**Spec:**
- Issue #34.
- Protótipo `docs/ui/prototipos/v9-marfim-final.html`: valores do CSS, layout e comportamento.
- Escolha do usuário: fonte **Sora** e números em IBM Plex Mono.
- `CLAUDE.md`: audio thread, IDs permanentes e não escrever "Finale" à mão (usar `JucePlugin_Name`).

## Global Constraints

- **Não mexer em `Source/DSP/`.** No processor, só os dois getters da Task 1.
- **Audio thread:** nada novo além dos atomics da Task 1. A UI só lê atomics e parâmetros: nada de lock, e nada é chamado do editor para dentro do processamento.
- **Wordmark:** `juce::String (JucePlugin_Name).toUpperCase()`. Nunca escrever o nome à mão.
- **Cores**, exatamente as do protótipo, como `juce::Colour` (`0xff` + hex):
  - `bg F4F1EA`, `surface FFFFFF`, `surface2 ECE7DC`, `rule D9D2C4`, `line BDB5A4`;
  - `text 1C1B19`, `textDim 5E5A52`, `accent 1F7A72`, `warn B7791F`, `danger C53030`.
- **Fontes:**
  - `Theme::ui (float px, int weight)`: Sora, pesos 400, 600, 700 e 800. Para 500, usar o 600; para 900, o 800.
  - `Theme::mono (float px, int weight)`: Plex Mono 400 e 500.
  - O tamanho é em px como no CSS: usar `juce::FontOptions (typeface).withPointHeight (px)`. Se a sua versão do JUCE não tiver `withPointHeight`, use `withHeight (px * 1.25f)` e diga no relatório.
  - Os typefaces são carregados uma vez, em `static` local, com `juce::Typeface::createSystemTypefaceFor (BinaryData::…, BinaryData::…Size)`.
  - Espaçamento de letra com `Font::withExtraKerningFactor`: 0,18 no wordmark e 0,12 nos rótulos em caixa alta.
- **Textos da interface em português**, iguais aos do protótipo. O rótulo do knob continua "AMOUNT", como no protótipo.
- **O ✓ é desenhado como `juce::Path`** (duas linhas), nunca como caractere: a Sora "latin" não tem esse glifo.
- **Janela fixa de 820 × 500**, com `setResizable (false, false)` e o comentário `// shortcut: janela fixa 820×500; quando tiver redimensionamento, escalar com AffineTransform`.
- Identificadores em inglês; comentários e commits em português. Rodar `clang-format -i` (22.1.8) nos `.h`/`.cpp` alterados.
- Build sem warnings novos; `ctest` 2/2.

## Review Focus

1. Nada de trabalho de UI no audio thread, e nenhum dado compartilhado sem atomic.
2. Cada controle está ligado ao parâmetro certo, com gesto de host (begin/end) no arraste.
3. O visual bate com o protótipo: cores, tamanhos, posições, estados selecionados.
4. O editor abre e fecha sem vazamento: o `Timer` para no destrutor e não fica ponteiro pendurado.

---

### Task 1: Getters de true peak e redução do limiter

**Files:** Modify `Source/Plugin/PluginProcessor.h`, `Source/Plugin/PluginProcessor.cpp`, `tests/PluginProcessorTests.cpp`

- [ ] **Header:**
  - `#include "DSP/TruePeakDetector.h"`.
  - Getters públicos, depois de `getLoudnessTargetLufs`, com o comentário "Para a interface; o editor lê de outra thread":
    - `float getOutputTruePeakDb() const`: maior true peak da saída no último pedaço, em dBTP. O piso é −100.
    - `float getLimiterReductionDb() const`: redução de ganho do limiter no último pedaço, em dB, ≥ 0.
  - Membros: `dsp::TruePeakDetector outputPeak;`, `std::atomic<float> outputTruePeakDb { -100.0f };` e `std::atomic<float> limiterReductionDb { 0.0f };`.
- [ ] **`prepareToPlay`:** `outputPeak.prepare (channels);`, usando o mesmo `channels` que já existe lá.
- [ ] **`processBlock`:** no fim de cada pedaço, **depois** de `abCompare.process (...)`, que é o que se ouve, medir:
  ```cpp
  // True peak do que se ouve (depois do A/B), para a interface.
  float peak = 0.0f;
  for (int ch = 0; ch < channels; ++ch)
      for (int i = 0; i < n; ++i)
          peak = std::max (peak, outputPeak.push (ch, chunk[ch][i]));
  outputTruePeakDb.store (peak > 1.0e-5f ? 20.0f * std::log10 (peak) : -100.0f, std::memory_order_relaxed);
  limiterReductionDb.store (limiter.getGainReductionDb(), std::memory_order_relaxed);
  ```
  Antes, confira em `abCompare.process` se o sinal final fica em `chunk`. Se ficar em outro buffer, meça esse buffer e diga no relatório.
- [ ] **Teste** no fim de `tests/PluginProcessorTests.cpp`:
  ```cpp
  TEST_CASE ("interface: true peak e redução do limiter")
  {
      const juce::ScopedJuceInitialiser_GUI juceInit;
      FinaleProcessor p;
      setParam (p, finale::ParamIDs::gain, 12.0f);
      setParam (p, finale::ParamIDs::amount, 100.0f);
      p.prepareToPlay (48000.0, 512);
      run (p, 2, 48000.0, 0.5f, 2.0, { 512 });
      CHECK (p.getOutputTruePeakDb() <= -1.0f + 0.1f);
      CHECK (p.getOutputTruePeakDb() > -6.0f);
      CHECK (p.getLimiterReductionDb() > 0.5f);

      FinaleProcessor quiet;
      quiet.prepareToPlay (48000.0, 512);
      juce::AudioBuffer<float> silence (2, 512);
      silence.clear();
      juce::MidiBuffer midi;
      quiet.processBlock (silence, midi);
      CHECK (quiet.getOutputTruePeakDb() == -100.0f);
      CHECK (quiet.getLimiterReductionDb() == Approx (0.0f).margin (1.0e-3));
  }
  ```
- [ ] Build, `ctest`, clang-format e commit: `"Plugin: true peak da saída e redução do limiter para a interface"`.

### Task 2: Tema e fontes embutidas

**Files:** Create `Source/Plugin/Theme.h`, `Source/Plugin/Fonts/README.md`; Modify `CMakeLists.txt`

- [ ] **CMake:** dentro de `if(FINALE_BUILD_PLUGIN)`, antes de `juce_add_plugin`:
  ```cmake
  # Fontes da interface (SIL OFL, licenças em Source/Plugin/Fonts/).
  juce_add_binary_data(finale_fonts SOURCES
      Source/Plugin/Fonts/Sora-400.ttf
      Source/Plugin/Fonts/Sora-600.ttf
      Source/Plugin/Fonts/Sora-700.ttf
      Source/Plugin/Fonts/Sora-800.ttf
      Source/Plugin/Fonts/IBMPlexMono-400.ttf
      Source/Plugin/Fonts/IBMPlexMono-500.ttf)
  ```
  Acrescentar `finale_fonts` ao `PRIVATE` do `target_link_libraries(finale_plugin …)`. Os nomes no `BinaryData` ficam `Sora400_ttf`, `IBMPlexMono400_ttf` etc. Confira no `BinaryData.h` gerado e use os nomes reais.
- [ ] **`Source/Plugin/Fonts/README.md`:** uma linha por arquivo: origem (Fontsource, latin), licença (OFL, arquivos `OFL-*.txt` ao lado) e uso.
- [ ] **`Theme.h`** (header-only, `namespace finale::theme`):
  - as cores das Global Constraints como `inline const juce::Colour`;
  - `juce::Font ui (float px, int weight = 400)` e `juce::Font mono (float px, int weight = 400)`, conforme as Global Constraints;
  - `void card (juce::Graphics&, juce::Rectangle<float> r, bool accentStrip)`: o "box branco". É sombra `juce::DropShadow (Colour (0x1a1c1b19), 28, { 0, 10 })` mais `DropShadow (Colour (0x0f1c1b19), 6, { 0, 2 })`, preenchimento `surface` com raio 12, borda 1 px `surface2` e, com `accentStrip`, uma faixa de 5 px no topo em `accent`, recortada pelo mesmo retângulo arredondado (use `g.reduceClipRegion` com o path arredondado);
  - `void check (juce::Graphics&, juce::Rectangle<float> box, juce::Colour)`: o ✓ como path (de 15 %/55 % a 40 %/80 % e daí a 85 %/20 % do box), com traço 1,6 px e pontas arredondadas.
- [ ] Build e commit: `"Plugin: tema da interface e fontes Sora/Plex Mono embutidas"`.

### Task 3: Peças da interface (`Widgets.h`)

**Files:** Create `Source/Plugin/Widgets.h`

Todas as peças recebem um `juce::RangedAudioParameter&` e usam `juce::ParameterAttachment` (veja a doc do JUCE) para ler e escrever. A peça redesenha no callback do attachment. O arraste usa `beginGesture()`, `setValueAsPartOfGesture()` e `endGesture()`. O clique usa `setValueAsCompleteGesture()`.

- [ ] **`AmountKnob`** (o knob herói). Visual, com o diâmetro `d` = o menor lado dos bounds:
  - **Arco:** de −135° a +135°, com raio `0,44 d` e traço de 14 px com pontas arredondadas. O trilho é `rule` e o valor é `accent`.
  - **Marcas:** 11, por fora do arco, de 2 px, em `textDim` com 50 % de opacidade.
  - **Disco:** branco, de raio `0,33 d`, com `DropShadow (0x241c1b19, 4, {0, 2})`.
  - **Ponteiro:** traço de 4 px em `text`, do raio `0,25 d` ao `0,33 d`, no ângulo do valor.
  - **No centro:** o número em `mono (0,2 d, 500)`; "%" em `mono (0,08 d)` cor `textDim`, logo depois do número; e "AMOUNT" em `ui (10, 700)` com espaçamento 0,16 e cor `textDim`, abaixo.
  - **Interação:** arraste vertical, 0,5 % por pixel (0,1 % com Shift); duplo clique volta para 50 % (`setValueAsCompleteGesture (50)`); roda do mouse ±1 %. O valor é 0–100 (é o `amount`).
- [ ] **`ChoiceSelector`** (um seletor de opções), com dois estilos:
  - **`Style::chips`** (gênero e caráter):
    - As opções ficam lado a lado, com 4 px de espaço e largura proporcional ao texto mais 22 px de folga, preenchendo a largura.
    - Altura 28 px, raio 14 px, texto `ui (11, 500)` em `text`.
    - Não selecionado: fundo `bg` e borda 1 px `surface2`.
    - Selecionado: fundo `accent`, sombra `DropShadow (0x591f7a72, 8, {0, 2})`, texto branco `ui (11, 700)` e ✓ branco (`theme::check`) num quadrado de 8 px a 6 px da borda esquerda. O texto fica sempre centralizado no chip e não muda de posição ao selecionar.
  - **`Style::segmented`** (assistente e A/B):
    - Fundo `surface2` de altura 32 px e raio 8 px, com 2 px de borda interna.
    - Os botões têm 28 px de altura, raio 6 px e texto `ui (12, 500)` em `textDim`.
    - O selecionado tem fundo `surface`, texto `ui (12, 600)` em `text`, uma linha de 2 px em `accent` embaixo, por dentro, e uma sombra leve.
    - A largura de cada botão é o texto mais 20 px.
  - **Comum:**
    - Construtor `(juce::RangedAudioParameter&, juce::StringArray labels, Style)`. O índice é o valor do parâmetro arredondado, que pode ser choice ou bool (bool: 0 e 1).
    - Clique seleciona. Setas esquerda/direita mudam com o componente em foco (`setWantsKeyboardFocus (true)`).
    - `int getPreferredWidth() const` para o layout.
    - `std::function<void()> onChange`, chamado depois do callback do attachment.
- [ ] **`MiniControl`** (os controles do Avançado):
  - **Visual:**
    - Rótulo à esquerda em `ui (11)` cor `textDim`; valor à direita em `mono (11, 500)` cor `text`.
    - Abaixo, uma barra de 4 px com raio 2 px: trilho `surface2` e preenchimento `accent` até o valor normalizado.
  - **Construtor:** `(juce::RangedAudioParameter&, juce::String label, std::function<juce::String (float)> format, bool showBar = true)`.
  - **Interação:**
    - Arraste horizontal: a largura inteira do controle vai de 0 a 1 normalizado (0,2× com Shift), com gesto.
    - Duplo clique volta ao padrão do parâmetro.
    - Sem barra (`showBar = false`), o controle vira uma linha de 16 px: rótulo à esquerda e valor à direita, com arraste vertical de 0,005 normalizado por pixel.
- [ ] Os três ficam em `namespace finale`, com `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR`. Compilar: incluir o header num `.cpp` existente provisoriamente, ou deixar para a Task 5, e dizer no relatório. Commit: `"Plugin: peças da interface (knob, seletor, mini-controle)"`.

### Task 4: Box de loudness (`LoudnessPanel.h`)

**Files:** Create `Source/Plugin/LoudnessPanel.h`

- [ ] **`class LoudnessPanel : public juce::Component`.**
  - Método `void update (float integrated, float shortTerm, float truePeakDb, float grDb, float target, juce::String genreName, float ceilingDb)`. Ele guarda os valores, decide o estado da pílula e chama `repaint()`. Quem chama é o editor, no timer.
  - **Pílula, com histerese:** fica "no alvo" se `|integrado − alvo| ≤ 0,8`; deixa de ficar se passar de 1,2. Senão mostra "↑ abaixo do alvo" ou "↓ acima do alvo". Sem sinal (integrado ≤ −70) mostra "sem sinal", com o mesmo estilo neutro.
  - **True peak com hold:** o máximo dos últimos 1,5 s, calculado no `update` a partir do tempo do `Timer`, ou com `juce::Time::getMillisecondCounter()`.
- [ ] **`paint`** (largura do box 216 px), copiando o `.meter` do protótipo:
  - **Fundo:** `theme::card (g, bounds, true)`. Padding de 14 px em cima e embaixo e 20 px dos lados.
  - **Cabeçalho (22 px):** "LOUDNESS" em `ui (11, 600)` com espaçamento 0,12 e cor `textDim`. À direita, a pílula: altura 20 px, raio 10 px, padding de 8 px, texto `ui (11, 600)`. No alvo, fundo `accent`, texto branco e o ✓ desenhado com `theme::check` antes do texto. Fora, fundo `surface2` e texto `text`.
  - **Número integrado:** `mono (44, 500)` em `text`, com vírgula decimal e o sinal "−" (U+2212). Logo depois vem "LUFS" em `ui (13, 500)` cor `textDim`. Sem sinal, mostra "—".
  - **Curto prazo:**
    - Linha com "Curto prazo" em `ui (12)` `textDim` e o valor em `mono (13, 500)`.
    - Acima da barra, o rótulo "alvo −9,5 · Pop" em `ui (12, 600)`, centrado na posição do alvo e preso dentro do box.
    - Barra de 12 px com raio 6, trilho `surface2` e preenchimento `accent` até o short-term, na escala −20 … 0.
    - Por cima do preenchimento, a faixa de ±1 LU: `0x241c1b19` com bordas de 1 px em `0x731c1b19`.
    - A linha do alvo é de 2 px em `text`, passando 4 px além da barra em cima e embaixo.
    - Abaixo da barra, a régua "−20" e "0" em `mono (11)` `textDim`.
  - **True peak:** linha com "True peak" e o valor ("−1,2 dBTP"); barra fina de 6 px na escala −6 … 0, com o marcador do teto na posição do `ceilingDb`; e a nota "teto −1,0 dBTP" em `ui (11)` `textDim`.
  - **GR limiter:** linha com "GR limiter" e o valor ("−1,6 dB"); barra fina na escala 0 … 12 dB, em `warn`.
  - Formatação: uma casa decimal, vírgula e "−".
- [ ] Commit: `"Plugin: box de loudness da interface"`.

### Task 5: O editor

**Files:** Modify `Source/Plugin/PluginEditor.h`, `Source/Plugin/PluginEditor.cpp`, `tests/PluginProcessorTests.cpp`

- [ ] **`FinaleEditor : juce::AudioProcessorEditor, private juce::Timer`.** Os membros são as peças das Tasks 3 e 4, ligadas a estes parâmetros:
  - `AmountKnob` → `amount`;
  - `ChoiceSelector` de gênero → `genre` (chips), com o texto das opções do parâmetro;
  - `ChoiceSelector` de caráter → `character` (chips);
  - `ChoiceSelector` do assistente → `assistant` (segmentado), com os rótulos `Off`, `Analisar`, `Auto`, `Congelar`;
  - `ChoiceSelector` do A/B → `compare` (segmentado), com `A · Finale` e `B · Original` (0 = A);
  - `LoudnessPanel`;
  - `MiniControl` × 4 → `eq`, `compression`, `saturation` e `width`, com o formato "100 %";
  - `MiniControl` sem barra: "Ajuste LUFS" → `lufsOffset` ("+0,0 LU") e "Teto" → `ceiling` ("−1,0 dBTP");
  - `ChoiceSelector` segmentado de "Alvo manual" → `manualTarget`, com `Desligado` e `Ligado`;
  - o botão "Avançado", um `juce::Component` próprio ou um `juce::TextButton` sem borda, com o texto e o caret desenhados no `paint` do editor.
- [ ] **Layout (`resized`)**, igual ao protótipo, com a janela de 820 × 500:
  - **Topo (72 px, padding horizontal de 32 px):**
    - à esquerda, o wordmark `ui (22, 800)` com espaçamento 0,18, desenhado no `paint`;
    - à direita, dois blocos alinhados à direita: o assistente com o status embaixo (6 px de espaço) e o A/B com "volume igualado" embaixo, com 24 px entre os blocos;
    - o status é uma bolinha de 6 px (vazada em Off, `textDim` em Congelar, `accent` nos outros) mais o texto em `ui (12)`. O texto vem do modo: "Desligado", "Analisando · +x,x dB", "Ajustando · +x,x dB" ou "Congelado · +x,x dB", com o valor de `getAssistantCorrectionDb()`.
  - **Corpo:** padding de 12 px vertical e 32 px horizontal, com 24 px de espaço entre colunas.
    - **Direita:** o `LoudnessPanel`, de 216 px de largura e da altura do corpo.
    - **Esquerda:**
      - em cima, a área do knob: o `AmountKnob` com 200 px de diâmetro, centrado (142 px com o Avançado aberto);
      - embaixo, o **box PRESET**: `theme::card` sem faixa, padding de 10/16 px, altura 144 px;
      - dentro do box: cabeçalho de 18 px com "PRESET" (estilo kicker) e, à direita, o resumo "Pop · Clean" em `ui (13, 700)`; depois "GÊNERO" em `ui (11, 700)` com espaçamento 0,12 e cor `text`, os chips de gênero (28 px), "CARÁTER" e os chips de caráter;
      - o box é desenhado no `paint`, e os chips são filhos posicionados dentro dele.
  - **Avançado** (76 px, só quando aberto, logo acima do rodapé): fundo `surface` com borda superior de 1 px `rule` e padding de 12/32 px.
    - À esquerda, os 4 `MiniControl` lado a lado, com 24 px de espaço.
    - À direita, uma coluna de 220 px com borda esquerda de 1 px `rule` e padding de 24 px, com as linhas "Ajuste LUFS", "Teto" e "Alvo manual" (o seletor em tamanho pequeno, altura 20 px; pode ser o segmentado com altura reduzida).
  - **Rodapé (48 px, borda superior de 1 px `rule`):** à esquerda, "Avançado ▸" em `ui (13, 600)`, com o caret ▸ como triângulo desenhado, que vira ▾ quando aberto. À direita, "Arraste · Shift: fino · Duplo clique: 50 %" em `ui (12)` `textDim`.
- [ ] **`timerCallback` (30 Hz, `startTimerHz (30)` no construtor e `stopTimer()` no destrutor):**
  - lê os getters do processor e chama `loudness.update (...)`, com o nome do gênero tirado do parâmetro `genre` e o teto do parâmetro `ceiling`;
  - atualiza o texto do status do assistente e o resumo do preset;
  - chama `repaint` só nas áreas que mudaram.
- [ ] **`paint`:** fundo `bg`, wordmark, status, "volume igualado", box PRESET com os títulos e rodapé.
- [ ] **Teste:** no fim de `tests/PluginProcessorTests.cpp`, adicionar um teste que abre o editor e o desenha numa imagem. Ele confere o tamanho de 820 × 500 e a cor do canto (`bg`), e salva a imagem para a revisão visual:
  ```cpp
  TEST_CASE ("interface: editor abre e desenha")
  {
      const juce::ScopedJuceInitialiser_GUI juceInit;
      FinaleProcessor p;
      p.prepareToPlay (48000.0, 512);
      std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
      REQUIRE (editor != nullptr);
      CHECK (editor->getWidth() == 820);
      CHECK (editor->getHeight() == 500);
      const auto image = editor->createComponentSnapshot (editor->getLocalBounds());
      CHECK (image.getPixelAt (4, 4) == juce::Colour (0xffF4F1EA));
      // Captura para revisão visual (fica na pasta temporária).
      juce::File out = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("finale-editor.png");
      juce::FileOutputStream stream (out);
      if (stream.openedOk())
      {
          stream.setPosition (0);
          stream.truncate();
          juce::PNGImageFormat().writeImageToStream (image, stream);
      }
  }
  ```
- [ ] **Build e conferência:**
  - build sem warnings novos;
  - `ctest` 2/2;
  - `clang-format`;
  - abrir `%TEMP%\finale-editor.png` e comparar com `docs/ui/prototipos/previas/v9-marfim-final-sora.png`, anotando as diferenças no relatório.
- [ ] Commit: `"Plugin: interface Marfim no editor"`.

---

## Fora deste plano

Redimensionamento e escala · tema escuro · acessibilidade de leitor de tela (`AccessibilityHandler`) · animação do knob ao abrir o Avançado · tooltips.
