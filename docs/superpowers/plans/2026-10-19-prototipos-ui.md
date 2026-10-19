# Protótipos de interface — Plano

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Cinco protótipos de tela do Finale em HTML estático, cada um numa direção visual diferente, mais uma galeria, para o usuário escolher qual vira o `PluginEditor`.

**Architecture:**
- Um arquivo HTML autocontido por versão, em `docs/ui/prototipos/`, sem build e sem dependência além do Google Fonts.
- Todas as versões mostram **os mesmos controles e os mesmos valores de exemplo**, só muda a forma. Assim a comparação é justa.
- A interação é mínima e de verdade: o knob gira arrastando, os seletores clicam e os medidores se mexem sozinhos.
- Issue #32. Não mexe no plugin.

**Spec:**
- Issue #32.
- `docs/pesquisa/2026-10-19-interfaces-modernas.md`: as seções "Cinco direções para os mockups" e "Lacunas".
- `docs/decisoes.md`, seção Interface.

## Global Constraints (valem para todas as versões)

- **Arquivo:** um `.html` autocontido (CSS e JS inline). Externo, só `<link>` do Google Fonts. Nenhuma imagem externa, nenhuma biblioteca.
- **Janela do plugin:** um elemento de **820 × 500 px** fixos, centralizado numa página de fundo neutro escuro (`#1e1e1e`, imitando a DAW). Abaixo da janela, uma legenda pequena com o nome da versão. O que importa é o que está dentro dos 820 × 500.
- **Textos da interface em português.** O nome do produto aparece como `FINALE` (wordmark), e a marca do estúdio não aparece.
- **Controles obrigatórios, com estes valores iniciais de exemplo:**
  1. **Knob principal "Amount"**, 0–100 %, começa em **62 %**. É o maior elemento da tela, exceto na versão 2. O número aparece no centro ou logo abaixo.
  2. **Gênero**, 8 opções: Trap, Rap/Boom bap, Lo-fi, Funk BR, House, EDM, Pop, Rock. **Pop** selecionado.
  3. **Caráter**, 5 opções: Clean, Warm, Punch, Wide, DOOM. **Clean** selecionado.
  4. **Assistente**, 4 estados: Off, Analisar, Auto, Congelar. **Auto** selecionado, com o texto de status "Ajustando · +2,4 dB".
  5. **A/B**: alternância "A · Finale" / "B · Original", com o subtítulo "volume igualado".
  6. **Medidor de loudness**: integrado **−9,6 LUFS**, com o **alvo −9,5 (Pop)** visível como linha ou faixa; short-term numa barra; **true peak −1,2 dBTP** com o teto **−1,0 dBTP**; e a redução de ganho (GR) do limiter.
  7. **Painel "Avançado"**, fechado por padrão, aberto num clique. Mostra 4 mini-controles: EQ 100 %, Compressão 100 %, Saturação 100 %, Largura 100 %. Mostra também o ajuste de LUFS "+0,0 LU" e o "Alvo manual: desligado".
- **Interação (JS puro, no máximo ~200 linhas por arquivo):**
  - Arrastar o knob na vertical muda o valor (com Shift, ajuste fino), e o duplo clique volta a 50 %. O arco e o número acompanham.
  - Os seletores (gênero, caráter, assistente, A/B) clicam e mudam o selecionado.
  - Os medidores animam com um passeio aleatório suave em volta dos valores de exemplo, a ~30 fps (`requestAnimationFrame` com limite).
  - O painel Avançado abre e fecha.
- **Legibilidade:**
  - Texto pequeno (≤ 12 px) com contraste ≥ 4,5:1 contra o fundo onde está.
  - Nenhum estado comunicado só por cor: o selecionado também tem forma, peso ou marcador, e o alerta tem texto ou ícone.
  - Números de dB e LUFS em fonte mono ou tabular (`font-variant-numeric: tabular-nums`).
- **Implementável em JUCE:** só formas vetoriais (arcos, retângulos arredondados, gradientes lineares ou radiais, sombras simples). Nada de blur pesado de fundo (`backdrop-filter`), vídeo ou 3D. A versão precisa poder ser desenhada com `juce::Path` e `ColourGradient` depois.
- **Paleta definida em variáveis CSS** no `:root`, com os nomes `--bg`, `--surface`, `--surface-2`, `--text`, `--text-dim`, `--accent`, `--accent-2`, `--ok`, `--warn`, `--danger`. Isso facilita portar para o JUCE.
- Comentário no topo de cada arquivo: a direção, a paleta e a fonte.

## Review Focus

1. Todos os 7 controles obrigatórios estão presentes, com os valores de exemplo.
2. A versão segue a direção dela e é visualmente diferente das outras.
3. Contraste e "não só cor".
4. Nada que o JUCE não consiga desenhar.

