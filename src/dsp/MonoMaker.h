#pragma once

#include <juce_dsp/juce_dsp.h>

/*  MonoMaker — collapses everything below the crossover to mono. [Beat B]
    Protects sub-bass on club systems / vinyl from phase cancellation.
    Linkwitz-Riley 4th-order split: LP path mono-summed, HP path untouched,
    recombined (LR sums flat). Sits FIRST in the signal path, pre-chain.    */
class MonoMaker
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        lowPass.prepare (spec);
        highPass.prepare (spec);
        lowPass.setType  (juce::dsp::LinkwitzRileyFilterType::lowpass);
        highPass.setType (juce::dsp::LinkwitzRileyFilterType::highpass);

        lowScratch.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize,
                            false, true, true);
        freqHz.reset (spec.sampleRate, 0.05);
        reset();
    }

    void reset()
    {
        lowPass.reset();
        highPass.reset();
    }

    void updateParams (float crossoverHz, bool enabledIn)
    {
        freqHz.setTargetValue (juce::jlimit (60.0f, 200.0f, crossoverHz));
        enabled = enabledIn;
    }

    void process (juce::dsp::AudioBlock<float> block)
    {
        if (! enabled || block.getNumChannels() < 2)
            return;

        const int numSamples = (int) block.getNumSamples();

        freqHz.skip (numSamples);
        lowPass.setCutoffFrequency (freqHz.getCurrentValue());
        highPass.setCutoffFrequency (freqHz.getCurrentValue());

        // copy input → low path scratch
        juce::dsp::AudioBlock<float> low (lowScratch);
        auto lowSub = low.getSubBlock (0, (size_t) numSamples);
        lowSub.copyFrom (block);

        juce::dsp::ProcessContextReplacing<float> lowCtx (lowSub);
        lowPass.process (lowCtx);

        juce::dsp::ProcessContextReplacing<float> highCtx (block);
        highPass.process (highCtx);

        auto* l = lowSub.getChannelPointer (0);
        auto* r = lowSub.getChannelPointer (1);
        auto* L = block.getChannelPointer (0);
        auto* R = block.getChannelPointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            const float mono = 0.5f * (l[i] + r[i]);
            L[i] += mono;
            R[i] += mono;
        }
    }

private:
    juce::dsp::LinkwitzRileyFilter<float> lowPass, highPass;
    juce::AudioBuffer<float> lowScratch;
    juce::SmoothedValue<float> freqHz { 110.0f };
    bool enabled = true;
};
