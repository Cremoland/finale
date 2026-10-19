#include "Plugin/PluginEditor.h"

#include "Plugin/Theme.h"

#include <cmath>
#include <initializer_list>
#include <vector>

namespace finale
{

namespace
{

// Medidas do protótipo v9 (docs/ui/prototipos/v9-marfim-final.html), em px.
constexpr int windowWidth = 820;
constexpr int windowHeight = 500;
constexpr int topHeight = 72;
constexpr int footerHeight = 48;
constexpr int advancedHeight = 76;
constexpr int sidePadding = 32;
constexpr int bodyPaddingY = 12;
constexpr int columnGap = 24;
constexpr int meterWidth = LoudnessPanel::width;
constexpr int presetHeight = 144;
constexpr int presetGap = 6;
constexpr int knobDiameter = 200;
constexpr int infoColumnWidth = 200; // CSS do protótipo (.info-col); o brief dizia 220
constexpr int infoColumnX = windowWidth - sidePadding - infoColumnWidth;

juce::StringArray labelsOf (std::initializer_list<juce::String> items)
{
    juce::StringArray out;
    for (const auto& item : items)
        out.add (item);
    return out;
}

juce::StringArray choicesOf (juce::RangedAudioParameter& parameter)
{
    return static_cast<juce::AudioParameterChoice&> (parameter).choices;
}

// Número com sinal: "+2,4" para positivo, "−2,4" para negativo e "0,0" sem sinal.
juce::String signedDecimal (float value)
{
    const auto rounded = std::round (value * 10.0f) / 10.0f;
    return (rounded > 0.0f ? juce::String ("+") : juce::String()) + formatDecimal (value);
}

juce::String percentText (float value)
{
    return juce::String (juce::roundToInt (value)) + " %";
}

juce::String lufsOffsetText (float value)
{
    return signedDecimal (value) + " LU";
}

juce::String ceilingText (float value)
{
    return formatDecimal (value) + " dBTP";
}

// Sombra do box de loudness. O LoudnessPanel só desenha dentro dos seus limites, então a sombra que cai para fora
// precisa vir do paint do editor, antes dos filhos (mesmas duas camadas de theme::card).
void paintCardShadow (juce::Graphics& g, juce::Rectangle<float> r)
{
    juce::Path shape;
    shape.addRoundedRectangle (r, 12.0f);
    juce::DropShadow (juce::Colour (0x1a1c1b19), 28, { 0, 10 }).drawForPath (g, shape);
    juce::DropShadow (juce::Colour (0x0f1c1b19), 6, { 0, 2 }).drawForPath (g, shape);
}

// Texto do status: "Desligado", ou o modo com o valor da correção (mono) em dB.
std::vector<TextRun> assistantStatusRuns (int mode, float correctionDb)
{
    if (mode <= 0)
        return { { theme::ui (12.0f), juce::String ("Desligado"), theme::text } };

    const juce::String label = mode == 1 ? juce::String ("Analisando ")
        : mode == 2                      ? juce::String ("Ajustando ")
                                         : juce::String ("Congelado ");
    return { { theme::ui (12.0f), label + middleDot() + " ", theme::text },
        { theme::mono (12.0f, 500), signedDecimal (correctionDb) + " dB", theme::text } };
}

} // namespace

// shortcut: janela fixa 820×500; quando tiver redimensionamento, escalar com AffineTransform
FinaleEditor::FinaleEditor (FinaleProcessor& p) :
    AudioProcessorEditor (p),
    processor (p),
    knob (*p.parameters.getParameter (ParamIDs::amount)),
    genreSelector (*p.parameters.getParameter (ParamIDs::genre),
        choicesOf (*p.parameters.getParameter (ParamIDs::genre)), ChoiceSelector::Style::chips),
    characterSelector (*p.parameters.getParameter (ParamIDs::character),
        choicesOf (*p.parameters.getParameter (ParamIDs::character)), ChoiceSelector::Style::chips),
    assistantSelector (*p.parameters.getParameter (ParamIDs::assistant),
        labelsOf ({ "Off", "Analisar", "Auto", "Congelar" }), ChoiceSelector::Style::segmented),
    compareSelector (*p.parameters.getParameter (ParamIDs::compare),
        labelsOf ({ juce::String::fromUTF8 ("A \xc2\xb7 ") + JucePlugin_Name,
            juce::String::fromUTF8 ("B \xc2\xb7 Original") }),
        ChoiceSelector::Style::segmented),
    manualTargetSelector (*p.parameters.getParameter (ParamIDs::manualTarget), labelsOf ({ "Desligado", "Ligado" }),
        ChoiceSelector::Style::segmented),
    eqControl (*p.parameters.getParameter (ParamIDs::eq), "EQ", percentText),
    compressionControl (*p.parameters.getParameter (ParamIDs::compression),
        juce::String::fromUTF8 ("Compress\xc3\xa3"
                                "o"),
        percentText),
    saturationControl (*p.parameters.getParameter (ParamIDs::saturation),
        juce::String::fromUTF8 ("Satura\xc3\xa7\xc3\xa3"
                                "o"),
        percentText),
    widthControl (*p.parameters.getParameter (ParamIDs::width), "Largura", percentText),
    lufsOffsetControl (*p.parameters.getParameter (ParamIDs::lufsOffset), "Ajuste LUFS", lufsOffsetText, false),
    ceilingControl (*p.parameters.getParameter (ParamIDs::ceiling), "Teto", ceilingText, false)
{
    genreIndexParam = p.parameters.getRawParameterValue (ParamIDs::genre);
    characterIndexParam = p.parameters.getRawParameterValue (ParamIDs::character);
    assistantModeParam = p.parameters.getRawParameterValue (ParamIDs::assistant);
    ceilingParam = p.parameters.getRawParameterValue (ParamIDs::ceiling);
    genreNames = choicesOf (*p.parameters.getParameter (ParamIDs::genre));
    characterNames = choicesOf (*p.parameters.getParameter (ParamIDs::character));

    addAndMakeVisible (knob);
    addAndMakeVisible (genreSelector);
    addAndMakeVisible (characterSelector);
    addAndMakeVisible (assistantSelector);
    addAndMakeVisible (compareSelector);
    addAndMakeVisible (loudness);
    addAndMakeVisible (advancedToggle);

    // Só existem quando o Avançado está aberto.
    addChildComponent (eqControl);
    addChildComponent (compressionControl);
    addChildComponent (saturationControl);
    addChildComponent (widthControl);
    addChildComponent (lufsOffsetControl);
    addChildComponent (ceilingControl);
    addChildComponent (manualTargetSelector);

    advancedToggle.onClick = [this] { setAdvancedOpen (!advancedOpen); };

    setResizable (false, false);
    setSize (windowWidth, windowHeight);
    startTimerHz (30);

    // Primeira atualização já com o gênero e o alvo, sem esperar o primeiro tick do timer.
    timerCallback();
}

FinaleEditor::~FinaleEditor()
{
    stopTimer();
}

void FinaleEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    // Ordem igual à do protótipo: as sombras dos cards vêm antes do painel do Avançado, que as cobre.
    paintCardShadow (g, meterArea.toFloat());
    theme::card (g, presetArea.toFloat(), false);

