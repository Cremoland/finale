# Ferramenta de curvas de referência por gênero — Plano de implementação

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Um programa de linha de comando (`finale_curvegen`) que lê os áudios de `references/<gênero>/`, mede o equilíbrio tonal nas mesmas 4 bandas do EQ tonal e regenera a tabela de curvas do plugin. Sai um PR pequeno com os números novos e nada mais muda no plugin.

**Architecture:** Três peças:
- **(1) `BandAnalyzer`** (`Source/DSP/`, C++ puro e testável): soma em mono, mede a energia em bandas de uma oitava centradas em 90 Hz, 300 Hz, 3 kHz e 10 kHz (passa-alta e passa-baixa Butterworth do `Biquad.h`) e devolve a *forma*, que é o nível de cada banda menos a média das 4. Assim o volume do arquivo não pesa.
- **(2) `CurveGen.h`** (`tools/curvegen/`, C++ puro e testável): calcula a tabela nova e escreve o arquivo `GenreCurveTable.h`.
- **(3) `main.cpp`** (`tools/curvegen/`, console app do JUCE): só lê os arquivos com o `AudioFormatManager` e liga as peças.

A tabela sai do `GenreCurves.h` e vai para um `GenreCurveTable.h` gerado, que o `GenreCurves.h` inclui. Um PR, Issue #22.

**Tech Stack:** C++20, Catch2 v3.16.0, JUCE 9.0.3 (só no `main.cpp` da ferramenta e no plugin).

**Spec:** `docs/decisoes.md` ("Curvas de referência: ferramenta no repo analisa WAVs de referência e gera … por gênero. WAVs ficam só na máquina de cada um, nunca no repo") + `CLAUDE.md` (`tools/` = ferramentas offline; nunca commitar áudio).

## Global Constraints

- `Source/DSP/` e `tools/curvegen/CurveGen.h` usam só a biblioteca padrão; nada de JUCE neles. JUCE só em `tools/curvegen/main.cpp`.
- A ferramenta é offline: pode alocar à vontade, mas não pode tocar no audio thread nem em nada do plugin além do arquivo da tabela.
- O áudio de referência nunca entra no repo. `references/` já está no `.gitignore`.
- Regra da curva, que é decisão do projeto e vale para o plano todo: os dados dizem **como os gêneros diferem entre si**, e o ouvido define o viés comum. Para cada gênero com referências, `novo[g][b] = mediaDaTabelaAtual[b] + clamp (forma[g][b] − mediaDasFormas[b], −6, +6)`. Gêneros sem referência mantêm a linha atual. Se menos de 2 gêneros tiverem referências, a ferramenta não muda nada e explica o motivo, porque com um gênero só não há diferença para medir.
- Saída em C++ (`Source/DSP/GenreCurveTable.h`) em vez de JSON, já que o plugin não tem leitor de JSON. Atualizar a linha do `docs/decisoes.md`.
- Pastas por gênero (sem barra no nome): `Trap`, `Rap-Boom bap`, `Lo-fi`, `Funk BR`, `House`, `EDM`, `Pop`, `Rock`. Formatos lidos: WAV, AIFF e FLAC (`registerBasicFormats`). MP3 fica de fora, e a ferramenta avisa quando o encontra.
- Identificadores em inglês; comentários, mensagens da ferramenta e commits em português. `clang-format -i` (22.1.8).

## Review Focus

1. Ruído rosa (energia igual por oitava) → forma plana. Teste na Task 1.
2. Volume do arquivo não muda a forma. Teste na Task 1.
3. Menos de 2 gêneros com referência → tabela intacta. Teste na Task 2.
4. Regenerar sem mudanças gera um `GenreCurveTable.h` idêntico ao commitado. Teste na Task 2.
5. Pasta vazia, arquivo corrompido ou MP3 → a ferramenta avisa e segue, sem travar. Verificação manual na Task 3.

---

### Task 1: `BandAnalyzer`

