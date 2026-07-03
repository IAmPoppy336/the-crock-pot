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
                             private juce::ValueTree::Listener,
                             private juce::Timer
{
public:
    explicit CrockPotEditor (CrockPotProcessor&);
    ~CrockPotEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeRedirected (juce::ValueTree&) override;
    void timerCallback() override;    // 30 fps: steam drift, ember glow, power tints

    void refreshChainButtons();       // relabel from processor order
    void moveSelected (int delta);    // shift selected block left/right

    CrockPotProcessor& processorRef;

    // ---- pizzazz state (message thread only) --------------------------------
    float steamPhase = 0.0f;          // drives the steam sway
    float glowLevel  = 0.0f;          // smoothed audio level for the ember
    std::atomic<float>* simmerRaw = nullptr;
    std::array<std::atomic<float>*, CrockPotProcessor::numBlocks> bypassRaw {};
    std::array<int, CrockPotProcessor::numBlocks> slotToBlock { 0,1,2,3,4,5,6,7 };
    std::array<int, CrockPotProcessor::numBlocks> lastTint { -1,-1,-1,-1,-1,-1,-1,-1 };

    juce::Slider simmerDial, outputSlider;
    juce::Label simmerLabel, outputLabel, hintLabel;

    std::array<juce::TextButton, CrockPotProcessor::numBlocks> chainButtons;
    juce::TextButton moveLeft { "<" }, moveRight { ">" };
    int selectedSlot = -1;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> simmerAttachment, outputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrockPotEditor)
};
