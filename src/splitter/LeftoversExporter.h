#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "DrumSplitter.h"

/*  LeftoversExporter — turns a DrumSplitter::Result into files on disk.

    OFFLINE ONLY (background/message thread). Output, for base name "MyLoop":
      MyLoop_stem_kick.wav ... MyLoop_stem_cymbals.wav      (full stems)
      MyLoop_kick_01.wav, MyLoop_kick_02.wav, ...           (one-shots)
    24-bit WAV at the source sample rate — drag straight into a Drum Rack.
*/
class LeftoversExporter
{
public:
    static juce::Result exportAll (const DrumSplitter::Result& split,
                                   const juce::File& outputDir,
                                   const juce::String& baseName)
    {
        if (! outputDir.isDirectory())
        {
            const auto r = outputDir.createDirectory();
            if (r.failed())
                return juce::Result::fail ("Couldn't create output folder: " + r.getErrorMessage());
        }

        // ---- full stems -------------------------------------------------------
        for (int stem = 0; stem < DrumSplitter::numStems; ++stem)
        {
            const auto& buf = split.stems[(size_t) stem];
            if (buf.getNumSamples() == 0)
                continue;

            const auto file = outputDir.getChildFile (
                baseName + "_stem_" + DrumSplitter::stemNames[(size_t) stem] + ".wav");

            const auto r = writeWav (buf, 0, buf.getNumSamples(), split.sampleRate, file);
            if (r.failed())
                return r;
        }

        // ---- one-shots ("Leftovers") -------------------------------------------
        std::array<int, DrumSplitter::numStems> counters {};

        for (const auto& shot : split.oneShots)
        {
            const auto& buf = split.stems[(size_t) shot.stem];
            const int count = ++counters[(size_t) shot.stem];

            const auto file = outputDir.getChildFile (
                baseName + "_" + DrumSplitter::stemNames[(size_t) shot.stem]
                         + "_" + juce::String (count).paddedLeft ('0', 2) + ".wav");

            const auto r = writeWav (buf, shot.startSample,
                                     juce::jmin (shot.lengthSamples,
                                                 buf.getNumSamples() - shot.startSample),
                                     split.sampleRate, file);
            if (r.failed())
                return r;
        }

        return juce::Result::ok();
    }

private:
    static juce::Result writeWav (const juce::AudioBuffer<float>& buf,
                                  int startSample, int numSamples,
                                  double sampleRate, const juce::File& file)
    {
        if (numSamples <= 0)
            return juce::Result::ok();   // nothing to write, not an error

        file.deleteFile();

        auto stream = file.createOutputStream();
        if (stream == nullptr)
            return juce::Result::fail ("Couldn't open for writing: " + file.getFullPathName());

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (stream.get(), sampleRate,
                                 (unsigned int) buf.getNumChannels(), 24, {}, 0));
        if (writer == nullptr)
            return juce::Result::fail ("Couldn't create WAV writer for: " + file.getFileName());

        stream.release();   // writer owns the stream now

        if (! writer->writeFromAudioSampleBuffer (buf, startSample, numSamples))
            return juce::Result::fail ("Write failed: " + file.getFileName());

        return juce::Result::ok();
    }
};
