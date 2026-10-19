#include "DSP/Saturator.h"
#include "DSP/TonalAssistant.h"
#include "DSP/TruePeakDetector.h"
#include "DSP/TruePeakLimiter.h"
#include "Plugin/PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <random>
#include <vector>

using Catch::Approx;
using finale::FinaleProcessor;

namespace
{

void setParam (FinaleProcessor& p, const char* id, float value)
{
    auto* param = p.parameters.getParameter (id);
    REQUIRE (param != nullptr);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

float rawParam (FinaleProcessor& p, const char* id)
{
    return p.parameters.getRawParameterValue (id)->load();
}

float toDb (float linear)
{
    return 20.0f * std::log10 (std::max (linear, 1.0e-12f));
}

struct RunResult
{
    bool finite = true;
    float maxTruePeakDb = -240.0f;
};

// Ruído gaussiano em blocos com os tamanhos dados (em ciclo), por `seconds` segundos.
// O processor já precisa estar preparado. Mede o true peak da saída com um detector próprio.
RunResult run (FinaleProcessor& p, int channels, double sampleRate, float amplitude, double seconds,
    std::initializer_list<int> blockSizes, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> noise (0.0f, amplitude);
    finale::dsp::TruePeakDetector detector;
    detector.prepare (channels);
    juce::MidiBuffer midi;
    RunResult result;
    float maxPeak = 0.0f;

    const auto total = static_cast<long long> (seconds * sampleRate);
    long long done = 0;
    while (done < total)
    {
        for (int size : blockSizes)
        {
            juce::AudioBuffer<float> buffer (channels, size);
            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < size; ++i)
                    buffer.setSample (ch, i, noise (rng));

            p.processBlock (buffer, midi);

            for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < size; ++i)
                {
                    const float s = buffer.getSample (ch, i);
                    result.finite = result.finite && std::isfinite (s);
                    maxPeak = std::max (maxPeak, detector.push (ch, s));
                }
            done += size;
        }
    }
    result.maxTruePeakDb = toDb (maxPeak);
    return result;
}

} // namespace

TEST_CASE ("parâmetros: IDs e padrões permanentes")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor p;

    struct Expected
    {
        const char* id;
        float value;
    };
    // Projetos salvos dependem destes IDs e padrões: mudar um deles quebra o projeto do usuário.
    const std::vector<Expected> expected { { finale::ParamIDs::gain, 0.0f }, { finale::ParamIDs::amount, 50.0f },
        { finale::ParamIDs::ceiling, -1.0f }, { finale::ParamIDs::targetLufs, -9.0f },
        { finale::ParamIDs::genre, 6.0f }, { finale::ParamIDs::character, 0.0f },
        { finale::ParamIDs::compression, 100.0f }, { finale::ParamIDs::saturation, 100.0f },
        { finale::ParamIDs::width, 100.0f }, { finale::ParamIDs::compare, 0.0f }, { finale::ParamIDs::assistant, 0.0f },
        { finale::ParamIDs::eq, 100.0f }, { finale::ParamIDs::lufsOffset, 0.0f },
        { finale::ParamIDs::manualTarget, 0.0f } };

    CHECK (p.getParameters().size() == static_cast<int> (expected.size()));
    for (const auto& e : expected)
    {
        INFO (e.id);
        REQUIRE (p.parameters.getParameter (e.id) != nullptr);
        CHECK (rawParam (p, e.id) == Approx (e.value));
    }
}

TEST_CASE ("latência reportada = limiter + saturação")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor p;

    for (double sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        INFO (sampleRate);
        p.prepareToPlay (sampleRate, 512);
        finale::dsp::TruePeakLimiter reference;
        reference.prepare (sampleRate, 2);
        CHECK (p.getLatencySamples() == reference.getLatencySamples() + finale::dsp::Saturator::latency);
    }

    p.prepareToPlay (48000.0, 512);
    CHECK (p.getLatencySamples() == 132);
}

