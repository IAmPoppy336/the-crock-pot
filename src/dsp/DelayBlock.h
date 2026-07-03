#pragma once

#include "CrockBlock.h"

/*  CrockBlock #7 — Delay. Echoes with a darkening feedback loop.
    Free-running ms in M2; tempo-sync arrives with M5. Time changes glide
    (100 ms smoothing + linear interp) → gentle tape-style pitch bends
    instead of clicks, which suits the pot.                                 */
class DelayBlock final : public CrockBlock
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate = spec.sampleRate;

        delay.prepare (spec);
        delay.setMaximumDelayInSamples ((int) std::ceil (2.0 * spec.sampleRate));

        loopFilter.prepare (spec);
        loopFilter.setType (juce::dsp::FirstOrderTPTFilterType::lowpass);

        mixStage.prepare (spec, 0.0f);
        timeSamples.reset (spec.sampleRate, 0.1);
        feedback.reset (spec.sampleRate, 0.03);
        toneHz.reset (spec.sampleRate, 0.05);
        reset();
    }

    void reset() override
    {
        delay.reset();
        loopFilter.reset();
        mixStage.reset();
    }

    //  time: 0..100% → 40..1500 ms (squared taper) · feedback: 0..95% · tone: dark..open
    //  synced: time = note division at host BPM (clamped to the 2 s line)
    void updateParams (float timePercent, float feedbackPercent, float tonePercent,
                       float mixPercent, bool bypassed,
                       bool synced, float divisionBeatLength, double bpm)
    {
        float ms;
        if (synced && bpm > 1.0)
            ms = (float) (divisionBeatLength * 60000.0 / bpm);
        else
        {
            const float n = timePercent * 0.01f;
            ms = juce::jmap (n * n, 40.0f, 1500.0f);
        }
        const float maxMs = (float) (1990.0);   // stay inside the 2 s delay line
        timeSamples.setTargetValue ((float) (juce::jlimit (10.0f, maxMs, ms) * 0.001 * sampleRate));

        feedback.setTargetValue (juce::jmin (feedbackPercent, 95.0f) * 0.01f);
        toneHz.setTargetValue (1200.0f * std::pow (12.0f, tonePercent * 0.01f)); // 1.2k → 14.4k
        mixStage.setTarget (mixPercent * 0.01f, bypassed);
    }

    void process (juce::dsp::AudioBlock<float> block) override
    {
        mixStage.pushDry (block);

        if (! mixStage.fullyDry())
        {
            const auto numCh = juce::jmin (block.getNumChannels(), (size_t) 2);

            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                const float t  = timeSamples.getNextValue();
                const float fb = feedback.getNextValue();
                loopFilter.setCutoffFrequency (toneHz.getNextValue());

                for (size_t ch = 0; ch < numCh; ++ch)
                {
                    auto* d = block.getChannelPointer (ch);
                    const float echo = delay.popSample ((int) ch, t, true);
                    const float darkened = loopFilter.processSample ((int) ch, echo);
                    delay.pushSample ((int) ch, d[i] + darkened * fb);
                    d[i] = darkened;   // wet path = echoes only; MixStage blends
                }
            }
        }
        else
        {
            timeSamples.skip ((int) block.getNumSamples());
            feedback.skip ((int) block.getNumSamples());
            toneHz.skip ((int) block.getNumSamples());
        }

        mixStage.mixWet (block);
    }

private:
    double sampleRate = 44100.0;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 96000 };
    juce::dsp::FirstOrderTPTFilter<float> loopFilter;
    MixStage mixStage;
    juce::SmoothedValue<float> timeSamples { 22050.0f }, feedback { 0.35f }, toneHz { 8000.0f };
};
