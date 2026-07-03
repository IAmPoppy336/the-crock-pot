#pragma once

#include "CrockBlock.h"

/*  CrockBlock #1 — Saturation. The core warmth.
    y = tanh(g·x)/sqrt(g), g = drive 0..100% → 0..30 dB, OVERSAMPLED 4x
    (linear-phase FIR) so bass harmonics don't alias. [Beat B]              */
class SaturationBlock final : public CrockBlock
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec) override
    {
        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            spec.numChannels, osOrder,
            juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple);
        oversampling->initProcessing (spec.maximumBlockSize);

        mixStage.prepare (spec, oversampling->getLatencyInSamples());

        driveDb.reset (spec.sampleRate * double (1 << osOrder), 0.03);
        reset();
    }

    void reset() override
    {
        if (oversampling) oversampling->reset();
        mixStage.reset();
    }

    void updateParams (float drivePercent, float mixPercent, bool bypassed)
    {
        driveDb.setTargetValue (juce::jmap (drivePercent, 0.0f, 100.0f, 0.0f, 30.0f));
        mixStage.setTarget (mixPercent * 0.01f, bypassed);
    }

    float getLatencyInSamples() const override
    {
        return oversampling ? oversampling->getLatencyInSamples() : 0.0f;
    }

    void process (juce::dsp::AudioBlock<float> block) override
    {
        mixStage.pushDry (block);

        if (! mixStage.fullyDry())
        {
            auto osBlock = oversampling->processSamplesUp (block);
            const auto numCh = osBlock.getNumChannels();

            for (size_t i = 0; i < osBlock.getNumSamples(); ++i)
            {
                const float g    = juce::Decibels::decibelsToGain (driveDb.getNextValue());
                const float comp = 1.0f / std::sqrt (juce::jmax (1.0f, g));

                for (size_t ch = 0; ch < numCh; ++ch)
                {
                    auto* d = osBlock.getChannelPointer (ch);
                    d[i] = std::tanh (g * d[i]) * comp;
                }
            }

            oversampling->processSamplesDown (block);
        }
        else
        {
            driveDb.skip ((int) block.getNumSamples());
        }

        mixStage.mixWet (block);
    }

private:
    static constexpr size_t osOrder = 2;   // 4x

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    MixStage mixStage;
    juce::SmoothedValue<float> driveDb { 0.0f };
};
