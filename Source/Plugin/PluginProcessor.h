#pragma once

#include "DSP/AbCompare.h"
#include "DSP/BandAnalyzer.h"
#include "DSP/Gain.h"
#include "DSP/GenreCurves.h"
#include "DSP/LoudnessAssistant.h"
#include "DSP/LoudnessMeter.h"
#include "DSP/Macro.h"
#include "DSP/MultibandCompressor.h"
#include "DSP/Saturator.h"
#include "DSP/StereoWidth.h"
#include "DSP/TonalAssistant.h"
#include "DSP/TonalEq.h"
#include "DSP/TruePeakLimiter.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <limits>
#include <vector>

namespace finale
{

namespace ParamIDs
{
inline constexpr auto gain = "gain";
inline constexpr auto amount = "amount";
inline constexpr auto ceiling = "ceiling";
inline constexpr auto targetLufs = "targetLufs";
inline constexpr auto genre = "genre";
inline constexpr auto character = "character";
inline constexpr auto compression = "compression";
inline constexpr auto saturation = "saturation";
inline constexpr auto width = "width";
inline constexpr auto compare = "compare";
inline constexpr auto assistant = "assistant";
inline constexpr auto eq = "eq";
inline constexpr auto lufsOffset = "lufsOffset";
inline constexpr auto manualTarget = "manualTarget";
} // namespace ParamIDs

class FinaleProcessor : public juce::AudioProcessor
{
public:
    FinaleProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override { }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override { }
    const juce::String getProgramName (int) override { return { }; }
    void changeProgramName (int, const juce::String&) override { }

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;

    // Leituras de LUFS da saída, para a interface. O editor lê de outra thread, por isso atomic.
    float getOutputMomentaryLufs() const { return outputMomentaryLufs.load (std::memory_order_relaxed); }
    float getOutputShortTermLufs() const { return outputShortTermLufs.load (std::memory_order_relaxed); }
    float getOutputIntegratedLufs() const { return outputIntegratedLufs.load (std::memory_order_relaxed); }

    // Correção atual do assistente em dB, para a interface. O editor lê de outra thread, por isso atomic.
    float getAssistantCorrectionDb() const { return assistantCorrectionDb.load (std::memory_order_relaxed); }

    // Alvo de loudness em uso (gênero + ajuste, ou o manual), para a interface. O editor lê de outra thread.
    float getLoudnessTargetLufs() const { return loudnessTargetLufs.load (std::memory_order_relaxed); }

    // Correção tonal atual do assistente em dB por banda (90 Hz, 300 Hz, 3 kHz, 10 kHz), antes da escala de
    // amount e EQ. O editor lê de outra thread, por isso atomic.
    std::array<float, 4> getTonalCorrectionDb() const
    {
        std::array<float, 4> out { };
        for (std::size_t b = 0; b < out.size(); ++b)
            out[b] = tonalCorrectionDb[b].load (std::memory_order_relaxed);
        return out;
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::atomic<float>* gainParam = nullptr;
    dsp::Gain gain;
    std::atomic<float>* amountParam = nullptr;
    std::atomic<float>* genreParam = nullptr;
    std::atomic<float>* eqParam = nullptr;
    dsp::TonalEq tonalEq;
    std::atomic<float>* compressionParam = nullptr;
    dsp::MultibandCompressor compressor;
    std::atomic<float>* characterParam = nullptr;
    std::atomic<float>* saturationParam = nullptr;
    dsp::Saturator saturator;
    std::atomic<float>* widthParam = nullptr;
    dsp::StereoWidth stereoWidth;
    dsp::Gain push;
    std::atomic<float>* ceilingParam = nullptr;
    dsp::TruePeakLimiter limiter;
    dsp::LoudnessMeter outputMeter;
    std::atomic<float> outputMomentaryLufs { dsp::LoudnessMeter::floorLufs };
    std::atomic<float> outputShortTermLufs { dsp::LoudnessMeter::floorLufs };
    std::atomic<float> outputIntegratedLufs { dsp::LoudnessMeter::floorLufs };

    std::atomic<float>* compareParam = nullptr;
    dsp::LoudnessMeter inputMeter; // LUFS do original, para igualar o A/B
    dsp::AbCompare abCompare;

    std::atomic<float>* assistantParam = nullptr;
    std::atomic<float>* targetLufsParam = nullptr;
    std::atomic<float>* lufsOffsetParam = nullptr;
    std::atomic<float>* manualTargetParam = nullptr;
    std::atomic<float> loudnessTargetLufs { -9.0f };
    dsp::LoudnessAssistant assistant;
    std::atomic<float> assistantCorrectionDb { 0.0f };
    // Correção lida do projeto, esperando o audio thread aplicá-la (NaN = nada pendente).
    // Só o processBlock mexe no assistant; o estado chega pela thread de mensagens.
    std::atomic<float> pendingCorrectionDb { std::numeric_limits<float>::quiet_NaN() };
    // Assistente tonal: mede o original (mono, antes do EQ) e corrige os ganhos do EQ tonal.
    dsp::BandAnalyzer tonalAnalyzer;
    dsp::TonalAssistant tonalAssistant;
    std::array<std::atomic<float>, 4> tonalCorrectionDb { };
    // Correção tonal lida do projeto, esperando o audio thread (vale quando hasPendingTonal é true).
    std::array<std::atomic<float>, 4> pendingTonalDb { };
    std::atomic<bool> hasPendingTonal { false };
    std::vector<float> dryMono; // mono do original por pedaço, pré-alocado
    // Cópia do original por pedaço, pré-alocada: processBlock não aloca.
    std::vector<float> dryLeft, dryRight;
    int maxChunk = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FinaleProcessor)
};

} // namespace finale