    if (advancedOpen)
    {
        g.setColour (theme::surface);
        g.fillRect (advancedPanelArea);
        g.setColour (theme::rule);
        g.fillRect (juce::Rectangle<int> (0, advancedPanelArea.getY(), getWidth(), 1));
        g.fillRect (
            juce::Rectangle<int> (infoColumnX, advancedPanelArea.getY() + 1, 1, advancedPanelArea.getHeight() - 1));
    }

    g.setColour (theme::rule);
    g.fillRect (juce::Rectangle<int> (0, footerArea.getY(), getWidth(), 1));

    // Topo: wordmark com o nome do produto em caixa alta.
    g.setColour (theme::text);
    g.setFont (theme::ui (22.0f, 800).withExtraKerningFactor (0.18f));
    g.drawText (juce::String (JucePlugin_Name).toUpperCase(),
        juce::Rectangle<float> (static_cast<float> (sidePadding), 0.0f, 300.0f, static_cast<float> (topHeight)),
        juce::Justification::centredLeft, false);

    // Status do assistente: bolinha (vazada em Off, textDim em Congelar, accent nos outros) e o texto do modo.
    const auto mode = juce::jlimit (0, 3, juce::roundToInt (assistantModeParam->load()));
    const auto statusRuns = assistantStatusRuns (mode, processor.getAssistantCorrectionDb());
    const auto statusArea = assistantStatusArea.toFloat();
    const auto dot = juce::Rectangle<float> (
        statusArea.getRight() - runsWidth (statusRuns) - 12.0f, statusArea.getCentreY() - 3.0f, 6.0f, 6.0f);
    if (mode == 0)
    {
        g.setColour (theme::textDim);
        g.drawEllipse (dot.reduced (0.75f), 1.5f);
    }
    else
    {
        g.setColour (mode == 3 ? theme::textDim : theme::accent);
        g.fillEllipse (dot);
    }
    drawRuns (g, statusRuns, statusArea, TextAlign::right);

    g.setFont (theme::ui (12.0f));
    g.setColour (theme::textDim);
    g.drawText (juce::String ("volume igualado"), compareSubArea.toFloat(), juce::Justification::centredRight, false);

