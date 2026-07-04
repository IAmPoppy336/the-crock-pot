#pragma once

#include "CrockBlock.h"

/*  CrockBlock #9 — The Skimmer (M-extra, Poppy's ask from the gaps dossier).

    v1 slice of "the missing dynamic EQ": ONE dynamic band that FOLLOWS the
    input's pitch (monophonic tracking — built for basslines), placed on the
    fundamental or its 2nd/3rd/4th harmonic, ducking with attack/release.

    Honest scope (v2 backlog, logged in session-log): external sidechain,
    spectrum/reference matching, 2000ms lookahead, multiband. v1 = zero
    latency, transparent, one band that rides the notes.

    RT-safety notes:
      - pitch tracking = normalized autocorrelation over a member ring buffer,
        geometric lag scan + refine, once per block. No allocation, no locks.
      - the band filter is a hand-rolled RBJ peak biquad: JUCE's
        IIR::Coefficients factories allocate (ref-counted), so we compute the
        five coefficients in place each block instead.                        */
class SkimmerBlock final : public CrockBlock
{
public:
    SkimmerBlock() = default;   // explicit: MSVC (run #8) refused the implicit one

    void prepare (const juce::dsp::ProcessSpec& spec) override
    {
        sampleRate = spec.sampleRate;
        mixStage.prepare (spec, 0.0f);

        freqSm.reset (spec.sampleRate, 0.05);
        freqSm.setCurrentAndTargetValue (110.0f);

        reset();
    }

    void reset() override
    {
        mixStage.reset();
        ring.fill (0.0f);
        ringPos = 0;
        lpState = 0.0f;
        env = 0.0f;
        envRef = 1.0e-4f;
        trackedHz = 110.0f;
        for (auto& s : biquadState) s = {};
    }

    //  freq: manual Hz used when tracking off · amount: dB (- = skim, + = boost)
    //  width: 0..100% → Q 0.4..8 · attack/release ms · harmonic 0..3 → 1x..4x
    void updateParams (bool trackOn, int harmonicIdx, float manualHz,
                       float amountDb, float widthPercent,
                       float attackMs, float releaseMs,
                       float mixPercent, bool bypassed)
    {
        tracking  = trackOn;
        harmonic  = (float) juce::jlimit (1, 4, harmonicIdx + 1);
        manualFreq = juce::jlimit (40.0f, 2000.0f, manualHz);
        amount    = juce::jlimit (-24.0f, 12.0f, amountDb);
        q         = juce::jmap (juce::jlimit (0.0f, 100.0f, widthPercent) * 0.01f, 0.4f, 8.0f);

        attackA  = std::exp (-1.0 / (juce::jmax (1.0f, attackMs)  * 0.001 * sampleRate));
        releaseA = std::exp (-1.0 / (juce::jmax (5.0f, releaseMs) * 0.001 * sampleRate));

        mixStage.setTarget (mixPercent * 0.01f, bypassed);
    }

    void process (juce::dsp::AudioBlock<float> block) override
    {
        mixStage.pushDry (block);

        if (! mixStage.fullyDry())
        {
            const auto numCh = juce::jmin (block.getNumChannels(), (size_t) 2);
            const int  n     = (int) block.getNumSamples();

            // ---- feed the tracker ring (mono, gently low-passed) ----------------
            for (int i = 0; i < n; ++i)
            {
                float m = 0.0f;
                for (size_t ch = 0; ch < numCh; ++ch)
                    m += block.getChannelPointer (ch)[i];
                m /= (float) numCh;

                lpState += 0.18f * (m - lpState);          // ~1.3 kHz one-pole
                ring[(size_t) ringPos] = lpState;
                ringPos = (ringPos + 1) & (ringSize - 1);
            }

            // ---- pitch estimate once per block -----------------------------------
            if (tracking)
                trackPitch();

            const float targetHz = juce::jlimit (30.0f, 4000.0f,
                (tracking ? trackedHz : manualFreq) * harmonic);
            freqSm.setTargetValue (targetHz);

            // ---- envelope at the band (detector biquad on mono) + apply ----------
            // per block: update coefficients from smoothed freq (mid-block glide
            // handled by 50 ms smoothing; per-sample coeff update not worth it)
            freqSm.skip (n);
            const float f = freqSm.getCurrentValue();

            // band-energy probe: boosted narrow peak minus dry ~= crude bandpass
            computePeakCoeffs (f, juce::jmax (2.0f, q), 12.0f, probe);

            float dynDb = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                float mono = 0.0f;
                for (size_t ch = 0; ch < numCh; ++ch)
                    mono += block.getChannelPointer (ch)[i];
                mono /= (float) numCh;

                const float banded  = processBiquad (probe, biquadState[2], mono) - mono; // boosted band minus dry
                const float rect    = std::abs (banded);

                const double a = (rect > env ? attackA : releaseA);
                env = (float) (a * env + (1.0 - a) * rect);
            }

            // slow auto-reference so "depth" adapts to material (v1 pragmatism)
            envRef = juce::jmax (env, 0.995f * envRef);
            const float depth = (envRef > 1.0e-6f ? juce::jlimit (0.0f, 1.0f, env / envRef) : 0.0f);
            dynDb = amount * depth;

            computePeakCoeffs (f, q, dynDb, band);

            for (size_t ch = 0; ch < numCh; ++ch)
            {
                auto* d = block.getChannelPointer (ch);
                auto& st = biquadState[ch];
                for (int i = 0; i < n; ++i)
                    d[i] = processBiquad (band, st, d[i]);
            }
        }

