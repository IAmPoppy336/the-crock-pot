#pragma once

#include "CrockBlock.h"

/*  CrockBlock #6 — Reverse. The special-case block.
    Realtime "reverse" = play the last window backwards, continuously:
    two alternating capture buffers; while A records, B plays reversed,
    with a short equal-power crossfade at each swap to kill clicks.
    Honest note: musicality of window length is very content-dependent —
    functional in M2, tuned by ear in M5.                                   */
class ReverseBlock final : public CrockBlock
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate = spec.sampleRate;
        maxWindow  = (int) std::ceil (2.0 * sampleRate);        // 2 s ceiling
        fadeLen    = (int) std::round (0.010 * sampleRate);     // 10 ms fade

        for (auto& b : capture)
            b.setSize ((int) spec.numChannels, maxWindow, false, true, true);

        mixStage.prepare (spec, 0.0f);
        reset();
    }

    void reset() override
    {
        for (auto& b : capture) b.clear();
        writePos = 0; playPos = 0; writeBuf = 0; window = 0;
        mixStage.reset();
    }

    //  window: 0..100% → 0.25..2.0 s (takes effect at the next swap)
    void updateParams (float windowPercent, float mixPercent, bool bypassed)
    {
        pendingWindow = juce::jlimit (0.25f, 2.0f,
                          juce::jmap (windowPercent * 0.01f, 0.25f, 2.0f));
        mixStage.setTarget (mixPercent * 0.01f, bypassed);
    }

    void process (juce::dsp::AudioBlock<float> block) override
    {
        mixStage.pushDry (block);

        if (! mixStage.fullyDry())
        {
            const auto numCh = juce::jmin ((int) block.getNumChannels(),
                                           capture[0].getNumChannels());

            if (window == 0)   // first run: adopt pending immediately
                window = (int) std::round (pendingWindow * sampleRate);

            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                const int readBuf = 1 - writeBuf;
                const int revIdx  = window - 1 - playPos;

                // equal-power fades at window edges
                float fade = 1.0f;
                if (playPos < fadeLen)               fade = std::sin (juce::MathConstants<float>::halfPi * (float) playPos / (float) fadeLen);
                else if (playPos > window - fadeLen) fade = std::sin (juce::MathConstants<float>::halfPi * (float) (window - playPos) / (float) fadeLen);

                for (int ch = 0; ch < numCh; ++ch)
                {
                    auto* d = block.getChannelPointer ((size_t) ch);
                    capture[writeBuf].setSample (ch, writePos, d[i]);
                    d[i] = capture[readBuf].getSample (ch, juce::jlimit (0, maxWindow - 1, revIdx)) * fade;
                }

                if (++writePos >= window) writePos = 0;
                if (++playPos  >= window)
                {
                    playPos  = 0;
                    writeBuf = readBuf;                       // swap roles
                    window   = (int) std::round (pendingWindow * sampleRate); // adopt new size at boundary
                }
            }
        }

        mixStage.mixWet (block);
    }

private:
    double sampleRate = 44100.0;
    int maxWindow = 0, fadeLen = 0;
    int writePos = 0, playPos = 0, writeBuf = 0, window = 0;
    float pendingWindow = 1.0f;

    std::array<juce::AudioBuffer<float>, 2> capture;
    MixStage mixStage;
};