TEST_CASE ("saída finita e abaixo do teto com qualquer bloco, caráter e A/B")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    for (float ceiling : { -1.0f, -3.0f })
        for (int character = 0; character < 5; ++character)
            for (bool compare : { false, true })
            {
                INFO ("teto " << ceiling << " caráter " << character << " A/B " << compare);
                FinaleProcessor p;
                setParam (p, finale::ParamIDs::gain, 12.0f); // entrada bem quente: o limiter tem trabalho
                setParam (p, finale::ParamIDs::amount, 100.0f);
                setParam (p, finale::ParamIDs::ceiling, ceiling);
                setParam (p, finale::ParamIDs::character, static_cast<float> (character));
                setParam (p, finale::ParamIDs::compare, compare ? 1.0f : 0.0f);
                p.prepareToPlay (48000.0, 512);

                // 0 e 1 amostra, maior que o preparado (o host pode mandar) e um tamanho irregular.
                const auto result = run (p, 2, 48000.0, 0.5f, 2.0, { 512, 1, 0, 2048, 37 });
                CHECK (result.finite);
                CHECK (result.maxTruePeakDb <= ceiling + 0.1f);
            }
}

TEST_CASE ("saída abaixo do teto em 44,1 e 96 kHz")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    for (double sampleRate : { 44100.0, 96000.0 })
    {
        INFO ("taxa " << sampleRate);
        FinaleProcessor p;
        setParam (p, finale::ParamIDs::gain, 12.0f);
        setParam (p, finale::ParamIDs::ceiling, -1.0f);
        setParam (p, finale::ParamIDs::amount, 100.0f);
        setParam (p, finale::ParamIDs::character, 4.0f); // DOOM: o caso mais quente
        setParam (p, finale::ParamIDs::compare, 1.0f);
        p.prepareToPlay (sampleRate, 512);

        const auto result = run (p, 2, sampleRate, 0.5f, 2.0, { 512, 1, 37 });
        CHECK (result.finite);
        CHECK (result.maxTruePeakDb <= -1.0f + 0.1f);
    }
}

TEST_CASE ("processBlock sem prepareToPlay não mexe no áudio")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor p;
    juce::AudioBuffer<float> buffer (2, 64);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 64; ++i)
            buffer.setSample (ch, i, 0.5f);
    juce::MidiBuffer midi;

    p.processBlock (buffer, midi);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 64; ++i)
            CHECK (buffer.getSample (ch, i) == 0.5f);
}

TEST_CASE ("layouts: mono e estéreo, entrada igual à saída")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor p;

    const auto layout = [] (const juce::AudioChannelSet& in, const juce::AudioChannelSet& out)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (in);
        l.outputBuses.add (out);
        return l;
    };
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();

    CHECK (p.isBusesLayoutSupported (layout (stereo, stereo)));
    CHECK (p.isBusesLayoutSupported (layout (mono, mono)));
    CHECK_FALSE (p.isBusesLayoutSupported (layout (mono, stereo)));
    CHECK_FALSE (p.isBusesLayoutSupported (layout (stereo, mono)));

    // Em mono a cadeia inteira também precisa respeitar o teto.
    REQUIRE (p.setBusesLayout (layout (mono, mono)));
    setParam (p, finale::ParamIDs::gain, 12.0f);
    p.prepareToPlay (48000.0, 512);
    const auto result = run (p, 1, 48000.0, 0.5f, 2.0, { 512, 1, 37 });
    CHECK (result.finite);
    CHECK (result.maxTruePeakDb <= -1.0f + 0.1f);
}

TEST_CASE ("estado: parâmetros salvos e restaurados")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor a;
    setParam (a, finale::ParamIDs::amount, 80.0f);
    setParam (a, finale::ParamIDs::genre, 2.0f);
    setParam (a, finale::ParamIDs::character, 4.0f);
    setParam (a, finale::ParamIDs::assistant, 3.0f);
    setParam (a, finale::ParamIDs::targetLufs, -12.0f);
    setParam (a, finale::ParamIDs::ceiling, -2.5f);
    setParam (a, finale::ParamIDs::width, 150.0f);
    setParam (a, finale::ParamIDs::compare, 1.0f);
    setParam (a, finale::ParamIDs::lufsOffset, -2.0f);
    setParam (a, finale::ParamIDs::manualTarget, 1.0f);

    juce::MemoryBlock state;
    a.getStateInformation (state);
    FinaleProcessor b;
    b.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    for (const char* id : { finale::ParamIDs::gain, finale::ParamIDs::amount, finale::ParamIDs::ceiling,
             finale::ParamIDs::targetLufs, finale::ParamIDs::genre, finale::ParamIDs::character,
             finale::ParamIDs::compression, finale::ParamIDs::saturation, finale::ParamIDs::width,
             finale::ParamIDs::compare, finale::ParamIDs::assistant, finale::ParamIDs::eq, finale::ParamIDs::lufsOffset,
             finale::ParamIDs::manualTarget })
    {
        INFO (id);
        CHECK (rawParam (b, id) == Approx (rawParam (a, id)).margin (1.0e-4));
    }
}

