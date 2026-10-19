#pragma once

#include "Plugin/Widgets.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

namespace finale
{

// Box de loudness do protótipo (.meter): LUFS integrado, short-term, true peak e GR do limiter.
// Quem chama é o editor, no timer: update() guarda os valores e repinta; o painel não lê o processor.
class LoudnessPanel : public juce::Component
{
public:
    // Largura do box, como no protótipo.
    static constexpr int width = 216;

    LoudnessPanel() { setInterceptsMouseClicks (false, false); }

    void update (float integrated, float shortTerm, float truePeakDb, float grDb, float target, juce::String genreName,
        float ceilingDb)
    {
        const auto before = shownState();

        integratedLufs = integrated;
        shortTermLufs = shortTerm;
        targetLufs = target;
        genre = std::move (genreName);
        ceilingDbtp = ceilingDb;
        grReductionDb = grDb;

        // Hold: o maior true peak dos últimos 1,5 s. Passado o tempo, volta a valer o valor atual.
        const auto now = juce::Time::getMillisecondCounter();
        if (truePeakDb >= peakHoldDb || now - peakHoldStamp > peakHoldMs)
        {
            peakHoldDb = truePeakDb;
            peakHoldStamp = now;
        }

        updatePill();

        // Repinta só quando algo que aparece na tela mudou (o timer chama update a 30 Hz).
        if (shownState() != before)
            repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto box = getLocalBounds().toFloat();
        theme::card (g, box, true);

        // Padding de 14 px em cima e embaixo e 20 px dos lados. Os blocos se espalham como o space-between do
        // protótipo.
        const auto content = box.reduced (20.0f, 14.0f);
        constexpr float gapMin = 8.0f;
        constexpr float headerH = 22.0f;
        constexpr float bigH = 44.0f;
        constexpr float rowH = 16.0f;
        // Curto prazo: linha (16) e 8 px, depois um bloco de 50: rótulo do alvo (16), barra (12) em y=18 e régua (14)
        // 6 px abaixo da barra.
        constexpr float shortH = rowH + gapMin + 50.0f;
        constexpr float truePeakH = rowH + gapMin + 6.0f + gapMin + 14.0f; // linha, barra fina, nota
        constexpr float grH = rowH + gapMin + 6.0f; // linha e barra fina
        const auto gap = std::max (gapMin, (content.getHeight() - (headerH + bigH + shortH + truePeakH + grH)) / 4.0f);

        auto y = content.getY();
        drawHeader (g, juce::Rectangle<float> (content.getX(), y, content.getWidth(), headerH));
        y += headerH + gap;
        drawBigNumber (g, juce::Rectangle<float> (content.getX(), y, content.getWidth(), bigH));
        y += bigH + gap;
        drawShortTerm (g, juce::Rectangle<float> (content.getX(), y, content.getWidth(), shortH));
        y += shortH + gap;
        drawTruePeak (g, juce::Rectangle<float> (content.getX(), y, content.getWidth(), truePeakH));
        y += truePeakH + gap;
        drawGainReduction (g, juce::Rectangle<float> (content.getX(), y, content.getWidth(), grH));
    }

private:
    enum class PillState
    {
        silent,
        onTarget,
        below,
        above
    };

    // Sem sinal abaixo de −70 LUFS. Histerese: entra em "no alvo" dentro de ±0,8 LU e só sai além de ±1,2 LU.
    static constexpr float silenceLufs = -70.0f;
    // Piso do true peak: o processor marca −100 dBTP sem sinal. Abaixo de −99 mostra "—" (não usa o −70 do LUFS).
    static constexpr float truePeakFloorDb = -99.0f;
    static constexpr float enterBand = 0.8f;
    static constexpr float leaveBand = 1.2f;
    static constexpr std::uint32_t peakHoldMs = 1500;

    // Tudo o que o paint mostra. Se nada disso mudou, o update não repinta.
    auto shownState() const
    {
        return std::make_tuple (
            integratedLufs, shortTermLufs, targetLufs, ceilingDbtp, grReductionDb, peakHoldDb, pillState, genre);
    }

    void updatePill()
    {
        if (integratedLufs <= silenceLufs)
        {
            inTarget = false;
            pillState = PillState::silent;
            return;
        }

        const auto diff = std::round ((integratedLufs - targetLufs) * 10.0f) / 10.0f;
        inTarget = std::abs (diff) <= (inTarget ? leaveBand : enterBand);
        if (inTarget)
            pillState = PillState::onTarget;
        else
            pillState = diff < 0.0f ? PillState::below : PillState::above;
    }

