# Finale — diretrizes do projeto

Este arquivo vale para todo Claude que trabalhar neste repo, em qualquer máquina. Duas pessoas (e dois Claudes) contribuem em paralelo; as regras abaixo existem para que ninguém quebre o trabalho do outro.

## O produto

Finale, da Cremoland: plugin de masterização "de um knob" (VST3 + Standalone, Windows), estilo Soundgoodizer, com um assistente simples que lê a música e ajusta a master para padrões de indústria. Todas as decisões de produto e técnicas estão em `docs/decisoes.md`. Leia antes de propor mudanças de escopo; se algo contradiz esse arquivo, pergunte em vez de assumir.

Cadeia de áudio planejada: EQ tonal → compressão 3 bandas → saturação → largura estéreo → limiter true peak. Presets = gênero/subgênero + caráter (Clean, Warm, Punch, Wide, DOOM).

## Estrutura

- `Source/DSP/`: processamento de áudio em C++ puro, **sem JUCE**. Todo algoritmo novo mora aqui e tem teste.
- `Source/Plugin/`: integração com o JUCE (processor, parâmetros, estado, editor).
- `tests/`: testes Catch2 do DSP.
- `tools/`: ferramentas offline (ex: gerar curvas de gênero a partir de referências).
- `docs/decisoes.md`: fonte da verdade das decisões.
- Nome do produto e do estúdio ficam **só** no topo do `CMakeLists.txt`. Nunca escreva "Finale" ou "Cremoland" à mão no código; use `JucePlugin_Name`.

## Compilar e testar

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

O VST3 sai em `build/finale_plugin_artefacts/Release/VST3/`. JUCE e Catch2 são baixados pelo CMake nas versões fixadas no `CMakeLists.txt`; não atualize essas versões num PR de feature, e sim num PR próprio.

Antes de commitar: `clang-format -i` nos arquivos alterados (versão 22.1.8, instalada com `pip install clang-format==22.1.8`). O CI rejeita código fora do formato.

## Regras do audio thread (`processBlock` e tudo que ele chama)

- Nada de alocação (`new`, `std::vector::push_back`, `juce::String`, …), locks, I/O ou log.
- Parâmetros são lidos via `std::atomic` (`getRawParameterValue`); mudanças de parâmetro são suavizadas para não gerar clique.
- Todo processamento precisa funcionar em qualquer sample rate e tamanho de bloco, inclusive bloco de 1 amostra e bloco de 0.
- A saída nunca pode ter NaN/infinito e nunca passa do teto de true peak configurado.
- Latência adicionada precisa ser reportada com `setLatencySamples`.

## Parâmetros e compatibilidade

- IDs de parâmetro (`ParamIDs`) são permanentes: projetos salvos dos usuários dependem deles. Nunca renomeie nem remova; adicione novos.
- Mudança que altera o som de um preset existente é mencionada no PR.

## Fluxo de trabalho (obrigatório)

1. Toda tarefa tem uma Issue. Antes de começar, veja se alguém já está atribuído; se não, atribua-se (`gh issue edit <n> --add-assignee @me`).
2. Crie uma branch a partir da `main` atualizada: `git fetch origin && git switch -c <tipo>/<n>-<descricao> origin/main` (tipo: `feat`, `fix`, `dsp`, `ui`, `ci`, `docs`).
3. PRs pequenos: uma coisa por PR, idealmente horas de trabalho, não semanas.
4. **Nunca** commite nem dê push direto na `main`. Nunca use `--force` e nunca reescreva o histórico de uma branch que o outro também usa.
5. Antes de abrir o PR: `git fetch origin && git merge origin/main`, compile, rode os testes e o clang-format.
6. Abra o PR com `gh pr create`, ligando a Issue (`Closes #n`) e preenchendo o template.
7. Merge só com o CI verde (build, testes de DSP, pluginval e formatação). Não é preciso aprovação do outro, mas revise o próprio diff antes.
8. Se o CI falhar, corrija a causa. Não desative, pule nem afrouxe teste para passar.

## Estilo de código

- C++20, namespace `finale` (DSP em `finale::dsp`).
- Código e identificadores em inglês; comentários, Issues, PRs e commits em português.
- Comente o *porquê*, não o *o quê*. Em DSP, cite a fonte da fórmula quando houver.
- Prefira classes JUCE (`juce::dsp::*`) na camada `Plugin/`; em `DSP/`, só a biblioteca padrão.

## Músicas de referência

WAVs/MP3s de referência ficam na pasta `references/` de cada máquina, que é ignorada pelo git. Nunca commite áudio comercial. Só as curvas geradas (JSON pequeno) entram no repo.
