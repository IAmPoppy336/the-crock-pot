#pragma once

#include "CrockBlock.h"

/*  CrockBlock #2 — Resampler / Bitcrush. Lo-fi grit.
    Sample-rate reduction (sample & hold) + bit-depth quantization.
    DELIBERATE deviation from the "oversample nonlinear blocks" rule:
    a bitcrusher's aliasing IS the effect — oversampling it would polish
    away the very dirt it exists to make. Flagged per Beat B, by design.   */
class ResamplerBlock final : public CrockBlock
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate = spec.sampleRate;
        mixStage.prepare (spec, 0.0f);
        rateHz.reset (spec.sampleRate, 0.05);
        bits.reset (spec.sampleRate, 0.05);
        reset();
    }

    void reset() override
    {
        mixStage.reset();
        for (auto& h : held)  h = 0.0f;
        for (auto& p : phase) p = 1.0f;   // capture immediately
    }

    //  downsample: 0..100% → 44.1k..500 Hz (log-ish via jmap on skew below)
    //  crush:      0..100% → 24..4 bits
    void updateParams (float downPercent, float crushPercent, float mixPercent, bool bypassed)
    {
        const float norm = downPercent * 0.01f;
        rateHz.setTargetValue (juce::jmap (norm * norm, 44100.0f, 500.0f)); // squared = musical taper
        bits.setTargetValue (juce::jmap (crushPercent * 0.01f, 24.0f, 4.0f));
        mixStage.setTarget (mixPercent * 0.01f, bypassed);
    }

    void process (juce::dsp::AudioBlock<float> block) override
    {
        mixStage.pushDry (block);

        if (! mixStage.fullyDry())
        {
            const auto numCh = juce::jmin (block.getNumChannels(), (size_t) maxChannels);

            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                const float step   = rateHz.getNextValue() / (float) sampleRate;
                const float b      = bits.getNextValue();
                const float levels = std::exp2 (b - 1.0f);

                for (size_t ch = 0; ch < numCh; ++ch)
                {
                    auto* d = block.getChannelPointer (ch);

                    phase[ch] += step;
                    if (phase[ch] >= 1.0f)
                    {
                        phase[ch] -= std::floor (phase[ch]);
                        // quantize at capture time
                        held[ch] = std::round (d[i] * levels) / levels;
                    }
                    d[i] = held[ch];
                }
            }
        }
        else
        {
            rateHz.skip ((int) block.getNumSamples());
            bits.skip ((int) block.getNumSamples());
        }

        mixStage.mixWet (block);
    }

private:
    static constexpr int maxChannels = 2;

    double sampleRate = 44100.0;
    MixStage mixStage;
    juce::SmoothedValue<float> rateHz { 44100.0f }, bits { 24.0f };
    std::array<float, maxChannels> held  {};
    std::array<float, maxChannels> phase {};
};
