# Finale

Plugin de masterização de um knob, da Cremoland. VST3 + Standalone para Windows.

Decisões do projeto: [docs/decisoes.md](docs/decisoes.md). Regras de colaboração: [CLAUDE.md](CLAUDE.md).

## Preparar a máquina (uma vez)

1. Visual Studio 2022 com a carga de trabalho **Desenvolvimento para desktop com C++**
2. [CMake](https://cmake.org/download/) 3.22 ou mais novo e [Git](https://git-scm.com/)
3. [GitHub CLI](https://cli.github.com/) e `gh auth login`
4. `pip install clang-format==22.1.8`

## Compilar

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Pra testar na DAW, copie `build/finale_plugin_artefacts/Release/VST3/Finale.vst3` para `C:\Program Files\Common Files\VST3\` e reescaneie os plugins. Cada PR também gera um build pronto pra baixar na aba **Actions** do GitHub.
