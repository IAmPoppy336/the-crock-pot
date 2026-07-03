#pragma once

#include <juce_dsp/juce_dsp.h>

/*  The CrockBlock interface — every FX block in the pedalboard speaks this.

    Contract (ARCHITECTURE.md):
      - prepare()/reset() run on the message thread: allocation allowed.
      - process() runs on the audio thread: NO alloc, NO locks, NO I/O.
      - Every block owns its dry/wet + bypass, smoothed (bypass = mix→0
        through the same path, so latency never jumps).
      - updateParams() is called once per audio block with plain floats the
        processor read from APVTS atomics (and seasoned by the Simmer macro).

    Virtual dispatch on the audio thread is fine — it's a pointer hop, not a
    lock. The chain holds blocks by pointer and runs them in user order.
*/
class CrockBlock
{
public:
    virtual ~CrockBlock() = default;

    virtual void prepare (const juce::dsp::ProcessSpec& spec) = 0;
    virtual void reset() = 0;
    virtual void process (juce::dsp::AudioBlock<float> block) = 0;

    virtual float getLatencyInSamples() const { return 0.0f; }

protected:
    //  Shared helper: block-wise smoothed dry/wet with optional latency align.
    struct MixStage
    {
        void prepare (const juce::dsp::ProcessSpec& spec, float wetLatency)
        {
            mixer.prepare (spec);
            mixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);
            mixer.setWetLatency (wetLatency);
            mix.reset (spec.sampleRate, 0.03);
        }

        void reset() { mixer.reset(); }

        void setTarget (float mix01, bool bypassed)
        {
            mix.setTargetValue (bypassed ? 0.0f : mix01);
        }

        void pushDry (juce::dsp::AudioBlock<float> block) { mixer.pushDrySamples (block); }

        void mixWet (juce::dsp::AudioBlock<float> block)
        {
            mix.skip ((int) block.getNumSamples());
            mixer.setWetMixProportion (mix.getCurrentValue());
            mixer.mixWetSamples (block);
        }

        bool fullyDry() const   // safe skip: block silent in the wet path
        {
            return mix.getCurrentValue() <= 0.0001f
                && mix.getTargetValue()  <= 0.0001f;
        }

        juce::dsp::DryWetMixer<float> mixer { 1024 };
        juce::SmoothedValue<float> mix { 1.0f };
    };
};
