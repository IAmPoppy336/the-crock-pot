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
    auto setupRotary = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        s.setColour (juce::Slider::rotarySliderFillColourId, lidShine);
        s.setColour (juce::Slider::rotarySliderOutlineColourId, knobBrown);
        s.setColour (juce::Slider::thumbColourId, cream);
        s.setColour (juce::Slider::textBoxTextColourId, cream);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (s);
    };
    setupRotary (simmerDial);
    setupRotary (outputSlider);

    auto setupLabel = [this] (juce::Label& l, const juce::String& text, float alpha)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, cream.withAlpha (alpha));
        addAndMakeVisible (l);
    };
    setupLabel (simmerLabel, "SIMMER", 0.95f);
    setupLabel (outputLabel, "Output", 0.8f);
    setupLabel (hintLabel, "block knobs: Live's device panel (real UI lands in M3)", 0.45f);

    auto& apvts = processorRef.apvts;
    simmerAttachment = std::make_unique<SliderAttachment> (apvts, params::simmer,     simmerDial);
    outputAttachment = std::make_unique<SliderAttachment> (apvts, params::outputTrim, outputSlider);

    for (int i = 0; i < CrockPotProcessor::numBlocks; ++i)
    {
        auto& b = chainButtons[(size_t) i];
        b.setClickingTogglesState (false);
        b.setColour (juce::TextButton::buttonColourId, knobBrown);
        b.setColour (juce::TextButton::textColourOffId, cream);
        b.onClick = [this, i]
        {
            selectedSlot = (selectedSlot == i ? -1 : i);
            refreshChainButtons();
        };
        addAndMakeVisible (b);
    }

    auto setupMover = [this] (juce::TextButton& b, int delta)
    {
        b.setColour (juce::TextButton::buttonColourId, potShade);
        b.setColour (juce::TextButton::textColourOffId, cream);
        b.onClick = [this, delta] { moveSelected (delta); };
        addAndMakeVisible (b);
    };
    setupMover (moveLeft, -1);
    setupMover (moveRight, +1);

    processorRef.apvts.state.addListener (this);
    refreshChainButtons();

    setResizable (true, true);
    setResizeLimits (540, 420, 1500, 1100);
    setSize (660, 480);
}

CrockPotEditor::~CrockPotEditor()
{
    processorRef.apvts.state.removeListener (this);
}

//==============================================================================
void CrockPotEditor::valueTreePropertyChanged (juce::ValueTree&,
                                               const juce::Identifier& property)
{
    if (property == juce::Identifier (params::chainOrderProperty))
        refreshChainButtons();
}

void CrockPotEditor::valueTreeRedirected (juce::ValueTree&)
{
    refreshChainButtons();
}

void CrockPotEditor::refreshChainButtons()
{
    auto tokens = juce::StringArray::fromTokens (processorRef.getChainOrderString(), ",", "");

    for (int i = 0; i < CrockPotProcessor::numBlocks; ++i)
    {
        auto& b = chainButtons[(size_t) i];
        const int idx = (i < tokens.size() ? tokens[i].trim().getIntValue() : i);
        const auto safe = juce::jlimit (0, CrockPotProcessor::numBlocks - 1, idx);

        b.setButtonText (juce::String (CrockPotProcessor::blockNames[(size_t) safe]));
        b.setColour (juce::TextButton::buttonColourId,
                     i == selectedSlot ? lidShine : knobBrown);
    }
}

void CrockPotEditor::moveSelected (int delta)
{
    if (selectedSlot < 0)
        return;

    const int target = selectedSlot + delta;
    if (target < 0 || target >= CrockPotProcessor::numBlocks)
        return;

    auto tokens = juce::StringArray::fromTokens (processorRef.getChainOrderString(), ",", "");
    if (tokens.size() != CrockPotProcessor::numBlocks)
        return;

    tokens.getReference (selectedSlot).swapWith (tokens.getReference (target));
    selectedSlot = target;
    processorRef.setChainOrderString (tokens.joinIntoString (","));
    // listener fires → refreshChainButtons()
}