---

### Task 1: Versão 1 — "Noir" (knob primeiro)

**Files:** Create `docs/ui/prototipos/v1-noir.html`

**Direção:** a mais próxima do Soundgoodizer, mas premium. A tela é quase só o knob.
- **Layout:**
  - **Topo:** wordmark `FINALE` à esquerda, em letras espaçadas. À direita, o assistente como controle segmentado de 4 partes, com o status logo abaixo, e o A/B ao lado.
  - **Centro:** knob enorme (~260 px), com arco de valor grosso e brilho suave (glow) na cor do acento. O número "62" fica grande no centro, com "%" pequeno.
  - **Abaixo do knob:** duas fileiras de "chips" arredondados, gênero e caráter.
  - **Direita:** coluna estreita com o medidor de loudness vertical. A barra short-term tem a faixa do alvo marcada, e abaixo dela ficam o número integrado grande, o true peak e o GR.
  - **Rodapé:** linha "Avançado ▸", que expande uma faixa por cima do rodapé.
- **Paleta:** `--bg #0E1116`, `--surface #171C24`, `--surface-2 #1F2630`, `--text #E6EAF0`, `--text-dim #8A94A6`, `--accent #4FD1C5`, `--accent-2 #7C5CFF`, `--ok #4FD1C5`, `--warn #F5B84B`, `--danger #FF5C6C`.
- **Fonte:** IBM Plex Sans (400/500/600) e IBM Plex Mono para os números.
- [ ] Criar o arquivo seguindo as Global Constraints e esta direção. Abrir no navegador e conferir os 7 controles e a interação.

### Task 2: Versão 2 — "Estúdio" (medidor primeiro)

**Files:** Create `docs/ui/prototipos/v2-estudio.html`

**Direção:** o painel de medição é o herói, no estilo FabFilter Pro-L 2. É para quem quer ver números.
- **Layout:**
  - **Esquerda (~200 px):** lista vertical de gêneros, com o selecionado marcado por barra lateral e negrito. Abaixo, o caráter como lista curta.
  - **Centro:** um **gráfico de loudness ao longo do tempo**, uma linha rolando da direita para a esquerda (short-term) e outra fina do momentâneo. A faixa do alvo (−9,5 ± 1 LU) fica sombreada. Embaixo do gráfico, três números grandes lado a lado: Integrado, Short-term e True peak.
  - **Direita (~220 px):** knob Amount médio (~150 px), com o assistente e o A/B abaixo dele.
  - **Barra do GR** horizontal, fina, no topo do gráfico.
  - **Avançado:** abre como gaveta na parte de baixo.
- **Paleta:**
  - Base: `--bg #0B1220`, `--surface #111A2E`, `--surface-2 #18233B`, `--text #E8EEF8`, `--text-dim #8FA0BD`.
  - Acentos: `--accent #5AA9FF`, `--accent-2 #9CC4FF`.
  - Estados: `--ok #3DDC97`, `--warn #FFC857`, `--danger #FF5A5F`.
  - Cores de medidor: entrada `--text-dim`, saída `--accent`, GR `--danger`.
- **Fonte:** Inter (400/600) e JetBrains Mono para os números.
- [ ] Criar o arquivo seguindo as Global Constraints e esta direção. O gráfico é desenhado num `<canvas>` ou SVG atualizado pelo JS.

### Task 3: Versão 3 — "Papel" (tema claro editorial)

**Files:** Create `docs/ui/prototipos/v3-papel.html`

**Direção:** clara, calma e editorial, como uma página de revista de design. É a mais diferente das escuras.
- **Layout:**
  - Grade de 12 colunas com margens generosas.
  - **Topo:** wordmark `Finale` numa serifada de display, grande, à esquerda. O assistente fica à direita, como texto sublinhado ("Off  Analisar  **Auto**  Congelar").
  - **Centro-esquerda:** knob grande e chato, sem glow: um disco branco com sombra curta, arco fino de 6 px no acento e um ponteiro preto.
  - **Centro-direita:** gênero e caráter como **listas tipográficas**, com o selecionado em negrito e um ponto antes.
  - **Medidor:** barra horizontal larga abaixo, com régua de −20 a 0 LUFS, a marca do alvo como triângulo e o número integrado grande à direita.
  - **A/B:** um interruptor pequeno.
  - **Avançado:** link "Avançado →" que abre uma faixa.
- **Paleta:**
  - Base: `--bg #F4F1EA`, `--surface #FFFFFF`, `--surface-2 #ECE7DC`, `--text #1C1B19`, `--text-dim #5E5A52` (conferir 4,5:1 sobre `--bg`).
  - Acentos: `--accent #2F5BEA`, `--accent-2 #E4572E`.
  - Estados: `--ok #1F8A5B`, `--warn #B7791F`, `--danger #C53030`.
