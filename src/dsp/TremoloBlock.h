#pragma once

#include "CrockBlock.h"

/*  CrockBlock #5 — Tremolo. Rhythmic amplitude wobble (sine LFO).
    Tempo-sync is an M5 nicety; free-running Hz for M2.                     */
class TremoloBlock final : public CrockBlock
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate = spec.sampleRate;
        mixStage.prepare (spec, 0.0f);
        rateHz.reset (spec.sampleRate, 0.05);
        depth.reset (spec.sampleRate, 0.03);
        phase = 0.0f;
        reset();
    }

    void reset() override { mixStage.reset(); }

    //  rate: 0..100% → 0.2..16 Hz (squared taper) · depth: 0..100%
    //  synced: one cycle per note division at host BPM
    void updateParams (float ratePercent, float depthPercent, float mixPercent, bool bypassed,
                       bool synced, float divisionBeatLength, double bpm)
    {
        if (synced && bpm > 1.0)
            rateHz.setTargetValue (juce::jlimit (0.05f, 30.0f,
                (float) (bpm / (60.0 * juce::jmax (0.05f, divisionBeatLength)))));
        else
        {
            const float n = ratePercent * 0.01f;
            rateHz.setTargetValue (juce::jmap (n * n, 0.2f, 16.0f));
        }
        depth.setTargetValue (depthPercent * 0.01f);
        mixStage.setTarget (mixPercent * 0.01f, bypassed);
    }

    void process (juce::dsp::AudioBlock<float> block) override
    {
        mixStage.pushDry (block);

        if (! mixStage.fullyDry())
        {
            const auto numCh = block.getNumChannels();

            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                phase += (float) (juce::MathConstants<double>::twoPi * rateHz.getNextValue() / sampleRate);
                if (phase > juce::MathConstants<float>::twoPi)
                    phase -= juce::MathConstants<float>::twoPi;

                const float d    = depth.getNextValue();
                const float gain = 1.0f - d * (0.5f + 0.5f * std::sin (phase));

                for (size_t ch = 0; ch < numCh; ++ch)
                    block.getChannelPointer (ch)[i] *= gain;
            }
        }
        else
        {
            rateHz.skip ((int) block.getNumSamples());
            depth.skip ((int) block.getNumSamples());
        }

        mixStage.mixWet (block);
    }

private:
    double sampleRate = 44100.0;
    MixStage mixStage;
    juce::SmoothedValue<float> rateHz { 4.0f }, depth { 0.5f };
    float phase = 0.0f;
};
