#include "Plugin/PluginEditor.h"
#include "Plugin/Theme.h" // cores, fontes e peças visuais da interface

namespace finale
{

FinaleEditor::FinaleEditor (FinaleProcessor& p) :
    AudioProcessorEditor (p),
    attachment (p.parameters, ParamIDs::gain, knob)
{
    addAndMakeVisible (knob);
    setSize (360, 360);
}

void FinaleEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121214));
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (22.0f));
    g.drawText (JucePlugin_Name, getLocalBounds().removeFromTop (60), juce::Justification::centred);
}

void FinaleEditor::resized()
{
    knob.setBounds (getLocalBounds().reduced (60).withTrimmedTop (20));
}

} // namespace finale
