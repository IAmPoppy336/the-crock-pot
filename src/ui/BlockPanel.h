#pragma once

#include "../PluginProcessor.h"
#include "CrockPotLookAndFeel.h"

/*  BlockPanel — the focused block's controls (RC-20 module pattern).
    Table-driven: setBlock(i) rebuilds rotaries for that block's params plus
    an ON/OFF power pill. The power attachment INVERTS the bypass parameter,
    so the UI says what users mean: ON = cooking. (M3 fix for the M2 trap.)  */
class BlockPanel final : public juce::Component
{
public:
    explicit BlockPanel (CrockPotProcessor& p) : processorRef (p)
    {
        title.setJustificationType (juce::Justification::centredLeft);
        title.setColour (juce::Label::textColourId, crockpot::cream);
        addAndMakeVisible (title);

        power.setButtonText ("ON");
        addAndMakeVisible (power);

        setBlock (0);
    }

    //==========================================================================
    void setBlock (int blockIndex)
    {
        block = juce::jlimit (0, CrockPotProcessor::numBlocks - 1, blockIndex);

        title.setText (CrockPotProcessor::blockNames[(size_t) block],
                       juce::dontSendNotification);

        // ---- rebuild knobs ----------------------------------------------------
        knobs.clear();
        for (const auto& row : table ((size_t) block))
        {
            auto k = std::make_unique<Knob>();
            k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 16);
            k->label.setText (row.label, juce::dontSendNotification);
            k->label.setJustificationType (juce::Justification::centred);
            k->label.setColour (juce::Label::textColourId, crockpot::cream.withAlpha (0.8f));
            k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                processorRef.apvts, row.id, k->slider);

            if (auto* param = processorRef.apvts.getParameter (row.id))
                k->slider.setDoubleClickReturnValue (true,
                    param->convertFrom0to1 (param->getDefaultValue()));
            addAndMakeVisible (k->slider);
            addAndMakeVisible (k->label);
            knobs.push_back (std::move (k));
        }

        // ---- tempo sync (delay + tremolo only) -----------------------------------
        syncButton.reset();
        divBox.reset();
        syncAttachment.reset();
        divAttachment.reset();

        //  aux slot: Sync+division for delay/trem · Track+harmonic for the Skimmer
        const char* toggleId = (block == 6 ? params::delaySync
                              : block == 4 ? params::tremSync
                              : block == 8 ? params::skimTrack : nullptr);
        const char* comboId  = (block == 6 ? params::delayDiv
                              : block == 4 ? params::tremDiv
                              : block == 8 ? params::skimHarmonic : nullptr);

        if (toggleId != nullptr)
        {
            syncButton = std::make_unique<juce::ToggleButton> (block == 8 ? "Track" : "Sync");
            addAndMakeVisible (*syncButton);
            syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                processorRef.apvts, toggleId, *syncButton);

            divBox = std::make_unique<juce::ComboBox>();
            if (block == 8)
                divBox->addItemList (juce::StringArray { "1x", "2x", "3x", "4x" }, 1);
            else
                divBox->addItemList (params::divisionNames, 1);   // ids 1..N
            addAndMakeVisible (*divBox);
            divAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                processorRef.apvts, comboId, *divBox);
        }

        // ---- power pill (inverted bypass) ---------------------------------------
        auto* param = processorRef.apvts.getParameter (bypassId ((size_t) block));
        jassert (param != nullptr);

        powerAttachment = std::make_unique<juce::ParameterAttachment> (
            *param,
            [this] (float bypassed)
            {
                power.setToggleState (bypassed < 0.5f, juce::dontSendNotification);
            });
        powerAttachment->sendInitialUpdate();

        power.onClick = [this]
        {
            // button state after click = desired power → bypass is the inverse
            if (powerAttachment != nullptr)
                powerAttachment->setValueAsCompleteGesture (power.getToggleState() ? 0.0f : 1.0f);
        };

        resized();
        repaint();
    }

    int getBlock() const { return block; }

    //==========================================================================
    void resized() override
    {
        auto r = getLocalBounds().reduced (10);

        auto header = r.removeFromTop (26);
        power.setBounds (header.removeFromRight (64).reduced (2));
        if (divBox != nullptr)
        {
            divBox->setBounds (header.removeFromRight (70).reduced (2));
            if (syncButton != nullptr)
                syncButton->setBounds (header.removeFromRight (66).reduced (2));
        }
        title.setBounds (header);

        if (knobs.empty())
            return;

        const int perKnob = r.getWidth() / (int) knobs.size();
        for (auto& k : knobs)
        {
            auto col = r.removeFromLeft (perKnob).reduced (2, 0);
            k->label.setBounds (col.removeFromBottom (16));
            k->slider.setBounds (col);
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (crockpot::bgTop.withAlpha (0.45f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
        g.setColour (crockpot::lidShine.withAlpha (0.25f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10.0f, 1.0f);
    }

private:
    struct Row { const char* id; const char* label; };

    static std::vector<Row> table (size_t blockIndex)
    {
        switch (blockIndex)
        {
            case 0: return { { params::satDrive, "Drive" },   { params::satMix, "Mix" } };
            case 1: return { { params::resDown, "Rate" },     { params::resCrush, "Bits" },
                             { params::resMix, "Mix" } };
            case 2: return { { params::tapeWarmth, "Warmth" },{ params::tapeWobble, "Wobble" },
                             { params::tapeTone, "Tone" },    { params::tapeMix, "Mix" } };
            case 3: return { { params::chorusRate, "Rate" },  { params::chorusDepth, "Depth" },
                             { params::chorusMix, "Mix" } };
            case 4: return { { params::tremRate, "Rate" },    { params::tremDepth, "Depth" },
                             { params::tremMix, "Mix" } };
            case 5: return { { params::revWindow, "Window" }, { params::revMix, "Mix" } };
            case 6: return { { params::delayTime, "Time" },   { params::delayFeedback, "Feedbk" },
                             { params::delayTone, "Tone" },   { params::delayMix, "Mix" } };
            case 7: return { { params::verbSize, "Size" },    { params::verbDamp, "Damp" },
                             { params::verbWidth, "Width" },  { params::verbMix, "Mix" } };
            default:return { { params::skimFreq, "Freq" },    { params::skimAmount, "Amount" },
                             { params::skimWidth, "Width" },  { params::skimAttack, "Attack" },
                             { params::skimRelease, "Rel" },  { params::skimMix, "Mix" } };
        }
    }

    static const char* bypassId (size_t blockIndex)
    {
        static constexpr std::array<const char*, CrockPotProcessor::numBlocks> ids {
            params::satBypass, params::resBypass, params::tapeBypass, params::chorusBypass,
            params::tremBypass, params::revBypass, params::delayBypass, params::verbBypass,
            params::skimBypass };
        return ids[blockIndex];
    }

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    CrockPotProcessor& processorRef;
    int block = 0;

    juce::Label title;
    juce::ToggleButton power;
    std::unique_ptr<juce::ParameterAttachment> powerAttachment;
    std::vector<std::unique_ptr<Knob>> knobs;

    std::unique_ptr<juce::ToggleButton> syncButton;
    std::unique_ptr<juce::ComboBox> divBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BlockPanel)
};
