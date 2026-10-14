# Parâmetros de masterização por gênero: linha de base para o assistente (Issue #20)

Data: 2026-10-14. Escopo: alvos de loudness (LUFS-I e short-term), true peak, dinâmica (crest/PLR/LRA), tendência tonal (90 Hz, 300 Hz, 3 kHz, 10 kHz) e imagem estéreo, para os gêneros Trap, Rap/Boom bap, Lo-fi, Funk BR, House, EDM, Pop e Rock. Os números são o ponto de partida para o assistente de medição e correção; não são normas.

## Resumo

1. **Plataformas de streaming:** o consenso é de integrado perto de −14 LUFS e true peak de −1 dBTP. Só o Spotify tem especificação oficial com número. Apple, YouTube, Tidal e Deezer aparecem em fontes secundárias, e a Deezer tem o número menos confirmado (−15 LUFS) [1][2][3][4][5][6][7][8][9].
2. **Gêneros de dance/pop e rock:** amostras medidas por terceiros (não revisadas por pares) colocam a mediana de integrado entre −8,4 e −9,7 LUFS para Metal, Dance/EDM, House, Pop, Hip-Hop/Rap e Rock, com faixa interquartil de cerca de 2 a 3 LU [17]. Isso fica bem acima de −14 LUFS: o alvo de streaming é normalizado no playback, então ser mais alto não ganha volume na prática [1][17].
3. **Dinâmica:** há muito pouca medição publicada por gênero. A única medida com LRA é uma amostra de 10 faixas do chart de dance da Billboard (LRA médio de 6,6 LU) [16]. Crest factor por gênero aparece só como faixas heurísticas de blog [21]. Não há PLR por gênero publicado com método claro.
4. **Tonal e estéreo:** não há curvas numéricas publicadas para 90, 300, 3k e 10k Hz em nenhum gênero. Os alvos "Orchestral", "Bass Heavy" e "Modern" da iZotope não têm valores públicos [13]. A única comparação de largura por gênero é qualitativa: pop é o mais largo e hip-hop o mais estreito, numa análise de 54 músicas do Global 200 (H1 2024) [20]. A regra de mono abaixo de 100–120 Hz é consenso de prática, não de norma [25].
5. **Funk BR, Lo-fi e Boom bap:** não encontrei dado de loudness ou dinâmica publicado para nenhum dos três. Funk BR não tem nenhum número em fonte alguma. As sugestões desses gêneros abaixo são hipóteses e devem ser medidas em referências próprias.

## Por gênero

Legenda de confiança: **Alta** = documento primário com número (fabricante, norma, plataforma). **Média** = fonte secundária com método descrito. **Baixa** = blog, fórum ou regra de bolso sem método. **Nenhuma** = não encontrado.

### Trap

| Parâmetro | Valor / tendência | Fonte | Confiança |
|---|---|---|---|
| LUFS-I | Mediana de −9,7 LUFS para a categoria Hip-Hop/Rap (Trap não separado), faixa interquartil de −11,2 a −8,5. Outro blog cita −7,3 a −9,4 para "hip-hop e trap", sem método. | [17]; [19] | Média (a categoria) / Baixa (o blog) |
| Short-term no trecho mais alto | Sem dado. | — | Nenhuma |
| PLR / crest | Crest de 10–14 dB para "pop moderno e hip-hop", heurística de blog. Não é PLR. | [21] | Baixa |
| LRA | Sem dado. | — | Nenhuma |
| Tonal 90 / 300 / 3k / 10k Hz | Sem número publicado. | — | Nenhuma |
| Estéreo / mono em graves | Hip-hop é o gênero com a menor largura medida no Global 200 (H1 2024, n=54). Mono abaixo de 100–120 Hz é prática comum. | [20]; [25] | Média (qualitativo) / Baixa (mono) |

### Rap / Boom bap

