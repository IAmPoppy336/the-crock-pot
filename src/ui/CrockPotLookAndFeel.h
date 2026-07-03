#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/*  CrockPotLookAndFeel — the pot's visual voice (M3, native only — D3).
    Warm ceramic knobs, ember arcs, pill toggles. Readability first,
    delight in moments (Beat D): no chrome, no glass, no clutter.          */
namespace crockpot
{
    const juce::Colour bgTop     { 0xff2e1d11 };
    const juce::Colour bgBottom  { 0xff523018 };
    const juce::Colour potBody   { 0xffb85c2a };
    const juce::Colour potShade  { 0xff8a4420 };
    const juce::Colour lidColour { 0xff9c4f24 };
    const juce::Colour lidShine  { 0xffc97a3e };
    const juce::Colour knobBrown { 0xff5e3013 };
    const juce::Colour ember     { 0xffe0662a };
    const juce::Colour cream     { 0xfff3e7cf };
}

class CrockPotLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    CrockPotLookAndFeel()
    {
        using namespace crockpot;
        setColour (juce::Slider::textBoxTextColourId, cream);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, cream);
        setColour (juce::TextButton::textColourOffId, cream);
        setColour (juce::TextButton::textColourOnId, cream);
        setColour (juce::TextButton::buttonColourId, knobBrown);
        setColour (juce::ToggleButton::textColourId, cream);
        setColour (juce::ToggleButton::tickColourId, cream);
        setColour (juce::PopupMenu::backgroundColourId, bgTop);
        setColour (juce::PopupMenu::textColourId, cream);
        setColour (juce::ProgressBar::backgroundColourId, knobBrown.darker (0.4f));
        setColour (juce::ProgressBar::foregroundColourId, ember);
    }

    //==========================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override
    {
        using namespace crockpot;

        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
        const float size   = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto  centre = bounds.getCentre();
        const float radius = size * 0.5f;
        const float angle  = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const float arcR   = radius - 2.0f;

        // track arc
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (knobBrown);
        g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        // ember value arc
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                             rotaryStartAngle, angle, true);
        g.setColour (ember.interpolatedWith (lidShine, 0.35f));
        g.strokePath (value, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        // ceramic body
        const float bodyR = radius * 0.72f;
        g.setGradientFill (juce::ColourGradient (potBody, centre.x, centre.y - bodyR,
                                                 potShade, centre.x, centre.y + bodyR, false));
        g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
        g.setColour (bgTop.withAlpha (0.35f));
        g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.2f);

        // cream pointer
        juce::Path pointer;
        pointer.addRoundedRectangle (-2.0f, -bodyR + 3.0f, 4.0f, bodyR * 0.52f, 2.0f);
        g.setColour (cream);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));

        // centre dot
        g.setColour (knobBrown);
        g.fillEllipse (centre.x - 2.5f, centre.y - 2.5f, 5.0f, 5.0f);
    }

    //==========================================================================
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                           bool highlighted, bool) override
    {
        using namespace crockpot;

        auto r = b.getLocalBounds().toFloat().reduced (2.0f);
        const bool on = b.getToggleState();
        const float corner = r.getHeight() * 0.5f;

        g.setColour (on ? ember.withAlpha (highlighted ? 1.0f : 0.9f)
                        : knobBrown.withAlpha (highlighted ? 1.0f : 0.75f));
        g.fillRoundedRectangle (r, corner);
        g.setColour (bgTop.withAlpha (0.4f));
        g.drawRoundedRectangle (r, corner, 1.0f);

        g.setColour (on ? cream : cream.withAlpha (0.55f));
        g.setFont (juce::Font (juce::FontOptions ((float) juce::jmin (14, b.getHeight() - 6))));
        g.drawText (b.getButtonText(), r, juce::Justification::centred, false);
    }

    //==========================================================================
    void drawButtonBackground (juce::Graphics& g, juce::Button& b,
                               const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        auto c = backgroundColour;
        if (down)             c = c.darker (0.25f);
        else if (highlighted) c = c.brighter (0.15f);

        g.setColour (c);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (crockpot::bgTop.withAlpha (0.35f));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }
};