TEST_CASE ("assistente Auto puxa o loudness para o alvo")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor p;
    setParam (p, finale::ParamIDs::assistant, 2.0f); // Auto
    p.prepareToPlay (48000.0, 512);

    // Ruído 0,05 sai em ~−17 LUFS sem correção (medido no spike); o alvo padrão (Pop) é −9,5.
    const auto result = run (p, 2, 48000.0, 0.05f, 60.0, { 512 });

    CHECK (result.finite);
    CHECK (p.getAssistantCorrectionDb() > 5.0f);
    CHECK (p.getOutputShortTermLufs() == Approx (-9.5f).margin (1.5f));
}

TEST_CASE ("estado: correção do assistente sobrevive ao salvar e abrir o projeto")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor a;
    setParam (a, finale::ParamIDs::assistant, 2.0f); // Auto
    a.prepareToPlay (48000.0, 512);
    run (a, 2, 48000.0, 0.05f, 30.0, { 512 });
    const float correction = a.getAssistantCorrectionDb();
    REQUIRE (correction > 2.0f);

    // Congelado: ao abrir o projeto, a correção salva tem que valer sem reanalisar.
    setParam (a, finale::ParamIDs::assistant, 3.0f);
    juce::MemoryBlock state;
    a.getStateInformation (state);

    FinaleProcessor b;
    b.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    // Projeto aberto e salvo de novo antes de tocar áudio: a correção pendente é a que vai.
    juce::MemoryBlock resaved;
    b.getStateInformation (resaved);

    for (auto* saved : { &state, &resaved })
    {
        FinaleProcessor c;
        c.setStateInformation (saved->getData(), static_cast<int> (saved->getSize()));
        c.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> silence (2, 512);
        silence.clear();
        juce::MidiBuffer midi;
        c.processBlock (silence, midi);
        CHECK (c.getAssistantCorrectionDb() == Approx (correction).margin (1.0e-4));
    }
}

namespace
{

// Ruído "abafado": gaussiano passando por dois passa-baixas de um polo em 300 Hz (sobra pouco agudo).
// Processa `seconds` em blocos de 512 a 48 kHz.
void runDull (FinaleProcessor& p, double seconds)
{
    std::mt19937 rng (3);
    std::normal_distribution<float> noise (0.0f, 0.5f);
    const float a = std::exp (-2.0f * 3.14159265f * 300.0f / 48000.0f);
    float s1 = 0.0f, s2 = 0.0f;
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    const int blocks = static_cast<int> (seconds * 48000.0 / 512.0);
    for (int blk = 0; blk < blocks; ++blk)
    {
        for (int i = 0; i < 512; ++i)
        {
            s1 = (1.0f - a) * noise (rng) + a * s1;
            s2 = (1.0f - a) * s1 + a * s2;
            buffer.setSample (0, i, s2);
            buffer.setSample (1, i, s2);
        }
        p.processBlock (buffer, midi);
    }
}

} // namespace

TEST_CASE ("assistente tonal: Off não corrige")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor p;
    p.prepareToPlay (48000.0, 512);
    runDull (p, 10.0);
    for (float c : p.getTonalCorrectionDb())
        CHECK (c == 0.0f);
}

TEST_CASE ("assistente tonal: Auto abre o agudo de uma mix abafada")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor p;
    setParam (p, finale::ParamIDs::assistant, 2.0f); // Auto
    p.prepareToPlay (48000.0, 512);
    runDull (p, 30.0);

    const auto correction = p.getTonalCorrectionDb();
    CHECK (correction[3] > 3.0f); // mais agudo
    CHECK (correction[0] < -3.0f); // menos grave
    for (float c : correction)
        CHECK (std::abs (c) <= finale::dsp::TonalAssistant::maxCorrectionDb);
}