| Parâmetro | Valor / tendência | Fonte | Confiança |
|---|---|---|---|
| LUFS-I | Sem dado para boom bap. A categoria Hip-Hop/Rap tem mediana de −9,7 LUFS [17]. | [17] | Média (proxy) |
| Short-term | Sem dado. | — | Nenhuma |
| PLR / crest | Sem dado por gênero. Estudo de 2016 diz que rap tem faixa dinâmica menor que música clássica, sem números publicados no resumo acessível [23]. | [23] | Baixa |
| LRA | Sem dado. | — | Nenhuma |
| Tonal 90 / 300 / 3k / 10k Hz | Sem número publicado. | — | Nenhuma |
| Estéreo / mono em graves | Hip-hop é o mais estreito, como em Trap [20]. | [20] | Média (qualitativo) |

### Lo-fi (hip hop)

| Parâmetro | Valor / tendência | Fonte | Confiança |
|---|---|---|---|
| LUFS-I | Sem dado para lo-fi. A mediana de "Ambient" (−14,1) não é um proxy aceitável: é outro gênero. | [17] | Nenhuma |
| Short-term | Sem dado. | — | Nenhuma |
| PLR / crest | Sem dado. | — | Nenhuma |
| LRA | Sem dado. | — | Nenhuma |
| Tonal 90 / 300 / 3k / 10k Hz | Sem número publicado. | — | Nenhuma |
| Estéreo / mono em graves | Sem dado. | — | Nenhuma |

### Funk BR (funk carioca / brasileiro)

| Parâmetro | Valor / tendência | Fonte | Confiança |
|---|---|---|---|
| LUFS-I | Nenhum número encontrado. O guia do Beat Funk só diz que compressão e limitação controlam a dinâmica, sem valor [27]. | [27] | Nenhuma |
| Short-term | Sem dado. | — | Nenhuma |
| PLR / crest | Sem dado. | — | Nenhuma |
| LRA | Sem dado. | — | Nenhuma |
| Tonal 90 / 300 / 3k / 10k Hz | Sem dado. | — | Nenhuma |
| Estéreo / mono em graves | Sem dado. | — | Nenhuma |

Não há literatura de masterização de funk BR nas fontes que consegui acessar. A única medida razoável é medir faixas comerciais de referência (ver a seção de implicações).

### House

| Parâmetro | Valor / tendência | Fonte | Confiança |
|---|---|---|---|
| LUFS-I | Mediana de −9,1 LUFS, faixa interquartil de −10,4 a −8,0 (12.508 faixas). | [17] | Média |
| Short-term | Sem dado para house. | — | Nenhuma |
| PLR / crest | Heurística de blog para EDM e techno: crest de 6–10 dB [21]. Não é PLR e não é específico de house. | [21] | Baixa |
| LRA | Sem dado para house. | — | Nenhuma |
| Tonal 90 / 300 / 3k / 10k Hz | Sem número publicado. | — | Nenhuma |
| Estéreo / mono em graves | Sem dado. | — | Nenhuma |

### EDM

| Parâmetro | Valor / tendência | Fonte | Confiança |
|---|---|---|---|
| LUFS-I | Mediana de −8,7 LUFS para Dance/EDM (18.514 faixas, faixa interquartil de −10,4 a −7,4) [17]. Uma análise de 10 faixas do chart de dance da Billboard deu média de −8,3 LUFS, com máximo de −6,2 e mínimo de −11,3 [16]. Um blog cita −4,2 a −7,1 para EDM/dubstep, sem método, e contradiz [17] [19]. | [16]; [17]; [19] | Média (mediana) / Baixa (blog) |
| Short-term no trecho mais alto | Amostra de 10 faixas: short-term médio de −6,1 LUFS, máximo de −4,5 ("Miles On It", refrões) [16]. | [16] | Baixa (n=10) |
| PLR / crest | Uma faixa com PLR de 7,5 dB (estéreo) e 14,7 dB (Atmos), sem média [16]. Heurística de crest de 6–10 dB [21]. | [16]; [21] | Baixa |
| LRA | Média de 6,6 LU nas 10 faixas, com máximo de 13,8 LU e mínimo de 3,5 LU [16]. | [16] | Baixa (n=10) |
| Tonal 90 / 300 / 3k / 10k Hz | Qualitativo, sem dB: uma faixa com graves "massivos", uma com verso de pouco grave como escolha de arranjo, uma com vocal estridente entre 5 e 8 kHz [16]. | [16] | Baixa |
| Estéreo / mono em graves | Sem medida de largura nas faixas de baixo. Baixo "largo" sem perder grave foi observado [16]. | [16] | Baixa |