- **Fonte:** "Fraunces" (wordmark e títulos) e "IBM Plex Sans" e "IBM Plex Mono" para o resto.
- [ ] Criar o arquivo seguindo as Global Constraints e esta direção.

### Task 4: Versão 4 — "Pulso" (assistente primeiro)

**Files:** Create `docs/ui/prototipos/v4-pulso.html`

**Direção:** o assistente é o centro da história. A tela mostra o que ele está fazendo, para o usuário confiar antes de exportar.
- **Layout:**
  - **Faixa do topo, de largura total: a "linha do tempo do assistente".** São 4 estados como etapas conectadas (Off → Analisar → Auto → Congelar). O ativo pulsa, com um anel que respira em CSS animation, e mostra "Ajustando · +2,4 dB · 00:42".
  - **Centro: equilíbrio tonal em 4 barras verticais** (Grave 90 Hz, Médio-grave 300 Hz, Médio-agudo 3 kHz, Agudo 10 kHz). Cada barra tem a medida da música e um marcador da "forma neutra" (alvo). A correção de cada banda aparece em número (ex.: +1,8 / −0,6 / +0,4 / +2,1 dB). Os valores de exemplo animam devagar.
  - **Direita:** knob Amount (~180 px), monocromático, com arco no acento. Abaixo dele, gênero (dropdown com a seta) e caráter (5 ícones/letras C W P Wi D, com o nome por extenso no tooltip e abaixo do selecionado).
  - **Rodapé:** loudness compacto numa linha (integrado vs alvo, TP, GR), com o A/B à direita e o "Avançado".
- **Paleta:** `--bg #0C0C0D`, `--surface #151517`, `--surface-2 #1E1E21`, `--text #F2F2F3`, `--text-dim #9A9AA2`, `--accent #FF5A1F`, `--accent-2 #FFB199`, `--ok #2BD576`, `--warn #FFC542`, `--danger #FF3B3B`.
- **Fonte:** "Space Grotesk" (títulos e rótulos) e "JetBrains Mono" para os números.
- [ ] Criar o arquivo seguindo as Global Constraints e esta direção.

### Task 5: Versão 5 — "Latão" (analógico quente, sem foto)

**Files:** Create `docs/ui/prototipos/v5-latao.html`

**Direção:** é um rack analógico desenhado em vetor, quente e tátil, sem textura de foto. Combina com os caracteres Warm e DOOM.
- **Layout:**
  - **Painel frontal** com dois "parafusos" vetoriais nos cantos superiores e uma linha gravada separando as seções.
  - **Esquerda:** VU de ponteiro semicircular (agulha animada) mostrando o short-term, com a zona do alvo em latão. Abaixo dele, os números integrado e TP em "visor" escuro, com dígitos em âmbar.
  - **Centro:** knob grande de latão, feito com gradiente radial e marcas de 0 a 100 gravadas em volta, mais um anel de LEDs âmbar (24 pontos) que acende até o valor.
  - **Direita:** chave rotativa de 5 posições para o caráter (Clean … DOOM, com rótulos em volta). O gênero fica num "visor" com setas ◀ ▶.
  - **Topo:** assistente como 4 botões de "teclado" com LED, e o A/B como chave de alavanca.
  - **Avançado:** abre como uma "tampa" que desliza.
- **Paleta:** `--bg #1B1714`, `--surface #2A2320`, `--surface-2 #3A302A`, `--text #EDE3D1`, `--text-dim #B3A58E`, `--accent #D8A657`, `--accent-2 #F0C987`, `--ok #9BC26B`, `--warn #F2A541`, `--danger #E4572E`.
- **Fonte:** "Barlow Condensed" (rótulos em caixa alta, espaçados) e "IBM Plex Mono" (visores).
- [ ] Criar o arquivo seguindo as Global Constraints e esta direção.

### Task 6: Galeria

**Files:** Create `docs/ui/prototipos/index.html`

- [ ] Uma página escura com as 5 versões em `<iframe>`, cada uma de 820 × 500, em escala 0,6 (CSS `transform: scale(.6)` com o contêiner no tamanho escalado), em grade de 2 colunas. Acima de cada uma vão o número, o nome e uma linha com a ideia (copiada das direções acima). Cada cartão tem um link "abrir em tamanho real" para o arquivo dele.

---

## Fora deste plano

Portar a versão escolhida para o `PluginEditor` (JUCE `LookAndFeel_V4`, fontes embutidas) · redimensionamento · acessibilidade por teclado no plugin real.
