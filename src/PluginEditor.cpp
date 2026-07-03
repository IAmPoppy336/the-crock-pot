#include "PluginEditor.h"

namespace
{
    // The Crock-Pot palette — warm kitchen browns + cream.
    const juce::Colour bgTop     { 0xff2e1d11 };
    const juce::Colour bgBottom  { 0xff523018 };
    const juce::Colour potBody   { 0xffb85c2a };
    const juce::Colour potShade  { 0xff8a4420 };
    const juce::Colour lidColour { 0xff9c4f24 };
    const juce::Colour lidShine  { 0xffc97a3e };
    const juce::Colour knobBrown { 0xff5e3013 };
    const juce::Colour cream     { 0xfff3e7cf };
}

//==============================================================================
CrockPotEditor::CrockPotEditor (CrockPotProcessor& p)
    : AudioProcessorEditor (p), processorRef (p)
{
    juce::ignoreUnused (processorRef);

    // Resizable from day one (M3's exit criterion starts here).
    setResizable (true, true);
    setResizeLimits (420, 300, 1400, 1000);
    setSize (560, 400);
}

//==============================================================================
void CrockPotEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // ---- warm gradient background ------------------------------------------
    g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f,
                                             bgBottom, 0.0f, h, false));
    g.fillAll();

    // ---- the pot ------------------------------------------------------------
    const float cx     = w * 0.5f;
    const float potH   = h * 0.30f;
    const float potW   = juce::jmin (potH * 1.6f, w * 0.5f);
    const float potTop = h * 0.28f;
    const juce::Rectangle<float> body { cx - potW / 2.0f, potTop, potW, potH };

    // soft simmer glow under the pot
    g.setColour (lidShine.withAlpha (0.10f));
    g.fillEllipse (body.getX() - potW * 0.10f, body.getBottom() - potH * 0.12f,
                   potW * 1.2f, potH * 0.35f);

    // body (vertical shade: lit top, darker base)
    g.setGradientFill (juce::ColourGradient (potBody, cx, body.getY(),
                                             potShade, cx, body.getBottom(), false));
    g.fillRoundedRectangle (body, potH * 0.18f);

    // side handles
    const float handleW = potW * 0.10f, handleH = potH * 0.16f;
    g.setColour (knobBrown);
    g.fillRoundedRectangle (body.getX() - handleW * 0.8f,
                            body.getY() + potH * 0.22f, handleW, handleH, handleH * 0.4f);
    g.fillRoundedRectangle (body.getRight() - handleW * 0.2f,
                            body.getY() + potH * 0.22f, handleW, handleH, handleH * 0.4f);

    // lid + knob
    const float lidH = potH * 0.34f;
    const juce::Rectangle<float> lid { body.getX() - potW * 0.04f,
                                       body.getY() - lidH * 0.55f,
                                       potW * 1.08f, lidH };
    g.setGradientFill (juce::ColourGradient (lidShine, cx, lid.getY(),
                                             lidColour, cx, lid.getBottom(), false));
    g.fillEllipse (lid);
    g.setColour (knobBrown);
    const float knobR = potW * 0.055f;
    g.fillEllipse (cx - knobR, lid.getCentreY() - lidH * 0.55f - knobR, knobR * 2.0f, knobR * 2.0f);

    // ---- steam (three lazy curls, statically simmering until M5) ------------
    auto steamCurl = [&] (float x, float scale, float alpha)
    {
        juce::Path s;
        const float baseY = lid.getY() - h * 0.015f;
        const float rise  = h * 0.16f * scale;
        s.startNewSubPath (x, baseY);
        s.cubicTo (x - w * 0.02f, baseY - rise * 0.4f,
                   x + w * 0.02f, baseY - rise * 0.6f,
                   x,             baseY - rise);
        g.setColour (cream.withAlpha (alpha));
        g.strokePath (s, juce::PathStrokeType (juce::jmax (2.0f, h * 0.008f),
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    };
    steamCurl (cx - potW * 0.22f, 0.85f, 0.30f);
    steamCurl (cx,                1.20f, 0.45f);
    steamCurl (cx + potW * 0.22f, 0.95f, 0.30f);

    // ---- words ---------------------------------------------------------------
    const float titlePx = juce::jlimit (22.0f, 54.0f, h * 0.105f);
    g.setColour (cream);
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                              titlePx, juce::Font::bold)));
    g.drawText ("The Crock-Pot",
                bounds.withTrimmedTop (h * 0.66f).withHeight (titlePx * 1.2f),
                juce::Justification::centredTop, false);

    g.setColour (cream.withAlpha (0.75f));
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                              juce::jlimit (12.0f, 20.0f, h * 0.042f),
                                              juce::Font::italic)));
    g.drawText ("low · slow · warm — hello from milestone 0",
                bounds.withTrimmedTop (h * 0.66f + titlePx * 1.35f).withHeight (h * 0.08f),
                juce::Justification::centredTop, false);

    // version stamp (About-panel duty until there IS an About panel)
    g.setColour (cream.withAlpha (0.55f));
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                              juce::jlimit (10.0f, 14.0f, h * 0.03f),
                                              juce::Font::plain)));
    g.drawText (juce::String ("v") + VERSION + " · M0 scaffold",
                getLocalBounds().reduced (10),
                juce::Justification::bottomRight, false);
}
