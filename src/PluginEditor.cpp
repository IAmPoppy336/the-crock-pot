#include "PluginEditor.h"

//==============================================================================
CrockPotEditor::CrockPotEditor (CrockPotProcessor& p)
    : AudioProcessorEditor (p), processorRef (p),
      cookPage (p), splitPage()
{
    setLookAndFeel (&lookAndFeel);

    // ---- header controls ------------------------------------------------------
    cookTab.onClick  = [this] { setPage (0); };
    splitTab.onClick = [this] { setPage (1); };
    addAndMakeVisible (cookTab);
    addAndMakeVisible (splitTab);

    shakeButton.setColour (juce::TextButton::buttonColourId, crockpot::potShade);
    shakeButton.onClick = [this] { shakeThePot(); };
    addAndMakeVisible (shakeButton);

    outputKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    outputKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    outputKnob.setPopupDisplayEnabled (true, true, this);
    addAndMakeVisible (outputKnob);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, params::outputTrim, outputKnob);

    // ---- pages ------------------------------------------------------------------
    addAndMakeVisible (cookPage);
    addChildComponent (splitPage);   // hidden until its tab is chosen
    setPage (0);

    setResizable (true, true);
    setResizeLimits (640, 480, 1600, 1200);
    setSize (780, 560);
}

CrockPotEditor::~CrockPotEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void CrockPotEditor::setPage (int newPage)
{
    currentPage = juce::jlimit (0, 1, newPage);
    cookPage.setVisible (currentPage == 0);
    splitPage.setVisible (currentPage == 1);

    using namespace crockpot;
    cookTab.setColour  (juce::TextButton::buttonColourId,
                        currentPage == 0 ? lidShine : knobBrown);
    splitTab.setColour (juce::TextButton::buttonColourId,
                        currentPage == 1 ? lidShine : knobBrown);
}

//==============================================================================
void CrockPotEditor::shakeThePot()
{
    //  Shake the Pot v1 (M3): randomize block character + power, musically
    //  bounded. Never touches Simmer, Mono, Output, or chain order.
    //  Per-block locks are the M3-polish follow-up.
    auto& rng = juce::Random::getSystemRandom();

    auto set01 = [this, &rng] (const char* id, float lo, float hi)
    {
        if (auto* param = processorRef.apvts.getParameter (id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (juce::jmap (rng.nextFloat(), lo, hi));
            param->endChangeGesture();
        }
    };

    auto setPower = [this, &rng] (const char* bypassParamId, float onChance)
    {
        if (auto* param = processorRef.apvts.getParameter (bypassParamId))
        {
            const bool on = rng.nextFloat() < onChance;
            param->beginChangeGesture();
            param->setValueNotifyingHost (on ? 0.0f : 1.0f);   // bypass = !on
            param->endChangeGesture();
        }
    };

    // saturation always cooks; the rest roll dice
    setPower (params::satBypass,    1.00f);
    setPower (params::resBypass,    0.35f);
    setPower (params::tapeBypass,   0.60f);
    setPower (params::chorusBypass, 0.40f);
    setPower (params::tremBypass,   0.30f);
    setPower (params::revBypass,    0.25f);
    setPower (params::delayBypass,  0.50f);
    setPower (params::verbBypass,   0.55f);

    struct R { const char* id; float lo, hi; };
    static constexpr R rolls[] = {
        { params::satDrive,      0.20f, 0.70f }, { params::satMix,      0.50f, 1.00f },
        { params::resDown,       0.15f, 0.65f }, { params::resCrush,    0.10f, 0.60f },
        { params::resMix,        0.30f, 0.80f },
        { params::tapeWarmth,    0.20f, 0.70f }, { params::tapeWobble,  0.05f, 0.50f },
        { params::tapeTone,      0.35f, 0.90f }, { params::tapeMix,     0.25f, 0.75f },
        { params::chorusRate,    0.10f, 0.60f }, { params::chorusDepth, 0.20f, 0.70f },
        { params::chorusMix,     0.25f, 0.65f },
        { params::tremRate,      0.20f, 0.70f }, { params::tremDepth,   0.25f, 0.80f },
        { params::tremMix,       0.50f, 1.00f },
        { params::revWindow,     0.20f, 0.80f }, { params::revMix,      0.20f, 0.60f },
        { params::delayTime,     0.15f, 0.70f }, { params::delayFeedback,0.15f, 0.60f },
        { params::delayTone,     0.30f, 0.85f }, { params::delayMix,    0.20f, 0.55f },
        { params::verbSize,      0.25f, 0.85f }, { params::verbDamp,    0.20f, 0.75f },
        { params::verbWidth,     0.50f, 1.00f }, { params::verbMix,     0.15f, 0.50f },
    };
    for (const auto& r : rolls)
        set01 (r.id, r.lo, r.hi);
}

//==============================================================================
void CrockPotEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (54).reduced (10, 8);

    // right side: output knob + shake
    outputKnob.setBounds (header.removeFromRight (44));
    header.removeFromRight (6);
    shakeButton.setBounds (header.removeFromRight (118).reduced (0, 4));
    header.removeFromRight (10);

    // centre-left: tabs after the title space
    header.removeFromLeft (170);   // title painted here
    cookTab.setBounds  (header.removeFromLeft (76).reduced (0, 4));
    header.removeFromLeft (4);
    splitTab.setBounds (header.removeFromLeft (76).reduced (0, 4));

    cookPage.setBounds (r);
    splitPage.setBounds (r);
}

//==============================================================================
void CrockPotEditor::paint (juce::Graphics& g)
{
    using namespace crockpot;

    g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f,
                                             bgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // header title + version
    auto header = getLocalBounds().removeFromTop (54).reduced (12, 6);
    g.setColour (cream);
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                              20.0f, juce::Font::bold)));
    g.drawText ("The Crock-Pot", header.removeFromTop (24),
                juce::Justification::topLeft, false);
    g.setColour (cream.withAlpha (0.5f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (juce::String ("v") + VERSION + " · cook & split",
                header, juce::Justification::topLeft, false);

    // header divider
    g.setColour (lidShine.withAlpha (0.25f));
    g.drawHorizontalLine (54, 8.0f, (float) getWidth() - 8.0f);
}
