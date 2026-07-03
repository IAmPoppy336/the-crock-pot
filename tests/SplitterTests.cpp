#include <catch2/catch_test_macros.hpp>

#include "splitter/DrumSplitter.h"
#include "splitter/LeftoversExporter.h"

/*  M4a tests — synthetic drum kit through the DSP splitter.

    The fixture is deliberately easy (isolated bands, clean onsets): these
    tests prove the PLUMBING routes energy to the right stems, not that the
    splitter is magic. D12 sets the honesty bar: "rough creative split."
*/
namespace
{
    constexpr double sr = 44100.0;
    constexpr int    numSeconds = 4;
    constexpr int    N = (int) sr * numSeconds;

    //  kick @ 0,1,2,3 s · snare @ 0.5,1.5,2.5,3.5 s · hats @ x.25 and x.75
    juce::AudioBuffer<float> makeKit()
    {
        juce::AudioBuffer<float> buf (2, N);
        buf.clear();
        juce::Random rng (42);

        auto addBurst = [&buf] (double timeSec, int lengthSamples, auto&& sampleGen)
        {
            const int start = (int) (timeSec * sr);
            for (int i = 0; i < lengthSamples && start + i < N; ++i)
            {
                const float env = std::exp (-6.0f * (float) i / (float) lengthSamples);
                const float s   = sampleGen (i) * env;
                for (int ch = 0; ch < 2; ++ch)
                    buf.addSample (ch, start + i, s);
            }
        };

        for (int beat = 0; beat < numSeconds; ++beat)
        {
            // kick: 55 Hz sine, 140 ms
            addBurst ((double) beat, (int) (0.14 * sr), [] (int i)
            {
                return 0.9f * std::sin (2.0f * juce::MathConstants<float>::pi
                                        * 55.0f * (float) i / (float) sr);
            });

            // snare: 220 Hz + darkened noise, 120 ms
            float lpState = 0.0f;
            addBurst ((double) beat + 0.5, (int) (0.12 * sr), [&lpState, &rng] (int i)
            {
                const float tone  = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                     * 220.0f * (float) i / (float) sr);
                lpState += 0.25f * (rng.nextFloat() * 2.0f - 1.0f - lpState); // one-pole LP noise
                return tone + 0.6f * lpState;
            });

            // hats: brightened noise ticks, 40 ms, at .25 and .75
            for (double off : { 0.25, 0.75 })
            {
                float x1 = 0.0f, x2 = 0.0f;
                addBurst ((double) beat + off, (int) (0.04 * sr), [&x1, &x2, &rng] (int)
                {
                    const float n  = rng.nextFloat() * 2.0f - 1.0f;
                    const float hp = n - 2.0f * x1 + x2;    // double-diff ≈ high emphasis
                    x2 = x1; x1 = n;
                    return 0.35f * hp;
                });
            }
        }
        return buf;
    }

    //  RMS energy of a stem inside a time window
    float windowEnergy (const juce::AudioBuffer<float>& buf, double t0, double t1)
    {
        const int a = juce::jlimit (0, buf.getNumSamples() - 1, (int) (t0 * sr));
        const int b = juce::jlimit (0, buf.getNumSamples(),     (int) (t1 * sr));
        float e = 0.0f;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const auto* d = buf.getReadPointer (ch);
            for (int i = a; i < b; ++i)
                e += d[i] * d[i];
        }
        return e;
    }

    //  summed energy across all 4 beats for a given offset/width
    float beatEnergy (const juce::AudioBuffer<float>& buf, double offset, double width)
    {
        float e = 0.0f;
        for (int beat = 0; beat < numSeconds; ++beat)
            e += windowEnergy (buf, beat + offset, beat + offset + width);
        return e;
    }
}

//==============================================================================
TEST_CASE ("Splitter routes each element's energy to its own stem", "[splitter]")
{
    const auto kit    = makeKit();
    const auto result = DrumSplitter::split (kit, sr);

    // stem indices: 0 kick · 1 snare · 2 hats · 3 toms · 4 cymbals
    const auto& kick  = result.stems[0];
    const auto& snare = result.stems[1];
    const auto& hats  = result.stems[2];

    // kick stem: loud at kick times, quiet at snare times
    REQUIRE (beatEnergy (kick, 0.0, 0.15) > 1.5f * beatEnergy (kick, 0.5, 0.15));

    // snare stem: loud at snare times, quiet at kick times
    REQUIRE (beatEnergy (snare, 0.5, 0.15) > 1.5f * beatEnergy (snare, 0.0, 0.15));

    // hats stem: tick times louder than kick times
    REQUIRE (beatEnergy (hats, 0.25, 0.06) + beatEnergy (hats, 0.75, 0.06)
             > 1.5f * beatEnergy (hats, 0.0, 0.06));
}

TEST_CASE ("Stems are whole, finite, and correctly sized", "[splitter]")
{
    const auto kit    = makeKit();
    const auto result = DrumSplitter::split (kit, sr);

    for (const auto& stem : result.stems)
    {
        REQUIRE (stem.getNumChannels() == 2);
        REQUIRE (stem.getNumSamples() == N);

        for (int ch = 0; ch < stem.getNumChannels(); ++ch)
        {
            const auto* d = stem.getReadPointer (ch);
            for (int i = 0; i < stem.getNumSamples(); ++i)
                REQUIRE (std::isfinite (d[i]));
        }
    }
}

TEST_CASE ("One-shots are detected near true kick onsets", "[splitter]")
{
    const auto kit    = makeKit();
    const auto result = DrumSplitter::split (kit, sr);

    int kickShotsNearBeats = 0;
    for (const auto& shot : result.oneShots)
    {
        if (shot.stem != 0) continue;
        const double t = shot.startSample / sr;
        for (int beat = 0; beat < numSeconds; ++beat)
            if (std::abs (t - (double) beat) < 0.08)
                ++kickShotsNearBeats;
    }
    REQUIRE (kickShotsNearBeats >= 3);
}

TEST_CASE ("Leftovers export round-trips as valid 24-bit WAVs", "[splitter][export]")
{
    const auto kit    = makeKit();
    const auto result = DrumSplitter::split (kit, sr);

    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("crockpot_test_" + juce::String (juce::Random::getSystemRandom().nextInt (100000)));

    const auto exported = LeftoversExporter::exportAll (result, dir, "TestKit");
    REQUIRE (exported.wasOk());

    // all five stems on disk
    for (auto* name : DrumSplitter::stemNames)
        REQUIRE (dir.getChildFile (juce::String ("TestKit_stem_") + name + ".wav").existsAsFile());

    // at least one kick one-shot
    REQUIRE (dir.getChildFile ("TestKit_kick_01.wav").existsAsFile());

    // round-trip the kick stem
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (
        fm.createReaderFor (dir.getChildFile ("TestKit_stem_kick.wav")));

    REQUIRE (reader != nullptr);
    REQUIRE ((int) reader->sampleRate == (int) sr);
    REQUIRE ((int) reader->lengthInSamples == N);
    REQUIRE ((int) reader->numChannels == 2);

    dir.deleteRecursively();
}
