#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
CrockPotProcessor::CrockPotProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

//==============================================================================
void CrockPotProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // M0: nothing to prepare. M1 will size DSP state here (message thread —
    // allocation is allowed in prepareToPlay, never in processBlock).
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void CrockPotProcessor::releaseResources() {}

bool CrockPotProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Mono→mono or stereo→stereo only, input matching output.
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo();
}

void CrockPotProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midiMessages)
{
    // Invariant #2 (Beat B): flush denormals for the whole block.
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    // Defensive: silence any output channels beyond the input count so we
    // never emit garbage. (Standard JUCE pattern; no allocation happens here.)
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // M0 is a clean passthrough: the buffer flows through untouched.
    // The pot is on the counter, plugged in, lid on. Cooking starts at M1.
}

//==============================================================================
juce::AudioProcessorEditor* CrockPotProcessor::createEditor()
{
    return new CrockPotEditor (*this);
}

//==============================================================================
void CrockPotProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // D9: full state save/restore so Live sets + rack presets recall.
    // M0 state is just an identity + version stamp; APVTS lands here in M1.
    juce::ValueTree state { "CrockPotState" };
    state.setProperty ("version", VERSION, nullptr);

    juce::MemoryOutputStream out (destData, false);
    state.writeToStream (out);
}

void CrockPotProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Tolerant restore: pluginval (strictness 5) feeds this junk on purpose.
    const auto state = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);

    if (state.isValid() && state.hasType ("CrockPotState"))
    {
        // Nothing to restore yet — M1 gives this real work.
    }
}

//==============================================================================
// This creates the plugin instance for every format JUCE builds.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrockPotProcessor();
}
