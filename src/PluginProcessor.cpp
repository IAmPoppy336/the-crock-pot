#include "PluginProcessor.h"
#include "PluginEditor.h"

const std::array<const char*, CrockPotProcessor::numBlocks>
CrockPotProcessor::blockNames { "Saturation", "Resampler", "Tape", "Chorus",
                                "Tremolo", "Reverse", "Delay", "Reverb", "Skimmer" };

//==============================================================================
CrockPotProcessor::CrockPotProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CrockPotState", params::createLayout())
{
    if (! apvts.state.hasProperty (params::chainOrderProperty))
        apvts.state.setProperty (params::chainOrderProperty,
                                 params::defaultChainOrder, nullptr);

    // resolve every atomic once — never on the audio thread
    auto r = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    rp = { r (params::satDrive), r (params::satMix), r (params::satBypass),
           r (params::resDown), r (params::resCrush), r (params::resMix), r (params::resBypass),
           r (params::tapeWarmth), r (params::tapeWobble), r (params::tapeTone), r (params::tapeMix), r (params::tapeBypass),
           r (params::chorusRate), r (params::chorusDepth), r (params::chorusMix), r (params::chorusBypass),
           r (params::tremRate), r (params::tremDepth), r (params::tremMix), r (params::tremBypass),
           r (params::revWindow), r (params::revMix), r (params::revBypass),
           r (params::delayTime), r (params::delayFeedback), r (params::delayTone), r (params::delayMix), r (params::delayBypass),
           r (params::verbSize), r (params::verbDamp), r (params::verbWidth), r (params::verbMix), r (params::verbBypass),
           r (params::delaySync), r (params::delayDiv), r (params::tremSync), r (params::tremDiv),
           r (params::skimTrack), r (params::skimHarmonic), r (params::skimFreq), r (params::skimAmount),
           r (params::skimWidth), r (params::skimAttack), r (params::skimRelease), r (params::skimMix), r (params::skimBypass),
           r (params::simmer), r (params::monoFreq), r (params::monoOn), r (params::outputTrim) };

    apvts.state.addListener (this);
    refreshChainOrder();
}

CrockPotProcessor::~CrockPotProcessor()
{
    apvts.state.removeListener (this);
}

//==============================================================================
void CrockPotProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec {
        sampleRate,
        (juce::uint32) samplesPerBlock,
        (juce::uint32) juce::jmax (getTotalNumInputChannels(),
                                   getTotalNumOutputChannels())
    };

    monoMaker.prepare (spec);
    for (auto* b : blocks)
        b->prepare (spec);

    outputGain.reset (sampleRate, 0.02);

    float totalLatency = 0.0f;
    for (auto* b : blocks)
        totalLatency += b->getLatencyInSamples();
    setLatencySamples ((int) std::round (totalLatency));
}

void CrockPotProcessor::releaseResources()
{
    monoMaker.reset();
    for (auto* b : blocks)
        b->reset();
}

bool CrockPotProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
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
    juce::ScopedNoDenormals noDenormals;   // invariant #2
    juce::ignoreUnused (midiMessages);

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // ---- host tempo (RT-safe playhead read; falls back to last known) ------
    if (auto* playhead = getPlayHead())
        if (auto position = playhead->getPosition())
            if (auto bpm = position->getBpm())
                if (*bpm > 1.0)
                    currentBpm = *bpm;

    // ---- one atomic read per param, Simmer seasoning applied (D11) ---------
    const float s01 = rp.simmer->load() * 0.01f;

    const auto seasoned = params::season (s01,
        rp.satDrive->load(),    rp.resDown->load(),
        rp.tapeWarmth->load(),  rp.tapeWobble->load(),
        rp.chorusDepth->load(), rp.delayFeedback->load(),
        rp.verbMix->load());

    monoMaker.updateParams (rp.monoFreq->load(), rp.monoOn->load() > 0.5f);

    saturation.updateParams (seasoned.satDrive, rp.satMix->load(),
                             rp.satBypass->load() > 0.5f);

    resampler.updateParams (seasoned.resDown, rp.resCrush->load(),
                            rp.resMix->load(), rp.resBypass->load() > 0.5f);

    tape.updateParams (seasoned.tapeWarmth, seasoned.tapeWobble,
                       rp.tapeTone->load(), rp.tapeMix->load(),
                       rp.tapeBypass->load() > 0.5f);

    chorus.updateParams (rp.chorusRate->load(), seasoned.chorusDepth,
                         rp.chorusMix->load(), rp.chorusBypass->load() > 0.5f);

    const auto divBeats = [] (float idx)
    {
        const auto i = juce::jlimit (0, (int) params::divisionBeats.size() - 1, (int) idx);
        return params::divisionBeats[(size_t) i];
    };

    tremolo.updateParams (rp.tremRate->load(), rp.tremDepth->load(),
                          rp.tremMix->load(), rp.tremBypass->load() > 0.5f,
                          rp.tremSync->load() > 0.5f,
                          divBeats (rp.tremDiv->load()), currentBpm);

    reverse.updateParams (rp.revWindow->load(), rp.revMix->load(),
                          rp.revBypass->load() > 0.5f);

    delay.updateParams (rp.delayTime->load(), seasoned.delayFeedback,
                        rp.delayTone->load(), rp.delayMix->load(),
                        rp.delayBypass->load() > 0.5f,
                        rp.delaySync->load() > 0.5f,
                        divBeats (rp.delayDiv->load()), currentBpm);

    reverb.updateParams (rp.verbSize->load(), rp.verbDamp->load(),
                         rp.verbWidth->load(), seasoned.verbMix,
                         rp.verbBypass->load() > 0.5f);

    // Skimmer: surgical tool — deliberately unseasoned by Simmer (D11 curated)
    skimmer.updateParams (rp.skimTrack->load() > 0.5f,
                          (int) rp.skimHarmonic->load(),
                          rp.skimFreq->load(),
                          rp.skimAmount->load(),
                          rp.skimWidth->load(),
                          rp.skimAttack->load(),
                          rp.skimRelease->load(),
                          rp.skimMix->load(),
                          rp.skimBypass->load() > 0.5f);

    // ---- the cook: mono floor first, then blocks in user order -------------
    juce::dsp::AudioBlock<float> block (buffer);
    monoMaker.process (block);

    const auto order = chainOrder.load (std::memory_order_relaxed);
    for (int slot = 0; slot < numBlocks; ++slot)
    {
        const auto idx = (int) ((order >> (slot * 4)) & 0xFull);
        if (idx < numBlocks)
            blocks[(size_t) idx]->process (block);
    }

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (rp.outputTrim->load()));
    outputGain.applyGain (buffer, buffer.getNumSamples());

    // feed the editor's ember glow (RT-safe: scan + relaxed store, no alloc)
    outputLevel.store (buffer.getMagnitude (0, buffer.getNumSamples()),
                       std::memory_order_relaxed);
}

