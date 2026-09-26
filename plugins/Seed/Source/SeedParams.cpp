#include "SeedParams.h"

namespace seed
{
using namespace juce;

namespace
{
    using Attr = AudioParameterFloatAttributes;

    Attr fmt (int decimals, const String& unit)
    {
        return Attr().withLabel (unit).withStringFromValueFunction (
            [decimals, unit] (float x, int) { return String (x, decimals) + (unit.isEmpty() ? "" : " " + unit); });
    }

    NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi, 0.0f);
        r.setSkewForCentre (centre);
        return r;
    }

    template <typename P, typename... Args>
    void add (AudioProcessorValueTreeState::ParameterLayout& l, Args&&... args)
    {
        l.add (std::make_unique<P> (std::forward<Args> (args)...));
    }
} // namespace

AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout L;
    const int v1 = 1, v2 = 2, v3 = 3; // VST3 version hints: the Seed version that ADDED the param

    // ======================= TUNING (the Calculator) =======================================
    add<AudioParameterFloat> (L, ParameterID { ids::fundamental, v2 }, "Fundamental", skewed (20.0f, 2000.0f, 300.0f), 261.6256f, fmt (2, "Hz"));
    // Seed 0.5..4. Below 1 = undertones, exactly 1 = every partial on one pitch. Weird is welcome.
    add<AudioParameterFloat> (L, ParameterID { ids::seed, v1 }, "Seed", NormalisableRange<float> (0.5f, 4.0f, 0.0f), 2.0f, fmt (6, ""));
    add<AudioParameterInt> (L, ParameterID { ids::partials, v1 }, "Partials", 1, xen::kSeriesPartials, 16);
    add<AudioParameterFloat> (L, ParameterID { ids::rolloff, v1 }, "Rolloff", NormalisableRange<float> (0.0f, 3.0f, 0.0f), 1.0f, fmt (2, ""));
    add<AudioParameterChoice> (L, ParameterID { ids::method, v3 }, "Scale method", StringArray { "Dissonance", "Consonance" }, 0);
    add<AudioParameterFloat> (L, ParameterID { ids::minDip, v2 }, "Min dip strength", NormalisableRange<float> (0.0f, 50.0f, 0.0f), 2.0f, fmt (1, "%"));
    add<AudioParameterFloat> (L, ParameterID { ids::tolerance, v3 }, "Consonance tolerance", NormalisableRange<float> (0.5f, 30.0f, 0.0f), 5.0f, fmt (1, "c"));
    add<AudioParameterInt> (L, ParameterID { ids::minShared, v3 }, "Min shared partials", 1, 8, 2);

    // Hand edits of the partials: offsets from the calculated value. 0 = unmodified.
    for (int n = 1; n <= xen::kSeriesPartials; ++n)
    {
        add<AudioParameterFloat> (L, ParameterID { ids::partialLevel (n), v3 }, "Partial " + String (n) + " level edit",
                                  NormalisableRange<float> (-60.0f, 12.0f, 0.0f), 0.0f, fmt (1, "dB"));
        if (n >= 2) // the fundamental can't move
            add<AudioParameterFloat> (L, ParameterID { ids::partialPos (n), v3 }, "Partial " + String (n) + " shift edit",
                                      NormalisableRange<float> (-1200.0f, 1200.0f, 0.0f), 0.0f, fmt (2, "c"));
    }
    for (int k = 1; k <= xen::kCustomPartials; ++k)
    {
        add<AudioParameterBool> (L, ParameterID { ids::customPartialOn (k), v3 }, "Custom partial " + String (k) + " on", false);
        add<AudioParameterFloat> (L, ParameterID { ids::customPartialCents (k), v3 }, "Custom partial " + String (k) + " cents",
                                  NormalisableRange<float> (0.0f, 9600.0f, 0.0f), 1200.0f * (float) k, fmt (2, "c"));
        add<AudioParameterFloat> (L, ParameterID { ids::customPartialLevel (k), v3 }, "Custom partial " + String (k) + " level",
                                  NormalisableRange<float> (-60.0f, 0.0f, 0.0f), -12.0f, fmt (1, "dB"));
    }
    // v0.4: which partials shape the SCALE (ArtHar v1.15 tick boxes). Unticked = sound only.
    // Series partials start ticked, hand-added (custom) ones unticked. The fundamental is always in.
    const int v4 = 5;
    for (int n = 2; n <= xen::kSeriesPartials; ++n)
        add<AudioParameterBool> (L, ParameterID { ids::partialInScale (n), v4 }, "Partial " + String (n) + " in scale", true);
    for (int k = 1; k <= xen::kCustomPartials; ++k)
        add<AudioParameterBool> (L, ParameterID { ids::customPartialInScale (k), v4 }, "Custom partial " + String (k) + " in scale", false);
    // Hand edits of the scale: per found degree on/off + offset from its home; custom notes.
    for (int d = 1; d <= xen::kEditableDegrees; ++d)
    {
        add<AudioParameterBool> (L, ParameterID { ids::noteOn (d), v3 }, "Note " + String (d) + " on", true);
        add<AudioParameterFloat> (L, ParameterID { ids::noteOffset (d), v3 }, "Note " + String (d) + " move",
                                  NormalisableRange<float> (-600.0f, 600.0f, 0.0f), 0.0f, fmt (2, "c"));
    }
    for (int k = 1; k <= xen::kCustomNotes; ++k)
    {
        add<AudioParameterBool> (L, ParameterID { ids::customNoteOn (k), v3 }, "Custom note " + String (k) + " on", false);
        add<AudioParameterFloat> (L, ParameterID { ids::customNoteCents (k), v3 }, "Custom note " + String (k) + " cents",
                                  NormalisableRange<float> (0.0f, 4800.0f, 0.0f), 100.0f * (float) k, fmt (2, "c"));
    }

    // ======================= SOUND (the Synth) =============================================
    // v0.4: times go down to 0 ms (user: "attack at 0 ms, even if it clips") with 2 decimals.
    add<AudioParameterFloat> (L, ParameterID { ids::attack, v1 }, "Attack", skewed (0.0f, 5000.0f, 120.0f), 8.0f, fmt (2, "ms"));
    add<AudioParameterFloat> (L, ParameterID { ids::decay, v2 }, "Decay", skewed (0.0f, 10000.0f, 500.0f), 600.0f, fmt (2, "ms"));
    add<AudioParameterFloat> (L, ParameterID { ids::sustain, v2 }, "Sustain", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 60.0f, fmt (1, "%"));
    add<AudioParameterFloat> (L, ParameterID { ids::release, v1 }, "Release", skewed (0.0f, 10000.0f, 500.0f), 400.0f, fmt (2, "ms"));
    add<AudioParameterFloat> (L, ParameterID { ids::velocity, v2 }, "Velocity sensitivity", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 100.0f, fmt (0, "%"));
    add<AudioParameterFloat> (L, ParameterID { ids::gain, v1 }, "Gain", NormalisableRange<float> (-48.0f, 0.0f, 0.0f), -12.0f, fmt (1, "dB"));
    add<AudioParameterBool> (L, ParameterID { ids::keysFollow, v1 }, "(old) Keys follow seed - unused", false);
    add<AudioParameterBool> (L, ParameterID { ids::drone, v1 }, "Drone", false);
    add<AudioParameterInt> (L, ParameterID { ids::droneNote, v1 }, "Drone note", 24, 96, 48);

    // Keys + pitch.
    add<AudioParameterChoice> (L, ParameterID { ids::keyMode, v3 }, "Key mode", keyModeNames(), 2); // v0.4 default: Scale (user picks keys by switching notes on/off)
    add<AudioParameterFloat> (L, ParameterID { ids::magnet, v3 }, "Magnet", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 100.0f, fmt (0, "%"));
    add<AudioParameterFloat> (L, ParameterID { ids::life, v3 }, "Life", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 0.0f, fmt (0, "%"));
    add<AudioParameterFloat> (L, ParameterID { ids::glide, v3 }, "Glide", skewed (0.0f, 3000.0f, 200.0f), 40.0f, fmt (0, "ms"));
    add<AudioParameterFloat> (L, ParameterID { ids::pitch, v3 }, "Pitch shift", NormalisableRange<float> (-2400.0f, 2400.0f, 0.0f), 0.0f, fmt (1, "c"));
    add<AudioParameterFloat> (L, ParameterID { ids::fshift, v3 }, "Frequency shift", NormalisableRange<float> (-1000.0f, 1000.0f, 0.0f), 0.0f, fmt (1, "Hz"));
    add<AudioParameterInt> (L, ParameterID { ids::bendRange, v3 }, "Bend range", 0, 24, 2);

    // Additive timbre tools (on top of the calculator's loudness).
    add<AudioParameterFloat> (L, ParameterID { ids::tilt, v3 }, "Tilt", NormalisableRange<float> (-12.0f, 12.0f, 0.0f), 0.0f, fmt (1, "dB/oct"));
    add<AudioParameterFloat> (L, ParameterID { ids::oddEven, v3 }, "Odd/Even", NormalisableRange<float> (-100.0f, 100.0f, 0.0f), 0.0f, fmt (0, "%"));
    add<AudioParameterFloat> (L, ParameterID { ids::spread, v3 }, "Stereo spread", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 0.0f, fmt (0, "%"));
    add<AudioParameterFloat> (L, ParameterID { ids::shimmer, v3 }, "Shimmer", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 0.0f, fmt (0, "%"));
    add<AudioParameterFloat> (L, ParameterID { ids::shimmerRate, v3 }, "Shimmer rate", skewed (0.05f, 20.0f, 2.0f), 1.5f, fmt (2, "Hz"));
    add<AudioParameterFloat> (L, ParameterID { ids::formFreq, v3 }, "Formant freq", skewed (50.0f, 12000.0f, 1000.0f), 1000.0f, fmt (0, "Hz"));
    add<AudioParameterFloat> (L, ParameterID { ids::formGain, v3 }, "Formant gain", NormalisableRange<float> (-24.0f, 24.0f, 0.0f), 0.0f, fmt (1, "dB"));
    add<AudioParameterFloat> (L, ParameterID { ids::formWidth, v3 }, "Formant width", NormalisableRange<float> (0.1f, 4.0f, 0.0f), 1.0f, fmt (2, "oct"));

    // Subtractive tools.
    add<AudioParameterFloat> (L, ParameterID { ids::noise, v3 }, "Noise", NormalisableRange<float> (-60.0f, 0.0f, 0.0f), -60.0f, fmt (1, "dB"));
    add<AudioParameterChoice> (L, ParameterID { ids::filterMode, v3 }, "Filter", filterModeNames(), 0);
    add<AudioParameterFloat> (L, ParameterID { ids::cutoff, v3 }, "Cutoff", skewed (20.0f, 20000.0f, 1500.0f), 4000.0f, fmt (0, "Hz"));
    add<AudioParameterFloat> (L, ParameterID { ids::reso, v3 }, "Resonance", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 20.0f, fmt (0, "%"));
    add<AudioParameterFloat> (L, ParameterID { ids::keyTrack, v3 }, "Key track", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 0.0f, fmt (0, "%"));
    add<AudioParameterFloat> (L, ParameterID { ids::filterEnv, v3 }, "Filter env", NormalisableRange<float> (-8.0f, 8.0f, 0.0f), 0.0f, fmt (2, "oct"));
    add<AudioParameterFloat> (L, ParameterID { ids::filterDecay, v3 }, "Filter decay", skewed (1.0f, 5000.0f, 300.0f), 300.0f, fmt (0, "ms"));

    // FX A: Partial Delay (only chosen partials go in; "spiral" re-pitches every repeat).
    add<AudioParameterFloat> (L, ParameterID { ids::sendA, v3 }, "Delay send", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 0.0f, fmt (0, "%"));
    add<AudioParameterChoice> (L, ParameterID { ids::ruleA, v3 }, "Delay picks", sendRuleNames(), 1);
    add<AudioParameterFloat> (L, ParameterID { ids::delayTime, v3 }, "Delay time", skewed (1.0f, 2000.0f, 300.0f), 350.0f, fmt (0, "ms"));
    add<AudioParameterFloat> (L, ParameterID { ids::delayFb, v3 }, "Delay feedback", NormalisableRange<float> (0.0f, 95.0f, 0.0f), 45.0f, fmt (0, "%"));
    add<AudioParameterFloat> (L, ParameterID { ids::spiral, v3 }, "Delay spiral", NormalisableRange<float> (-1200.0f, 1200.0f, 0.0f), 0.0f, fmt (1, "c"));
    add<AudioParameterFloat> (L, ParameterID { ids::delayLevel, v3 }, "Delay level", NormalisableRange<float> (-48.0f, 6.0f, 0.0f), 0.0f, fmt (1, "dB"));
    // FX B: Rough Reverb (resonators on the scale's notes + beating twins).
    add<AudioParameterFloat> (L, ParameterID { ids::sendB, v3 }, "Reverb send", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 0.0f, fmt (0, "%"));
    add<AudioParameterChoice> (L, ParameterID { ids::ruleB, v3 }, "Reverb picks", sendRuleNames(), 0);
    add<AudioParameterFloat> (L, ParameterID { ids::rvbDecay, v3 }, "Reverb decay", skewed (0.1f, 20.0f, 3.0f), 3.0f, fmt (2, "s"));
    add<AudioParameterFloat> (L, ParameterID { ids::rough, v3 }, "Roughness", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 60.0f, fmt (0, "%"));
    add<AudioParameterFloat> (L, ParameterID { ids::rvbLevel, v3 }, "Reverb level", NormalisableRange<float> (-48.0f, 6.0f, 0.0f), -6.0f, fmt (1, "dB"));

    // Chaos toys (v0.3.1).
    const int v31 = 4;
    add<AudioParameterInt> (L, ParameterID { ids::chordSize, v31 }, "Chord size", 1, 5, 1);
    add<AudioParameterInt> (L, ParameterID { ids::chordStep, v31 }, "Chord step (keys)", 1, 12, 2);
    add<AudioParameterFloat> (L, ParameterID { ids::wander, v31 }, "Partial wander", NormalisableRange<float> (0.0f, 400.0f, 0.0f), 0.0f, fmt (1, "c"));
    add<AudioParameterFloat> (L, ParameterID { ids::wanderRate, v31 }, "Wander rate", skewed (0.02f, 20.0f, 1.0f), 0.5f, fmt (2, "Hz"));
    add<AudioParameterFloat> (L, ParameterID { ids::scramble, v31 }, "Loudness scramble", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 0.0f, fmt (0, "%"));

    // Modulators (random modifiers and friends).
    for (int m = 1; m <= kNumMods; ++m)
    {
        add<AudioParameterChoice> (L, ParameterID { ids::modShape (m), v3 }, "Mod " + String (m) + " shape", modShapeNames(), 5);
        add<AudioParameterFloat> (L, ParameterID { ids::modRate (m), v3 }, "Mod " + String (m) + " rate", skewed (0.01f, 20.0f, 1.0f), 0.3f, fmt (3, "Hz"));
        add<AudioParameterFloat> (L, ParameterID { ids::modDepth (m), v3 }, "Mod " + String (m) + " depth", NormalisableRange<float> (-100.0f, 100.0f, 0.0f), 0.0f, fmt (0, "%"));
        add<AudioParameterChoice> (L, ParameterID { ids::modTarget (m), v3 }, "Mod " + String (m) + " target", modTargetNames(), 0);
    }
    return L;
}
} // namespace seed