**Files:**
- Create: `Source/DSP/BandAnalyzer.h`
- Create: `tests/BandAnalyzerTests.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `makeHighPass`, `makeLowPass`, `BiquadState` (`Biquad.h`).
- Produces:
  - `class finale::dsp::BandAnalyzer`
  - `static constexpr std::array<double, 4> centersHz { 90.0, 300.0, 3000.0, 10000.0 };` (as bandas do `TonalEq`)
  - `void prepare (double sampleRate)`: zera as somas.
  - `void process (const float* mono, int numSamples)`: acumula a energia de cada banda.
  - `std::array<float, 4> getShapeDb() const`: nível de cada banda em dB (10·log10 da média do quadrado) menos a média dos 4 níveis. Se ainda não houver energia, devolve {0, 0, 0, 0}.

Cada banda é uma oitava, de f/√2 a f·√2: dois passa-altas Butterworth (Q = 1/√2) em f/√2 e dois passa-baixas em f·√2, em cascata (≈ 24 dB/oitava de cada lado). Se f·√2 passar de 0.45·fs, a borda alta é limitada a 0.45·fs. Estado e somas em double. Entrada não-finita vira 0.

- [ ] **Step 1: Testes que falham**, em `tests/BandAnalyzerTests.cpp`. Os sinais de teste são somas de senos com fase aleatória fixa (`std::mt19937 { 7 }`), 10 s a 48 kHz, analisando só a partir de 1 s, para o transitório dos filtros não entrar:
  - `"rosa dá forma plana"`: 40 senos por oitava, com amplitudes iguais e frequências em escala log de 20 Hz a 20 kHz (energia igual por oitava) → cada banda da forma = `Approx (0.0).margin (0.75)` dB.
  - `"branco sobe com a frequência"`: 2000 senos de amplitudes iguais, espaçados linearmente de 20 Hz a 20 kHz → forma[3] − forma[0] = `Approx (20.5).margin (1.5)` dB (10·log10 (10000/90), porque a energia é proporcional à largura da banda).
  - `"volume não muda a forma"`: o sinal rosa multiplicado por 0.01 → mesma forma do rosa original (`margin (1e-3)`).
  - `"seno cai na banda certa"`: seno puro de 300 Hz → forma[1] é a maior das quatro, por mais de 20 dB acima das outras.
  - `"analisador: sem sinal"`: sem chamar `process`, ou só com zeros → {0, 0, 0, 0}.
  - `"analisador: entrada não-finita"`: rosa com um NaN e um +inf no meio → forma finita e igual à do rosa (`margin (0.1)`).
- [ ] **Step 2:** Rodar os testes. Esperado: FALHA.
- [ ] **Step 3:** Implementar `Source/DSP/BandAnalyzer.h`.
- [ ] **Step 4:** Rodar os testes. Esperado: PASSAM.
- [ ] **Step 5:** `clang-format -i` e commit: `"DSP: analisador de equilíbrio tonal por bandas"`.

---

### Task 2: Tabela gerada + `CurveGen.h`

**Files:**
- Create: `Source/DSP/GenreCurveTable.h` (a tabela sai do `GenreCurves.h` e vem para cá, com os mesmos valores)
- Modify: `Source/DSP/GenreCurves.h` (inclui o `GenreCurveTable.h`; a função e o `numGenres` ficam)
- Create: `tools/curvegen/CurveGen.h`
- Create: `tests/CurveGenTests.cpp`
- Modify: `CMakeLists.txt` (o teste e o include de `tools/` no alvo de testes)

**Interfaces:**
- Produces, em `tools/curvegen/CurveGen.h`, namespace `finale::tools`:
  ```cpp
  using CurveTable = std::array<std::array<float, 4>, 8>;
  struct CurveResult { CurveTable table; bool changed; std::string message; };
  CurveResult computeCurves (const CurveTable& current, const std::array<std::optional<std::array<float, 4>>, 8>& measuredShapes);
  std::string writeCurveTableHeader (const CurveTable& table);
  inline constexpr std::array<const char*, 8> genreFolders { "Trap", "Rap-Boom bap", "Lo-fi", "Funk BR", "House", "EDM", "Pop", "Rock" };
  ```
- `GenreCurveTable.h` passa a conter só a tabela `genreEqTable[8][4]` (mesmos valores e comentários de linha). No topo, o comentário `// Gerado por tools/curvegen (finale_curvegen); edite à mão só para ajuste por ouvido.`

Regras do `computeCurves`, iguais às da seção Global Constraints: com 2 ou mais gêneros medidos, `novo[g][b] = mediaAtual[b] + clamp (forma[g][b] − mediaDasFormas[b], −6, 6)` para os medidos, e os outros ficam iguais. Os valores são arredondados para 0.1 dB. A mensagem lista quais gêneros mudaram. Com menos de 2, `changed = false`, a tabela volta igual e a mensagem explica. O `writeCurveTableHeader` gera o texto exato do `GenreCurveTable.h`: mesmo cabeçalho, uma linha por gênero, números com uma casa decimal e sufixo `f` (por exemplo `2.5f` e `-1.0f`) e o nome do gênero em comentário.

