cmake_minimum_required(VERSION 3.22)

# Nome do produto e do estúdio: único lugar onde ficam definidos.
set(FINALE_PRODUCT_NAME "Finale")
set(FINALE_COMPANY_NAME "Cremoland")

project(Finale VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Runtime estático no Windows: o usuário não precisa instalar o VC++ Redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(FINALE_BUILD_PLUGIN "Compila o plugin (precisa do JUCE)" ON)
option(FINALE_BUILD_TESTS "Compila os testes de DSP" ON)

include(FetchContent)

# DSP puro em C++, sem JUCE: testável sem abrir a DAW.
add_library(finale_dsp INTERFACE)
target_include_directories(finale_dsp INTERFACE Source)

if(FINALE_BUILD_PLUGIN)
    # Versão do JUCE fixada: os dois (e o CI) compilam exatamente com a mesma.
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.3
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(finale_plugin
        PRODUCT_NAME "${FINALE_PRODUCT_NAME}"
        COMPANY_NAME "${FINALE_COMPANY_NAME}"
        PLUGIN_MANUFACTURER_CODE Crml
        PLUGIN_CODE Fnle
        FORMATS VST3 Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        VST3_CATEGORIES Fx Mastering
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(finale_plugin PRIVATE
        Source/Plugin/PluginProcessor.cpp
        Source/Plugin/PluginEditor.cpp)

    target_compile_definitions(finale_plugin PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(finale_plugin
        PRIVATE
            finale_dsp
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

if(FINALE_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.16.0
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(Catch2)

    enable_testing()
    add_executable(finale_tests
        tests/GainTests.cpp
        tests/TruePeakDetectorTests.cpp
        tests/TruePeakLimiterTests.cpp
        tests/KWeightingTests.cpp
        tests/LoudnessMeterTests.cpp
        tests/CrossoverTests.cpp
        tests/MultibandCompressorTests.cpp
        tests/OversamplerTests.cpp
        tests/SaturatorTests.cpp
        tests/StereoWidthTests.cpp
        tests/MacroTests.cpp
        tests/AbCompareTests.cpp
        tests/LoudnessAssistantTests.cpp
        tests/TonalEqTests.cpp)
    target_link_libraries(finale_tests PRIVATE finale_dsp Catch2::Catch2WithMain)
    add_test(NAME dsp COMMAND finale_tests)
endif()
