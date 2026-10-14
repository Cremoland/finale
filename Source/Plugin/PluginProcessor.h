#pragma once

#include "DSP/AbCompare.h"
#include "DSP/Gain.h"
#include "DSP/LoudnessAssistant.h"
#include "DSP/LoudnessMeter.h"
#include "DSP/Macro.h"
#include "DSP/MultibandCompressor.h"
#include "DSP/Saturator.h"
#include "DSP/StereoWidth.h"
#include "DSP/TruePeakLimiter.h"

#include <juce_audio_processors/juce_audio_processors.h>

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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::atomic<float>* gainParam = nullptr;
    dsp::Gain gain;
    std::atomic<float>* amountParam = nullptr;
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
    dsp::LoudnessAssistant assistant;
    std::atomic<float> assistantCorrectionDb { 0.0f };
    // Cópia do original por pedaço, pré-alocada: processBlock não aloca.
    std::vector<float> dryLeft, dryRight;
    int maxChunk = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FinaleProcessor)
};

} // namespace finale
