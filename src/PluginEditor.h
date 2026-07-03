#pragma once

#include "PluginProcessor.h"

//==============================================================================
/*  The Crock-Pot — editor (M2: Simmer Dial + chain strip).

    D3: native JUCE Components ONLY. Per-block control panels arrive with the
    real UI in M3; until then block knobs live in Live's device panel, and
    this window owns the two things Live can't do for us: the Simmer macro
    front-and-center, and drag-free chain reordering.
*/
class CrockPotEditor final : public juce::AudioProcessorEditor,
                             private juce::ValueTree::Listener
{
public:
    explicit CrockPotEditor (CrockPotProcessor&);
    ~CrockPotEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeRedirected (juce::ValueTree&) override;

    void refreshChainButtons();       // relabel from processor order
    void moveSelected (int delta);    // shift selected block left/right

    CrockPotProcessor& processorRef;

    juce::Slider simmerDial, outputSlider;
    juce::Label simmerLabel, outputLabel, hintLabel;

    std::array<juce::TextButton, CrockPotProcessor::numBlocks> chainButtons;
    juce::TextButton moveLeft { "<" }, moveRight { ">" };
    int selectedSlot = -1;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> simmerAttachment, outputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrockPotEditor)
};
