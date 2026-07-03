#pragma once

#include "CrockBlock.h"

/*  CrockBlock #8 — Reverb. The space at the end of the chain.
    JUCE's dsp::Reverb is Freeverb-lineage — exactly Beat B's "start here"
    recommendation. Dattorro plate is the M5+ upgrade path if wanted.
    Engine runs full-wet; OUR MixStage owns dry/wet.                        */
class ReverbBlock final : public CrockBlock
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec) override
    {
        reverb.prepare (spec);
        mixStage.prepare (spec, 0.0f);
        reset();
    }

    void reset() override
    {
        reverb.reset();
        mixStage.reset();
    }

    void updateParams (float sizePercent, float dampPercent, float widthPercent,
                       float mixPercent, bool bypassed)
    {
        juce::dsp::Reverb::Parameters p;
        p.roomSize   = sizePercent  * 0.01f;
        p.damping    = dampPercent  * 0.01f;
        p.width      = widthPercent * 0.01f;
        p.wetLevel   = 1.0f;    // full wet inside the block
        p.dryLevel   = 0.0f;
        p.freezeMode = 0.0f;
        reverb.setParameters (p);

        mixStage.setTarget (mixPercent * 0.01f, bypassed);
    }

    void process (juce::dsp::AudioBlock<float> block) override
    {
        mixStage.pushDry (block);

        if (! mixStage.fullyDry())
        {
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            reverb.process (ctx);
        }

        mixStage.mixWet (block);
    }

private:
    juce::dsp::Reverb reverb;
    MixStage mixStage;
};
