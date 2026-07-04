#pragma once

#include "BlockPanel.h"

/*  CookPage — the FX side of the pot (M3 layout, RC-20-informed):
    art + steam up top · big Simmer macro left · focused block panel right ·
    mono-maker row · reorderable chain strip with LIVE power tints below.
    Click a block in the strip → its controls appear in the panel.           */
class CookPage final : public juce::Component,
                       private juce::ValueTree::Listener,
                       private juce::Timer
{
public:
    explicit CookPage (CrockPotProcessor& p)
        : processorRef (p), blockPanel (p)
    {
        using namespace crockpot;

        // ---- Simmer ------------------------------------------------------------
        simmerDial.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        simmerDial.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        addAndMakeVisible (simmerDial);
        simmerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processorRef.apvts, params::simmer, simmerDial);
        if (auto* param = processorRef.apvts.getParameter (params::simmer))
            simmerDial.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

        simmerLabel.setText ("SIMMER", juce::dontSendNotification);
        simmerLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (simmerLabel);

        addAndMakeVisible (blockPanel);

        // ---- mono row -------------------------------------------------------------
        monoToggle.setButtonText ("Mono Floor");
        addAndMakeVisible (monoToggle);
        monoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            processorRef.apvts, params::monoOn, monoToggle);

        monoFreq.setSliderStyle (juce::Slider::LinearHorizontal);
        monoFreq.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 16);
        addAndMakeVisible (monoFreq);
        monoFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processorRef.apvts, params::monoFreq, monoFreq);
        if (auto* param = processorRef.apvts.getParameter (params::monoFreq))
            monoFreq.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

        // ---- chain strip -------------------------------------------------------------
        for (int i = 0; i < CrockPotProcessor::numBlocks; ++i)
        {
            auto& b = chainButtons[(size_t) i];
            b.onClick = [this, i]
            {
                selectedSlot = i;
                blockPanel.setBlock (slotToBlock[(size_t) i]);
                for (auto& t : lastTint) t = -1;
            };
            addAndMakeVisible (b);
        }

        auto setupMover = [this] (juce::TextButton& b, int delta)
        {
            b.setColour (juce::TextButton::buttonColourId, crockpot::potShade);
            b.onClick = [this, delta] { moveSelected (delta); };
            addAndMakeVisible (b);
        };
        setupMover (moveLeft, -1);
        setupMover (moveRight, +1);

        hintLabel.setText ("lit block = cooking - click a block to open its knobs",
                           juce::dontSendNotification);
        hintLabel.setJustificationType (juce::Justification::centred);
        hintLabel.setColour (juce::Label::textColourId, cream.withAlpha (0.45f));
        addAndMakeVisible (hintLabel);

        // ---- plumbing ---------------------------------------------------------------
        simmerRaw = processorRef.apvts.getRawParameterValue (params::simmer);
        const std::array<const char*, CrockPotProcessor::numBlocks> bypassIds {
            params::satBypass, params::resBypass, params::tapeBypass, params::chorusBypass,
            params::tremBypass, params::revBypass, params::delayBypass, params::verbBypass,
            params::skimBypass };
        for (int i = 0; i < CrockPotProcessor::numBlocks; ++i)
            bypassRaw[(size_t) i] = processorRef.apvts.getRawParameterValue (bypassIds[(size_t) i]);

        processorRef.apvts.state.addListener (this);
        refreshChainButtons();
        startTimerHz (30);
    }

    ~CookPage() override
    {
        processorRef.apvts.state.removeListener (this);
    }

    //==========================================================================
    void resized() override
    {
        auto r = getLocalBounds();

        auto strip = r.removeFromBottom (64).reduced (10, 4);
        auto movers = strip.removeFromRight (60);
        moveLeft.setBounds  (movers.removeFromLeft (28).reduced (1, 12));
        moveRight.setBounds (movers.reduced (1, 12));
        hintLabel.setBounds (strip.removeFromBottom (15));
        const int bw = strip.getWidth() / CrockPotProcessor::numBlocks;
        for (auto& b : chainButtons)
            b.setBounds (strip.removeFromLeft (bw).reduced (2, 3));

        auto monoRow = r.removeFromBottom (26).reduced (12, 1);
        monoToggle.setBounds (monoRow.removeFromLeft (110));
        monoFreq.setBounds (monoRow.removeFromLeft (juce::jmin (240, monoRow.getWidth())));

        artArea = r.removeFromTop (juce::jmax (90, r.getHeight() * 2 / 5));

        auto controls = r.reduced (8, 2);
        auto simmerArea = controls.removeFromLeft (controls.getWidth() * 2 / 5);
        simmerLabel.setBounds (simmerArea.removeFromBottom (16));
        simmerDial.setBounds (simmerArea);
        blockPanel.setBounds (controls.reduced (4, 0));
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        using namespace crockpot;

        const auto a  = artArea.toFloat();
        if (a.isEmpty()) return;
        const float w = (float) getWidth();
        const float cx = a.getCentreX();

        const float potH   = a.getHeight() * 0.44f;
        const float potW   = juce::jmin (potH * 1.6f, a.getWidth() * 0.34f);
        const float potTop = a.getY() + a.getHeight() * 0.34f;
        const juce::Rectangle<float> body { cx - potW / 2.0f, potTop, potW, potH };

        const float simmer01 = (simmerRaw != nullptr ? simmerRaw->load() * 0.01f : 0.0f);

        // ember glow (Simmer + audio level)
        const float emberA = juce::jlimit (0.06f, 0.40f,
                                           0.08f + 0.14f * simmer01 + 0.20f * glowLevel);
        const auto emberC  = lidShine.interpolatedWith (ember, 0.5f * simmer01 + 0.3f * glowLevel);
        g.setColour (emberC.withAlpha (emberA));
        g.fillEllipse (body.getX() - potW * 0.10f, body.getBottom() - potH * 0.12f,
                       potW * 1.2f, potH * 0.35f);
        g.setColour (emberC.withAlpha (emberA * 0.5f));
        g.fillEllipse (body.getX() - potW * 0.22f, body.getBottom() - potH * 0.06f,
                       potW * 1.44f, potH * 0.5f);

        // body + handles + lid
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

        // living steam
        auto steamCurl = [&] (float x, float scale, float alpha, float phaseOffset)
        {
            const float sway  = std::sin (steamPhase + phaseOffset);
            const float sway2 = std::sin (steamPhase * 0.63f + phaseOffset * 1.7f);

            juce::Path s;
            const float baseY = lid.getY() - 4.0f;
            const float rise  = a.getHeight() * (0.30f + 0.10f * simmer01) * scale
                              * (1.0f + 0.06f * sway2);
            const float drift = w * 0.012f;

            s.startNewSubPath (x, baseY);
            s.cubicTo (x - drift * (1.0f + 0.5f * sway),  baseY - rise * 0.4f,
                       x + drift * (1.0f + 0.5f * sway2), baseY - rise * 0.6f,
                       x + drift * sway * 0.8f,           baseY - rise);

            g.setColour (cream.withAlpha (alpha * (0.75f + 0.5f * simmer01)
                                                * (0.9f + 0.1f * sway2)));
            g.strokePath (s, juce::PathStrokeType (juce::jmax (2.0f, a.getHeight() * 0.012f),
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        };
        steamCurl (cx - potW * 0.22f, 0.85f, 0.30f, 0.0f);
        steamCurl (cx,                1.20f, 0.45f, 2.1f);
        steamCurl (cx + potW * 0.22f, 0.95f, 0.30f, 4.2f);
        if (simmer01 > 0.5f)
            steamCurl (cx + potW * 0.38f, 0.70f, (simmer01 - 0.5f) * 0.6f, 1.3f);
    }

private:
    //==========================================================================
    void timerCallback() override
    {
        steamPhase += 0.045f;
        if (steamPhase > juce::MathConstants<float>::twoPi)
            steamPhase -= juce::MathConstants<float>::twoPi;

        glowLevel = 0.85f * glowLevel
                  + 0.15f * juce::jlimit (0.0f, 1.0f, processorRef.getOutputLevel());

        for (int i = 0; i < CrockPotProcessor::numBlocks; ++i)
        {
            const bool bypassed = bypassRaw[(size_t) slotToBlock[(size_t) i]]->load() > 0.5f;
            const int tint = (i == selectedSlot ? 2 : (bypassed ? 0 : 1));

            if (tint != lastTint[(size_t) i])
            {
                using namespace crockpot;
                lastTint[(size_t) i] = tint;
                auto& b = chainButtons[(size_t) i];
                b.setColour (juce::TextButton::buttonColourId,
                             tint == 2 ? lidShine : tint == 1 ? potBody
                                                              : bgTop.brighter (0.06f));
                b.setColour (juce::TextButton::textColourOffId,
                             tint == 0 ? cream.withAlpha (0.40f) : cream);
            }
        }

        repaint (artArea);
    }

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& property) override
    {
        if (property == juce::Identifier (params::chainOrderProperty))
            refreshChainButtons();
    }

    void valueTreeRedirected (juce::ValueTree&) override { refreshChainButtons(); }

    void refreshChainButtons()
    {
        auto tokens = juce::StringArray::fromTokens (processorRef.getChainOrderString(), ",", "");

        for (int i = 0; i < CrockPotProcessor::numBlocks; ++i)
        {
            const int idx  = (i < tokens.size() ? tokens[i].trim().getIntValue() : i);
            const int safe = juce::jlimit (0, CrockPotProcessor::numBlocks - 1, idx);

            slotToBlock[(size_t) i] = safe;
            lastTint[(size_t) i] = -1;
            chainButtons[(size_t) i].setButtonText (
                juce::String (CrockPotProcessor::blockNames[(size_t) safe]));
        }
    }

    void moveSelected (int delta)
    {
        if (selectedSlot < 0) return;
        const int target = selectedSlot + delta;
        if (target < 0 || target >= CrockPotProcessor::numBlocks) return;

        auto tokens = juce::StringArray::fromTokens (processorRef.getChainOrderString(), ",", "");
        if (tokens.size() != CrockPotProcessor::numBlocks) return;

        tokens.getReference (selectedSlot).swapWith (tokens.getReference (target));
        selectedSlot = target;
        processorRef.setChainOrderString (tokens.joinIntoString (","));
    }

    //==========================================================================
    CrockPotProcessor& processorRef;

    juce::Rectangle<int> artArea;
    juce::Slider simmerDial;
    juce::Label simmerLabel, hintLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> simmerAttachment;

    BlockPanel blockPanel;

    juce::ToggleButton monoToggle;
    juce::Slider monoFreq;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> monoFreqAttachment;

    std::array<juce::TextButton, CrockPotProcessor::numBlocks> chainButtons;
    juce::TextButton moveLeft { "<" }, moveRight { ">" };
    int selectedSlot = 0;

    float steamPhase = 0.0f, glowLevel = 0.0f;
    std::atomic<float>* simmerRaw = nullptr;
    std::array<std::atomic<float>*, CrockPotProcessor::numBlocks> bypassRaw {};
    std::array<int, CrockPotProcessor::numBlocks> slotToBlock { 0,1,2,3,4,5,6,7 };
    std::array<int, CrockPotProcessor::numBlocks> lastTint { -1,-1,-1,-1,-1,-1,-1,-1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CookPage)
};
