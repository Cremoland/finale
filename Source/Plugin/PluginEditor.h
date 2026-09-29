#pragma once

#include "Plugin/PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace finale
{

// Interface provisória: um knob. O design de verdade vem depois.
class FinaleEditor : public juce::AudioProcessorEditor
{
public:
    explicit FinaleEditor (FinaleProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider knob { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FinaleEditor)
};

} // namespace finale
