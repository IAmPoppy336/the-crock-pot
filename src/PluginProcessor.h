#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

//==============================================================================
/*  The Crock-Pot — audio processor (M0: clean passthrough).

    Hard invariants (02_Build/ARCHITECTURE.md — do not violate):
      - processBlock() never allocates, locks, does I/O, or makes syscalls.
      - Registered as an audio EFFECT (IS_SYNTH FALSE in CMake) so Ableton
        treats us as a device-chain / Audio Effect Rack citizen. [D9]
      - State save/restore must always round-trip, even with junk input.
*/
class CrockPotProcessor final : public juce::AudioProcessor
{
public:
    CrockPotProcessor();
    ~CrockPotProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }

    //==========================================================================
    const juce::String getName() const override          { return JucePlugin_Name; }
    bool acceptsMidi() const override                    { return false; }
    bool producesMidi() const override                   { return false; }
    bool isMidiEffect() const override                   { return false; }
    double getTailLengthSeconds() const override         { return 0.0; }

    //==========================================================================
    int getNumPrograms() override                        { return 1; }
    int getCurrentProgram() override                     { return 0; }
    void setCurrentProgram (int) override                {}
    const juce::String getProgramName (int) override     { return "Default Recipe"; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CrockPotProcessor)
};
