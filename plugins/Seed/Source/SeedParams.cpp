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
    add<AudioParameterChoice> (L, ParameterID { ids::method, v3 }, "Scale method", methodNames(), 0);
    // v0.6: EDO / Seed-EDO methods: the notes are N equal steps of 1200 c / of the seed's "octave".
    add<AudioParameterInt> (L, ParameterID { ids::edoSteps, 7 }, "EDO steps", 1, 120, 12);
    // v0.6: freeze the current scale. The notes stay while seed / partials / method change.
    add<AudioParameterBool> (L, ParameterID { ids::scaleLock, 7 }, "Scale lock", false);
    // v0.7: freeze the partials (seed stretch, rolloff, count) so the timbre stays while the seed moves.
    add<AudioParameterBool> (L, ParameterID { ids::partialsLock, 8 }, "Partials lock", false);
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
    add<AudioParameterInt> (L, ParameterID { ids::baseKey, 7 }, "Base key (plays the unison)", 0, 127, 60);
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

    // Chaos toys (v0.3.1).
    const int v31 = 4;
    add<AudioParameterInt> (L, ParameterID { ids::chordSize, v31 }, "Chord size", 1, 5, 1);
    add<AudioParameterInt> (L, ParameterID { ids::chordStep, v31 }, "Chord step (keys)", 1, 12, 2);
    add<AudioParameterFloat> (L, ParameterID { ids::wander, v31 }, "Partial wander", NormalisableRange<float> (0.0f, 400.0f, 0.0f), 0.0f, fmt (1, "c"));
    add<AudioParameterFloat> (L, ParameterID { ids::wanderRate, v31 }, "Wander rate", skewed (0.02f, 20.0f, 1.0f), 0.5f, fmt (2, "Hz"));
    add<AudioParameterFloat> (L, ParameterID { ids::scramble, v31 }, "Loudness scramble", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 0.0f, fmt (0, "%"));

    // ---- v0.5 ------------------------------------------------------------------------------
    const int v5 = 6;
    add<AudioParameterBool> (L, ParameterID { ids::shimmerSync, v5 }, "Shimmer sync (BPM)", false);
    add<AudioParameterChoice> (L, ParameterID { ids::shimmerDiv, v5 }, "Shimmer division (old)", syncDivNames(), 4);
    add<AudioParameterBool> (L, ParameterID { ids::wanderSync, v5 }, "Wander sync (BPM)", false);
    add<AudioParameterChoice> (L, ParameterID { ids::wanderDiv, v5 }, "Wander division (old)", syncDivNames(), 2);
    // ---- v0.6: one division list with straight / triplet / dotted versions (the old *Div params stay, unused).
    const int v7 = 7;
    add<AudioParameterChoice> (L, ParameterID { ids::shimmerBeat, v7 }, "Shimmer division", beatNames(), kBeatDefaultBeat);
    add<AudioParameterChoice> (L, ParameterID { ids::wanderBeat, v7 }, "Wander division", beatNames(), kBeatDefaultBar);

    // FX rack: 5 slots, processed top to bottom. Default: delay, rough reverb, chirp reverb (all
    // silent until their Mix is raised), a transparent Squash comp, and an empty slot.
    const int defType[kNumFx] = { FxDelay, FxRough, FxChirp, FxSquash, FxOff, FxOff, FxOff, FxOff, FxOff, FxOff, FxOff, FxOff };
    const int defIn[kNumFx] = { 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    const float defPicks[kNumFx] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    const float defMix[kNumFx] = { 0.0f, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    for (int sl = 1; sl <= kNumFx; ++sl)
    {
        const String P = "FX " + String (sl) + " ";
        auto F = [sl] (const char* n) { return ids::fx (sl, n); };
        add<AudioParameterChoice> (L, ParameterID { F ("type"), v5 }, P + "type", fxTypeNames(), defType[sl - 1]);
        add<AudioParameterBool> (L, ParameterID { F ("on"), v5 }, P + "on", true);
        add<AudioParameterChoice> (L, ParameterID { F ("in"), v5 }, P + "input", fxInputNames(), defIn[sl - 1]);
        // Picks: a continuous morph over the pick rules (1.5 = halfway between Odd and Even).
        add<AudioParameterFloat> (L, ParameterID { F ("picks"), v5 }, P + "picks",
                                  NormalisableRange<float> (0.0f, (float) (sendRuleNames().size() - 1), 0.0f), defPicks[sl - 1],
                                  AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) {
                                      const auto& n = sendRuleNames();
                                      const int i0 = juce::jlimit (0, n.size() - 1, (int) std::floor (x));
                                      const float f = x - (float) i0;
                                      if (f < 0.005f || i0 == n.size() - 1) return n[i0];
                                      return n[i0] + " " + String (juce::roundToInt ((1.0f - f) * 100)) + "% / " + n[i0 + 1] + " " + String (juce::roundToInt (f * 100)) + "%";
                                  }));
        add<AudioParameterFloat> (L, ParameterID { F ("mix"), v5 }, P + "mix", NormalisableRange<float> (0.0f, 100.0f, 0.0f), defMix[sl - 1], fmt (1, "%"));
        add<AudioParameterFloat> (L, ParameterID { F ("pan"), v5 }, P + "pan", NormalisableRange<float> (-100.0f, 100.0f, 0.0f), 0.0f, fmt (1, "%"));
        add<AudioParameterFloat> (L, ParameterID { F ("width"), v5 }, P + "width", NormalisableRange<float> (0.0f, 200.0f, 0.0f), 100.0f, fmt (0, "%"));
        // Partial Delay
        add<AudioParameterFloat> (L, ParameterID { F ("dTime"), v5 }, P + "delay time", skewed (1.0f, 4000.0f, 300.0f), 350.0f, fmt (2, "ms"));
        add<AudioParameterBool> (L, ParameterID { F ("dSync"), v5 }, P + "delay sync (BPM)", false);
        add<AudioParameterChoice> (L, ParameterID { F ("dDiv"), v5 }, P + "delay division (old)", syncDivNames(), 5);
        add<AudioParameterChoice> (L, ParameterID { F ("dBeat"), v7 }, P + "delay division", beatNames(), 3 * 3); // 2/16 = an eighth
        add<AudioParameterFloat> (L, ParameterID { F ("dFb"), v5 }, P + "delay feedback", NormalisableRange<float> (0.0f, 95.0f, 0.0f), 45.0f, fmt (1, "%"));
        add<AudioParameterFloat> (L, ParameterID { F ("dSpiral"), v5 }, P + "delay spiral", NormalisableRange<float> (-1200.0f, 1200.0f, 0.0f), 0.0f, fmt (2, "c"));
        add<AudioParameterFloat> (L, ParameterID { F ("dPing"), v5 }, P + "delay ping-pong", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 100.0f, fmt (0, "%"));
        add<AudioParameterFloat> (L, ParameterID { F ("dTone"), v5 }, P + "delay tone", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 80.0f, fmt (0, "%"));
        // Rough Reverb
        add<AudioParameterFloat> (L, ParameterID { F ("rDecay"), v5 }, P + "rough decay", skewed (0.1f, 20.0f, 3.0f), 3.0f, fmt (2, "s"));
        add<AudioParameterFloat> (L, ParameterID { F ("rRough"), v5 }, P + "roughness", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 60.0f, fmt (1, "%"));
        add<AudioParameterFloat> (L, ParameterID { F ("rDamp"), v5 }, P + "rough damping", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 40.0f, fmt (0, "%"));
        // Chirp Reverb
        add<AudioParameterFloat> (L, ParameterID { F ("cDecay"), v5 }, P + "chirp decay", skewed (0.1f, 20.0f, 3.0f), 2.5f, fmt (2, "s"));
        add<AudioParameterFloat> (L, ParameterID { F ("cChirp"), v5 }, P + "chirp amount", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 60.0f, fmt (0, "%"));
        add<AudioParameterFloat> (L, ParameterID { F ("cSize"), v5 }, P + "chirp size", skewed (10.0f, 300.0f, 60.0f), 60.0f, fmt (1, "ms"));
        add<AudioParameterFloat> (L, ParameterID { F ("cDamp"), v5 }, P + "chirp damping", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 40.0f, fmt (0, "%"));
        // Squash compressor
        add<AudioParameterFloat> (L, ParameterID { F ("kThr"), v5 }, P + "squash threshold", NormalisableRange<float> (-60.0f, 0.0f, 0.0f), -18.0f, fmt (1, "dB"));
        add<AudioParameterFloat> (L, ParameterID { F ("kSlope"), v5 }, P + "squash slope", NormalisableRange<float> (-1.0f, 1.0f, 0.0f), 1.0f, fmt (2, ""));
        add<AudioParameterFloat> (L, ParameterID { F ("kAtt"), v5 }, P + "squash attack", skewed (0.05f, 200.0f, 10.0f), 10.0f, fmt (2, "ms"));
        add<AudioParameterFloat> (L, ParameterID { F ("kRel"), v5 }, P + "squash release", skewed (1.0f, 2000.0f, 150.0f), 150.0f, fmt (1, "ms"));
        add<AudioParameterFloat> (L, ParameterID { F ("kMake"), v5 }, P + "squash makeup", NormalisableRange<float> (0.0f, 24.0f, 0.0f), 0.0f, fmt (1, "dB"));
    }

    // Modulators (random modifiers and friends). v0.6: 8 of them; targets are LINKS made by dragging a
    // modulator onto a field (stored in the plugin state, not as parameters). Mods 1-4 keep their old
    // single-target / division parameters as legacy (old projects are converted to links on load).
    for (int m = 1; m <= kNumMods; ++m)
    {
        add<AudioParameterChoice> (L, ParameterID { ids::modShape (m), v3 }, "Mod " + String (m) + " shape", modShapeNames(), 5);
        add<AudioParameterFloat> (L, ParameterID { ids::modRate (m), v3 }, "Mod " + String (m) + " rate", skewed (0.01f, 20.0f, 1.0f), 0.3f, fmt (3, "Hz"));
        add<AudioParameterFloat> (L, ParameterID { ids::modDepth (m), v3 }, "Mod " + String (m) + " depth", NormalisableRange<float> (-100.0f, 100.0f, 0.0f), 0.0f, fmt (3, "%"));
        if (m <= kLegacyMods)
            add<AudioParameterChoice> (L, ParameterID { ids::modTarget (m), v3 }, "Mod " + String (m) + " target (old)", modTargetNames(), 0);
        add<AudioParameterBool> (L, ParameterID { ids::modSync (m), m <= kLegacyMods ? 6 : 7 }, "Mod " + String (m) + " sync (BPM)", false);
        if (m <= kLegacyMods)
            add<AudioParameterChoice> (L, ParameterID { ids::modDiv (m), 6 }, "Mod " + String (m) + " division (old)", syncDivNames(), 2);
        add<AudioParameterChoice> (L, ParameterID { ids::modBeat (m), v7 }, "Mod " + String (m) + " division", beatNames(), kBeatDefaultBar);
        // v0.7: trigger. Never = free-running; MIDI = restarts on every note-on; Audio = restarts when the level
        // of the chosen source rises through the threshold.
        add<AudioParameterChoice> (L, ParameterID { ids::modTrig (m), 8 }, "Mod " + String (m) + " trigger", trigNames(), 0);
        add<AudioParameterFloat> (L, ParameterID { ids::modThr (m), 8 }, "Mod " + String (m) + " audio threshold", NormalisableRange<float> (-60.0f, 0.0f, 0.0f), -24.0f, fmt (1, "dB"));
        add<AudioParameterChoice> (L, ParameterID { ids::modSrc (m), 8 }, "Mod " + String (m) + " audio source", trigSourceNames(), 1);
        add<AudioParameterBool> (L, ParameterID { ids::modLoop (m), 8 }, "Mod " + String (m) + " loop", true);
    }

    // 5-band EQ (per voice, before the FX rack). Bands start switched off.
    const int defEqType[kEqBands] = { pvst::eq::LowShelf, pvst::eq::Bell, pvst::eq::Bell, pvst::eq::Bell, pvst::eq::HighShelf };
    const float defEqFreq[kEqBands] = { 100.0f, 300.0f, 1000.0f, 3500.0f, 9000.0f };
    StringArray eqTypes;
    for (auto* n : pvst::eq::kTypeNames) eqTypes.add (n);
    for (int b = 1; b <= kEqBands; ++b)
    {
        const String P = "EQ " + String (b) + " ";
        add<AudioParameterBool> (L, ParameterID { ids::eq (b, "on"), v7 }, P + "on", false);
        add<AudioParameterChoice> (L, ParameterID { ids::eq (b, "type"), v7 }, P + "type", eqTypes, defEqType[b - 1]);
        add<AudioParameterFloat> (L, ParameterID { ids::eq (b, "freq"), v7 }, P + "frequency", skewed (20.0f, 20000.0f, 1000.0f), defEqFreq[b - 1], fmt (0, "Hz"));
        add<AudioParameterFloat> (L, ParameterID { ids::eq (b, "gain"), v7 }, P + "gain", NormalisableRange<float> (-24.0f, 24.0f, 0.0f), 0.0f, fmt (1, "dB"));
        add<AudioParameterFloat> (L, ParameterID { ids::eq (b, "q"), v7 }, P + "Q", skewed (0.1f, 18.0f, 1.0f), 0.7071f, fmt (2, ""));
        add<AudioParameterChoice> (L, ParameterID { ids::eq (b, "steep"), v7 }, P + "steepness", eqSteepNames(), 0);
        add<AudioParameterFloat> (L, ParameterID { ids::eq (b, "kt"), v7 }, P + "key track", NormalisableRange<float> (0.0f, 100.0f, 0.0f), 0.0f, fmt (0, "%"));
    }
    return L;
}
} // namespace seed