### Pop

| Parâmetro | Valor / tendência | Fonte | Confiança |
|---|---|---|---|
| LUFS-I | Mediana de −9,5 LUFS, faixa interquartil de −11,2 a −8,1 (46.316 faixas) [17]. Top 10 do Global 200 (H1 2024, n=54): média de −8,3 LUFS, desvio de 1 LU [20]. | [17]; [20] | Média |
| Short-term | Sem dado para pop. | — | Nenhuma |
| PLR / crest | Crest de 14–18 dB para "pop com dinâmica" e rock, heurística de blog [21]. Não é PLR. | [21] | Baixa |
| LRA | Sem dado para pop. | — | Nenhuma |
| Tonal 90 / 300 / 3k / 10k Hz | Sem número publicado. | — | Nenhuma |
| Estéreo / mono em graves | Pop é o mais largo entre os gêneros medidos no Global 200 [20]. | [20] | Média (qualitativo) |

### Rock

| Parâmetro | Valor / tendência | Fonte | Confiança |
|---|---|---|---|
| LUFS-I | Mediana de −9,2 LUFS, faixa interquartil de −11,0 a −7,8 (13.966 faixas) [17]. | [17] | Média |
| Short-term | Sem dado. | — | Nenhuma |
| PLR / crest | Crest de 14–18 dB para rock e pop com dinâmica, heurística de blog [21]. | [21] | Baixa |
| LRA | Sem dado para rock. | — | Nenhuma |
| Tonal 90 / 300 / 3k / 10k Hz | Sem número publicado. | — | Nenhuma |
| Estéreo / mono em graves | Sem dado. | — | Nenhuma |

## Streaming e referências de entrega (independentes de gênero)

| Plataforma | Alvo | True peak | Fonte | Confiança |
|---|---|---|---|---|
| Spotify | −14 LUFS-I (normalização no playback). Premium: "Alto" −11, "Normal" −14, "Baixo" −19. Se o master for mais alto que −14 LUFS-I, true peak abaixo de −2 dBTP. | Abaixo de −1 dBTP. Limitador de segurança em −1 dB, ataque de 5 ms e liberação de 100 ms. | [1] | Alta |
| Apple Music | Sem número oficial encontrado na página de Sound Check, que só diz que nivela o volume entre faixas [2]. −16 LUFS é citado com frequência em fontes secundárias [3]. | −1 dBTP recomendado em fontes secundárias [3]. | [2]; [3] | Baixa |
| YouTube | Aproximadamente −14 LUFS como referência de normalização, segundo fonte secundária [4]. A página oficial de "estável volume" descreve a função sem valor numérico [5]. | −1 dBTP como regra de bolso [4]. | [4]; [5] | Média-baixa |
| Tidal | −14 LUFS, com normalização por álbum, segundo fontes secundárias [6][7]. | −1 dBTP [6]. | [6]; [7] | Média-baixa |
| Deezer | −15 LUFS segundo FAQ de terceiros [8]. A página oficial de normalização para ouvintes não traz número [9]. Uma moderação de fórum relatando a declaração de desenvolvedores também cita −15 LUFS [8]. | Abaixo de −1 dBTP segundo [8]. | [8]; [9] | Baixa |
| Referência de broadcast (não streaming) | EBU R 128: −23,0 LUFS-I, tolerância de ±0,2 LU na revisão 3 de 2020 [10]. Para streaming, a EBU diz que o programa pode ser entregue sem alteração a −23 LUFS [11]. | −1 dBTP [10][11]. | [10]; [11] | Alta (para R 128) |

Limitação: o Spotify é a única plataforma com número na página oficial. Os demais valores vêm de fontes secundárias ou de fóruns e podem estar desatualizados. Antes de fechar um valor de produto, confirmar nas páginas de suporte de cada plataforma.

## O que isso implica para o Finale

Todos os valores abaixo são **sugestões derivadas das fontes**, não normas. Servem como ponto de partida para o assistente e devem ser validados com medições de referências próprias (ver item final).

