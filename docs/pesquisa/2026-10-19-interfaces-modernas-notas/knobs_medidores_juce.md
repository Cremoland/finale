# Knobs, medidores, feedback e implementação em JUCE

Escopo: como plugins de áudio modernos desenham e animam knobs, medidores e feedback visual, e como isso se implementa em JUCE (Windows, VST3, JUCE 8/9). Pesquisa feita com buscas e leitura de fontes públicas. Onde a fonte primária não foi lida (principalmente o PDF EBU Tech 3341 e os manuais de fabricantes), isso está marcado em Gaps. Nada aqui foi copiado de texto protegido.

## 1. Knobs: padrões de design

### Takeaway
Knobs modernos usam um arco de valor (ring/arc) para mostrar a posição, um readout numérico ao lado ou acima, e um modificador para ajuste fino. Os padrões exatos de cada fabricante (FabFilter, iZotope, Sonible, Baby Audio, Minimal Audio, Kilohearts, Image-Line, Xfer) não foram verificados nesta pesquisa.

### Cited Findings
- Baby Audio (Crystalline) usa gráficos abstratos em vez de números: a largura é um círculo que contrai e expande, e a faixa acima de 100% aparece como divisão visual; o "sparkle" é um conjunto de pontos que se multiplica. Sem readouts numéricos, controles em blocos quadrados, pouco texto. — [Tape Op, review do Crystalline](https://new.tapeop.com/reviews/gear/150/crystalline-reverb-plug-in); [Sound On Sound, review do Crystalline](https://www.soundonsound.com/reviews/baby-audio-crystalline)
- Soundgoodizer (Image-Line) tem interface mínima: um knob grande de "Amount" (0–100%) e quatro botões de preset A–D. O knob grande controla o mesmo parâmetro do knob de mix LMH do Maximus, que define a mistura entre o sinal de entrada e as saídas dos compressores low/mid/high. — [Manual Image-Line, Soundgoodizer](https://www.image-line.com/support/flstudio_online_manual/html/plugins/Soundgoodizer.htm); [Audeobox, guia do Soundgoodizer](https://www.audeobox.com/learn/fl-studio/how-to-use-soundgoodizer-in-fl-studio/)
- Um guia de gain-staging alerta que girar o Soundgoodizer aumenta o volume, o que faz o efeito parecer melhor; a recomendação é comparar em nível igualado. Fonte de baixa qualidade. — [Audeobox](https://www.audeobox.com/learn/fl-studio/how-to-use-soundgoodizer-in-fl-studio/)
- No JUCE, o padrão de knob customizado é desenhar com `Graphics` (sem filmstrip), via `LookAndFeel`. Um arco de valor é feito com `Path` (pie segment ou arco) a partir do ângulo inicial até o ângulo atual, preenchido com `fillPath`. — [Forum JUCE, "How to create knob control"](https://forum.juce.com/t/how-to-create-knob-control-on-juce/17545/3); [Forum JUCE, "Rotary with central position and arc to left/right"](https://forum.juce.com/t/rotary-that-has-central-position-and-draws-arc-to-the-left-or-right-of-center/15819)
- Um tópico do fórum mostra como desenhar valor e rótulo dentro do knob usando LookAndFeel_V4 e o cálculo de raio e centro a partir de `bounds`. — [Forum JUCE, "include value and label on RotaryKnob"](https://forum.juce.com/t/tip-how-to-include-value-and-label-on-rotaryknob-without-declaring-a-label/35926)
- Para plugins, o fórum recomenda `juce::SliderParameterAttachment` para ligar o slider ao parâmetro (lida com faixa, skew e gestos). — [Forum JUCE, "How to create knob control"](https://forum.juce.com/t/how-to-create-knob-control-on-juce/17545/3)

### Inferences
- Um knob "de plugin moderno" típico combina: arco de valor colorido sobre trilho apagado; readout numérico no hover ou ao arrastar (não sempre visível); ponto de centro ou detent para parâmetros bipolares (ex.: pan, EQ ±dB), que o próprio arco desenha a partir do centro; zona de sensibilidade menor com Shift/Ctrl para ajuste fino; duplo clique para reset ao padrão. Isso é prática de mercado conhecida, mas não foi confirmada com manuais de fabricantes nesta pesquisa.
- Para o Finale, um knob "de um controle" grande (estilo Soundgoodizer) é coerente com a decisão de produto de simplicidade, e um arco com a posição relativa a um alvo de gênero pode servir como feedback sem texto.

### Gaps
- Convenções de drag sensitivity, double-click reset, fine-adjust com modificador e posição de readout de FabFilter, iZotope, Sonible, Minimal Audio, Kilohearts e Xfer OTT: não pesquisadas com fonte primária (manuais e vídeos de demo não foram abertos).
- Não há fonte confirmada sobre glow/halo e centro detent em plugins específicos.

## 2. Medidores: LUFS, true peak, gain reduction, espectro

### Takeaway
As janelas de tempo do medidor de loudness estão confirmadas: momentâneo 400 ms, curto prazo 3 s, integrado de início a fim. Os detalhes de ballistics (taxa de atualização, suavização na UI) e a gating não foram lidos na fonte primária, por isso ficam em aberto.

### Cited Findings
- Tech 3341 (EBU "EBU Mode") define três medidores: Momentary (400 ms), Short term (3 s) e Integrated (do início ao fim), com sinais de teste para verificar medidores. Objetivo: consistência entre fabricantes. — [EBU, página da Tech 3341](https://tech.ebu.ch/publications/tech3341); [Wikipedia, Loudness monitoring](https://en.wikipedia.org/wiki/Loudness_monitoring) (confirma 400 ms e 3 s)
- O PDF da Tech 3341 tem uma seção "Integration – times and methods, meter ballistics" (seção 2.2), mas o PDF não pôde ser lido como texto na busca. — [EBU, PDF da Tech 3341](https://tech.ebu.ch/docs/tech/tech3341.pdf)
- Um artigo da AES (Skovenborg) compara definições de ballistics para a escala momentânea, incluindo as versões ITU e EBU e uma variante ITU mais rápida. — [Skovenborg, momentary loudness metering](https://cdn.mediavalet.com/aunsw/musictribe/wuxAVIXLIk-yqApoeUJL_w/ojEKiIDp8k-gN5kd7iQo5w/Original/momentary_loudness_metering--skovenborg_2012.pdf)
- Pro-L 2 (FabFilter): botão Loudness Meter Target com predefinidos -9 (CD), -14 (streaming aproximado), -23 (EBU R128) e -24 (ATSC A/85/TR-B32), mais valor custom. O alvo é a meta para a medida Integrated. Os botões Loudness Time Scale escolhem Momentary, Short Term ou Integrated como leitura principal. — [FabFilter, Loudness metering](https://fabfilter.com/help/pro-l/using/loudnessmetering)
- Pro-L 2: a escala do medidor de loudness é +9 ou +18 LU relativa ao alvo, com modo absoluto/relativo; há um readout de texto abaixo; uma curva de loudness ao longo do tempo; máximos de Momentary/Short Term acima do medidor; LRA com Integrated; reset zera máximo de true peak e gain reduction. A ajuda recomenda limitação true peak com saída em -1 dBTP para EBU e streaming. — [FabFilter, Loudness metering](https://fabfilter.com/help/pro-l/using/loudnessmetering)
- Pro-L 2 não especifica taxa de atualização do medidor na ajuda. — [FabFilter, Loudness metering](https://fabfilter.com/help/pro-l/using/loudnessmetering)
- A dica de ballistics de janela curta (400 ms e 3 s) é o que um medidor curto/momentâneo precisa calcular; a exibição pode ser mais lenta que o cálculo.

### Inferences
- Para o Finale, a combinação mais alinhada à decisão de produto é: Integrated como número principal com faixa-alvo (ex.: alvo de gênero ± tolerância), Short Term como barra ou curva secundária, Momentary só como referência rápida, true peak com teto visível, e gain reduction do limiter em linha separada. Isso segue o padrão Pro-L 2, mas é inferência, não recomendação do fabricante.
- Um "indicador de faixa-alvo" (verde dentro da faixa, âmbar fora) é um padrão de UI, mas as cores e tolerâncias exatas não foram verificadas em fonte.
- Um gráfico de histórico de loudness (curva ao longo do tempo) é o mesmo conceito do gráfico de histórico do Pro-L 2.

### Gaps
- Taxa de atualização do medidor e suavização (ballistics de UI) para LUFS e peak: não encontrada em fonte confirmada. O PDF da Tech 3341 precisa ser lido em outro formato (texto extraído) para obter a seção 2.2.
- Valores de gating (absoluto -70 LKFS, relativo -10 LU) aparecem como conhecimento geral, mas não foram confirmados em fonte lida nesta pesquisa. Recomendo ler a Tech 3341 antes de implementar.
- Display de true peak, gain reduction e espectro/curva tonal contra alvo em iZotope, Sonible, Minimal Audio, Kilohearts e Xfer OTT: não pesquisado.
- Tolerância atual do EBU R 128 divergem entre versões de página: uma fonte cita ±0,5 LU (±1 LU para ao vivo) e outra ±0,2 LU. Conferir o documento R 128 atual antes de usar. — [EBU, página de loudness](https://tech.ebu.ch/loudness)

## 3. Animação e feedback visual

### Takeaway
Plugins de estilo minimalista (Baby Audio) usam animação ligada ao valor do parâmetro, não ao sinal de áudio. Visuais reativos ao áudio foram procurados e não encontrados em fontes confiáveis nesta pesquisa.

### Cited Findings
- A animação do Crystalline é ligada aos parâmetros: o círculo de largura cresce e contrai ao arrastar, e os pontos de "sparkle" se multiplicam conforme o valor sobe. — [Tape Op, Crystalline](https://new.tapeop.com/reviews/gear/150/crystalline-reverb-plug-in)
- Sobre frame rate e suavização de animação, o fórum JUCE confirma que `Timer` com alta taxa de repaint aumenta carga de CPU, mas não traz números fixos. — [Forum JUCE, "Repaint performance"](https://forum.juce.com/t/repainting-performance/5559); [Forum JUCE, "Laggy GUIs on OSX El Capitan"](https://forum.juce.com/t/laggy-guis-on-osx-el-capitan/15717?page=2)

### Inferences
- Para o Finale, feedback que reflete o que o assistente faz (ex.: barra de alvo que anima para o novo valor de LUFS após escolher gênero) é compatível com a estética de Baby Audio, sem visuais de espectro pesados.
- Animações de transição de knob e medidor devem ser suavizadas no lado da UI (interpolação simples por quadro), separada da suavização de parâmetro no áudio, que é obrigação do DSP (CLAUDE.md).

### Gaps
- Visuais reativos ao áudio (waveform ou espectro animados, "Soundgoodizer-like" reactive) em Baby Audio, Soundgoodizer e outros: não confirmados. A busca não trouxe manuais ou análises desses detalhes.
- Taxas de frame recomendadas para UI de plugin (30, 60 Hz): não há fonte primária lida. Tratar 30–60 Hz como convenção a validar no hardware do usuário.

## 4. Implementação em JUCE

### Takeaway
A base recomendada é `LookAndFeel_V4` com `drawRotarySlider` sobrescrito, desenho vetorial com `Path` e `Graphics`, e `Timer` para atualizar medidores com repaint pequeno e componentes opacos. Fontes embutidas via BinaryData e `Typeface::createSystemTypefaceFor`, com ressalvas de plataforma e licença.

### Cited Findings
- Tutorial oficial JUCE (movido para juce.com): usar `LookAndFeel_V4` como base, sobrescrever apenas os métodos necessários, assinatura de `drawRotarySlider` recebe `sliderPos` normalizado de 0 a 1 e `rotaryStartAngle`/`rotaryEndAngle`, ângulo = start + pos·(end − start). Usar `setLookAndFeel` no componente e `setLookAndFeel(nullptr)` no destrutor; declarar o look-and-feel antes dos widgets. — [JUCE, tutorial de look-and-feel](https://juce.com/tutorials/tutorial_look_and_feel_customisation)
- Para desenhos com `Path`, o tutorial usa `AffineTransform::rotation(...).translated(...)` e `g.fillPath`. — [JUCE, tutorial de look-and-feel](https://juce.com/tutorials/tutorial_look_and_feel_customisation)
- Fontes: adicionar o TTF/OTF ao projeto (vai para BinaryData) e criar o Typeface com `Typeface::createSystemTypefaceFor(data, size)`. Cachear o `Font` estático, não carregar dentro de `paint`, porque carregar fonte é lento e pode travar a janela (relato no macOS). Em JUCE 8, o construtor antigo de Font está deprecado; usar `FontOptions`. Relato de fonte que funciona no Mac e não no Windows. Alerta de licença: muitas licenças de fonte proíbem embutir. — [Forum JUCE, "Modern custom font guide"](https://forum.juce.com/t/modern-custom-font-guide/20841); [Forum JUCE, "Embedding fonts in JUCE 8"](https://forum.juce.com/t/embedding-fonts-in-juce-8/66430); [Forum JUCE, "Using a custom font"](https://forum.juce.com/t/using-a-custom-font-do-i-have-to-bundle-the-ttf-font-file-or-can-juce-embed-it/54828)
- `setDefaultSansSerifTypeface` / `getTypefaceForFont` só funcionam se o look-and-feel estiver definido como padrão (`LookAndFeel::setDefaultLookAndFeel`). — [Forum JUCE, "Modern custom font guide"](https://forum.juce.com/t/modern-custom-font-guide/20841)
- Repaint: manter repaints pequenos; usar `setOpaque(true)` em componentes de medidor (senão o pai é repintado a cada quadro); redesenhar só quando o nível muda; `JUCE_ENABLE_REPAINT_DEBUGGING` em `juce_Config.h` mostra as áreas repintadas. Um Timer alto deixa o loop de eventos ocupado. — [Forum JUCE, "Advice for drawing VU meters"](https://forum.juce.com/t/advice-for-drawing-vu-meters/330); [Forum JUCE, "Repaint terrible performance"](https://forum.juce.com/t/repaint-terrible-performance-why/59808); [Forum JUCE, "Repaint weirdness"](https://forum.juce.com/t/repaint-weirdness/2589)
- Um tópico antigo sugere reduzir a taxa do medidor e pular atualizações com menos de 100 ms desde a última. — [Forum JUCE, "Calling repaint in intervals"](https://forum.juce.com/t/calling-repaint-in-intervals-without-needing-to-use-timer/2628)
- OpenGL no Windows: `OpenGLContext::attachTo` nem sempre acelera desenho 2D no Windows, e pode piorar em macOS. A resposta de fórum sugere Direct2D no Windows e testar build release. Há relatos de drivers Intel problemáticos. — [Forum JUCE, "OpenGL n00b"](https://forum.juce.com/t/opengl-n00b-when-where-why/10985); [Forum JUCE, "Trying to get OpenGL rendering for regular components"](https://forum.juce.com/t/trying-to-get-opengl-rendering-for-regular-components-to-work/34912); [Forum JUCE, "Graphics context rendered in hardware or software?"](https://forum.juce.com/t/graphics-context-rendered-in-hardware-or-software/15906)
- Acessibilidade: `AccessibilityHandler` existe com papel `slider`. Relatos de forum: `createAccessibilityHandler()` é privado na maioria das classes de componente, o que impede handlers customizados; sliders não recebem foco de teclado pelo leitor em alguns casos; Narrator no Windows teve problemas com slider em JUCE 6. JUCE mais recente adicionou controle de teclado para sliders no Win32. — [JUCE, módulo de acessibilidade](https://ccrma.stanford.edu/~jos/juce_modules/group__juce__gui__basics-accessibility.html); [Forum JUCE, "Accessibility follows tabKey, not for Sliders"](https://forum.juce.com/t/accessibility-screen-reader-follows-tabkey-keyboardfocus-but-not-for-sliders/49212); [Forum JUCE, "Accessibility: private createAccessibilityHandler"](https://forum.juce.com/t/accessibility-private-createaccessibilityhandler-causing-an-issue/54843)

### Inferences
- Arquitetura sugerida para a camada `Plugin/` (JUCE): `LookAndFeel_V4` derivado com `drawRotarySlider`, `drawLinearSlider` e fundo; `Timer` (ex.: 30 Hz) só para medidores, lendo valores de atômicos publicados pelo DSP (sem lock e sem alocação no audio thread); `setOpaque(true)` nos medidores; desenho com `Path` cacheado quando a geometria não muda; escala para HiDPI pelo tamanho lógico com `AffineTransform`.
- Redimensionamento: `setResizable` com `setResizeLimits` e escala do conteúdo com `AffineTransform`, desenhando em tamanho base e escalando, evita refazer layout.
- Software rendering é o padrão seguro; OpenGL só deve ser medido em build release com o alvo real antes de adotar.

### Gaps
- Exemplo oficial de `setResizable`/escala com `AffineTransform` e de HiDPI: não lido nesta pesquisa. Usar docs.juce.com da versão fixada no CMakeLists.txt.
- Custo real de repaint em Windows com JUCE 8/9 não medido: os relatos são antigos (2012–2017).

## 5. Acessibilidade e feedback de teclado

### Takeaway
A acessibilidade de sliders em JUCE é limitada e depende da plataforma. Teclado (Tab, setas, Shift) e tooltip de valor são controles que o próprio plugin deve implementar.

### Cited Findings
- Relatos de Narrator, VoiceOver e NVDA variam; NVDA tenta FullDescription antes de HelpText. — [Forum JUCE, "Unable to interact with Sliders on Windows 10 with Narrator"](https://forum.juce.com/t/accessibility-unable-to-interact-with-sliders-on-windows-10-with-default-narrator/49635)
- Para entrada de texto em slider (caixa de valor), o rótulo interno é privado, o que dificulta acesso sem modificar JUCE. — [Forum JUCE, "Accessibility follows tabKey, not for Sliders"](https://forum.juce.com/t/accessibility-screen-reader-follows-tabkey-keyboardfocus-but-not-for-sliders/49212)

### Inferences
- Para o Finale, uma tabela de parâmetros acessível ao teclado (setas ajustam, Shift fino, Home reseta) e um tooltip com valor em texto (LUFS, dB) cobrem a maior parte da necessidade, mesmo sem suporte completo a leitor de tela.

### Gaps
- Padrões de teclado e tooltip de fabricantes (FabFilter, iZotope etc.): não pesquisados.

## Resumo para decisão

- Medidor principal: Integrated LUFS com alvo de gênero (ex.: -14 streaming, -9 CD), Short Term secundário, true peak com teto -1 dBTP visível. Janelas de 400 ms e 3 s confirmadas; ballistics e taxa de atualização ainda precisam da Tech 3341 (seção 2.2).
- Knob: LookAndFeel_V4 com arco de valor por Path, readout no drag, SliderParameterAttachment para ligação ao parâmetro.
- Medidores: Timer ~30 Hz, setOpaque(true), repaint só do medidor, valores via atômicos.
- Fontes: BinaryData + createSystemTypefaceFor, cache estático, testar no Windows, checar licença.
- Pendências: ler Tech 3341 em texto; manuais de iZotope, Sonible, Minimal Audio, Kilohearts, Xfer e Image-Line para convenções de knob e feedback; benchmark próprio de Direct2D/OpenGL/software no Windows.
