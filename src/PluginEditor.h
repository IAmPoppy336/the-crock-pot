#pragma once

#include "PluginProcessor.h"
#include "ui/CrockPotLookAndFeel.h"
#include "ui/CookPage.h"
#include "ui/SplitPage.h"

//==============================================================================
/*  The Crock-Pot — editor shell (M3 layout: header + tabbed pages).

    D3: native JUCE Components ONLY. Layout follows the patterns the loved
    multi-FX plugins share (RC-20 module bank + master macro; Output-style
    submodule pages — see 05_Assets/ui-design notes):
      header: title · COOK/SPLIT tabs · Shake the Pot · Output knob
      COOK:   art + Simmer + focused block panel + reorderable chain strip
      SPLIT:  drop a loop → background split → Leftovers on disk (M4b)
*/
class CrockPotEditor final : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit CrockPotEditor (CrockPotProcessor&);
    ~CrockPotEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setPage (int newPage);
    void shakeThePot();
    void unshake();
    void saveRecipe();
    void loadRecipe();
    void timerCallback() override;   // repaints just the header meter

    CrockPotProcessor& processorRef;
    CrockPotLookAndFeel lookAndFeel;

    juce::TextButton cookTab { "COOK" }, splitTab { "SPLIT" };
    juce::TextButton shakeButton { "Shake!" }, unshakeButton { "Unshake" };
    juce::TextButton saveButton { "Save" }, loadButton { "Load" };
    juce::ComboBox recipeBox;
    std::vector<const char*> recipeResources;   // BinaryData resource names, menu order
    juce::Slider outputKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;

    juce::ValueTree shakeUndoState;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> meterArea;
    float meterLevel = 0.0f;

    CookPage cookPage;
    SplitPage splitPage;
    int currentPage = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrockPotEditor)
};
