#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <array>

/*  DrumSplitter — offline DSP drum-element separation (M4a, "Cook" mode).

    OFFLINE ONLY (invariant #4): runs on a background/message thread, allocates
    freely, and must NEVER be called from processBlock().

    Method (Beat C, DSP v1 — honest "rough creative split"):
      1. STFT the mono mix (2048 / hop 512, Hann).
      2. HPSS: median-filter the magnitude spectrogram along TIME → harmonic
         emphasis; along FREQUENCY → percussive emphasis; Wiener-style soft
         masks from the two.
      3. Five stem masks = percussive/harmonic mask × frequency band weights:
           kick     percussive lows        (~<140 Hz, fading out by 220)
           toms     percussive low-mids    (~120–420 Hz)          [roughest]
           snare    percussive mids        (~300–3500 Hz)
           hats     percussive highs       (>~5.5 kHz)
           cymbals  harmonic/sustained highs (>~3 kHz)            [roughest]
      4. Apply masks to each channel's spectrum, ISTFT, overlap-add.
      5. Per-stem onset detection → one-shot slice candidates ("Leftovers").

    Quality expectations are set by D12: kick/snare land well, toms/cymbals
    are the roughest cut. Bleed is part of the dish, not a defect, in v1.
*/
class DrumSplitter
{
public:
    static constexpr int numStems = 5;
    static constexpr std::array<const char*, numStems> stemNames
        { "kick", "snare", "hats", "toms", "cymbals" };

    struct OneShot
    {
        int stem = 0;
        int startSample = 0;
        int lengthSamples = 0;
        float peak = 0.0f;
    };

    struct Result
    {
        std::array<juce::AudioBuffer<float>, numStems> stems;
        std::vector<OneShot> oneShots;
        double sampleRate = 44100.0;
    };

    struct Progress
    {
        std::atomic<float> ratio { 0.0f };
        std::atomic<bool>  cancelled { false };
    };

    //==========================================================================
    static Result split (const juce::AudioBuffer<float>& input, double sampleRate,
                         Progress* progress = nullptr)
    {
        Result result;
        result.sampleRate = sampleRate;

        const int numSamples  = input.getNumSamples();
        const int numChannels = juce::jlimit (1, 2, input.getNumChannels());

        if (numSamples < fftSize)
        {
            for (auto& s : result.stems)
                s.setSize (numChannels, juce::jmax (1, numSamples), false, true, false);
            return result;
        }

        const int numFrames = 1 + (numSamples - fftSize) / hop + 2; // + tail pad

        // ---- window ----------------------------------------------------------
        std::vector<float> window ((size_t) fftSize);
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                         * (float) i / (float) fftSize);

        juce::dsp::FFT fft { fftOrder };

        // ---- 1. STFT: mono magnitudes + per-channel complex spectra ----------
        // complex layout per frame/channel: interleaved re,im — (numBins*2) floats
        std::vector<std::vector<float>> mono ((size_t) numFrames,
                                              std::vector<float> ((size_t) numBins));
        // full packed spectra (2*fftSize floats) — keeps JUCE's real-FFT layout
        // intact, including any conjugate-mirror half, whatever it contains
        std::vector<std::vector<std::vector<float>>> spectra (
            (size_t) numChannels,
            std::vector<std::vector<float>> ((size_t) numFrames,
                                             std::vector<float> ((size_t) fftSize * 2)));

        std::vector<float> fftBuf ((size_t) fftSize * 2);

        for (int f = 0; f < numFrames; ++f)
        {
            const int start = f * hop;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                std::fill (fftBuf.begin(), fftBuf.end(), 0.0f);
                const auto* src = input.getReadPointer (ch);

                for (int i = 0; i < fftSize; ++i)
                {
                    const int idx = start + i;
                    fftBuf[(size_t) i] = (idx < numSamples ? src[idx] * window[(size_t) i] : 0.0f);
                }

                fft.performRealOnlyForwardTransform (fftBuf.data());

                auto& spec = spectra[(size_t) ch][(size_t) f];
                std::copy (fftBuf.begin(), fftBuf.end(), spec.begin());
            }

