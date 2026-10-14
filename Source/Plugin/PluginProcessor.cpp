#include "Plugin/PluginProcessor.h"
#include "Plugin/PluginEditor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

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
    compareParam = parameters.getRawParameterValue (ParamIDs::compare);
    assistantParam = parameters.getRawParameterValue (ParamIDs::assistant);
    targetLufsParam = parameters.getRawParameterValue (ParamIDs::targetLufs);
    genreParam = parameters.getRawParameterValue (ParamIDs::genre);
    eqParam = parameters.getRawParameterValue (ParamIDs::eq);
    tonalAnalyzer.setDecaySeconds (dsp::TonalAssistant::analyzerMemorySeconds);
}

juce::AudioProcessorValueTreeState::ParameterLayout FinaleProcessor::createParameterLayout()
{
    // A ordem dos gêneros é permanente: projetos salvos guardam o índice, não o nome.
    // Deve bater com as linhas de finale::dsp::genreEqTable (o static_assert confere só a quantidade).
    static constexpr std::array<const char*, 8> genreNames { "Trap", "Rap/Boom bap", "Lo-fi", "Funk BR", "House", "EDM",
        "Pop", "Rock" };
    static_assert (genreNames.size() == static_cast<std::size_t> (finale::dsp::numGenres),
        "genreNames deve ter uma entrada por linha de genreEqTable");

    juce::StringArray genreOptions;
    for (const char* name : genreNames)
        genreOptions.add (name);

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
        std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamIDs::genre, 1 }, "Genre", genreOptions, 6),
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
        std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParamIDs::compare, 1 }, "Compare", false),
        // A ordem das opções é permanente: projetos salvos guardam o índice, não o nome.
        std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::assistant, 1 }, "Assistant",
            juce::StringArray { "Off", "Analyze", "Auto", "Freeze" }, 0),
        // EQ é uma quantidade que escala o amount, como compressão e saturação.
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { ParamIDs::eq, 1 }, "EQ",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")),
    };
}

void FinaleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxChunk = std::max (1, samplesPerBlock);
    dryLeft.assign (static_cast<std::size_t> (maxChunk), 0.0f);
    dryRight.assign (static_cast<std::size_t> (maxChunk), 0.0f);
    dryMono.assign (static_cast<std::size_t> (maxChunk), 0.0f);

    const int channels = std::max (1, getTotalNumOutputChannels());

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

    inputMeter.prepare (sampleRate, channels);
    // O teto precisa estar no AbCompare antes do prepare (o limiter interno o usa ao preparar).
    abCompare.setCeilingDecibels (ceilingParam->load());
    abCompare.setCompare (compareParam->load() > 0.5f);
    abCompare.prepare (sampleRate, channels, dsp::Saturator::latency, maxChunk);

    tonalEq.prepare (sampleRate, channels);
    assistant.prepare (sampleRate);
    tonalAnalyzer.prepare (sampleRate);
    tonalAssistant.prepare (sampleRate);
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

    if (maxChunk <= 0)
        return; // sem prepareToPlay: nada pré-alocado para o original

    // Correção do projeto carregado: aplicada aqui para o assistant ser tocado só pelo audio thread.
    const float pending = pendingCorrectionDb.exchange (std::numeric_limits<float>::quiet_NaN());
    if (std::isfinite (pending))
        assistant.setCorrectionDb (pending);

    if (hasPendingTonal.exchange (false, std::memory_order_acquire))
    {
        std::array<float, 4> pendingTonal { };
        for (std::size_t b = 0; b < pendingTonal.size(); ++b)
            pendingTonal[b] = pendingTonalDb[b].load (std::memory_order_relaxed);
        tonalAssistant.setCorrectionDb (pendingTonal);
    }

    const int numSamples = buffer.getNumSamples();
    const int channels = std::min (buffer.getNumChannels(), 2);

    // Valores que valem para o bloco todo: lidos uma vez, fora do laço.
    const auto targets = dsp::mapAmount (amountParam->load() / 100.0f, juce::roundToInt (characterParam->load()),
        compressionParam->load() / 100.0f, saturationParam->load() / 100.0f, widthParam->load() / 100.0f);
    const float gainDb = gainParam->load();
    const float ceilingDb = ceilingParam->load();
    const bool compare = compareParam->load() > 0.5f;
    const auto genreGains = dsp::genreEqGains (
        amountParam->load() / 100.0f, juce::roundToInt (genreParam->load()), eqParam->load() / 100.0f);
    // A correção tonal segue o mesmo amount × EQ da curva do gênero (0 % de EQ desliga as duas).
    const auto unit = [] (float v) { return std::isfinite (v) ? std::clamp (v, 0.0f, 1.0f) : 0.0f; };
    const float tonalScale = unit (amountParam->load() / 100.0f) * unit (eqParam->load() / 100.0f);

    const auto mode
        = static_cast<dsp::LoudnessAssistant::Mode> (std::clamp (juce::roundToInt (assistantParam->load()), 0, 3));
    assistant.setMode (mode);
    tonalAssistant.setMode (mode);
    assistant.setTargetLufs (targetLufsParam->load());

    float* dryBuffers[2] { dryLeft.data(), dryRight.data() };
    float* chunk[2] { nullptr, nullptr };
    const float* dry[2] { nullptr, nullptr };

    // Pedaços de no máximo maxChunk: o original copiado cabe nos buffers pré-alocados.
    for (int start = 0; start < numSamples; start += maxChunk)
    {
        const int n = std::min (maxChunk, numSamples - start);

        // O original é copiado antes do ganho de entrada.
        for (int ch = 0; ch < channels; ++ch)
        {
            chunk[ch] = buffer.getWritePointer (ch, start);
            std::copy_n (buffer.getReadPointer (ch, start), n, dryBuffers[ch]);
            dry[ch] = dryBuffers[ch];
        }

        inputMeter.process (dry, channels, n);

        // Forma tonal do original, antes do EQ: a correção não realimenta a medida.
        const bool tonalValid = channels > 0 && inputMeter.getShortTermLufs() > dsp::TonalAssistant::inputGateLufs;
        if (tonalValid)
        {
            for (int i = 0; i < n; ++i)
                dryMono[static_cast<std::size_t> (i)] = channels == 2 ? 0.5f * (dry[0][i] + dry[1][i]) : dry[0][i];
            tonalAnalyzer.process (dryMono.data(), n);
        }
        tonalAssistant.update (tonalAnalyzer.getShapeDb(), tonalValid, n);

        const auto tonal = tonalAssistant.getCorrectionDb();
        auto eqGains = genreGains;
        for (std::size_t b = 0; b < eqGains.size(); ++b)
        {
            eqGains[b] += tonalScale * tonal[b];
            tonalCorrectionDb[b].store (tonal[b], std::memory_order_relaxed);
        }
        tonalEq.setGainsDecibels (eqGains);

        gain.setGainDecibels (gainDb);
        gain.process (chunk, channels, n);

        tonalEq.process (chunk, channels, n);

        compressor.setRatio (targets.ratio);
        compressor.process (chunk, channels, n);

        saturator.setDriveDecibels (targets.saturationDriveDb);
        saturator.setMix (targets.saturationMix);
        saturator.process (chunk, channels, n);

        stereoWidth.setWidth (targets.width);
        stereoWidth.process (chunk, channels, n);

        push.setGainDecibels (targets.pushDb + assistant.getCorrectionDb());
        push.process (chunk, channels, n);

        limiter.setCeilingDecibels (ceilingDb);
        limiter.process (chunk, channels, n);

        // Mede depois do limiter: o que o usuário ouve é o que conta para o LUFS.
        outputMeter.process (chunk, channels, n);
        outputMomentaryLufs.store (outputMeter.getMomentaryLufs(), std::memory_order_relaxed);
        outputShortTermLufs.store (outputMeter.getShortTermLufs(), std::memory_order_relaxed);
        outputIntegratedLufs.store (outputMeter.getIntegratedLufs(), std::memory_order_relaxed);

        // A malha é fechada na saída, então a correção também compensa o limiter.
        assistant.update (outputMeter.getShortTermLufs(), n, limiter.getGainReductionDb());
        assistantCorrectionDb.store (assistant.getCorrectionDb(), std::memory_order_relaxed);

        // Igualação: o original entra no mesmo LUFS de curto prazo que a saída processada.
        abCompare.setMatchFromLufs (inputMeter.getShortTermLufs(), outputMeter.getShortTermLufs());

        abCompare.setCompare (compare);
        abCompare.setCeilingDecibels (ceilingDb);
        abCompare.process (dry, chunk, channels, n);
    }
}