    // As setas saem da Plex Mono: a Sora não tem o glifo.
    std::vector<TextRun> pillRuns (juce::Colour ink) const
    {
        if (pillState == PillState::onTarget)
            return { { theme::ui (11.0f, 600), juce::String ("no alvo"), ink } };
        if (pillState == PillState::below)
            return { { theme::mono (11.0f, 500), arrowUp(), ink },
                { theme::ui (11.0f, 600), juce::String (" abaixo do alvo"), ink } };
        if (pillState == PillState::above)
            return { { theme::mono (11.0f, 500), arrowDown(), ink },
                { theme::ui (11.0f, 600), juce::String (" acima do alvo"), ink } };
        return { { theme::ui (11.0f, 600), juce::String ("sem sinal"), ink } };
    }

    void drawHeader (juce::Graphics& g, juce::Rectangle<float> area)
    {
        g.setColour (theme::textDim);
        g.setFont (theme::ui (11.0f, 600).withExtraKerningFactor (0.12f));
        g.drawText (juce::String ("LOUDNESS"), area, juce::Justification::centredLeft, false);

        const auto onTarget = pillState == PillState::onTarget;
        const juce::Colour ink = onTarget ? juce::Colours::white : theme::text;
        const auto runs = pillRuns (ink);

        // Pílula de 20 px de altura. No alvo, o ✓ (desenhado como path) fica antes do texto.
        const auto pillWidth = runsWidth (runs) + (onTarget ? 28.0f : 16.0f);
        const juce::Rectangle<float> pill (area.getRight() - pillWidth, area.getCentreY() - 10.0f, pillWidth, 20.0f);

        juce::Path shape;
        shape.addRoundedRectangle (pill, 10.0f);
        g.setColour (onTarget ? theme::accent : theme::surface2);
        g.fillPath (shape);

        if (onTarget)
            theme::check (g, juce::Rectangle<float> (pill.getX() + 8.0f, pill.getCentreY() - 4.0f, 8.0f, 8.0f),
                juce::Colours::white);

        drawRuns (g, runs, pill.withTrimmedLeft (onTarget ? 20.0f : 8.0f).withTrimmedRight (8.0f), TextAlign::left);
    }

    void drawBigNumber (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto silent = integratedLufs <= silenceLufs;
        const auto number = silent ? dashSign() : formatDecimal (integratedLufs);
        const auto numberFont = theme::mono (44.0f, 500).withExtraKerningFactor (-0.02f);
        const auto unitFont = theme::ui (13.0f, 500);

        // Número e "LUFS" compartilham a linha de base. A base de um texto centralizado na área é
        // centro + (ascent − descent) / 2.
        const auto baseline = area.getCentreY() + (numberFont.getAscent() - numberFont.getDescent()) / 2.0f;
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (numberFont, number, area.getX(), baseline);
        g.setColour (theme::text);
        glyphs.draw (g);

        juce::GlyphArrangement unit;
        unit.addLineOfText (
            unitFont, juce::String ("LUFS"), area.getX() + textWidth (numberFont, number) + 6.0f, baseline);
        g.setColour (theme::textDim);
        unit.draw (g);
    }

    void drawShortTerm (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto row = area.removeFromTop (16.0f);
        g.setColour (theme::textDim);
        g.setFont (theme::ui (12.0f));
        g.drawText (juce::String ("Curto prazo"), row, juce::Justification::centredLeft, false);
        g.setColour (theme::text);
        g.setFont (theme::mono (13.0f, 500));
        const auto shortText = shortTermLufs <= silenceLufs ? dashSign() : formatDecimal (shortTermLufs);
        g.drawText (shortText, row, juce::Justification::centredRight, false);

        area.removeFromTop (8.0f);
        const auto block = area.removeFromTop (50.0f);
        const juce::Rectangle<float> labelArea (block.getX(), block.getY(), block.getWidth(), 16.0f);
        const juce::Rectangle<float> bar (block.getX(), block.getY() + 18.0f, block.getWidth(), 12.0f);
        const juce::Rectangle<float> scaleArea (block.getX(), bar.getBottom() + 6.0f, block.getWidth(), 14.0f);

        // Escala de −20 a 0 LUFS.
        const auto xOf = [bar] (float lufs) { return bar.getX() + (lufs + 20.0f) / 20.0f * bar.getWidth(); };

        g.setColour (theme::surface2);
        g.fillRoundedRectangle (bar, 6.0f);

        const auto fraction = juce::jlimit (0.0f, 1.0f, (shortTermLufs + 20.0f) / 20.0f);
        if (fraction > 0.0f)
        {
            g.setColour (theme::accent);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * fraction), 6.0f);
        }

        // Faixa de ±1 LU por cima do preenchimento, depois a linha do alvo que passa 4 px da barra.
        const auto bandLeft = xOf (targetLufs - 1.0f);
        const auto bandRight = xOf (targetLufs + 1.0f);
        const juce::Rectangle<float> band (bandLeft, bar.getY(), bandRight - bandLeft, bar.getHeight());
        g.setColour (juce::Colour (0x241c1b19));
        g.fillRect (band);
        g.setColour (juce::Colour (0x731c1b19));
        g.fillRect (juce::Rectangle<float> (band.getX(), band.getY(), 1.0f, band.getHeight()));
        g.fillRect (juce::Rectangle<float> (band.getRight() - 1.0f, band.getY(), 1.0f, band.getHeight()));

