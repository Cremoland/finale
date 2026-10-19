#pragma once

#include "Plugin/Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace finale
{

// Trecho de texto com fonte e cor próprias: deixa misturar Sora e IBM Plex Mono na mesma linha.
struct TextRun
{
    juce::Font font;
    juce::String text;
    juce::Colour colour;
};

enum class TextAlign
{
    left,
    center,
    right
};

// Caracteres não-ASCII saem daqui como UTF-8 explícito. A Sora não tem as setas para cima e para baixo, por isso elas
// saem da Plex Mono. Cada escape fica sozinho na sua string, porque \x aceita dígitos hex em sequência.
inline juce::String minusSign()
{
    return juce::String::fromUTF8 ("\xe2\x88\x92");
}
inline juce::String dashSign()
{
    return juce::String::fromUTF8 ("\xe2\x80\x94");
}
inline juce::String middleDot()
{
    return juce::String::fromUTF8 ("\xc2\xb7");
}
inline juce::String arrowUp()
{
    return juce::String::fromUTF8 ("\xe2\x86\x91");
}
inline juce::String arrowDown()
{
    return juce::String::fromUTF8 ("\xe2\x86\x93");
}

inline float textWidth (const juce::Font& font, const juce::String& text)
{
    return juce::GlyphArrangement::getStringWidth (font, text);
}

inline float runsWidth (const std::vector<TextRun>& runs)
{
    float total = 0.0f;
    for (const auto& run : runs)
        total += textWidth (run.font, run.text);
    return total;
}

// Desenha os trechos em sequência, alinhados dentro de area, sempre na mesma linha de base.
inline void drawRuns (
    juce::Graphics& g, const std::vector<TextRun>& runs, juce::Rectangle<float> area, TextAlign alignment)
{
    const auto total = runsWidth (runs);

    float x = area.getX();
    if (alignment == TextAlign::center)
        x = area.getCentreX() - total / 2.0f;
    else if (alignment == TextAlign::right)
        x = area.getRight() - total;

    for (const auto& run : runs)
    {
        const auto w = textWidth (run.font, run.text);
        g.setFont (run.font);
        g.setColour (run.colour);
        g.drawText (run.text, juce::Rectangle<float> (x, area.getY(), w + 2.0f, area.getHeight()),
            juce::Justification::centredLeft, false);
        x += w;
    }
}

// Como drawRuns, mas na linha de base dada: trechos de fontes diferentes (número e unidade) ficam alinhados pela base,
// não pelo centro. A base é a coordenada y do texto; GlyphArrangement desenha a partir dela.
inline void drawRunsOnBaseline (
    juce::Graphics& g, const std::vector<TextRun>& runs, float left, float right, float baselineY, TextAlign alignment)
{
    const auto total = runsWidth (runs);

    float x = left;
    if (alignment == TextAlign::center)
        x = (left + right) / 2.0f - total / 2.0f;
    else if (alignment == TextAlign::right)
        x = right - total;

    for (const auto& run : runs)
    {
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (run.font, run.text, x, baselineY);
        g.setColour (run.colour);
        glyphs.draw (g);
        x += textWidth (run.font, run.text);
    }
}

// Número com uma casa, vírgula decimal e sinal "−", como no protótipo. Zero arredondado sai sem sinal.
inline juce::String formatDecimal (float value)
{
    const auto rounded = std::round (value * 10.0f) / 10.0f;
    const auto digits = juce::String (static_cast<double> (std::abs (rounded)), 1).replace (".", ",");
    return rounded < 0.0f ? minusSign() + digits : digits;
}

// Knob "Amount": arco de −135° a +135°, disco e número no centro. Arraste vertical de 0–100 %.
class AmountKnob : public juce::Component
{
public:
    explicit AmountKnob (juce::RangedAudioParameter& parameter) :
        attachment (parameter, [this] (float v) { onParameterChanged (v); })
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        attachment.sendInitialUpdate();
    }

    // Nunca deixa um gesto do host aberto quando o knob some.
    ~AmountKnob() override { closeGesture(); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto d = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto centre = bounds.getCentre();
        const auto fraction = value / 100.0f;

        // Ângulo em graus: 0 é o topo e cresce no sentido horário, como no protótipo.
        const auto pointAt = [centre] (float radius, float degrees)
        {
            const auto a = juce::degreesToRadians (degrees);
            return centre + juce::Point<float> (radius * std::sin (a), -radius * std::cos (a));
        };
        const auto valueDegrees = -135.0f + 270.0f * fraction;

        // Geometria do SVG do protótipo (viewBox 200, R = 84, traço 14), escalada pelo diâmetro d: arco em 0,42 d e
        // traço de 14·d/200. Assim o knob fica igual em qualquer tamanho.
        const auto arcRadius = 0.42f * d;
        const juce::PathStrokeType arcStroke (0.07f * d, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, juce::degreesToRadians (-135.0f),
            juce::degreesToRadians (135.0f), true);
        g.setColour (theme::rule);
        g.strokePath (track, arcStroke);

        if (fraction > 0.0005f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, juce::degreesToRadians (-135.0f),
                juce::degreesToRadians (valueDegrees), true);
            g.setColour (theme::accent);
            g.strokePath (valueArc, arcStroke);
        }

        // Marcas: 11, por fora do arco (0,455 d), em textDim com 50 % de opacidade. As de múltiplos de 5 são maiores,
        // como no protótipo (raios 94 a 97 e 99 num viewBox de 200), com traço de 2·d/200.
        const juce::PathStrokeType tickStroke (0.01f * d, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour (theme::textDim.withAlpha (0.5f));
        for (int i = 0; i <= 10; ++i)
        {
            const auto degrees = -135.0f + 27.0f * static_cast<float> (i);
            const auto outer = i % 5 == 0 ? 0.495f * d : 0.485f * d;
            juce::Path tick;
            tick.startNewSubPath (pointAt (0.47f * d, degrees));
            tick.lineTo (pointAt (outer, degrees));
            g.strokePath (tick, tickStroke);
        }

        // Disco branco com sombra suave: raio 0,32 d (64 num viewBox de 200).
        juce::Path disc;
        disc.addEllipse (centre.x - 0.32f * d, centre.y - 0.32f * d, 0.64f * d, 0.64f * d);
        juce::DropShadow (juce::Colour (0x241c1b19), 4, { 0, 2 }).drawForPath (g, disc);
        g.setColour (theme::surface);
        g.fillPath (disc);

        // Ponteiro: de 0,24 d a 0,29 d, no ângulo do valor, com traço de 4·d/200.
        juce::Path pointer;
        pointer.startNewSubPath (pointAt (0.24f * d, valueDegrees));
        pointer.lineTo (pointAt (0.29f * d, valueDegrees));
        g.setColour (theme::text);
        g.strokePath (
            pointer, juce::PathStrokeType (0.02f * d, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Leitura no centro: número, "%" logo depois, e "AMOUNT" abaixo.
        // O número tem letter-spacing −0,02 em. O "%" fica na mesma linha de base do número, não centralizado.
        const auto numberHeight = 0.2f * d;
        const auto numberFont = theme::mono (numberHeight, 500).withExtraKerningFactor (-0.02f);
        const auto blockHeight = numberHeight + 6.0f + 12.0f;
        const auto top = centre.y - blockHeight / 2.0f;
        const auto baseline = top + numberFont.getAscent();
        const std::vector<TextRun> readout {
            { numberFont, juce::String (juce::roundToInt (value)), theme::text },
            { theme::mono (0.08f * d), juce::String ("%"), theme::textDim },
        };
        drawRunsOnBaseline (g, readout, bounds.getX(), bounds.getRight(), baseline, TextAlign::center);

        g.setColour (theme::textDim);
        g.setFont (theme::ui (10.0f, 700).withExtraKerningFactor (0.16f));
        g.drawText ("AMOUNT",
            juce::Rectangle<float> (bounds.getX(), top + numberHeight + 6.0f, bounds.getWidth(), 12.0f),
            juce::Justification::centred, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        closeGesture(); // um gesto novo nunca começa com outro ainda aberto
        dragLastY = e.y;
        dragValue = value;
        // Duplo clique não abre gesto: quem trata é mouseDoubleClick.
        if (e.getNumberOfClicks() < 2)
        {
            attachment.beginGesture();
            gestureOpen = true;
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (!gestureOpen)
            return;

        const auto factor = e.mods.isShiftDown() ? 0.1f : 0.5f; // Shift: ajuste fino
        dragValue = juce::jlimit (0.0f, 100.0f, dragValue + static_cast<float> (dragLastY - e.y) * factor);
        dragLastY = e.y;
        value = dragValue;
        attachment.setValueAsPartOfGesture (dragValue);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override { closeGesture(); }

    void mouseDoubleClick (const juce::MouseEvent&) override { attachment.setValueAsCompleteGesture (50.0f); }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (wheel.deltaY == 0.0f)
            return;

        const auto next = juce::jlimit (0.0f, 100.0f, value + (wheel.deltaY > 0.0f ? 1.0f : -1.0f));
        attachment.setValueAsCompleteGesture (next);
    }

private:
    void closeGesture()
    {
        if (gestureOpen)
        {
            attachment.endGesture();
            gestureOpen = false;
        }
    }

    void onParameterChanged (float v)
    {
        value = v;
        repaint();
    }

    float value = 50.0f;
    int dragLastY = 0;
    float dragValue = 50.0f;
    bool gestureOpen = false;
    juce::ParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmountKnob)
};

// Seletor de opções ligado a um parâmetro choice ou bool (índice 0 e 1).
class ChoiceSelector : public juce::Component
{
public:
    enum class Style
    {
        chips,
        segmented
    };

    ChoiceSelector (juce::RangedAudioParameter& parameter, juce::StringArray optionLabels, Style selectorStyle) :
        labels (std::move (optionLabels)),
        style (selectorStyle),
        attachment (parameter, [this] (float v) { onParameterChanged (v); })
    {
        setWantsKeyboardFocus (true);
        attachment.sendInitialUpdate();
    }

    // Largura natural do seletor, para o layout de quem chama.
    int getPreferredWidth() const
    {
        const auto count = labels.size();
        float total = 0.0f;
        for (const auto& label : labels)
            total += style == Style::chips ? textWidth (chipMeasureFont(), label) + 22.0f
                                           : textWidth (segmentMeasureFont(), label) + 20.0f;

        total += style == Style::chips ? 4.0f * static_cast<float> (std::max (0, count - 1))
                                       : 2.0f * static_cast<float> (std::max (0, count - 1)) + 4.0f;
        return static_cast<int> (std::ceil (total));
    }

    void resized() override { layoutCells(); }

    void paint (juce::Graphics& g) override
    {
        if (style == Style::chips)
            paintChips (g);
        else
            paintSegmented (g);
    }

    void mouseMove (const juce::MouseEvent& e) override { setHovered (indexAt (e.position)); }
    void mouseExit (const juce::MouseEvent&) override { setHovered (-1); }
    void mouseDown (const juce::MouseEvent& e) override { select (indexAt (e.position)); }

    bool keyPressed (const juce::KeyPress& key) override
    {
        const auto code = key.getKeyCode();
        if (code == juce::KeyPress::leftKey)
        {
            select (index - 1);
            return true;
        }
        if (code == juce::KeyPress::rightKey)
        {
            select (index + 1);
            return true;
        }
        return false;
    }

private:
    static juce::Font chipMeasureFont() { return theme::ui (11.0f, 700); }
    static juce::Font segmentMeasureFont() { return theme::ui (12.0f, 600); }

    void onParameterChanged (float v)
    {
        index = juce::jlimit (0, labels.size() - 1, juce::roundToInt (v));
        repaint();
    }

    void select (int i)
    {
        if (i < 0 || i >= labels.size() || i == index)
            return;
        attachment.setValueAsCompleteGesture (static_cast<float> (i));
    }

    void setHovered (int i)
    {
        if (i != hovered)
        {
            hovered = i;
            repaint();
        }
    }

    int indexAt (juce::Point<float> p) const
    {
        for (int i = 0; i < static_cast<int> (cells.size()); ++i)
            if (cells[static_cast<std::size_t> (i)].contains (p))
                return i;
        return -1;
    }

    // Chips: largura proporcional ao texto mais 22 px, preenchendo a largura. Segmentado: texto mais 20 px.
    void layoutCells()
    {
        cells.clear();
        const auto bounds = getLocalBounds().toFloat();
        const auto count = labels.size();
        if (count == 0)
            return;

        if (style == Style::chips)
        {
            constexpr float gap = 4.0f;
            std::vector<float> basis;
            float sum = 0.0f;
            for (const auto& label : labels)
            {
                basis.push_back (textWidth (chipMeasureFont(), label) + 22.0f);
                sum += basis.back();
            }

            const auto available = bounds.getWidth() - gap * static_cast<float> (count - 1);
            const auto y = bounds.getCentreY() - 14.0f;
            float x = bounds.getX();
            for (std::size_t i = 0; i < basis.size(); ++i)
            {
                const auto w = basis[i] / sum * available;
                cells.emplace_back (x, y, w, 28.0f);
                x += w + gap;
            }
        }
        else
        {
            const auto inner = bounds.reduced (2.0f);
            float x = inner.getX();
            for (const auto& label : labels)
            {
                const auto w = textWidth (segmentMeasureFont(), label) + 20.0f;
                cells.emplace_back (x, inner.getY(), w, inner.getHeight());
                x += w + 2.0f;
            }
        }
    }

    void paintChips (juce::Graphics& g)
    {
        for (int i = 0; i < static_cast<int> (cells.size()); ++i)
        {
            const auto cell = cells[static_cast<std::size_t> (i)];
            const auto& label = labels[i];

            juce::Path shape;
            shape.addRoundedRectangle (cell, 14.0f);

            if (i == index)
            {
                juce::DropShadow (juce::Colour (0x591f7a72), 8, { 0, 2 }).drawForPath (g, shape);
                g.setColour (theme::accent);
                g.fillPath (shape);

                g.setColour (juce::Colours::white);
                g.setFont (theme::ui (11.0f, 700));
                // Corte de 6 px à esquerda: o ✓ (x+1 a x+9) fica a pelo menos 4 px do rótulo, que sai quase
                // centralizado (3 px).
                g.drawText (label, cell.withTrimmedLeft (6.0f), juce::Justification::centred, false);
                theme::check (g, juce::Rectangle<float> (cell.getX() + 1.0f, cell.getCentreY() - 4.0f, 8.0f, 8.0f),
                    juce::Colours::white);
            }
            else
            {
                g.setColour (theme::bg);
                g.fillPath (shape);
                g.setColour (i == hovered ? theme::line : theme::surface2);
                g.drawRoundedRectangle (cell.reduced (0.5f), 13.5f, 1.0f);

                g.setColour (theme::text);
                g.setFont (theme::ui (11.0f, 500));
                g.drawText (label, cell.withTrimmedLeft (6.0f), juce::Justification::centred, false); // mesmo slot
            }
        }
    }

    void paintSegmented (juce::Graphics& g)
    {
        g.setColour (theme::surface2);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

        for (int i = 0; i < static_cast<int> (cells.size()); ++i)
        {
            const auto cell = cells[static_cast<std::size_t> (i)];
            const auto& label = labels[i];

            if (i == index)
            {
                juce::Path shape;
                shape.addRoundedRectangle (cell, 6.0f);
                juce::DropShadow (juce::Colour (0x1f1c1b19), 2, { 0, 1 }).drawForPath (g, shape);
                g.setColour (theme::surface);
                g.fillPath (shape);

                // Linha de 2 px em accent, por dentro do botão.
                g.saveState();
                g.reduceClipRegion (shape);
                g.setColour (theme::accent);
                g.fillRect (juce::Rectangle<float> (cell.getX(), cell.getBottom() - 2.0f, cell.getWidth(), 2.0f));
                g.restoreState();

                g.setColour (theme::text);
                g.setFont (theme::ui (12.0f, 600));
            }
            else
            {
                g.setColour (i == hovered ? theme::text : theme::textDim);
                g.setFont (theme::ui (12.0f, 500));
            }
            g.drawText (label, cell, juce::Justification::centred, false);
        }
    }

    juce::StringArray labels;
    Style style;
    int index = 0;
    int hovered = -1;
    std::vector<juce::Rectangle<float>> cells;
    juce::ParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoiceSelector)
};

// Mini-controle do Avançado: rótulo à esquerda, valor à direita e, opcionalmente, uma barra de 4 px.
class MiniControl : public juce::Component
{
public:
    MiniControl (juce::RangedAudioParameter& parameter, juce::String labelText,
        std::function<juce::String (float)> formatValue, bool showBar = true) :
        param (parameter),
        label (std::move (labelText)),
        format (std::move (formatValue)),
        barVisible (showBar),
        attachment (parameter, [this] (float v) { onParameterChanged (v); })
    {
        attachment.sendInitialUpdate();
    }

    // Nunca deixa um gesto do host aberto quando o mini-controle some.
    ~MiniControl() override { closeGesture(); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto normalised = param.convertTo0to1 (value);

        // Com barra: linha de 16 px, 10 px de espaço e barra de 4 px. Sem barra: só a linha.
        const auto blockHeight = barVisible ? 30.0f : 16.0f;
        const auto top = bounds.getY() + (bounds.getHeight() - blockHeight) / 2.0f;
        const auto row = juce::Rectangle<float> (bounds.getX(), top, bounds.getWidth(), 16.0f);

        g.setColour (theme::textDim);
        g.setFont (theme::ui (11.0f));
        g.drawText (label, row, juce::Justification::centredLeft, false);

        g.setColour (theme::text);
        g.setFont (theme::mono (11.0f, 500));
        g.drawText (format (value), row, juce::Justification::centredRight, false);

        if (barVisible)
        {
            const auto bar = juce::Rectangle<float> (bounds.getX(), top + 26.0f, bounds.getWidth(), 4.0f);
            g.setColour (theme::surface2);
            g.fillRoundedRectangle (bar, 2.0f);

            const auto filled = juce::jlimit (0.0f, 1.0f, normalised);
            if (filled > 0.0f)
            {
                g.setColour (theme::accent);
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * filled), 2.0f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        closeGesture(); // um gesto novo nunca começa com outro ainda aberto
        if (e.getNumberOfClicks() >= 2)
            return; // duplo clique: quem trata é mouseDoubleClick

        dragLastX = e.x;
        dragLastY = e.y;
        dragNorm = param.getValue();
        attachment.beginGesture();
        gestureOpen = true;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (!gestureOpen)
            return;

        if (barVisible)
        {
            // Largura inteira = de 0 a 1 normalizado; Shift: 0,2 vezes.
            const auto factor = e.mods.isShiftDown() ? 0.2f : 1.0f;
            const auto width = std::max (1.0f, static_cast<float> (getWidth()));
            dragNorm += static_cast<float> (e.x - dragLastX) / width * factor;
            dragLastX = e.x;
        }
        else
        {
            // Sem barra: 0,005 normalizado por pixel, para cima aumenta.
            dragNorm += static_cast<float> (dragLastY - e.y) * 0.005f;
            dragLastY = e.y;
        }

        dragNorm = juce::jlimit (0.0f, 1.0f, dragNorm);
        attachment.setValueAsPartOfGesture (param.convertFrom0to1 (dragNorm));
    }

    void mouseUp (const juce::MouseEvent&) override { closeGesture(); }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
    }

private:
    void closeGesture()
    {
        if (gestureOpen)
        {
            attachment.endGesture();
            gestureOpen = false;
        }
    }

    void onParameterChanged (float v)
    {
        value = v;
        repaint();
    }

    juce::RangedAudioParameter& param;
    juce::String label;
    std::function<juce::String (float)> format;
    bool barVisible;
    float value = 0.0f;
    float dragNorm = 0.0f;
    int dragLastX = 0;
    int dragLastY = 0;
    bool gestureOpen = false;
    juce::ParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiniControl)
};

} // namespace finale
