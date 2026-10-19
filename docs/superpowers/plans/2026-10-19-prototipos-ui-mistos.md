# Protótipos de interface, rodada 2: Noir + Papel — Plano

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Três protótipos que misturam a versão 1 (Noir) com a versão 3 (Papel), as duas que o usuário escolheu, para decidir a definitiva.

**Architecture:** Mesmo formato da rodada 1: um HTML autocontido por versão em `docs/ui/prototipos/`, e a galeria `index.html` ganha uma seção "Rodada 2". Issue #32.

**Spec:**
- Escolha do usuário (19/10): "gostei da versão 1 Noir e também da versão 3 com branco/ivory".
- As Global Constraints da rodada 1 valem todas (`docs/superpowers/plans/2026-10-19-prototipos-ui.md`), inclusive:
  - os 7 controles e os valores de exemplo;
  - o alvo por gênero: Trap, Rap/Boom bap, Lo-fi, Funk BR, House e Rock −9,0; EDM −8,5; Pop −9,5. Ao trocar o gênero, o alvo muda em todo lugar onde aparece;
  - os valores iniciais GR −1,6 dB e short-term −9,0;
  - os medidores com passeio aleatório suave (passo ≤ 0,2 por quadro no short-term e ≤ 0,03 nos outros);
  - a legenda abaixo da janela.

## O que levar de cada uma
- **Do Noir (v1):**
  - o knob herói grande, com arco grosso, marcas e número grande no centro;
  - as fileiras de chips para gênero e caráter;
  - a coluna de medidor à direita, com a faixa do alvo;
  - o controle segmentado do assistente e o A/B no topo.
- **Do Papel (v3):**
  - o fundo marfim (`#F4F1EA`) e o branco;
  - a tipografia editorial: wordmark serifado Fraunces e listas tipográficas com o selecionado marcado por ponto e negrito;
  - o arco fino e o disco branco com sombra curta;
  - a calma: sem glow, margens generosas.

Leia os dois arquivos (`v1-noir.html` e `v3-papel.html`) antes de começar e reaproveite o JS deles.

---

### Task 1: Versão 6 — "Marfim" (Noir em tema claro)

**Files:** Create `docs/ui/prototipos/v6-marfim.html`

**Direção:** o layout do Noir quase idêntico, pintado no marfim do Papel. É a forma mais direta de juntar as duas: a estrutura que você gostou com a luz que você gostou.
- **Layout:** igual ao v1:
  - topo com wordmark à esquerda, assistente segmentado e A/B à direita;
  - knob herói no centro, com chips de gênero e caráter abaixo;
  - coluna de medidor à direita;
  - rodapé "Avançado".
- **Visual:**
  - O knob é um disco branco com sombra curta, como no Papel, e arco de valor **grosso** (14 px) no acento, sem glow.
  - O selecionado nos chips é preenchido no acento, com texto branco e negrito.
  - Wordmark `FINALE` em Fraunces 600, com espaçamento largo.
- **Paleta:**
  - Base: `--bg #F4F1EA`, `--surface #FFFFFF`, `--surface-2 #ECE7DC`, `--text #1C1B19`, `--text-dim #5E5A52`.
  - Acentos: `--accent #1F7A72` (verde-petróleo, o "teal" do Noir escurecido para contraste no claro, ≥ 4,5:1 com texto branco), `--accent-2 #2F5BEA`.
  - Estados: `--ok #1F8A5B`, `--warn #B7791F`, `--danger #C53030`.
- **Fonte:** Fraunces (wordmark) e IBM Plex Sans/Mono.
- [ ] Criar o arquivo.

### Task 2: Versão 7 — "Noir editorial" (Noir escuro com a tipografia do Papel)

**Files:** Create `docs/ui/prototipos/v7-noir-editorial.html`

**Direção:** o fundo escuro e o knob com brilho do Noir, organizados como página editorial: o texto em marfim faz o papel do branco.
- **Layout:**
  - **Topo:** wordmark `FINALE` grande em Fraunces, cor marfim. À direita, o assistente como texto sublinhado (como no Papel) e, abaixo, o status em mono.
  - **Esquerda (~280 px):** o knob herói do Noir (~240 px), com glow suave no acento.
  - **Centro e direita:** gênero e caráter como **listas tipográficas** em duas colunas (selecionado com ponto no acento e negrito marfim).
  - **Faixa de baixo, de largura total:** o medidor horizontal do Papel, com régua de −20 a 0 LUFS, triângulo do alvo, barra de short-term e o número integrado grande à direita. Abaixo dele, TP e GR em linha.
  - **A/B:** chave pequena ao lado do wordmark. **Avançado:** link "Avançado →".
- **Paleta:** `--bg #0E1116`, `--surface #151A21`, `--surface-2 #1E242D`, `--text #F2EDE3` (marfim), `--text-dim #9C9686`, `--accent #4FD1C5`, `--accent-2 #E9DFC8`, `--ok #4FD1C5`, `--warn #F5B84B`, `--danger #FF5C6C`.
- **Fonte:** Fraunces (wordmark) e IBM Plex Sans/Mono.
- [ ] Criar o arquivo.

### Task 3: Versão 8 — "Dia e Noite" (um layout, dois temas)

**Files:** Create `docs/ui/prototipos/v8-dia-noite.html`

**Direção:** um só layout, com um **botão de tema** (☀ / ☾) no topo que troca entre marfim e Noir na hora, com transição de 200 ms. A pergunta deixa de ser "claro ou escuro" e vira uma opção do usuário. Começa no **claro**.
- **Layout (mistura):**
  - **Topo:** wordmark `FINALE` em Fraunces à esquerda. Ao centro, o assistente segmentado do Noir, com o status embaixo. À direita, o A/B e o botão de tema.
  - **Centro-esquerda:** o knob herói (~230 px). No claro, é o disco branco com arco grosso; no escuro, é o corpo escuro com glow.
  - **Centro-direita:** gênero como **lista tipográfica** em 2 colunas de 4 e caráter como **chips** abaixo dela, misturando as duas linguagens.
  - **Direita:** coluna de medidor do Noir.
  - **Rodapé:** "Avançado ▸".
- **Paleta:** dois conjuntos das mesmas variáveis, em `:root[data-theme="light"]` e `:root[data-theme="dark"]`.
  - Claro: os valores da Task 1.
  - Escuro: os do Noir (`--bg #0E1116`, `--surface #171C24`, `--surface-2 #1F2630`, `--text #E6EAF0`, `--text-dim #8A94A6`, `--accent #4FD1C5`, `--accent-2 #7C5CFF`, `--ok #4FD1C5`, `--warn #F5B84B`, `--danger #FF5C6C`).
  - O contraste ≥ 4,5:1 vale nos dois temas.
- **Fonte:** Fraunces (wordmark) e IBM Plex Sans/Mono.
- [ ] Criar o arquivo.

### Task 4: Galeria

**Files:** Modify `docs/ui/prototipos/index.html`

- [ ] Acima das 5 versões, adicionar a seção "Rodada 2 · Noir + Papel" com os cartões 06, 07 e 08 (mesmo formato dos existentes). As 5 originais ficam abaixo, numa seção "Rodada 1".