1. **Teto de true peak:** −1,0 dBTP para todos os gêneros, como padrão. É o valor que aparece em Spotify, Ozone (Destination "Streaming" = −1,0 dB) e Pro-L 2 [1][12][26]. Se o integrado ficar acima de −14 LUFS, avisar que o true peak pode precisar de −2 dBTP no Spotify [1].
2. **Alvo de integrado sugerido (faixas de trabalho, não de regra):**
   - Trap e Rap/Boom bap: −9 a −10 LUFS-I, a partir da mediana de hip-hop/rap [17]. Para Boom bap, a faixa pode ser mais baixa, mas não há dado para isso.
   - House e EDM: −8,5 a −9,5 LUFS-I, a partir de [17] e [16].
   - Pop: −9 a −10 LUFS-I [17][20].
   - Rock: −9 a −11 LUFS-I [17].
   - Lo-fi: sem base. Sugestão provisória de −11 a −13 LUFS-I, apenas como hipótese; medir antes de usar.
   - Funk BR: sem base. Sugestão provisória de −10 a −12 LUFS-I, a ser substituída pela medição de referências comerciais do gênero.
   - Como as plataformas normalizam para −14 (Spotify) ou −16 (Apple), o assistente deve avisar quando o integrado passa muito do alvo, mas não precisa forçar −14 [1][17].
3. **Short-term no refrão ou drop:** para EDM, a amostra de 10 faixas sugere máximo de short-term em torno de −4,5 a −6 LUFS [16]. Como a amostra é pequena, usar isso só como limite de alerta.
4. **Dinâmica:** para dance/pop, LRA de 4 a 7 LU é o intervalo onde ficam as faixas de referência [16]. Rock e gêneros de banda devem permitir mais LRA; não há número publicado para isso. Para PLR, a regra de Lars Lentz de PLR acima de 12 para "alta qualidade dinâmica" é uma opinião de blog, sem método [22], e não deve virar regra do assistente.
5. **Tonal:** não usar curvas de terceiros. A iZotope não publica valores, então qualquer curva de referência precisa vir de medições das referências locais (pasta `references/`, ignorada pelo git). Só o JSON com as curvas geradas entra no repo, como pede `CLAUDE.md`. Os quatro pontos (90, 300, 3k, 10k Hz) devem ser medidos, não copiados de fontes.
6. **Estéreo:** mono abaixo de 100–120 Hz como padrão para todos os gêneros, com possibilidade de ajuste para Funk BR e Trap, que costumam ter graves dominantes, se as medições mostrarem outra coisa [25]. Correlação de graves próxima de +1 quando a banda é mono. Para a imagem geral, as fontes citam correlação entre 0 e +1 como faixa típica, mas isso é regra de prática [15][25].
7. **Próximo passo recomendado:** medir de 10 a 20 faixas comerciais por gênero (incluindo funk carioca) com um medidor que siga BS.1770 e Tech 3341, e registrar integrado, short-term máximo, true peak, LRA, espectro médio por banda e correlação. Isso substitui os itens "sem dado" e permite validar as sugestões acima. As medidas servem como curvas de referência, sem publicar o áudio.

## Fontes

[1] Spotify for Artists, "Loudness normalization on Spotify" (página em português, obtida pela ferramenta de leitura; o texto citado é a tradução do conteúdo). https://support.spotify.com/artists/article/loudness-normalization/ (primária, vendor)

[2] Apple Support, "Sound Check" (página sobre a configuração de Sound Check no Music; sem número). https://support.apple.com/en-gb/109331 (primária, vendor; sem LUFS)

[3] Violet Recording, "How to master for Apple Music". https://violetrecording.com/how-to-master-for-apple-music/ (secundária; apenas o resumo de busca foi lido, não a página inteira)

[4] MeterPlugs, "YouTube changes loudness reference to -14 LUFS" (2019). https://meterplugs.com/blog/2019/09/18/youtube-changes-loudness-reference-to-14-lufs.html (secundária; apenas o título e o resumo de busca foram lidos)

[5] YouTube Help, "Video & audio quality enhancements" (página sobre "estável volume" e voice boost; resumo de busca). https://support.google.com/youtube/answer/16619284?hl=en (primária, vendor; sem número)

