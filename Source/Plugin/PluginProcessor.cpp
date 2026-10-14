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
    amountParam = parameters.getRawParameterValue (ParamIDs::amount);
    ceilingParam = parameters.getRawParameterValue (ParamIDs::ceiling);
    compressionParam = parameters.getRawParameterValue (ParamIDs::compression);
    characterParam = parameters.getRawParameterValue (ParamIDs::character);
    saturationParam = parameters.getRawParameterValue (ParamIDs::saturation);
    widthParam = parameters.getRawParameterValue (ParamIDs::width);
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
        // Compressão e saturação são quantidades que escalam o amount (100 % = alvo do caráter).
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamIDs::compression, 1 }, "Compression",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")),
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamIDs::saturation, 1 }, "Saturation",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")),
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamIDs::width, 1 }, "Width",
            juce::NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")),
    };
}

void FinaleProcessor::prepareToPlay (double sampleRate, int)
{
    gain.setGainDecibels (gainParam->load());
    gain.prepare (sampleRate);

    compressor.prepare (sampleRate, std::max (1, getTotalNumOutputChannels()));
    limiter.prepare (sampleRate, std::max (1, getTotalNumOutputChannels()));
    saturator.prepare (sampleRate, std::max (1, getTotalNumOutputChannels()));
    stereoWidth.setWidth (1.0f); // a largura real chega no primeiro bloco, com suavização
    stereoWidth.prepare (sampleRate);
    push.setGainDecibels (0.0f);
    push.prepare (sampleRate);
    outputMeter.prepare (sampleRate, std::max (1, getTotalNumOutputChannels()));
    limiter.setCeilingDecibels (ceilingParam->load());
    setLatencySamples (limiter.getLatencySamples() + dsp::Saturator::latency);
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

    const auto targets = dsp::mapAmount (amountParam->load() / 100.0f, juce::roundToInt (characterParam->load()),
        compressionParam->load() / 100.0f, saturationParam->load() / 100.0f, widthParam->load() / 100.0f);

    compressor.setRatio (targets.ratio);
    compressor.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());

    saturator.setDriveDecibels (targets.saturationDriveDb);
    saturator.setMix (targets.saturationMix);
    saturator.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());

    stereoWidth.setWidth (targets.width);
    stereoWidth.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());

    push.setGainDecibels (targets.pushDb);
    push.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());

    limiter.setCeilingDecibels (ceilingParam->load());
    limiter.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());

    // Mede depois do limiter: o que o usuário ouve é o que conta para o LUFS.
    outputMeter.process (buffer.getArrayOfReadPointers(), buffer.getNumChannels(), buffer.getNumSamples());
    outputMomentaryLufs.store (outputMeter.getMomentaryLufs(), std::memory_order_relaxed);
    outputShortTermLufs.store (outputMeter.getShortTermLufs(), std::memory_order_relaxed);
    outputIntegratedLufs.store (outputMeter.getIntegratedLufs(), std::memory_order_relaxed);
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