//==============================================================================
juce::AudioProcessorEditor* CrockPotProcessor::createEditor()
{
    return new CrockPotEditor (*this);
}

//==============================================================================
void CrockPotProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("version", VERSION, nullptr);

    juce::MemoryOutputStream out (destData, false);
    state.writeToStream (out);
}

void CrockPotProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto state = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);

    if (state.isValid() && state.hasType (apvts.state.getType()))
    {
        apvts.replaceState (state);   // fires valueTreeRedirected → refresh

        // older saves (M0/M1) carry no chain_order — restore the default
        if (! apvts.state.hasProperty (params::chainOrderProperty))
            apvts.state.setProperty (params::chainOrderProperty,
                                     params::defaultChainOrder, nullptr);
        refreshChainOrder();
    }
}

//==============================================================================
juce::String CrockPotProcessor::saveStateToXml() const
{
    auto state = apvts.copyState();
    state.setProperty ("version", VERSION, nullptr);
    return state.toXmlString();
}

bool CrockPotProcessor::restoreFromXml (const juce::String& xmlText)
{
    const auto xml = juce::parseXML (xmlText);
    if (xml == nullptr)
        return false;

    const auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid() || ! state.hasType (apvts.state.getType()))
        return false;

    apvts.replaceState (state);
    if (! apvts.state.hasProperty (params::chainOrderProperty))
        apvts.state.setProperty (params::chainOrderProperty,
                                 params::defaultChainOrder, nullptr);
    refreshChainOrder();
    return true;
}

juce::String CrockPotProcessor::getChainOrderString() const
{
    return apvts.state.getProperty (params::chainOrderProperty,
                                    params::defaultChainOrder).toString();
}

void CrockPotProcessor::setChainOrderString (const juce::String& csv)
{
    apvts.state.setProperty (params::chainOrderProperty, csv, nullptr);
}

void CrockPotProcessor::valueTreePropertyChanged (juce::ValueTree&,
                                                  const juce::Identifier& property)
{
    if (property == juce::Identifier (params::chainOrderProperty))
        refreshChainOrder();
}

void CrockPotProcessor::valueTreeRedirected (juce::ValueTree&)
{
    refreshChainOrder();
}

void CrockPotProcessor::refreshChainOrder()
{
    // Parse "3,0,1,2,..." → packed nibbles. Tolerant: bad/missing/duplicate
    // entries fall back to the identity order. Message thread only.
    auto tokens = juce::StringArray::fromTokens (getChainOrderString(), ",", "");

    std::array<bool, numBlocks> seen {};
    std::array<int, numBlocks> order {};
    bool valid = (tokens.size() == numBlocks);

    if (valid)
    {
        for (int i = 0; i < numBlocks; ++i)
        {
            const int v = tokens[i].trim().getIntValue();
            if (v < 0 || v >= numBlocks || seen[(size_t) v]) { valid = false; break; }
            seen[(size_t) v]  = true;
            order[(size_t) i] = v;
        }
    }

    juce::uint64 packed = 0;
    for (int i = 0; i < numBlocks; ++i)
        packed |= (juce::uint64) (valid ? order[(size_t) i] : i) << (i * 4);

    chainOrder.store (packed, std::memory_order_relaxed);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CrockPotProcessor();
}