- [ ] **Step 1: Testes que falham**, em `tests/CurveGenTests.cpp`:
  - `"regenerar sem mudança é idêntico"`: `writeCurveTableHeader` aplicado à `genreEqTable` atual == o conteúdo do arquivo `Source/DSP/GenreCurveTable.h` (o teste lê o arquivo pelo caminho absoluto que o CMake passa numa definição, `FINALE_SOURCE_DIR`).
  - `"um gênero só não muda nada"`: só Trap medido → `changed == false` e a tabela == a atual.
  - `"diferença entre gêneros vira curva"`: tabela atual toda 0, Trap medido {+2, 0, 0, −2} e Pop medido {−2, 0, 0, +2} → Trap = {2, 0, 0, −2} e Pop = {−2, 0, 0, 2}; os outros gêneros iguais (0).
  - `"viés comum vem da tabela atual"`: tabela atual com a coluna 0 toda +1 e o resto 0; mesmas medidas do teste anterior → Trap[0] == 3 e Pop[0] == −1.
  - `"diferença limitada a 6 dB"`: Trap {+20, 0, 0, 0} e Pop {−20, 0, 0, 0}, com a tabela atual 0 → Trap[0] == 6 e Pop[0] == −6.
  - `"arredonda para 0.1 dB"`: Trap {0.123, …} e Pop {−0.123, …} → Trap[0] == 0.1f.
- [ ] **Step 2:** Rodar os testes. Esperado: FALHA.
- [ ] **Step 3:** Mover a tabela para o `GenreCurveTable.h` exatamente no formato que o `writeCurveTableHeader` gera, e implementar o `CurveGen.h`. Os testes existentes de `GenreCurves` têm que continuar passando sem mudança; troque o `g <= 7` deles por `g < finale::dsp::numGenres`, que ficou pendente da #6.
- [ ] **Step 4:** Rodar os testes. Esperado: PASSAM.
- [ ] **Step 5:** `clang-format -i` (o `GenreCurveTable.h` tem que já sair formatado do gerador; se o clang-format mudar algo, ajuste o gerador) e commit: `"Tools: cálculo das curvas e tabela gerada separada"`.

---

### Task 3: `finale_curvegen` (console app)

**Files:**
- Create: `tools/curvegen/main.cpp`
- Modify: `CMakeLists.txt` (`juce_add_console_app (finale_curvegen …)` dentro do `if (FINALE_BUILD_PLUGIN)`, ligando `juce::juce_audio_formats`, `juce::juce_core` e `finale_dsp`)
- Modify: `docs/decisoes.md` (a linha das curvas de referência: gera `Source/DSP/GenreCurveTable.h`; pastas com os nomes de `genreFolders`; formatos WAV/AIFF/FLAC)
- Modify: `README.md` (uma seção curta "Gerar as curvas de referência" com o comando)

**Interfaces:**
- Uso: `finale_curvegen [pastaDeReferencias] [arquivoDeSaida]`, com padrões `references` e `Source/DSP/GenreCurveTable.h`, relativos ao diretório atual.

Comportamento: para cada gênero de `genreFolders`, procura arquivos `.wav`, `.aif`, `.aiff` e `.flac` em `<pasta>/<gênero>/`. Cada arquivo é lido inteiro com o `AudioFormatManager`, somado em mono e passado por um `BandAnalyzer` próprio. A forma do gênero é a média, em dB, das formas dos arquivos. Arquivo ilegível ou `.mp3` → aviso e o arquivo é pulado. No fim, chama `computeCurves` com a tabela atual (`genreEqTable`), imprime a mensagem e uma tabela legível (gênero, número de arquivos, os 4 valores novos) e, se `changed`, grava o arquivo de saída com `writeCurveTableHeader`. Código de saída 0 em sucesso (mudando ou não) e 1 em erro de argumento ou pasta inexistente.

- [ ] **Step 1:** Implementar o `main.cpp` e o alvo no CMake.
- [ ] **Step 2:** Compilar tudo (`cmake --build build --config Release --parallel`). Esperado: o plugin, os testes e o `finale_curvegen` compilam.
- [ ] **Step 3:** Verificação manual, sem áudio comercial: gere numa pasta temporária do scratchpad (nunca no repo) dois WAVs de 10 s com um script Python (`wave` + `math`), um "escuro" em `Trap/` (rosa com os agudos atenuados) e um "claro" em `Pop/` (rosa com os agudos reforçados). Também crie um `.mp3` falso de texto e uma pasta de gênero vazia. Rode `finale_curvegen <pastaTemp> <arquivoTemp>.h` → a saída mostra Trap com o 10 kHz abaixo do Pop, avisa sobre o `.mp3` e grava o `.h` temporário. Rode também com só uma pasta preenchida → "não mudou" e nada é gravado. Registre as saídas no relatório. Não grave por cima do `Source/DSP/GenreCurveTable.h` real.
- [ ] **Step 4:** `ctest`, `clang-format -i` e as edições do `decisoes.md` e do `README.md`.
- [ ] **Step 5:** Commit: `"Tools: finale_curvegen gera as curvas a partir das referências"`.

---

## Fora deste plano

Leitura de MP3 · subgêneros · ponderar arquivos por duração · curvas diferentes por caráter · rodar a ferramenta no CI (as referências só existem na máquina do usuário) · assistente tonal (#20).