TEST_CASE ("estado: correção tonal sobrevive ao salvar e abrir o projeto")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor a;
    setParam (a, finale::ParamIDs::assistant, 2.0f); // Auto
    a.prepareToPlay (48000.0, 512);
    runDull (a, 30.0);
    const auto correction = a.getTonalCorrectionDb();
    REQUIRE (correction[3] > 3.0f);

    setParam (a, finale::ParamIDs::assistant, 3.0f); // Congelar
    juce::MemoryBlock state;
    a.getStateInformation (state);

    FinaleProcessor b;
    b.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    juce::MemoryBlock resaved;
    b.getStateInformation (resaved);

    for (auto* saved : { &state, &resaved })
    {
        FinaleProcessor c;
        c.setStateInformation (saved->getData(), static_cast<int> (saved->getSize()));
        c.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> silence (2, 512);
        silence.clear();
        juce::MidiBuffer midi;
        c.processBlock (silence, midi);

        const auto restored = c.getTonalCorrectionDb();
        for (std::size_t band = 0; band < 4; ++band)
            CHECK (restored[band] == Approx (correction[band]).margin (1.0e-4));
    }
}

TEST_CASE ("estado: projeto antigo zera a correção tonal")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor a;
    setParam (a, finale::ParamIDs::assistant, 2.0f); // Auto
    a.prepareToPlay (48000.0, 512);
    runDull (a, 30.0);
    REQUIRE (a.getTonalCorrectionDb()[3] > 3.0f);

    // Congelar antes de salvar: o silêncio não move a correção, então um zero depois prova o reset.
    setParam (a, finale::ParamIDs::assistant, 3.0f);
    juce::MemoryBlock state;
    a.getStateInformation (state);

    // Estado de projeto antigo: sem as propriedades da correção tonal.
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));
    REQUIRE (xml != nullptr);
    for (int b = 0; b < 4; ++b)
        xml->removeAttribute ("tonalCorrectionDb" + juce::String (b));
    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary (*xml, oldState);

    // Mesma instância: a correção de antes não pode sobrar.
    a.setStateInformation (oldState.getData(), static_cast<int> (oldState.getSize()));
    juce::AudioBuffer<float> silence (2, 512);
    silence.clear();
    juce::MidiBuffer midi;
    a.processBlock (silence, midi);

    for (float c : a.getTonalCorrectionDb())
        CHECK (c == 0.0f);
}

namespace
{

// Processa um bloco curto de silêncio e devolve o alvo de loudness em uso.
float targetAfterBlock (FinaleProcessor& p)
{
    juce::AudioBuffer<float> silence (2, 64);
    silence.clear();
    juce::MidiBuffer midi;
    p.processBlock (silence, midi);
    return p.getLoudnessTargetLufs();
}

// Estado salvo sem os filhos PARAM dos IDs dados nem as propriedades dadas: imita um projeto antigo.
juce::MemoryBlock withoutParts (const juce::MemoryBlock& state, std::initializer_list<const char*> paramIds,
    std::initializer_list<const char*> properties)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));
    REQUIRE (xml != nullptr);
    for (const char* id : paramIds)
        if (auto* child = xml->getChildByAttribute ("id", id))
            xml->removeChildElement (child, true);
    for (const char* property : properties)
        xml->removeAttribute (property);
    juce::MemoryBlock out;
    juce::AudioProcessor::copyXmlToBinary (*xml, out);
    return out;
}

} // namespace

TEST_CASE ("alvo de LUFS: gênero, ajuste e manual")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor p;
    p.prepareToPlay (48000.0, 512);

    CHECK (targetAfterBlock (p) == Approx (-9.5f).margin (1.0e-4)); // Pop, o padrão
    setParam (p, finale::ParamIDs::genre, 5.0f); // EDM
    CHECK (targetAfterBlock (p) == Approx (-8.5f).margin (1.0e-4));
    setParam (p, finale::ParamIDs::lufsOffset, -3.0f);
    CHECK (targetAfterBlock (p) == Approx (-11.5f).margin (1.0e-4));
    setParam (p, finale::ParamIDs::manualTarget, 1.0f);
    setParam (p, finale::ParamIDs::targetLufs, -14.0f);
    CHECK (targetAfterBlock (p) == Approx (-14.0f).margin (1.0e-4));
}