//==============================================================================
void CrockPotEditor::resized()
{
    auto bounds = getLocalBounds();

    // ---- bottom: chain strip -------------------------------------------------
    auto strip = bounds.removeFromBottom (66).reduced (12, 6);
    auto movers = strip.removeFromRight (64);
    moveLeft.setBounds  (movers.removeFromLeft (30).reduced (1, 14));
    moveRight.setBounds (movers.reduced (1, 14));

    hintLabel.setBounds (strip.removeFromBottom (16));
    const int bw = strip.getWidth() / CrockPotProcessor::numBlocks;
    for (auto& b : chainButtons)
        b.setBounds (strip.removeFromLeft (bw).reduced (2, 4));

    // ---- middle: Simmer + Output ----------------------------------------------
    auto controls = bounds.removeFromBottom (juce::jmax (150, bounds.getHeight() * 2 / 5));
    auto simmerArea = controls.removeFromLeft (controls.getWidth() * 3 / 5).reduced (10, 0);
    simmerLabel.setBounds (simmerArea.removeFromBottom (18));
    simmerDial.setBounds (simmerArea.reduced (4));

    auto outArea = controls.reduced (14, 10);
    outputLabel.setBounds (outArea.removeFromBottom (16));
    outputSlider.setBounds (outArea.withSizeKeepingCentre (
        juce::jmin (outArea.getWidth(), 110), juce::jmin (outArea.getHeight(), 110)));
}

//==============================================================================
void CrockPotEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();
    const float artH = h * 0.40f;

    g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f,
                                             bgBottom, 0.0f, h, false));
    g.fillAll();

    // ---- the pot (compact, upper area) ---------------------------------------
    const float cx     = w * 0.5f;
    const float potH   = artH * 0.42f;
    const float potW   = juce::jmin (potH * 1.6f, w * 0.36f);
    const float potTop = artH * 0.34f;
    const juce::Rectangle<float> body { cx - potW / 2.0f, potTop, potW, potH };

    g.setColour (lidShine.withAlpha (0.10f));
    g.fillEllipse (body.getX() - potW * 0.10f, body.getBottom() - potH * 0.12f,
                   potW * 1.2f, potH * 0.35f);

    g.setGradientFill (juce::ColourGradient (potBody, cx, body.getY(),
                                             potShade, cx, body.getBottom(), false));
    g.fillRoundedRectangle (body, potH * 0.18f);

    const float handleW = potW * 0.10f, handleH = potH * 0.16f;
    g.setColour (knobBrown);
    g.fillRoundedRectangle (body.getX() - handleW * 0.8f,
                            body.getY() + potH * 0.22f, handleW, handleH, handleH * 0.4f);
    g.fillRoundedRectangle (body.getRight() - handleW * 0.2f,
                            body.getY() + potH * 0.22f, handleW, handleH, handleH * 0.4f);

    const float lidH = potH * 0.34f;
    const juce::Rectangle<float> lid { body.getX() - potW * 0.04f,
                                       body.getY() - lidH * 0.55f,
                                       potW * 1.08f, lidH };
    g.setGradientFill (juce::ColourGradient (lidShine, cx, lid.getY(),
                                             lidColour, cx, lid.getBottom(), false));
    g.fillEllipse (lid);
    g.setColour (knobBrown);
    const float knobR = potW * 0.055f;
    g.fillEllipse (cx - knobR, lid.getCentreY() - lidH * 0.55f - knobR,
                   knobR * 2.0f, knobR * 2.0f);

    auto steamCurl = [&] (float x, float scale, float alpha)
    {
        juce::Path s;
        const float baseY = lid.getY() - h * 0.010f;
        const float rise  = artH * 0.30f * scale;
        s.startNewSubPath (x, baseY);
        s.cubicTo (x - w * 0.015f, baseY - rise * 0.4f,
                   x + w * 0.015f, baseY - rise * 0.6f,
                   x,              baseY - rise);
        g.setColour (cream.withAlpha (alpha));
        g.strokePath (s, juce::PathStrokeType (juce::jmax (2.0f, h * 0.006f),
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    };
    steamCurl (cx - potW * 0.22f, 0.85f, 0.30f);
    steamCurl (cx,                1.20f, 0.45f);
    steamCurl (cx + potW * 0.22f, 0.95f, 0.30f);

    // ---- title over the art ----------------------------------------------------
    const float titlePx = juce::jlimit (18.0f, 34.0f, artH * 0.17f);
    g.setColour (cream);
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                              titlePx, juce::Font::bold)));
    g.drawText ("The Crock-Pot",
                bounds.withTrimmedTop (artH * 0.78f).withHeight (titlePx * 1.25f),
                juce::Justification::centredTop, false);

    g.setColour (cream.withAlpha (0.55f));
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                              juce::jlimit (10.0f, 13.0f, h * 0.024f),
                                              juce::Font::plain)));
    g.drawText (juce::String ("v") + VERSION + " · M2 full pedalboard",
                getLocalBounds().reduced (10),
                juce::Justification::topRight, false);
}
