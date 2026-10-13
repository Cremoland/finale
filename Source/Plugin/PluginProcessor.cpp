#include "Plugin/PluginProcessor.h"
#include "Plugin/PluginEditor.h"

namespace finale
{

FinaleProcessor::FinaleProcessor() :
    AudioProcessor (BusesProperties()
            .withInput ("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
    parameters (*this, nullptr, "FinaleState", createParameterLayout())
{
    gainParam = parameters.getRawParameterValue (ParamIDs::gain);
}

juce::AudioProcessorValueTreeState::ParameterLayout FinaleProcessor::createParameterLayout()
{
    return {
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamIDs::gain, 1 }, "Gain",
            juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")),
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamIDs::amount, 1 }, "Amount",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")),
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamIDs::ceiling, 1 }, "Ceiling",
            juce::NormalisableRange<float> (-6.0f, 0.0f, 0.1f), -1.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dBTP")),
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamIDs::targetLufs, 1 }, "Target LUFS",
            juce::NormalisableRange<float> (-16.0f, -5.0f, 0.5f), -9.0f,
            juce::AudioParameterFloatAttributes().withLabel ("LUFS")),
        // A ordem das opções é permanente: projetos salvos guardam o índice, não o nome.
        std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::genre, 1 }, "Genre",
            juce::StringArray { "Trap", "Rap/Boom bap", "Lo-fi", "Funk BR", "House", "EDM", "Pop", "Rock" }, 6),
        // A ordem das opções é permanente: projetos salvos guardam o índice, não o nome.
        std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::character, 1 }, "Character",
            juce::StringArray { "Clean", "Warm", "Punch", "Wide", "DOOM" }, 0),
    };
}

void FinaleProcessor::prepareToPlay (double sampleRate, int)
{
    gain.setGainDecibels (gainParam->load());
    gain.prepare (sampleRate);
}

bool FinaleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
        && out == layouts.getMainInputChannelSet();
}

void FinaleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    gain.setGainDecibels (gainParam->load());
    gain.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* FinaleProcessor::createEditor()
{
    return new FinaleEditor (*this);
}

void FinaleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FinaleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes); xml && xml->hasTagName (parameters.state.getType()))
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

} // namespace finale

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new finale::FinaleProcessor();
}