TEST_CASE ("estado: projeto antigo mantém o alvo absoluto")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor a;
    setParam (a, finale::ParamIDs::targetLufs, -12.0f);
    juce::MemoryBlock state;
    a.getStateInformation (state);

    // Projeto de antes do #29: sem lufsOffset nem manualTarget.
    const auto oldState = withoutParts (state, { finale::ParamIDs::lufsOffset, finale::ParamIDs::manualTarget }, { });
    FinaleProcessor b;
    b.setStateInformation (oldState.getData(), static_cast<int> (oldState.getSize()));
    CHECK (rawParam (b, finale::ParamIDs::manualTarget) == Approx (1.0f));
    b.prepareToPlay (48000.0, 512);
    CHECK (targetAfterBlock (b) == Approx (-12.0f).margin (1.0e-4));

    // Projeto novo, salvo com o alvo do gênero: continua no alvo do gênero.
    FinaleProcessor c;
    c.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    CHECK (rawParam (c, finale::ParamIDs::manualTarget) == Approx (0.0f));
    c.prepareToPlay (48000.0, 512);
    CHECK (targetAfterBlock (c) == Approx (-9.5f).margin (1.0e-4));
}

TEST_CASE ("estado: projeto sem correção de loudness zera a correção")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor a;
    setParam (a, finale::ParamIDs::assistant, 2.0f); // Auto
    a.prepareToPlay (48000.0, 512);
    run (a, 2, 48000.0, 0.05f, 30.0, { 512 });
    REQUIRE (a.getAssistantCorrectionDb() > 2.0f);

    // Congelar antes de salvar: o silêncio não move a correção, então um zero depois prova o reset.
    setParam (a, finale::ParamIDs::assistant, 3.0f);
    juce::MemoryBlock state;
    a.getStateInformation (state);
    const auto oldState = withoutParts (state, { }, { "assistantCorrectionDb" });

    // Mesma instância: a correção de antes não pode sobrar.
    a.setStateInformation (oldState.getData(), static_cast<int> (oldState.getSize()));
    targetAfterBlock (a);
    CHECK (a.getAssistantCorrectionDb() == 0.0f);
}

TEST_CASE ("estado: projeto antigo numa instância reaproveitada usa os padrões")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor a;
    setParam (a, finale::ParamIDs::lufsOffset, -2.0f);
    setParam (a, finale::ParamIDs::eq, 40.0f);
    setParam (a, finale::ParamIDs::targetLufs, -12.0f);
    juce::MemoryBlock fresh;
    a.getStateInformation (fresh);

    // Projeto antigo: sem lufsOffset, manualTarget nem eq (eq é de antes do #23).
    const auto oldState = withoutParts (
        fresh, { finale::ParamIDs::lufsOffset, finale::ParamIDs::manualTarget, finale::ParamIDs::eq }, { });

    // Instância que já tinha outro projeto aberto: o antigo não pode herdar os valores dele.
    FinaleProcessor r;
    setParam (r, finale::ParamIDs::lufsOffset, 3.0f);
    setParam (r, finale::ParamIDs::eq, 70.0f);
    r.setStateInformation (oldState.getData(), static_cast<int> (oldState.getSize()));

    CHECK (rawParam (r, finale::ParamIDs::lufsOffset) == Approx (0.0f));
    CHECK (rawParam (r, finale::ParamIDs::eq) == Approx (100.0f));
    CHECK (rawParam (r, finale::ParamIDs::manualTarget) == Approx (1.0f));
    CHECK (rawParam (r, finale::ParamIDs::targetLufs) == Approx (-12.0f));
    r.prepareToPlay (48000.0, 512);
    CHECK (targetAfterBlock (r) == Approx (-12.0f).margin (1.0e-4));
}

TEST_CASE ("interface: true peak e redução do limiter")
{
    const juce::ScopedJuceInitialiser_GUI juceInit;
    FinaleProcessor p;
    setParam (p, finale::ParamIDs::gain, 12.0f);
    setParam (p, finale::ParamIDs::amount, 100.0f);
    p.prepareToPlay (48000.0, 512);
    run (p, 2, 48000.0, 0.5f, 2.0, { 512 });
    CHECK (p.getOutputTruePeakDb() <= -1.0f + 0.1f);
    CHECK (p.getOutputTruePeakDb() > -6.0f);
    CHECK (p.getLimiterReductionDb() > 0.5f);

    FinaleProcessor quiet;
    quiet.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> silence (2, 512);
    silence.clear();
    juce::MidiBuffer midi;
    quiet.processBlock (silence, midi);
    CHECK (quiet.getOutputTruePeakDb() == -100.0f);
    CHECK (quiet.getLimiterReductionDb() == Approx (0.0f).margin (1.0e-3));
}