    // Box PRESET: cabeçalho com o resumo, e os títulos dos grupos.
    const auto presetInnerX = static_cast<float> (presetArea.getX() + 16);
    const auto presetY = static_cast<float> (presetArea.getY());
    g.setColour (theme::textDim);
    g.setFont (theme::ui (11.0f, 600).withExtraKerningFactor (0.12f));
    g.drawText (juce::String ("PRESET"), presetHeaderArea.toFloat(), juce::Justification::centredLeft, false);
    g.setColour (theme::text);
    g.setFont (theme::ui (13.0f, 700));
    g.drawText (presetSummary(), presetHeaderArea.toFloat(), juce::Justification::centredRight, false);

    g.setFont (theme::ui (11.0f, 700).withExtraKerningFactor (0.12f));
    g.drawText (juce::String::fromUTF8 ("G\xc3\x8a"
                                        "NERO"),
        juce::Rectangle<float> (presetInnerX, presetY + 35.0f, 200.0f, 14.0f), juce::Justification::centredLeft, false);
    g.drawText (juce::String::fromUTF8 ("CAR\xc3\x81"
                                        "TER"),
        juce::Rectangle<float> (presetInnerX, presetY + 88.0f, 200.0f, 14.0f), juce::Justification::centredLeft, false);

    // Avançado aberto: rótulo da linha do alvo manual.
    if (advancedOpen)
    {
        g.setColour (theme::textDim);
        g.setFont (theme::ui (12.0f));
        g.drawText (juce::String ("Alvo manual"), manualRowArea.toFloat(), juce::Justification::centredLeft, false);
    }

    // Rodapé: "Avançado" com o caret desenhado (▸ fechado, ▾ aberto) e a dica de uso à direita.
    const auto footerY = static_cast<float> (footerArea.getY());
    const auto labelFont = theme::ui (13.0f, 600);
    const auto labelText = juce::String::fromUTF8 ("Avan\xc3\xa7"
                                                   "ado");
    g.setFont (labelFont);
    g.setColour (theme::text);
    g.drawText (labelText,
        juce::Rectangle<float> (static_cast<float> (sidePadding), footerY, textWidth (labelFont, labelText) + 4.0f,
            static_cast<float> (footerHeight)),
        juce::Justification::centredLeft, false);

    const auto caretX = static_cast<float> (sidePadding) + textWidth (labelFont, labelText) + 8.0f;
    const auto caretCentreX = caretX + 3.0f;
    const auto caretCentreY = footerY + static_cast<float> (footerHeight) / 2.0f;
    juce::Path caret;
    if (advancedOpen)
    {
        caret.startNewSubPath (caretCentreX + 4.0f, caretCentreY - 3.0f);
        caret.lineTo (caretCentreX, caretCentreY + 3.0f);
        caret.lineTo (caretCentreX - 4.0f, caretCentreY - 3.0f);
    }
    else
    {
        caret.startNewSubPath (caretCentreX - 3.0f, caretCentreY - 4.0f);
        caret.lineTo (caretCentreX + 3.0f, caretCentreY);
        caret.lineTo (caretCentreX - 3.0f, caretCentreY + 4.0f);
    }
    caret.closeSubPath();
    g.fillPath (caret);

    const std::vector<TextRun> hint {
        { theme::ui (12.0f),
            juce::String ("Arraste ") + middleDot() + " Shift: fino " + middleDot() + " Duplo clique: ",
            theme::textDim },
        { theme::mono (12.0f), juce::String ("50 %"), theme::textDim },
    };
    drawRuns (g, hint,
        juce::Rectangle<float> (static_cast<float> (sidePadding), footerY,
            static_cast<float> (getWidth() - 2 * sidePadding), static_cast<float> (footerHeight)),
        TextAlign::right);
}

