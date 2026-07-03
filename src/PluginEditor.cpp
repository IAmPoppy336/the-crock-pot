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
    setupLabel (hintLabel, "lit block = cooking · knobs live in Live's device panel until M3", 0.45f);

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

    // pizzazz plumbing: cached atomics (resolved once) + the 30 fps heartbeat
    simmerRaw = apvts.getRawParameterValue (params::simmer);
    const std::array<const char*, CrockPotProcessor::numBlocks> bypassIds {
        params::satBypass, params::resBypass, params::tapeBypass, params::chorusBypass,
        params::tremBypass, params::revBypass, params::delayBypass, params::verbBypass };
    for (int i = 0; i < CrockPotProcessor::numBlocks; ++i)
        bypassRaw[(size_t) i] = apvts.getRawParameterValue (bypassIds[(size_t) i]);

    refreshChainButtons();
    startTimerHz (30);

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

        slotToBlock[(size_t) i] = safe;
        lastTint[(size_t) i] = -1;   // force the timer to re-tint

        b.setButtonText (juce::String (CrockPotProcessor::blockNames[(size_t) safe]));
    }
}

void CrockPotEditor::timerCallback()
{
    // steam sway + ember smoothing
    steamPhase += 0.045f;
    if (steamPhase > 6.2831853f) steamPhase -= 6.2831853f;

    glowLevel = 0.85f * glowLevel
              + 0.15f * juce::jlimit (0.0f, 1.0f, processorRef.getOutputLevel());

    // chain buttons wear their power state: lit = cooking, dim = bypassed
    for (int i = 0; i < CrockPotProcessor::numBlocks; ++i)
    {
        const bool bypassed = bypassRaw[(size_t) slotToBlock[(size_t) i]]->load() > 0.5f;
        const int tint = (i == selectedSlot ? 2 : (bypassed ? 0 : 1));

        if (tint != lastTint[(size_t) i])
        {
            lastTint[(size_t) i] = tint;
            auto& b = chainButtons[(size_t) i];
            b.setColour (juce::TextButton::buttonColourId,
                         tint == 2 ? lidShine
                       : tint == 1 ? potBody
                                   : bgTop.brighter (0.06f));
            b.setColour (juce::TextButton::textColourOffId,
                         tint == 0 ? cream.withAlpha (0.40f) : cream);
        }
    }

    repaint();   // steam + ember live in paint()
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

    // ---- ember glow: breathes with Simmer + the audio actually flowing ------
    const float simmer01 = (simmerRaw != nullptr ? simmerRaw->load() * 0.01f : 0.0f);
    const float emberA   = juce::jlimit (0.06f, 0.40f,
                                         0.08f + 0.14f * simmer01 + 0.20f * glowLevel);
    const auto  ember    = lidShine.interpolatedWith (juce::Colour (0xffe0662a),
                                                      0.5f * simmer01 + 0.3f * glowLevel);
    g.setColour (ember.withAlpha (emberA));
    g.fillEllipse (body.getX() - potW * 0.10f, body.getBottom() - potH * 0.12f,
                   potW * 1.2f, potH * 0.35f);
    g.setColour (ember.withAlpha (emberA * 0.5f));
    g.fillEllipse (body.getX() - potW * 0.22f, body.getBottom() - potH * 0.06f,
                   potW * 1.44f, potH * 0.5f);

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

    // ---- living steam: sways on the timer, thickens as Simmer rises ---------
    auto steamCurl = [&] (float x, float scale, float alpha, float phaseOffset)
    {
        const float sway  = std::sin (steamPhase + phaseOffset);
        const float sway2 = std::sin (steamPhase * 0.63f + phaseOffset * 1.7f);

        juce::Path s;
        const float baseY = lid.getY() - h * 0.010f;
        const float rise  = artH * (0.30f + 0.10f * simmer01) * scale
                          * (1.0f + 0.06f * sway2);
        const float drift = w * 0.012f;

        s.startNewSubPath (x, baseY);
        s.cubicTo (x - drift * (1.0f + 0.5f * sway),  baseY - rise * 0.4f,
                   x + drift * (1.0f + 0.5f * sway2), baseY - rise * 0.6f,
                   x + drift * sway * 0.8f,           baseY - rise);

        g.setColour (cream.withAlpha (alpha * (0.75f + 0.5f * simmer01)
                                            * (0.9f + 0.1f * sway2)));
        g.strokePath (s, juce::PathStrokeType (juce::jmax (2.0f, h * 0.006f),
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    };
    steamCurl (cx - potW * 0.22f, 0.85f, 0.30f, 0.0f);
    steamCurl (cx,                1.20f, 0.45f, 2.1f);
    steamCurl (cx + potW * 0.22f, 0.95f, 0.30f, 4.2f);
    if (simmer01 > 0.5f)   // hard simmer earns a fourth wisp
        steamCurl (cx + potW * 0.38f, 0.70f, (simmer01 - 0.5f) * 0.6f, 1.3f);

    // ---- title over the art (soft shadow for depth) -----------------------------
    const float titlePx = juce::jlimit (18.0f, 34.0f, artH * 0.17f);
    const auto titleArea = bounds.withTrimmedTop (artH * 0.78f).withHeight (titlePx * 1.25f);
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                              titlePx, juce::Font::bold)));
    g.setColour (bgTop.withAlpha (0.55f));
    g.drawText ("The Crock-Pot", titleArea.translated (0.0f, 2.0f),
                juce::Justification::centredTop, false);
    g.setColour (cream);
    g.drawText ("The Crock-Pot", titleArea,
                juce::Justification::centredTop, false);

    g.setColour (cream.withAlpha (0.55f));
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                              juce::jlimit (10.0f, 13.0f, h * 0.024f),
                                              juce::Font::plain)));
    g.drawText (juce::String ("v") + VERSION + " · pedalboard + splitter engine",
                getLocalBounds().reduced (10),
                juce::Justification::topRight, false);
}