        g.setColour (theme::text);
        g.fillRoundedRectangle (
            juce::Rectangle<float> (xOf (targetLufs) - 1.0f, bar.getY() - 4.0f, 2.0f, bar.getHeight() + 8.0f), 1.0f);

        // Rótulo do alvo centralizado na linha, preso dentro da barra.
        const std::vector<TextRun> targetRuns {
            { theme::ui (12.0f, 600), juce::String ("alvo "), theme::text },
            { theme::mono (12.0f, 500), formatDecimal (targetLufs), theme::text },
            { theme::ui (12.0f, 600), juce::String (" ") + middleDot() + " " + genre, theme::text },
        };
        const auto labelWidth = runsWidth (targetRuns);
        const auto labelLeft = std::max (
            labelArea.getX(), std::min (xOf (targetLufs) - labelWidth / 2.0f, labelArea.getRight() - labelWidth));
        drawRuns (g, targetRuns,
            juce::Rectangle<float> (labelLeft, labelArea.getY(), labelWidth, labelArea.getHeight()), TextAlign::left);

        g.setColour (theme::textDim);
        g.setFont (theme::mono (11.0f));
        g.drawText (minusSign() + juce::String ("20"), scaleArea, juce::Justification::centredLeft, false);
        g.drawText (juce::String ("0"), scaleArea, juce::Justification::centredRight, false);
    }

    void drawTruePeak (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto row = area.removeFromTop (16.0f);
        g.setColour (theme::textDim);
        g.setFont (theme::ui (12.0f));
        g.drawText (juce::String ("True peak"), row, juce::Justification::centredLeft, false);
        g.setColour (theme::text);
        g.setFont (theme::mono (13.0f, 500));
        // Sem sinal, o valor vira "—", como no integrado.
        const auto peakText = peakHoldDb <= truePeakFloorDb ? dashSign() : formatDecimal (peakHoldDb);
        g.drawText (peakText + juce::String (" dBTP"), row, juce::Justification::centredRight, false);

        // Barra fina na escala de −6 a 0 dBTP, com o marcador do teto.
        area.removeFromTop (8.0f);
        const auto bar = area.removeFromTop (6.0f);
        g.setColour (theme::surface2);
        g.fillRoundedRectangle (bar, 3.0f);

        const auto fraction = juce::jlimit (0.0f, 1.0f, (peakHoldDb + 6.0f) / 6.0f);
        if (fraction > 0.0f)
        {
            g.setColour (theme::accent);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * fraction), 3.0f);
        }

        const auto ceilingX = bar.getX() + (ceilingDbtp + 6.0f) / 6.0f * bar.getWidth();
        g.setColour (theme::text);
        g.fillRoundedRectangle (
            juce::Rectangle<float> (ceilingX - 1.0f, bar.getY() - 4.0f, 2.0f, bar.getHeight() + 8.0f), 1.0f);

        area.removeFromTop (8.0f);
        const std::vector<TextRun> note {
            { theme::ui (11.0f), juce::String ("teto "), theme::textDim },
            { theme::mono (11.0f), formatDecimal (ceilingDbtp) + juce::String (" dBTP"), theme::textDim },
        };
        drawRuns (g, note, area.removeFromTop (14.0f), TextAlign::left);
    }

    void drawGainReduction (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto row = area.removeFromTop (16.0f);
        g.setColour (theme::textDim);
        g.setFont (theme::ui (12.0f));
        g.drawText (juce::String ("GR limiter"), row, juce::Justification::centredLeft, false);
        g.setColour (theme::text);
        g.setFont (theme::mono (13.0f, 500));
        g.drawText (
            formatDecimal (-grReductionDb) + juce::String (" dB"), row, juce::Justification::centredRight, false);

        // Barra fina na escala de 0 a 12 dB, em warn.
        area.removeFromTop (8.0f);
        const auto bar = area.removeFromTop (6.0f);
        g.setColour (theme::surface2);
        g.fillRoundedRectangle (bar, 3.0f);

        const auto fraction = juce::jlimit (0.0f, 1.0f, grReductionDb / 12.0f);
        if (fraction > 0.0f)
        {
            g.setColour (theme::warn);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * fraction), 3.0f);
        }
    }

    float integratedLufs = -100.0f;
    float shortTermLufs = -100.0f;
    float targetLufs = -9.0f;
    float ceilingDbtp = -1.0f;
    float grReductionDb = 0.0f;
    float peakHoldDb = -100.0f;
    std::uint32_t peakHoldStamp = 0;
    juce::String genre;
    PillState pillState = PillState::silent;
    bool inTarget = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessPanel)
};

} // namespace finale
