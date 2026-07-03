#pragma once

#include "PluginProcessor.h"

//==============================================================================
/*  The Crock-Pot — editor (M0: "Hello Crock-Pot").

    D3: native JUCE Components ONLY. Everything below is drawn with
    juce::Graphics — no WebView, no HTML, no bundled anything.
    M0 is a static hello; the animation framework joins at M5.
*/
class CrockPotEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CrockPotEditor (CrockPotProcessor&);
    ~CrockPotEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override {}   // everything in paint() is bounds-relative

private:
    CrockPotProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrockPotEditor)
};