void FinaleEditor::resized()
{
    auto area = getLocalBounds();
    const auto top = area.removeFromTop (topHeight);
    footerArea = area.removeFromBottom (footerHeight);
    advancedPanelArea = advancedOpen ? area.removeFromBottom (advancedHeight) : juce::Rectangle<int>();
    auto content = area.reduced (sidePadding, bodyPaddingY);

    // Topo: dois blocos alinhados à direita, com 24 px entre eles e 6 px entre o seletor e o texto embaixo.
    constexpr int blockHeight = 32 + 6 + 16;
    const auto blockTop = top.getY() + (top.getHeight() - blockHeight) / 2;
    const auto right = top.getRight() - sidePadding;

    const auto compareWidth = compareSelector.getPreferredWidth();
    compareSelector.setBounds (right - compareWidth, blockTop, compareWidth, 32);
    compareSubArea = juce::Rectangle<int> (right - compareWidth, blockTop + 38, compareWidth, 16);

    const auto assistantRight = right - compareWidth - columnGap;
    const auto assistantWidth = assistantSelector.getPreferredWidth();
    assistantSelector.setBounds (assistantRight - assistantWidth, blockTop, assistantWidth, 32);
    assistantStatusArea = juce::Rectangle<int> (assistantRight - assistantWidth, blockTop + 38, assistantWidth, 16);

    // Coluna da direita: o medidor, com a altura do corpo.
    meterArea = content.removeFromRight (meterWidth);
    content.removeFromRight (columnGap);
    loudness.setBounds (meterArea);

    // Coluna da esquerda: o knob centrado na área de cima e o box PRESET embaixo.
    presetArea = content.removeFromBottom (presetHeight);
    content.removeFromBottom (presetGap);
    // Diâmetro: 200 px, ou menos quando a área é curta (Avançado aberto). Com 4 px de folga em cima e embaixo, o
    // knob fica a pelo menos 8 px do box PRESET (6 px de presetGap + 2 px).
    const auto knobSize = juce::jmin (knobDiameter, content.getHeight() - 4);
    knob.setBounds (content.withSizeKeepingCentre (knobSize, knobSize));

    const auto presetInner = presetArea.reduced (16, 0);
    presetHeaderArea = juce::Rectangle<int> (presetInner.getX(), presetArea.getY() + 10, presetInner.getWidth(), 18);
    genreSelector.setBounds (presetInner.getX(), presetArea.getY() + 53, presetInner.getWidth(), 28);
    characterSelector.setBounds (presetInner.getX(), presetArea.getY() + 106, presetInner.getWidth(), 28);

    advancedToggle.setBounds (sidePadding, footerArea.getY() + 8, 110, 32);

    if (advancedOpen)
    {
        const auto panel = advancedPanelArea.withTrimmedTop (1);
        const auto infoX = infoColumnX;

        // Quatro mini-controles lado a lado, com 24 px de espaço, até a coluna de informação.
        const auto miniWidth = (infoX - columnGap - sidePadding - 3 * columnGap) / 4;
        auto miniX = sidePadding;
        for (auto* mini : { &eqControl, &compressionControl, &saturationControl, &widthControl })
        {
            mini->setBounds (miniX, panel.getY(), miniWidth, panel.getHeight());
            miniX += miniWidth + columnGap;
        }

        // Coluna de informação: borda de 1 px e 24 px de padding à esquerda, até a margem direita de 32 px.
        // Quatro linhas empilhadas (16, 16, 16 e 20 px, com 2 px entre elas): "Alvo manual" não cabe na mesma linha
        // que o seletor dentro de 200 px.
        constexpr int rowGap = 2;
        const auto innerX = infoX + 25;
        const auto innerWidth = windowWidth - sidePadding - innerX;
        const auto stackTop = panel.getY() + (panel.getHeight() - (16 * 3 + 20 + 3 * rowGap)) / 2;
        lufsOffsetControl.setBounds (innerX, stackTop, innerWidth, 16);
        ceilingControl.setBounds (innerX, stackTop + 16 + rowGap, innerWidth, 16);

        manualRowArea = juce::Rectangle<int> (innerX, stackTop + 2 * (16 + rowGap), innerWidth, 16);
        const auto selectorWidth = manualTargetSelector.getPreferredWidth();
        manualTargetSelector.setBounds (innerX, manualRowArea.getBottom() + rowGap, selectorWidth, 20);
    }
}

void FinaleEditor::timerCallback()
{
    // Só repinta a área do status e do resumo quando o texto muda; o medidor repinta a si mesmo no update.
    const auto mode = juce::jlimit (0, 3, juce::roundToInt (assistantModeParam->load()));
    const auto statusKey = juce::String (mode) + "|" + signedDecimal (processor.getAssistantCorrectionDb());
    if (statusKey != lastStatusKey)
    {
        lastStatusKey = statusKey;
        repaint (assistantStatusArea);
    }

    const auto summary = presetSummary();
    if (summary != lastPresetSummary)
    {
        lastPresetSummary = summary;
        repaint (presetHeaderArea);
    }

    const auto genreName = genreNames[juce::roundToInt (genreIndexParam->load())];
    loudness.update (processor.getOutputIntegratedLufs(), processor.getOutputShortTermLufs(),
        processor.getOutputTruePeakDb(), processor.getLimiterReductionDb(), processor.getLoudnessTargetLufs(),
        genreName, ceilingParam->load());
}

void FinaleEditor::setAdvancedOpen (bool open)
{
    advancedOpen = open;
    juce::Component* parts[] = { &eqControl, &compressionControl, &saturationControl, &widthControl, &lufsOffsetControl,
        &ceilingControl, &manualTargetSelector };
    for (auto* part : parts)
        part->setVisible (open);

    resized();
    repaint();
}

juce::String FinaleEditor::presetSummary() const
{
    const auto genre = juce::roundToInt (genreIndexParam->load());
    const auto character = juce::roundToInt (characterIndexParam->load());
    return genreNames[genre] + (" " + middleDot() + " ") + characterNames[character];
}

} // namespace finale
