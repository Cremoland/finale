# Finale — decisões do projeto

Plugin de masterização "de um knob" no estilo Soundgoodizer, com assistente que lê a música (inspirado no Ozone). Feito por Caio + amigo, como hobby.

## Produto

- **Nome:** Finale (codinome; existe o "Grand Finale" da Klevgrand e o Finale da MakeMusic, então rever antes de lançar publicamente). O nome do produto fica definido em um único lugar no código.
- **Estúdio / org GitHub:** Cremoland — repo https://github.com/Cremoland/finale
- **Tipo:** efeito para a master, usado no fim do projeto
- **Formatos:** VST3 + Standalone (Windows). CLAP depois. Sem AU.
- **Objetivo:** hobby, repo privado

## Som

- **Cadeia:** EQ tonal → compressão 3 bandas → saturação → largura estéreo (graves em mono) → limiter true peak
- **Análise mede:** loudness (LUFS), equilíbrio tonal, dinâmica (crest factor), imagem estéreo
- **Modos de leitura:**
  - **Analisar:** toca a música, o plugin mede e fixa os ajustes
  - **Auto:** adapta em tempo real, devagar (média de ~10–30 s), sem bombear
  - **Congelar:** fixa o estado atual do Auto. Usar antes de exportar.
- **"IA":** regras determinísticas (medir → comparar com alvo → ajustar). Sem machine learning na v1.
- **Knob principal:** 0% ≈ sem processamento, 100% = master completa no alvo do preset. Não passa de 100%.
- **Presets = gêneros/subgêneros**, cada um com curva tonal alvo, alvo de LUFS e caráter de compressão/saturação. v1: Trap, Rap/Boom bap, Lo-fi, Funk BR, House, EDM, Pop, Rock.
- **Seletor de caráter** por cima do gênero: **Clean** (padrão, transparente), **Warm** (mais saturação, corpo), **Punch** (compressão rápida, transientes), **Wide** (largura e brilho), **DOOM** (crunch estourado/clipado, mas harmônico: soft clip agressivo com oversampling, respeitando o teto de true peak)
- **Loudness:** alvo por gênero (ex: Lo-fi −12, Pop −9, Funk/EDM −7 LUFS), teto −1 dBTP, ajustáveis
- **A/B com volume igualado** + medidor de LUFS (atual vs alvo)
- **Qualidade:** crossovers Linkwitz-Riley (latência zero) + oversampling 2x na saturação; o limiter detecta true peak com interpolação 4x inspirada na ITU-R BS.1770-4 (coeficientes gerados, não a tabela da norma), porque pega os picos entre amostras como os medidores de dBTP, com menos CPU que rodar o limiter em 2x (decidido 13/10)
- **Curvas de referência:** `finale_curvegen` (`tools/curvegen/`) lê `references/<pasta do gênero>/` com WAV, AIFF ou FLAC (MP3 não entra) e gera `Source/DSP/GenreCurveTable.h`. É um header C++, não JSON, para o plugin não precisar de parser. Pastas: `Trap`, `Rap-Boom bap`, `Lo-fi`, `Funk BR`, `House`, `EDM`, `Pop`, `Rock`. As referências definem a diferença entre os gêneros; o viés comum é a média, por banda, da tabela atual só dos gêneros medidos (gênero sem referência não entra nessa média nem muda). Precisa de pelo menos 2 gêneros com referência; ±6 dB é o limite da diferença entre gêneros, não do valor final. As bandas das pontas cobrem 25–127 Hz e 7–20 kHz (o que os shelves afetam), e cada banda é normalizada pela largura efetiva da resposta real dos filtros. Taxa mínima das referências: 32 kHz. WAVs ficam só na máquina de cada um, nunca no repo. (decidido 14/10)

## Interface

- **Frente:** knob principal, gênero/subgênero, caráter, Analisar/Auto/Congelar, A/B, medidor de LUFS
- **"Configurações avançadas"** (fechada por padrão): alvo de LUFS, teto dBTP, quantidade de EQ/compressão/saturação/largura
- **Visual:** clean, profissional, moderno, elegante. Design fica para depois. Começa com knob desenhado em código.

## Tecnologia

- **JUCE** (licença Starter), **CMake**, versão do JUCE fixada no repo
- **Visual Studio 2022**, os dois no Windows
- **CI (GitHub Actions, Windows):** compila, roda testes de DSP (silêncio, NaN etc.), roda pluginval, checa clang-format
- **CLAUDE.md** no repo com convenções compartilhadas pelos dois Claudes
- **CPU (14/10/2026, `finale_benchmark`, i7-9750H, Release, amount 100):** cadeia inteira ≈ 3,7 % de um núcleo a 48 kHz e ≈ 7,3 % a 96 kHz, parecido em qualquer caráter, com A/B e com assistente (o A/B e o saturador rodam sempre). p99 por bloco: ≤ 7 % a 48 kHz e ≤ 13 % a 96 kHz. Metade do custo é o saturador (medição por bloco num spike a 48 kHz/512: saturador ≈ 2,0 %, compressor ≈ 0,6 %, A/B ≈ 0,4 %, limiter ≈ 0,3 %) (oversampling 2x com FIR de 63 taps, três convoluções por amostra), seguido do compressor. Decidido: não otimizar agora. Se precisar, o primeiro passo é o oversampler pular os taps nulos da meia banda e os zeros inseridos no upsample, o que corta cerca de 2/3 das multiplicações dele (de 189 para ~66 por amostra e canal).

## Colaboração

- Cada um no seu PC, com seu Claude Code, no mesmo repo
- Org GitHub **Cremoland** gratuita, os dois como donos, repo privado
- Ninguém commita direto na `main`: branch curta → PR → CI verde → merge (sem aprovação obrigatória, Claude revisa)
- GitHub Free não protege branch em repo privado. Decidido (29/09): garantir por combinado + `.claude/settings.json` (Claudes bloqueados de push na main) + hook local `.githooks/pre-push`. Migrar pro GitHub Team se virar problema.
- Divisão por feature (os dois mexem em tudo), compensada por:
  - toda tarefa vira Issue, e quem pega se atribui antes de começar
  - PRs pequenos (horas, não semanas)
  - clang-format obrigatório no CI
  - puxar a `main` atualizada antes de abrir PR
- Primeiro marco: esqueleto (knob de ganho) rodando na DAW dos dois + CI verde, antes do trabalho de DSP