            auto& m = mono[(size_t) f];
            for (int b = 0; b < numBins; ++b)
            {
                float re = 0.0f, im = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    re += spectra[(size_t) ch][(size_t) f][(size_t) (b * 2)];
                    im += spectra[(size_t) ch][(size_t) f][(size_t) (b * 2 + 1)];
                }
                m[(size_t) b] = std::sqrt (re * re + im * im) / (float) numChannels;
            }

            if (progress) progress->ratio.store (0.30f * (float) f / (float) numFrames);
            if (progress && progress->cancelled.load()) return result;
        }

        // ---- 2. HPSS via median filters ---------------------------------------
        // harmonic: median across time; percussive: median across frequency
        std::vector<std::vector<float>> harm ((size_t) numFrames,
                                              std::vector<float> ((size_t) numBins));
        std::vector<std::vector<float>> perc = harm;

        std::vector<float> scratch ((size_t) medianLen);

        for (int b = 0; b < numBins; ++b)
        {
            for (int f = 0; f < numFrames; ++f)
            {
                for (int k = 0; k < medianLen; ++k)
                {
                    const int ff = juce::jlimit (0, numFrames - 1, f + k - medianLen / 2);
                    scratch[(size_t) k] = mono[(size_t) ff][(size_t) b];
                }
                harm[(size_t) f][(size_t) b] = median (scratch);
            }
        }
        for (int f = 0; f < numFrames; ++f)
        {
            for (int b = 0; b < numBins; ++b)
            {
                for (int k = 0; k < medianLen; ++k)
                {
                    const int bb = juce::jlimit (0, numBins - 1, b + k - medianLen / 2);
                    scratch[(size_t) k] = mono[(size_t) f][(size_t) bb];
                }
                perc[(size_t) f][(size_t) b] = median (scratch);
            }
            if (progress) progress->ratio.store (0.30f + 0.25f * (float) f / (float) numFrames);
            if (progress && progress->cancelled.load()) return result;
        }

        // ---- 3+4. stem masks → per-channel resynthesis -------------------------
        const float binHz = (float) sampleRate / (float) fftSize;

        for (auto& s : result.stems)
            s.setSize (numChannels, numSamples, false, true, false);

        std::vector<float> olaNorm ((size_t) numSamples, 1.0e-6f);
        {   // window² overlap sum (identical for all stems/channels)
            for (int f = 0; f < numFrames; ++f)
                for (int i = 0; i < fftSize; ++i)
                {
                    const int idx = f * hop + i;
                    if (idx < numSamples)
                        olaNorm[(size_t) idx] += window[(size_t) i] * window[(size_t) i];
                }
        }

        std::vector<float> synthBuf ((size_t) fftSize * 2);

        for (int stem = 0; stem < numStems; ++stem)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* out = result.stems[(size_t) stem].getWritePointer (ch);

                for (int f = 0; f < numFrames; ++f)
                {
                    const auto& spec = spectra[(size_t) ch][(size_t) f];
                    const auto& H    = harm[(size_t) f];
                    const auto& P    = perc[(size_t) f];

                    std::copy (spec.begin(), spec.end(), synthBuf.begin());

                    for (int b = 0; b < numBins; ++b)
                    {
                        const float h2 = H[(size_t) b] * H[(size_t) b];
                        const float p2 = P[(size_t) b] * P[(size_t) b];
                        const float pMask = p2 / (h2 + p2 + 1.0e-9f);   // Wiener-ish
                        const float hMask = 1.0f - pMask;

                        const float freq = (float) b * binHz;
                        const float m    = stemMask (stem, freq, pMask, hMask);

                        synthBuf[(size_t) (b * 2)]     *= m;
                        synthBuf[(size_t) (b * 2 + 1)] *= m;

                        // conjugate-mirror bin (if the packed layout carries one):
                        const int mb = fftSize - b;
                        if (b > 0 && mb > numBins - 1 && mb < fftSize)
                        {
                            synthBuf[(size_t) (mb * 2)]     *= m;
                            synthBuf[(size_t) (mb * 2 + 1)] *= m;
                        }
                    }

                    fft.performRealOnlyInverseTransform (synthBuf.data());

                    const int start = f * hop;
                    for (int i = 0; i < fftSize; ++i)
                    {
                        const int idx = start + i;
                        if (idx < numSamples)
                            out[idx] += synthBuf[(size_t) i] * window[(size_t) i];
                    }
                }

                for (int i = 0; i < numSamples; ++i)
                    out[i] /= olaNorm[(size_t) i];
            }

            if (progress) progress->ratio.store (0.55f + 0.40f * (float) (stem + 1) / (float) numStems);
            if (progress && progress->cancelled.load()) return result;
        }

        // ---- 5. onsets → one-shot candidates ------------------------------------
        detectOneShots (result);

        if (progress) progress->ratio.store (1.0f);
        return result;
    }

