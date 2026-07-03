#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

#include "Params.h"
#include "dsp/MonoMaker.h"
#include "dsp/SaturationBlock.h"
#include "dsp/ResamplerBlock.h"
#include "dsp/TapeBlock.h"
#include "dsp/ChorusBlock.h"
#include "dsp/TremoloBlock.h"
#include "dsp/ReverseBlock.h"
#include "dsp/DelayBlock.h"
#include "dsp/ReverbBlock.h"

//==============================================================================
/*  The Crock-Pot — audio processor (M2: the full pedalboard).

    Signal path (ARCHITECTURE.md):
      input → MonoMaker(<110 Hz) → 8 CrockBlocks in USER ORDER → output trim.

    Chain order is a state property (not a parameter): the editor writes
    "chain_order" into apvts.state; a ValueTree listener packs it into an
    atomic uint64 (one nibble per slot) that the audio thread decodes —
    lock-free reorder, no allocation, no waiting. [Beat B rules]
*/
class CrockPotProcessor final : public juce::AudioProcessor,
                                private juce::ValueTree::Listener
{
public:
    CrockPotProcessor();
    ~CrockPotProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }

    //==========================================================================
    const juce::String getName() const override          { return JucePlugin_Name; }
    bool acceptsMidi() const override                    { return false; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 0.0; }

    //==========================================================================
    int getNumPrograms() override                        { return 1; }
    int getCurrentProgram() override                     { return 0; }
    void setCurrentProgram (int) override                {}
    const juce::String getProgramName (int) override     { return "Default Recipe"; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    static constexpr int numBlocks = 8;
    static const std::array<const char*, numBlocks> blockNames;

    juce::String getChainOrderString() const;
    void setChainOrderString (const juce::String& csv);   // message thread only

    //  Recipe save/load (message thread): full state as XML text.
    juce::String saveStateToXml() const;
    bool restoreFromXml (const juce::String& xmlText);   // tolerant; false = bad file

    //  Editor eye-candy feed: block peak level, one relaxed atomic store per
    //  block. Written on the audio thread (no alloc/lock), read by the UI timer.
    float getOutputLevel() const { return outputLevel.load (std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState apvts;

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeRedirected (juce::ValueTree&) override;
    void refreshChainOrder();                              // parse property → atomic

    //  Atomic param handles, resolved ONCE in the constructor — the audio
    //  thread must never do per-block string lookups.
    struct Raw
    {
        std::atomic<float> *satDrive, *satMix, *satBypass,
                           *resDown, *resCrush, *resMix, *resBypass,
                           *tapeWarmth, *tapeWobble, *tapeTone, *tapeMix, *tapeBypass,
                           *chorusRate, *chorusDepth, *chorusMix, *chorusBypass,
                           *tremRate, *tremDepth, *tremMix, *tremBypass,
                           *revWindow, *revMix, *revBypass,
                           *delayTime, *delayFeedback, *delayTone, *delayMix, *delayBypass,
                           *verbSize, *verbDamp, *verbWidth, *verbMix, *verbBypass,
                           *delaySync, *delayDiv, *tremSync, *tremDiv,
                           *simmer, *monoFreq, *monoOn, *outputTrim;
    };
    Raw rp {};

    double currentBpm = 120.0;   // audio-thread only; refreshed from the playhead

    //==========================================================================
    MonoMaker monoMaker;
    SaturationBlock saturation;
    ResamplerBlock  resampler;
    TapeBlock       tape;
    ChorusBlock     chorus;
    TremoloBlock    tremolo;
    ReverseBlock    reverse;
    DelayBlock      delay;
    ReverbBlock     reverb;

    std::array<CrockBlock*, numBlocks> blocks {
        &saturation, &resampler, &tape, &chorus,
        &tremolo, &reverse, &delay, &reverb };

    std::atomic<juce::uint64> chainOrder { 0x76543210ull };   // nibble per slot

    juce::SmoothedValue<float> outputGain { 1.0f };
    std::atomic<float> outputLevel { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrockPotProcessor)
};
