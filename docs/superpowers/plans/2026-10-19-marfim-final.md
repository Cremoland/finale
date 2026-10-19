# Marfim final — Plano

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Refinar a v6 Marfim, que o usuário escolheu como definitiva, em `docs/ui/prototipos/v9-marfim-final.html`. São três mudanças pedidas: trocar a fonte, dar mais destaque às seleções do preset e dar mais destaque ao box branco do medidor. A v6 fica intacta para comparação. Issue #32.

**Spec:** pedido do usuário em 19/10: "a versão 6 marfim ficou muito boa! vamos seguir com ela mas mudando a fonte das letras e dando um pouco mais de destaque pras seleções do preset e para o box branco ao lado!" Continuam valendo todas as Global Constraints de `docs/superpowers/plans/2026-10-19-prototipos-ui.md` e da rodada 2:
- os 7 controles e os valores de exemplo;
- o alvo por gênero, que muda com o gênero;
- os medidores suaves;
- a legenda abaixo da janela;
- contraste ≥ 4,5:1 e nada comunicado só por cor;
- só formas que o JUCE desenha.

## Global Constraints

- Partir de uma **cópia** de `docs/ui/prototipos/v6-marfim.html`, com a mesma estrutura, a mesma paleta e o mesmo JS. Não alterar a v6.
- O que não está listado abaixo fica como está.

---

### Task 1: `v9-marfim-final.html`

**Files:** Create `docs/ui/prototipos/v9-marfim-final.html`

1. **Fonte.** O usuário não disse qual, então o protótipo oferece **3 opções, trocáveis ao vivo**.
   - **Onde fica o seletor:** **fora** da janela de 820 × 500, logo abaixo da legenda. São três botões pequenos: "Fonte: Manrope · Sora · Outfit".
   - **Como funciona:** cada opção troca a família de **todo o texto da interface, inclusive o wordmark `FINALE`**, pela variável `--font-ui`. Os números de dB, LUFS e % continuam em IBM Plex Mono (`--font-mono`, tabular).
   - **Wordmark:** na fonte escolhida, com peso 800, caixa alta e `letter-spacing` 0,18em. O Fraunces sai.
   - **Padrão:** Manrope. Carregar as três pelo Google Fonts, nos pesos 400, 500, 600, 700 e 800.
2. **Mais destaque para as seleções do preset (gênero e caráter).**
   - **Box do preset:** gênero e caráter passam para um **box branco** (`--surface`) com o mesmo raio, borda e sombra do box do medidor (item 3), ocupando a largura da coluna da esquerda abaixo do knob.
   - **Topo do box:** o rótulo "PRESET" e, à direita, um resumo vivo, por exemplo **"Pop · Clean"** em peso 700, que muda com as escolhas.
   - **Chips não selecionados:** fundo `--bg`, borda 1 px `--surface-2` e texto `--text`, peso 500. A altura sobe para 32 px.
   - **Chip selecionado:** fundo `--accent`, texto branco, peso 700, um "✓" pequeno antes do nome e sombra curta na cor do acento (`0 2px 8px rgba(31,122,114,.35)`). A troca de seleção tem transição de 150 ms.
   - **Rótulos "Gênero" e "Caráter":** 11 px, peso 700, cor `--text` (antes `--text-dim`).
3. **Mais destaque para o box branco do medidor (à direita).**
   - **Elevação maior:** borda 1 px `--surface-2` e sombra `0 10px 28px rgba(28,27,25,.10), 0 2px 6px rgba(28,27,25,.06)`.
   - **Faixa de 4 px no topo**, na cor do acento.
   - **Cabeçalho:** o rótulo "LOUDNESS" à esquerda e, à direita, uma **pílula de estado**. Mostra "✓ no alvo" (fundo `--accent`, texto branco) quando o integrado está a até ±1 LU do alvo do gênero. Fora disso, mostra "↑ abaixo do alvo" ou "↓ acima do alvo" (fundo `--surface-2`, texto `--text`). Atualiza junto com os medidores e com a troca de gênero.
   - **Número integrado** maior: 44 px. A unidade LUFS fica ao lado, em 13 px.
   - O resto do box (curto prazo, true peak e GR) fica como na v6.
4. **Legenda:** "Versão 9 · Marfim final".
5. **Galeria:** em `docs/ui/prototipos/index.html`, adicionar no topo a seção "Escolhida · Marfim final" com o cartão 09 (ideia: "A v6 Marfim refinada: fonte nova, preset e loudness em destaque."), acima da "Rodada 2".

- [ ] Criar o arquivo e atualizar a galeria. Conferir contraste: texto branco sobre `--accent` dá 5,14:1. Conferir também os 7 controles, a troca de gênero (EDM → alvo −8,5 e a pílula atualiza), o seletor de fonte e a ausência de sobreposição com o Avançado aberto.