        mixStage.mixWet (block);
    }

private:
    //==========================================================================
    struct Coeffs { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    struct State  { float z1 = 0, z2 = 0; };

    void computePeakCoeffs (float freq, float Q, float gainDb, Coeffs& c) const
    {
        // RBJ peaking EQ (Audio EQ Cookbook), computed in place — no allocation
        const float A     = std::pow (10.0f, gainDb / 40.0f);
        const float w0    = juce::MathConstants<float>::twoPi * freq / (float) sampleRate;
        const float cosw  = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * Q);

        const float a0 = 1.0f + alpha / A;
        c.b0 = (1.0f + alpha * A) / a0;
        c.b1 = (-2.0f * cosw) / a0;
        c.b2 = (1.0f - alpha * A) / a0;
        c.a1 = (-2.0f * cosw) / a0;
        c.a2 = (1.0f - alpha / A) / a0;
    }

    static float processBiquad (const Coeffs& c, State& s, float x)
    {
        // transposed direct form II
        const float y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    //==========================================================================
    void trackPitch()
    {
        // normalized autocorrelation on the most recent window, decimated 2x.
        // geometric lag sweep 40..800 Hz, refine +-1, confidence gate.
        constexpr int window = 512;                       // decimated samples
        const float sr2 = (float) sampleRate * 0.5f;

        std::array<float, window> x {};
        int idx = (ringPos - window * 2 + ringSize * 4) & (ringSize - 1);
        for (int i = 0; i < window; ++i)
        {
            x[(size_t) i] = ring[(size_t) idx];
            idx = (idx + 2) & (ringSize - 1);
        }

        float energy = 1.0e-9f;
        for (auto v : x) energy += v * v;
        if (energy < 1.0e-5f)
            return;                                        // silence: hold

        const int minLag = juce::jmax (2,  (int) (sr2 / 800.0f));
        const int maxLag = juce::jmin (window - 8, (int) (sr2 / 40.0f));

        float bestScore = 0.0f;
        int bestLag = 0;

        auto scoreAt = [&x] (int lag)
        {
            float acc = 0.0f;
            for (int i = 0; i + lag < window; ++i)
                acc += x[(size_t) i] * x[(size_t) (i + lag)];
            return acc;
        };

        for (float lagF = (float) minLag; lagF <= (float) maxLag; lagF *= 1.03f)
        {
            const int lag = (int) lagF;
            const float sc = scoreAt (lag);
            if (sc > bestScore) { bestScore = sc; bestLag = lag; }
        }

        if (bestLag > minLag && bestLag < maxLag)          // refine
            for (int lag = bestLag - 1; lag <= bestLag + 1; ++lag)
            {
                const float sc = scoreAt (lag);
                if (sc > bestScore) { bestScore = sc; bestLag = lag; }
            }

        const float confidence = bestScore / energy;
        if (bestLag > 0 && confidence > 0.30f)
            trackedHz = sr2 / (float) bestLag;             // else: hold last
    }

    //==========================================================================
    static constexpr int ringSize = 4096;                  // power of two

    double sampleRate = 44100.0;
    MixStage mixStage;

    std::array<float, ringSize> ring {};
    int ringPos = 0;
    float lpState = 0.0f;

    bool  tracking = true;
    float harmonic = 1.0f, manualFreq = 110.0f;
    float amount = -6.0f, q = 2.0f;
    double attackA = 0.99, releaseA = 0.999;

    float env = 0.0f, envRef = 1.0e-4f, trackedHz = 110.0f;

    juce::SmoothedValue<float> freqSm { 110.0f };
    Coeffs band, probe;
    std::array<State, 3> biquadState {};                   // ch0, ch1, probe

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkimmerBlock)
};
