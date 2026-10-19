#pragma once

#include "Plugin/LoudnessPanel.h"
#include "Plugin/PluginProcessor.h"
#include "Plugin/Widgets.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace finale
{

// Janela da interface Marfim: layout do protótipo v9, com o Avançado recolhido ou aberto.
class FinaleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FinaleEditor (FinaleProcessor&);
    ~FinaleEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setAdvancedOpen (bool open);
    juce::String presetSummary() const;

    // Botão "Avançado": sem desenho próprio, o texto e o caret saem do paint do editor.
    class AdvancedToggle : public juce::Button
    {
    public:
        AdvancedToggle() :
            juce::Button ("Avancado")
        {
        }
        void paintButton (juce::Graphics&, bool, bool) override { }
    };

    FinaleProcessor& processor;

    AmountKnob knob;
    ChoiceSelector genreSelector;
    ChoiceSelector characterSelector;
    ChoiceSelector assistantSelector;
    ChoiceSelector compareSelector;
    ChoiceSelector manualTargetSelector;
    LoudnessPanel loudness;
    MiniControl eqControl;
    MiniControl compressionControl;
    MiniControl saturationControl;
    MiniControl widthControl;
    MiniControl lufsOffsetControl;
    MiniControl ceilingControl;
    AdvancedToggle advancedToggle;

    // Parâmetros lidos no timer. Os ponteiros vêm do APVTS e só são lidos, nunca escritos.
    std::atomic<float>* genreIndexParam = nullptr;
    std::atomic<float>* characterIndexParam = nullptr;
    std::atomic<float>* assistantModeParam = nullptr;
    std::atomic<float>* ceilingParam = nullptr;
    juce::StringArray genreNames;
    juce::StringArray characterNames;

    // Textos já pintados: o timer só repinta a área quando o texto muda.
    juce::String lastStatusKey;
    juce::String lastPresetSummary;

    bool advancedOpen = false;

    // Áreas calculadas no resized e usadas no paint e no timer.
    juce::Rectangle<int> assistantStatusArea;
    juce::Rectangle<int> compareSubArea;
    juce::Rectangle<int> presetArea;
    juce::Rectangle<int> presetHeaderArea;
    juce::Rectangle<int> meterArea;
    juce::Rectangle<int> advancedPanelArea;
    juce::Rectangle<int> manualRowArea;
    juce::Rectangle<int> footerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FinaleEditor)
};

} // namespace finale