private:
    static constexpr int fftOrder  = 11;
    static constexpr int fftSize   = 1 << fftOrder;   // 2048
    static constexpr int hop       = fftSize / 4;     // 512
    static constexpr int numBins   = fftSize / 2 + 1;
    static constexpr int medianLen = 17;

    //==========================================================================
    static float median (std::vector<float>& v)
    {
        auto mid = v.begin() + (long) v.size() / 2;
        std::nth_element (v.begin(), mid, v.end());
        return *mid;
    }

    //  Linear fade helper: 0 below lo0 / full between lo1..hi1 / 0 above hi0.
    static float bandWeight (float f, float lo0, float lo1, float hi1, float hi0)
    {
        if (f <= lo0 || f >= hi0) return 0.0f;
        if (f < lo1)  return (f - lo0) / juce::jmax (1.0f, lo1 - lo0);
        if (f > hi1)  return (hi0 - f) / juce::jmax (1.0f, hi0 - hi1);
        return 1.0f;
    }

    static float stemMask (int stem, float freq, float pMask, float hMask)
    {
        switch (stem)
        {
            case 0: return pMask * bandWeight (freq,    20.0f,   45.0f,  140.0f,  220.0f); // kick
            case 1: return pMask * bandWeight (freq,   200.0f,  300.0f, 3500.0f, 5000.0f); // snare
            case 2: return pMask * bandWeight (freq,  4000.0f, 5500.0f, 20000.0f, 22050.0f); // hats
            case 3: return pMask * bandWeight (freq,    90.0f,  120.0f,  420.0f,  650.0f); // toms
            case 4: return hMask * bandWeight (freq,  2000.0f, 3000.0f, 20000.0f, 22050.0f); // cymbals
            default: return 0.0f;
        }
    }

    //==========================================================================
    static void detectOneShots (Result& result)
    {
        const int frame = 1024, hopE = 512;

        for (int stem = 0; stem < numStems; ++stem)
        {
            const auto& buf = result.stems[(size_t) stem];
            const int n = buf.getNumSamples();
            if (n < frame * 4) continue;

            // frame energy envelope (mono sum)
            std::vector<float> env;
            env.reserve ((size_t) (n / hopE + 1));

            for (int start = 0; start + frame <= n; start += hopE)
            {
                float e = 0.0f;
                for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                {
                    const auto* d = buf.getReadPointer (ch);
                    for (int i = 0; i < frame; ++i)
                        e += d[start + i] * d[start + i];
                }
                env.push_back (e);
            }

            const float globalPeak = *std::max_element (env.begin(), env.end());
            if (globalPeak <= 1.0e-9f) continue;

            std::vector<OneShot> found;

            for (size_t t = 4; t < env.size(); ++t)
            {
                float prev = 0.0f;
                for (size_t k = t - 4; k < t; ++k) prev += env[k];
                prev *= 0.25f;

                const bool rising = env[t] > 2.2f * (prev + 1.0e-9f)
                                 && env[t] > 0.02f * globalPeak;
                if (! rising) continue;

                const int startSample = (int) t * hopE;

                // end: decay below 5% of this hit's peak, next onset, or 1 s
                float hitPeak = env[t];
                size_t tEnd = t + 1;
                for (; tEnd < env.size(); ++tEnd)
                {
                    hitPeak = juce::jmax (hitPeak, env[tEnd]);
                    if (env[tEnd] < 0.05f * hitPeak) break;
                    if (env[tEnd] > 2.2f * env[tEnd - 1] && tEnd > t + 4) break;
                    if ((int) (tEnd - t) * hopE > (int) result.sampleRate) break;
                }

                const int length = juce::jmin ((int) (tEnd - t + 1) * hopE,
                                               buf.getNumSamples() - startSample);
                found.push_back ({ stem, startSample, length, hitPeak });

                t = tEnd;   // skip past this hit
            }

            // keep the loudest 8 per stem, in time order
            std::sort (found.begin(), found.end(),
                       [] (const OneShot& a, const OneShot& b) { return a.peak > b.peak; });
            if (found.size() > 8) found.resize (8);
            std::sort (found.begin(), found.end(),
                       [] (const OneShot& a, const OneShot& b) { return a.startSample < b.startSample; });

            result.oneShots.insert (result.oneShots.end(), found.begin(), found.end());
        }
    }
};