[6] Fora Soft, "LUFS targets per platform 2026" (tabela com Tidal a −14 LUFS e −1 dBTP; resumo de busca). https://www.forasoft.com/learn/audio-for-video/articles-audio/lufs-targets-per-platform-2026 (secundária)

[7] MeterPlugs, "Tidal adds loudness normalization" (2016; resumo de busca). https://meterplugs.com/blog/2016/10/07/tidal-adds-loudness-normalization.html (secundária)

[8] Recordjet FAQ, "How loud should my master be? What are the store requirements" (Deezer −15 LUFS; resumo de busca). https://faq.recordjet.com/en/articles/6959419-how-loud-should-my-master-be-what-are-the-store-requirements (secundária)

[9] Deezer Help, "Volume Normalization on Deezer" (página para ouvintes, sem número; lida). https://support.deezer.com/hc/en-gb/articles/360005369938-Volume-Normalization-on-Deezer (primária, vendor; sem LUFS)

[10] EBU Tech 3343, "Guidelines for production of programmes in accordance with EBU R 128" (−23 LUFS, tolerância ±0,2 LU, −1 dBTP; resumo de busca). https://tech.ebu.ch/docs/tech/tech3343.pdf (primária, norma)

[11] EBU R 128 s2, "Loudness in streaming" (Supplement 2, nov. 2023; resumo de busca). https://tech.ebu.ch/docs/r/r128s2.pdf (primária, norma)

[12] iZotope Ozone 9, "Master Assistant" (Intensity: LOW −14, MEDIUM −12, HIGH −11 LUFS; Destination: Streaming −1,0 dB, CD −0,3 dB; lido). https://s3.amazonaws.com/izotopedownloads/docs/ozone9/en/master-assistant/index.html (primária, vendor)

[13] iZotope Tonal Balance Control, "Target Curves & Metering" (lido; lista Orchestral, Bass Heavy e Modern; sem valores numéricos). https://downloads.izotope.com/docs/tonal-balance-control/meters-and-target-curves/index.html (primária, vendor)

[14] iZotope Learn, "Why iZotope created the Tonal Balance Control plug-in" (resumo de busca; afirma que a ferramenta agora cobre "12 gêneros e estilos", incluindo hip hop e EDM). https://www.izotope.com/en/learn/why-izotope-created-the-tonal-balance-control-plug-in.html (secundária; contradiz [12] em número de curvas, ver incertezas)

[15] iZotope Ozone 9, "Imager" (manual; resumo de busca sobre largura e correlação). https://s3.amazonaws.com/izotopedownloads/docs/ozone9/en/imager/index.html (primária, vendor)

[16] iZotope Learn, "A LUFS and tonal analysis of 10 top-charting dance and electronic songs" (lido; 10 faixas do chart de dance da Billboard, lossless do Qobuz; medidor não nomeado). https://www.izotope.com/en/learn/dance-electronic-analysis (secundária, vendor; n=10)

[17] Freshly Baked Studios, "Average LUFS by Genre: We Measured 314,876 Tracks (2026 Data)" (lido; pipeline do autor, LUFS-I com modelo de correção, erro médio de 0,5 dB segundo o autor; sem PLR nem LRA por gênero). https://freshlybakedstudios.com/blog/average-lufs-by-genre (secundária; sem revisão por pares)

[18] LuvLang Studio, "What hundreds of commercial masters told me about loudness in 2026" (resumo de busca; 151 masters, metal com −14 LUFS e pop com cerca de −8 LUFS, contradizendo [17] em metal). https://luvlang.studio/research/loudness-2026 (secundária)

[19] Soundcamps, "Spotify LUFS analysis" (resumo de busca; trap/hip-hop −7,3 a −9,4 e EDM/dubstep −4,2 a −7,1, sem método). https://soundcamps.com/blog/spotify-lufs/ (secundária; sem método)

[20] iZotope Learn, "Mastering trends in 2024" (resumo de busca; top 10 do Global 200, H1 2024, n=54, média −8,3 LUFS, pop com maior largura e hip-hop com menor). https://www.izotope.com/en/learn/mastering-trends (secundária, vendor)

