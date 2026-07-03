#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/*  The Crock-Pot — parameter IDs + layout (M2: the full pedalboard).

    IDs are a stable contract with Ableton (automation, macros, saved sets):
    NEVER rename or remove an ID once shipped — add new ones instead.

    Defaults are curated for "instant warmth": Saturation + gentle Tape are
    live, everything else starts bypassed. Simmer at 0 = untouched dish.
*/
namespace params
{
    // --- saturation ---------------------------------------------------------
    inline constexpr auto satDrive      = "sat_drive";
    inline constexpr auto satMix        = "sat_mix";
    inline constexpr auto satBypass     = "sat_bypass";
    // --- resampler / bitcrush ----------------------------------------------
    inline constexpr auto resDown       = "res_down";
    inline constexpr auto resCrush      = "res_crush";
    inline constexpr auto resMix        = "res_mix";
    inline constexpr auto resBypass     = "res_bypass";
    // --- tape ----------------------------------------------------------------
    inline constexpr auto tapeWarmth    = "tape_warmth";
    inline constexpr auto tapeWobble    = "tape_wobble";
    inline constexpr auto tapeTone      = "tape_tone";
    inline constexpr auto tapeMix       = "tape_mix";
    inline constexpr auto tapeBypass    = "tape_bypass";
    // --- chorus ---------------------------------------------------------------
    inline constexpr auto chorusRate    = "chorus_rate";
    inline constexpr auto chorusDepth   = "chorus_depth";
    inline constexpr auto chorusMix     = "chorus_mix";
    inline constexpr auto chorusBypass  = "chorus_bypass";
    // --- tremolo ----------------------------------------------------------------
    inline constexpr auto tremRate      = "trem_rate";
    inline constexpr auto tremDepth     = "trem_depth";
    inline constexpr auto tremMix       = "trem_mix";
    inline constexpr auto tremBypass    = "trem_bypass";
    // --- reverse -----------------------------------------------------------------
    inline constexpr auto revWindow     = "rev_window";
    inline constexpr auto revMix        = "rev_mix";
    inline constexpr auto revBypass     = "rev_bypass";
    // --- delay --------------------------------------------------------------------
    inline constexpr auto delayTime     = "delay_time";
    inline constexpr auto delayFeedback = "delay_feedback";
    inline constexpr auto delayTone     = "delay_tone";
    inline constexpr auto delayMix      = "delay_mix";
    inline constexpr auto delayBypass   = "delay_bypass";
    // --- reverb ----------------------------------------------------------------------
    inline constexpr auto verbSize      = "verb_size";
    inline constexpr auto verbDamp      = "verb_damp";
    inline constexpr auto verbWidth     = "verb_width";
    inline constexpr auto verbMix       = "verb_mix";
    inline constexpr auto verbBypass    = "verb_bypass";
    // --- global -------------------------------------------------------------------------
    inline constexpr auto simmer        = "simmer";
    inline constexpr auto monoFreq      = "mono_freq";
    inline constexpr auto monoOn        = "mono_on";
    inline constexpr auto outputTrim    = "output_trim";

    // non-param state property: user block order, e.g. "0,1,2,3,4,5,6,7"
    inline constexpr auto chainOrderProperty = "chain_order";
    inline constexpr auto defaultChainOrder  = "0,1,2,3,4,5,6,7";

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using FloatParam = juce::AudioParameterFloat;
        using BoolParam  = juce::AudioParameterBool;
        using Attr       = juce::AudioParameterFloatAttributes;
        using Range      = juce::NormalisableRange<float>;

        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        auto pct = [] (const char* id, const char* name, float def)
        {
            return std::make_unique<FloatParam> (juce::ParameterID { id, 1 }, name,
                                                 Range { 0.0f, 100.0f, 0.1f }, def,
                                                 Attr{}.withLabel ("%"));
        };
        auto onOff = [] (const char* id, const char* name, bool def)
        {
            return std::make_unique<BoolParam> (juce::ParameterID { id, 1 }, name, def);
        };

