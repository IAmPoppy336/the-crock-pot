#pragma once

#include "CrockBlock.h"

/*  CrockBlock #4 — Chorus. Width and movement.
    M2 uses JUCE's dsp::Chorus engine (modulated delay under the hood, per
    Pirkle ch.10 lineage); a custom-flavored modulator can replace it in M5
    polish if Poppy's ears ask for it. Engine runs full-wet; OUR MixStage
    owns dry/wet so every block behaves identically.                        */
class ChorusBlock final : public CrockBlock
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec) override
    {
        chorus.prepare (spec);
        chorus.setMix (1.0f);                 // full wet inside the block
        chorus.setCentreDelay (7.0f);
        chorus.setFeedback (0.0f);
        mixStage.prepare (spec, 0.0f);
        rate.reset (spec.sampleRate, 0.05);
        depth.reset (spec.sampleRate, 0.05);
        reset();
    }

    void reset() override
    {
        chorus.reset();
        mixStage.reset();
    }

    //  rate: 0..100% → 0.1..6 Hz · depth: 0..100% → 0..0.6
    void updateParams (float ratePercent, float depthPercent, float mixPercent, bool bypassed)
    {
        rate.setTargetValue (juce::jmap (ratePercent * 0.01f, 0.1f, 6.0f));
        depth.setTargetValue (depthPercent * 0.01f * 0.6f);
        mixStage.setTarget (mixPercent * 0.01f, bypassed);
    }

    void process (juce::dsp::AudioBlock<float> block) override
    {
        mixStage.pushDry (block);

        if (! mixStage.fullyDry())
        {
            rate.skip ((int) block.getNumSamples());
            depth.skip ((int) block.getNumSamples());
            chorus.setRate (rate.getCurrentValue());
            chorus.setDepth (depth.getCurrentValue());

            juce::dsp::ProcessContextReplacing<float> ctx (block);
            chorus.process (ctx);
        }

        mixStage.mixWet (block);
    }

private:
    juce::dsp::Chorus<float> chorus;
    MixStage mixStage;
    juce::SmoothedValue<float> rate { 1.0f }, depth { 0.25f };
};