juce::AudioProcessorEditor* FinaleProcessor::createEditor()
{
    return new FinaleEditor (*this);
}

void FinaleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // A correção não é parâmetro: vai como propriedade extra do estado. Se ainda há uma
    // pendente (projeto carregado sem tocar áudio), ela é a que vale e não a da atomic.
    auto state = parameters.copyState();
    const float pending = pendingCorrectionDb.load();
    const float correction = std::isfinite (pending) ? pending : assistantCorrectionDb.load (std::memory_order_relaxed);
    state.setProperty ("assistantCorrectionDb", correction, nullptr);

    // Correção tonal: uma propriedade por banda. A pendente (projeto aberto sem tocar áudio) vale mais.
    const bool tonalPending = hasPendingTonal.load (std::memory_order_acquire);
    for (std::size_t b = 0; b < tonalCorrectionDb.size(); ++b)
    {
        const float value = tonalPending ? pendingTonalDb[b].load (std::memory_order_relaxed)
                                         : tonalCorrectionDb[b].load (std::memory_order_relaxed);
        state.setProperty ("tonalCorrectionDb" + juce::String (static_cast<int> (b)), value, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void FinaleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes); xml && xml->hasTagName (parameters.state.getType()))
    {
        const auto state = juce::ValueTree::fromXml (*xml);
        if (state.hasProperty ("assistantCorrectionDb"))
            pendingCorrectionDb.store (
                static_cast<float> (static_cast<double> (state.getProperty ("assistantCorrectionDb"))));

        // Projetos de antes do assistente tonal não têm as propriedades: a correção tonal fica em 0.
        bool hasTonal = true;
        for (int b = 0; b < 4; ++b)
            hasTonal = hasTonal && state.hasProperty ("tonalCorrectionDb" + juce::String (b));
        if (hasTonal)
        {
            for (std::size_t b = 0; b < pendingTonalDb.size(); ++b)
                pendingTonalDb[b].store (static_cast<float> (static_cast<double> (state.getProperty (
                                             "tonalCorrectionDb" + juce::String (static_cast<int> (b))))),
                    std::memory_order_relaxed);
            hasPendingTonal.store (true, std::memory_order_release);
        }

        parameters.replaceState (state);
    }
}

} // namespace finale

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new finale::FinaleProcessor();
}