[21] Monka Audio Lab, "dBFS, RMS, LUFS, True Peak and Crest Factor: guide" (resumo de busca; faixas heurísticas de crest por gênero). https://monkaaudiolab.com/en/resources/db-peak-rms-lufs-true-peak-and-crest-factor-comparison-and-summary-table (secundária; heurística)

[22] Lars Lentz Audio, "Essential loudness guidelines for music producers" (resumo de busca; opinião sobre PLR acima de 12). https://larslentzaudio.com/2025/12/03/levels-in-mastering-for-the-various-streaming-platforms/ (secundária; opinião)

[23] Kirchberger, M. e Russo, F. A. (2016), "Dynamic Range Across Music Genres and the Perception of Dynamic Compression in Hearing-Impaired Listeners", Trends in Hearing 20. DOI 10.1177/2331216516630549. https://journals.sagepub.com/doi/pdf/10.1177/2331216516630549 (acadêmica, revisada por pares; o PDF retornou 403 e só o resumo de busca foi lido; rap e rock aparecem com faixa menor que clássico, sem números confirmados)

[24] Antares Tech, "Dynamic range explained" (faixas aproximadas de DR por gênero, sem método; resumo de busca). https://www.antarestech.com/blog/a-beginners-guide-to-dynamic-range-in-audio-production (secundária; sem método)

[25] Master the Mix, "How to add width to bass without losing mono compatibility" (regra de mono em 100–120 Hz; resumo de busca). https://www.masteringthemix.com/blogs/learn/how-to-add-width-to-bass-without-losing-mono-compatibility (secundária; prática, não norma)

[26] FabFilter, Pro-L 2 Help: "Loudness metering" e "True peak limiting" (resumo de busca; alvos −9, −14, −23, −24 LUFS; o resumo diz que o manual lista −1,0 dBTP para Spotify e YouTube, não verificado na página). https://www.fabfilter.com/help/pro-l/using/loudnessmetering e https://www.fabfilter.com/help/pro-l/using/truepeaklimiting ; manual em PDF: https://www.fabfilter.com/downloads/pdf/help/ffprol2-manual.pdf (primária, vendor)

[27] Canadian AF, "The ultimate guide to crafting captivating Brazilian beat funk" (sem números; resumo de busca). https://canadianafund.gc.ca/?p=48122 (secundária; sem dado quantitativo)

## Incertezas

- **Funk BR, Lo-fi e Boom bap:** nenhuma medição publicada encontrada. As sugestões desses gêneros são hipóteses.
- **Trap separado de Hip-Hop/Rap:** a medição de [17] é só da categoria. O número de trap vem de um blog sem método [19].
- **Short-term, LRA e PLR por gênero:** quase inexistentes. As amostras de EDM e pop têm n = 10 e n = 54.
- **Tonal:** não há valores numéricos publicados para os quatro pontos (90, 300, 3k, 10k Hz) em nenhum gênero. Não está confirmado se a iZotope tem curvas separadas "Hip-Hop" e "EDM". O resumo de busca cita "12 gêneros" [14], mas a página do Ozone 9 fala em "dez curvas" [12], e a página de Tonal Balance Control que li lista só três curvas de fábrica [13]. Isso precisa ser checado no próprio app.
- **Apple Music:** sem número oficial. −16 LUFS é uma citação recorrente em fontes secundárias, não verificada numa página da Apple.
- **YouTube:** a referência de −14 LUFS vem de uma fonte secundária. A página oficial que li não traz valor. Não consegui confirmar o "Stats for Nerds" mostrando loudness normalizada.
- **Tidal e Deezer:** não consegui abrir páginas oficiais com número. O −15 LUFS da Deezer vem de uma FAQ de terceiros e de um relato de moderador de fórum.
- **Medidor das fontes:** não está claro qual medidor foi usado em [16] e [17] ([17] afirma usar um modelo de correção próprio). Os números de integrado podem ter diferença de até 0,5 dB em relação a um medidor de referência.
- **Viés de amostra:** as fontes de [17] e [18] são de catálogos de terceiros, com seleção desconhecida. Discordam em metal (de −8,4 a −14 LUFS). Por isso, a confiança nos valores de mediana é média, e não alta.
- **Kirchberger e Russo (2016):** o texto completo não foi lido (403). Os valores de DR de rap e rock não foram confirmados.
