#pragma once

#include <BinaryData.h>

#include <juce_graphics/juce_graphics.h>

namespace finale::theme
{

// Cores do protótipo docs/ui/prototipos/v9-marfim-final.html.
inline const juce::Colour bg { 0xfff4f1ea };
inline const juce::Colour surface { 0xffffffff };
inline const juce::Colour surface2 { 0xffece7dc };
inline const juce::Colour rule { 0xffd9d2c4 };
inline const juce::Colour line { 0xffbdb5a4 };
inline const juce::Colour text { 0xff1c1b19 };
inline const juce::Colour textDim { 0xff5e5a52 };
inline const juce::Colour accent { 0xff1f7a72 };
inline const juce::Colour warn { 0xffb7791f };
inline const juce::Colour danger { 0xffc53030 };

// Sora em 400, 600, 700 e 800. 500 usa o 600 e 900 usa o 800; abaixo de 500 cai no 400.
inline juce::Font ui (float px, int weight = 400)
{
    static const auto sora400
        = juce::Typeface::createSystemTypefaceFor (BinaryData::Sora400_ttf, BinaryData::Sora400_ttfSize);
    static const auto sora600
        = juce::Typeface::createSystemTypefaceFor (BinaryData::Sora600_ttf, BinaryData::Sora600_ttfSize);
    static const auto sora700
        = juce::Typeface::createSystemTypefaceFor (BinaryData::Sora700_ttf, BinaryData::Sora700_ttfSize);
    static const auto sora800
        = juce::Typeface::createSystemTypefaceFor (BinaryData::Sora800_ttf, BinaryData::Sora800_ttfSize);

    const auto& face = weight >= 800 ? sora800 : weight >= 700 ? sora700 : weight >= 500 ? sora600 : sora400;
    return juce::Font (juce::FontOptions (face).withPointHeight (px));
}

// IBM Plex Mono em 400 e 500 (números). 500 ou mais usa o 500.
inline juce::Font mono (float px, int weight = 400)
{
    static const auto plex400
        = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMono400_ttf, BinaryData::IBMPlexMono400_ttfSize);
    static const auto plex500
        = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMono500_ttf, BinaryData::IBMPlexMono500_ttfSize);

    const auto& face = weight >= 500 ? plex500 : plex400;
    return juce::Font (juce::FontOptions (face).withPointHeight (px));
}

// O "box branco": sombra em duas camadas, fundo surface, raio 12, borda de 1 px e, opcionalmente,
// uma faixa de 5 px em accent no topo, recortada pelo mesmo retângulo arredondado.
inline void card (juce::Graphics& g, juce::Rectangle<float> r, bool accentStrip)
{
    constexpr float radius = 12.0f;

    juce::Path shape;
    shape.addRoundedRectangle (r, radius);

    juce::DropShadow (juce::Colour (0x1a1c1b19), 28, { 0, 10 }).drawForPath (g, shape);
    juce::DropShadow (juce::Colour (0x0f1c1b19), 6, { 0, 2 }).drawForPath (g, shape);

    g.setColour (surface);
    g.fillPath (shape);

    // Borda dentro do retângulo, para não vazar 0,5 px para fora.
    g.setColour (surface2);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);

    if (accentStrip)
    {
        g.saveState();
        g.reduceClipRegion (shape);
        g.setColour (accent);
        g.fillRect (r.withHeight (5.0f));
        g.restoreState();
    }
}

// O ✓ como path, nunca como caractere: a Sora latin não tem o glifo.
inline void check (juce::Graphics& g, juce::Rectangle<float> box, juce::Colour colour)
{
    juce::Path tick;
    tick.startNewSubPath (box.getX() + 0.15f * box.getWidth(), box.getY() + 0.55f * box.getHeight());
    tick.lineTo (box.getX() + 0.40f * box.getWidth(), box.getY() + 0.80f * box.getHeight());
    tick.lineTo (box.getX() + 0.85f * box.getWidth(), box.getY() + 0.20f * box.getHeight());

    g.setColour (colour);
    g.strokePath (tick, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

} // namespace finale::theme