        layout.add (pct (satDrive, "Sat Drive", 35.0f));
        layout.add (pct (satMix,   "Sat Mix",  100.0f));
        layout.add (onOff (satBypass, "Sat Bypass", false));

        layout.add (pct (resDown,  "Crush Rate",  30.0f));
        layout.add (pct (resCrush, "Crush Bits",  25.0f));
        layout.add (pct (resMix,   "Crush Mix",  100.0f));
        layout.add (onOff (resBypass, "Crush Bypass", true));

        layout.add (pct (tapeWarmth, "Tape Warmth", 30.0f));
        layout.add (pct (tapeWobble, "Tape Wobble", 15.0f));
        layout.add (pct (tapeTone,   "Tape Tone",   70.0f));
        layout.add (pct (tapeMix,    "Tape Mix",    35.0f));
        layout.add (onOff (tapeBypass, "Tape Bypass", false));

        layout.add (pct (chorusRate,  "Chorus Rate",  25.0f));
        layout.add (pct (chorusDepth, "Chorus Depth", 40.0f));
        layout.add (pct (chorusMix,   "Chorus Mix",   50.0f));
        layout.add (onOff (chorusBypass, "Chorus Bypass", true));

        layout.add (pct (tremRate,  "Trem Rate",  40.0f));
        layout.add (pct (tremDepth, "Trem Depth", 50.0f));
        layout.add (pct (tremMix,   "Trem Mix",  100.0f));
        layout.add (onOff (tremBypass, "Trem Bypass", true));

        layout.add (pct (revWindow, "Reverse Window", 50.0f));
        layout.add (pct (revMix,    "Reverse Mix",    50.0f));
        layout.add (onOff (revBypass, "Reverse Bypass", true));

        layout.add (pct (delayTime,     "Delay Time",     45.0f));
        layout.add (pct (delayFeedback, "Delay Feedback", 35.0f));
        layout.add (pct (delayTone,     "Delay Tone",     60.0f));
        layout.add (pct (delayMix,      "Delay Mix",      35.0f));
        layout.add (onOff (delayBypass, "Delay Bypass", true));

        layout.add (pct (verbSize,  "Verb Size",  55.0f));
        layout.add (pct (verbDamp,  "Verb Damp",  45.0f));
        layout.add (pct (verbWidth, "Verb Width", 90.0f));
        layout.add (pct (verbMix,   "Verb Mix",   30.0f));
        layout.add (onOff (verbBypass, "Verb Bypass", true));

        layout.add (pct (simmer, "Simmer", 0.0f));

        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { monoFreq, 1 }, "Mono Below",
            Range { 60.0f, 200.0f, 1.0f }, 110.0f, Attr{}.withLabel ("Hz")));
        layout.add (onOff (monoOn, "Mono Maker", true));

        layout.add (std::make_unique<FloatParam> (
            juce::ParameterID { outputTrim, 1 }, "Output",
            Range { -24.0f, 24.0f, 0.1f }, 0.0f, Attr{}.withLabel ("dB")));

        return layout;
    }

    /*  Simmer fan-out (D11 — character only, curated; clean → cooked).
        Additive seasoning applied at param-read time: user knobs never move.
        Weights tuned by ear at the M2 listening test — expect edits.        */
    struct Seasoned
    {
        float satDrive, resDown, tapeWarmth, tapeWobble,
              chorusDepth, delayFeedback, verbMix;
    };

    inline Seasoned season (float simmer01,
                            float satDriveV, float resDownV, float tapeWarmthV,
                            float tapeWobbleV, float chorusDepthV,
                            float delayFeedbackV, float verbMixV)
    {
        auto lift = [simmer01] (float base, float amount)
        {
            return juce::jlimit (0.0f, 100.0f, base + amount * simmer01);
        };
        return {
            lift (satDriveV,      30.0f),
            lift (resDownV,       12.0f),
            lift (tapeWarmthV,    25.0f),
            lift (tapeWobbleV,    10.0f),
            lift (chorusDepthV,    8.0f),
            lift (delayFeedbackV, 12.0f),
            lift (verbMixV,       10.0f)
        };
    }
} // namespace params
