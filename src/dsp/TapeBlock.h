#pragma once

#include "CrockBlock.h"

/*  CrockBlock #3 — Tape. Analog glue: gentle saturation + wow/flutter
    (slow + fast pitch wobble via a modulated delay line) + tone rolloff.
    Drive here stays gentle (≤ ~12 dB) and runs at base rate — the heavy
    aliasing-prone drive lives in SaturationBlock (which IS oversampled).
    Flagged as a Beat-B tradeoff; revisit in the M5 tuning pass.           */
class TapeBlock final : public CrockBlock
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate = spec.sampleRate;

        delay.prepare (spec);
        delay.setMaximumDelayInSamples ((int) std::ceil (0.02 * spec.sampleRate)); // 20 ms head
        toneFilter.prepare (spec);
        toneFilter.setType (juce::dsp::FirstOrderTPTFilterType::lowpass);

        wowPhase = flutterPhase = 0.0f;

        mixStage.prepare (spec, (float) centreDelaySamples());

        warmthDb.reset (spec.sampleRate, 0.03);
        wobble.reset (spec.sampleRate, 0.05);
        toneHz.reset (spec.sampleRate, 0.05);
        reset();
    }

    void reset() override
    {
        delay.reset();
        toneFilter.reset();
        mixStage.reset();
    }

    void updateParams (float warmthPercent, float wobblePercent, float tonePercent,
                       float mixPercent, bool bypassed)
    {
        warmthDb.setTargetValue (juce::jmap (warmthPercent, 0.0f, 100.0f, 0.0f, 12.0f));
        wobble.setTargetValue (wobblePercent * 0.01f);
        // tone: 0% = dark (2 kHz) .. 100% = open (18 kHz), log taper
        const float t = tonePercent * 0.01f;
        toneHz.setTargetValue (2000.0f * std::pow (9.0f, t));   // 2k → 18k
        mixStage.setTarget (mixPercent * 0.01f, bypassed);
    }

    float getLatencyInSamples() const override { return (float) centreDelaySamples(); }

    void process (juce::dsp::AudioBlock<float> block) override
    {
        mixStage.pushDry (block);

        if (! mixStage.fullyDry())
        {
            const auto numCh = juce::jmin (block.getNumChannels(), (size_t) 2);
            const float wowInc     = (float) (2.0 * juce::MathConstants<double>::pi * 0.6 / sampleRate);
            const float flutterInc = (float) (2.0 * juce::MathConstants<double>::pi * 6.3 / sampleRate);

            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                wowPhase     += wowInc;
                flutterPhase += flutterInc;
                if (wowPhase     > juce::MathConstants<float>::twoPi) wowPhase     -= juce::MathConstants<float>::twoPi;
                if (flutterPhase > juce::MathConstants<float>::twoPi) flutterPhase -= juce::MathConstants<float>::twoPi;

                const float wob = wobble.getNextValue();
                const float modMs = 0.9f * wob * std::sin (wowPhase)      // slow, deep
                                  + 0.25f * wob * std::sin (flutterPhase); // fast, shallow
                const float delaySamples = (float) centreDelaySamples()
                                         + modMs * 0.001f * (float) sampleRate;

                const float g    = juce::Decibels::decibelsToGain (warmthDb.getNextValue());
                const float comp = 1.0f / std::sqrt (juce::jmax (1.0f, g));
                toneFilter.setCutoffFrequency (toneHz.getNextValue());

                for (size_t ch = 0; ch < numCh; ++ch)
                {
                    auto* d = block.getChannelPointer (ch);
                    delay.pushSample ((int) ch, d[i]);
                    float s = delay.popSample ((int) ch, delaySamples, true);
                    s = std::tanh (g * s) * comp;                       // gentle glue
                    d[i] = toneFilter.processSample ((int) ch, s);
                }
            }
        }
        else
        {
            warmthDb.skip ((int) block.getNumSamples());
            wobble.skip   ((int) block.getNumSamples());
            toneHz.skip   ((int) block.getNumSamples());
        }

        mixStage.mixWet (block);
    }

private:
    int centreDelaySamples() const { return (int) std::round (0.005 * sampleRate); } // 5 ms

    double sampleRate = 44100.0;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 4096 };
    juce::dsp::FirstOrderTPTFilter<float> toneFilter;
    MixStage mixStage;
    juce::SmoothedValue<float> warmthDb { 0.0f }, wobble { 0.0f }, toneHz { 18000.0f };
    float wowPhase = 0.0f, flutterPhase = 0.0f;
};
