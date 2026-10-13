# Medidor LUFS (BS.1770 / EBU R128): reusar ou implementar do zero

Data: 2026-10-13. Escopo: medidor de momentary (400 ms), short-term (3 s) e integrated com gate, para a UI, o A/B e o modo Auto (ver `docs/decisoes.md`).

## Recomendação

**Implementar do zero em `Source/DSP`, header-only, sem dependência externa de runtime, e usar o libebur128 apenas como oráculo offline nos testes.** Nenhuma biblioteca C/C++ encontrada serve para vendor direto: a principal (libebur128, MIT) aloca memória no caminho de processamento e tem API em C; as demais são GPL-3.0, sem commits recentes ou jovens demais para depender delas. O algoritmo é pequeno (K-weighting de 2 biquads, média de quadrados, gate em blocos de 400 ms) e está inteiramente especificado no próprio BS.1770, então o custo de escrever é baixo e a verificação é objetiva.

## Por que (fatos principais)

1. **O algoritmo é curto e a norma o especifica por completo.** O Anexo 1 do BS.1770-5 define os quatro estágios (K-weighting, média de quadrados por canal, soma ponderada, gate em blocos de 400 ms com 75% de sobreposição, com limiares de −70 LKFS e −10 LU). Os coeficientes de 48 kHz estão nas Tabelas 1 e 2 ([BS.1770-5, Anexo 1](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf)).
2. **libebur128 aloca no caminho de processamento.** Em `ebur128_calc_gating_block`, o modo lista chama `malloc` sempre que `block_list_size` não atingiu `block_list_max`. Em `ebur128_init`, `block_list_max = history / 100`, e `history` começa em `ULONG_MAX` (conforme leitura de `ebur128/ebur128.c`, não verificado linha a linha). Ou seja, sem `EBUR128_MODE_HISTOGRAM` ele aloca a cada bloco de gate de 100 ms durante quase toda a sessão. O cabeçalho `ebur128.h` não diz nada sobre tempo real, e as assinaturas `ebur128_add_frames_*` retornam `EBUR128_ERROR_NOMEM`. Violaria a regra "nada de alocação" de `CLAUDE.md`.
3. **JUCE não tem medidor de loudness.** A lista de módulos da branch `master` tem 24 pastas (`juce_dsp`, `juce_audio_processors` etc.) e nenhuma é de loudness/LUFS/BS.1770 ([JUCE/modules](https://github.com/juce-framework/JUCE/tree/master/modules)). Não verifiquei a árvore da tag 9.x.

## Algoritmo (fonte: BS.1770-5, Anexo 1, salvo indicação)

- **K-weighting** em dois estágios: shelving de cabeça (Tabela 1) e high-pass RLB (Tabela 2). Valores a 48 kHz:
  - Estágio 1: b0 = 1.53512485958697, b1 = −2.69169618940638, b2 = 1.19839281085285, a1 = −1.69065929318241, a2 = 0.73248077421585.
  - Estágio 2: b0 = 1.0, b1 = −2.0, b2 = 1.0, a1 = −1.99004745483398, a2 = 0.99007225036621.
  - Para outras taxas, o texto diz: "Implementations at other sampling rates will require different coefficient values, which should be chosen to provide the same frequency response that the specified filter provides at 48 kHz." Ou seja, recalcular os coeficientes para a taxa real (libebur128 faz isso com `K = tan(M_PI * f0 / samplerate)` em `ebur128_init_filter`) e testar a resposta em frequência contra a tabela de 48 kHz.
- **Média de quadrados** por canal, eq. (1): z_i = média de y_i² no intervalo.
- **Loudness**, eq. (2): L_K = −0.691 + 10·log10(Σ G_i · z_i).
- **Pesos** (Tabela 3): L, R, C = 1.0; Ls, Rs = 1.41 (~+1.5 dB). LFE excluído (Anexo 1, texto após a Figura 1).
- **Gate:** blocos de Tg = 400 ms, passo de 100 ms (overlap 75%), eq. (3)–(4). Limiar absoluto a = −70 LKFS, eq. (6). Limiar relativo r = (loudness dos blocos acima de a) − 10 LU, eq. (6) e (7). Blocos acima de max(r, a) entram no integrado, eq. (7).
- **Tech 3341 (EBU Mode)**, seção 2.2: momentary é janela retangular deslizante de 0.4 s sem gate; short-term é janela de 3 s sem gate; taxa de atualização mínima de 10 Hz para short-term e 1 Hz para integrated em medidores "live". Não é permitido ballistics extra (ataque/liberação) em EBU Mode ([Tech 3341](https://tech.ebu.ch/docs/tech/tech3341.pdf), seção 2.2).
- **Tech 3341, seção 2.3:** o gate do integrado usa o mesmo método do BS.1770 (−70 LUFS absoluto, −10 LU relativo, blocos de 400 ms com 75%). Em medidor "live", o integrado tem de ser recalculado a partir dos blocos armazenados a cada atualização. Isso exige armazenamento dos blocos ou um histograma.
- **Tech 3341, seção 2.9:** calibração de 1 kHz estéreo em fase com pico de −18 dBFS deve ler −18.0 LUFS.

## Sinais de teste (fonte: Tech 3341, Tabela 1)

Os casos 1–14 são sinais sintéticos (senoides, tons com durações e níveis especificados) com tolerância de ±0.1 LU. Eles podem ser gerados em C++ dentro do teste, sem baixar WAV nenhum, o que é o melhor caminho para o repo:

- Caso 1: senoide estéreo de 1000 Hz, −23.0 dBFS por canal, 20 s. Esperado M = S = I = −23.0 LUFS.
- Caso 2: mesma coisa a −33.0 dBFS. Esperado −33.0 LUFS.
- Caso 3: tons de 10 s a −36 dBFS, 60 s a −23 dBFS, 10 s a −36 dBFS. Esperado I = −23.0 LUFS (testa o gate relativo).
- Caso 4: com tons a −72 dBFS. Esperado I = −23.0 LUFS (testa o gate absoluto).
- Caso 9: S constante após 3 s. Caso 12: M constante após 1 s.
- Casos 10 e 11 (e 13 e 14): para meter "live", 20 leituras sucessivas, o que testa a resposta temporal.
- Casos 15–23 são de true peak (não são escopo do medidor LUFS, mas estão no mesmo documento).

Tech 3341 diz que os arquivos oficiais são sintetizados em 48 kHz e disponíveis para download no site da EBU. Não baixei; a licença do conjunto não foi verificada, então não devemos commitá-los.

## Candidatos

| Projeto | Licença (fonte) | Linguagem | Último commit | Aloca/trava no processo? | Taxas arbitrárias | Vendor |
|---|---|---|---|---|---|---|
| **libebur128** (jiixyj) | MIT, arquivo `COPYING` ("Copyright (c) 2011 Jan Kokemüller") | C (ANSI, "Portable ANSI C code") | 2021-02-14 (commit `67b33ab`, [commits](https://github.com/jiixyj/libebur128/commits/master)) | **Sim**: `malloc` em `ebur128_calc_gating_block` (modo lista) e em `EBUR128_ADD_FRAMES` (LRA); sem mutex/pthread. Modo histograma evita o malloc do gate, mas quantiza em bins. | Sim: `K = tan(M_PI*f0/samplerate)` por taxa; aceita 16 Hz a 2.822.400 Hz (`VALIDATE_CHANNELS_AND_SAMPLERATE`, `ebur128.c`) | Médio. Não é C++. Teria que ser embrulhado e os `malloc` removidos. Licença permite fechado e público. |
| **klangfreund/LUFSMeter** | MIT segundo `README.rst` ("The core measurement implementation is published here, under the MIT license."). Um fórum citado na busca diz GPLv2: contradição, vale o arquivo do repo, mas confirmar. | C++ com JUCE (`Ebu128LoudnessMeter.cpp`) | 2022-02-21 ([API commits](https://api.github.com/repos/klangfreund/LUFSMeter/commits?per_page=1)) | `processBlock` faz `bufferForMeasurement = buffer` (cópia) e `std::map` aloca em nós novos a cada 0.1 LU (na cadência de gate). Sem lock, mas os getters lêem membros escritos pelo áudio sem sincronização (race se chamados de outra thread). | Coeficientes com `prepareToPlay(sampleRate,...)`; filtro não incluído, não verificado | Baixo: depende de JUCE e de `juce::AudioSampleBuffer`, e os canais de surround parecem off-by-one (`k == 3 || k == 4`, com LFE no índice 3 numa ordem 5.1 padrão). |
| **keithhetrick/bellweather-audio-core** | Apache-2.0 (`LICENSE`, README) | C++, CMake, `cpp20` (topics) | Não visível no página; 7 commits no total; README com "Last Verified: 2026-10-07" (não é data de commit) | README diz que `Bs1770Meter::process()` e `TruePeakMeter::process()` têm testes de alocação no audio thread, e que os testes de Tech 3341 estão no repo. Não li o código. | Não verificado | Repo muito novo e pequeno. Vale estudar como referência de testes, não depender dele. |
| **deeuu/loudness** | GPL-3.0 (sidebar) | C++ com bindings Python | não visível | não verificado | README não cita BS.1770 nem taxas | Não serve: GPL-3.0 e README diz "not stable". |

Licença do JUCE (relevante ao projeto como um todo): o JUCE "modules are dual-licensed under the AGPLv3 and the commercial JUCE licence", com o EULA do JUCE 9 para o uso comercial ([JUCE LICENSE.md](https://raw.githubusercontent.com/juce-framework/JUCE/master/LICENSE.md)). Os termos específicos da licença Starter não foram verificados nesta fonte.

## Riscos de licença (flag explícito)

- **Vendor de libebur128 (MIT)**: permitido em código fechado e em release pública, desde que o aviso de copyright seja mantido. Sem problema de link estático.
- **klangfreund (MIT segundo o README)**: idem, mas depende de JUCE, então o problema de licença do JUCE já existe com ou sem ele.
- **deeuu/loudness (GPL-3.0)**: link estático ou vendor tornaria o plugin inteiro GPL. Descartar.
- **Bellweather (Apache-2.0)**: compatível com fechado e público, mas não recomendado pela idade do projeto.

## Plano de verificação (se implementarmos do zero)

1. **Testes Catch2 com os casos de Tech 3341 Tabela 1 (1–14)**, gerados em código, com tolerância de ±0.1 LU. Incluem o gate relativo (caso 3), o absoluto (caso 4) e a resposta temporal (casos 9–14).
2. **Teste de resposta em frequência**: a magnitude do K-weighting calculado para 44.1, 96 e outras taxas deve bater com o filtro de 48 kHz nas mesmas frequências, como exige o Anexo 1.
3. **Comparação offline com libebur128**: uma ferramenta em `tools/` (ou um teste opcional) gera sinais aleatórios, roda os dois e compara M/S/I. A dependência fica fora do plugin e do DSP, e a versão é fixada no CMake, conforme a regra do repo.
4. **Teste de tempo real**: `process()` com bloco de 0 e de 1 amostra, sem NaN, e checagem de alocação (por exemplo, um operador `new` substituído nos testes).
5. **Taxas**: prepare() com 44.1, 48, 88.2, 96 e 192 kHz, sem alocação depois disso.

## Tamanho estimado da implementação do zero (estimativa minha, não medida)

- K-weighting (dois biquads com coeficientes calculados em `prepare()`): cerca de 60 linhas.
- Média de quadrados e blocos de 100 ms com janela de 400 ms: cerca de 80 linhas.
- Momentary, short-term (janela de 3 s) e média de Auto (10–30 s, que não é medida BS.1770 e sim uma média própria): cerca de 60 linhas.
- Integrado com gate: cerca de 120 linhas. Para evitar alocação, usar histograma de tamanho fixo ou buffer pré-alocado com limite em `prepare()`.
- Total de código: cerca de 300–400 linhas em `Source/DSP`, mais cerca de 300 de testes.

## O que não foi verificado

- Texto completo da Tech 3341 para os casos 15–23 (true peak) e a Tech 3342 (LRA): li apenas a Tabela 1 e a seção 2.
- Conteúdo do arquivo `ebur128.c` linha a linha: os trechos vieram de leitura por partes, e os números de linha não puderam ser confirmados.
- Se libebur128 passa nos casos de Tech 3341 (não executei).
- Licença Starter do JUCE e a árvore de módulos da tag 9.x (li a branch `master`).
- Data do último commit do klangfreund: a API mostrou 2022-02-21 para o commit mais recente da lista, mas não verifiquei a página do repo.
- Data do último commit do Bellweather e a lista completa de testes.
- Se o modo histograma do libebur128 mantém a precisão exigida pelo gate (não medi o erro de quantização).
- Licença exata do conjunto de sinais de teste da EBU (não baixado).

## Fontes

- ITU-R BS.1770-5 (11/2023): https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf (Anexo 1, Tabelas 1–3, eq. 1–7; Anexo 2 sobre true peak)
- EBU Tech 3341 "EBU Mode" (V4, nov. 2023): https://tech.ebu.ch/docs/tech/tech3341.pdf (seções 2.1–2.3, 2.9, Tabela 1)
- libebur128: https://github.com/jiixyj/libebur128 ; `COPYING`: https://raw.githubusercontent.com/jiixyj/libebur128/master/COPYING ; `ebur128/ebur128.c` e `ebur128/ebur128.h`: https://raw.githubusercontent.com/jiixyj/libebur128/master/ebur128/ebur128.c ; commits: https://github.com/jiixyj/libebur128/commits/master
- klangfreund/LUFSMeter: https://github.com/klangfreund/LUFSMeter ; `README.rst`: https://raw.githubusercontent.com/klangfreund/LUFSMeter/master/README.rst ; `Ebu128LoudnessMeter.cpp`: https://raw.githubusercontent.com/klangfreund/LUFSMeter/master/Ebu128LoudnessMeter.cpp
- Bellweather Audio Core: https://github.com/keithhetrick/bellweather-audio-core ; listagem JUCE Marketplace: https://marketplace.juce.com/products/bellweather-audio-core
- deeuu/loudness: https://github.com/deeuu/loudness
- JUCE: módulos https://github.com/juce-framework/JUCE/tree/master/modules ; licença https://raw.githubusercontent.com/juce-framework/JUCE/master/LICENSE.md
