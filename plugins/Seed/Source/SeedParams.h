#pragma once
// Every Seed parameter in one place: IDs, ranges, defaults, and which GROUP it belongs to.
//
// Two groups, kept strictly apart (user: "the synth should be separate from the calculator"):
//   TUNING = the Calculator: fundamental, seed, partials, rolloff, method + its settings, and all
//            hand edits of partials and notes. Presets can load ONLY this group ("same tuning,
//            new sound").
//   SOUND  = the Synth: envelope, keys, timbre, filter, FX, modulators, output.
// Everything the user can change is a host parameter, so FL Studio can automate all of it.
//
// ID RULES: never rename or reuse an ID (FL projects and automation clips store them). Legacy
// IDs stay even when unused (e.g. keysFollow, replaced by keyMode in v0.3).
#include <juce_audio_processors/juce_audio_processors.h>
#include "SeedEngine.h"
#include "xen/CalcModel.h"

namespace seed
{
inline constexpr const char* kVersion = "0.7.0";

namespace ids
{
    // v0.1
    inline constexpr const char* seed = "seed";
    inline constexpr const char* partials = "partials";
    inline constexpr const char* rolloff = "rolloff";
    inline constexpr const char* attack = "attack";
    inline constexpr const char* release = "release";
    inline constexpr const char* gain = "gain";
    inline constexpr const char* keysFollow = "keysFollow"; // legacy (unused since v0.3)
    inline constexpr const char* drone = "drone";
    inline constexpr const char* droneNote = "droneNote";
    // v0.2
    inline constexpr const char* fundamental = "fundamental";
    inline constexpr const char* minDip = "minDip";
    inline constexpr const char* decay = "decay";
    inline constexpr const char* sustain = "sustain";
    inline constexpr const char* velocity = "velocity";
    // v0.3 — calculator
    inline constexpr const char* method = "method";
    inline constexpr const char* tolerance = "tolerance";
    inline constexpr const char* minShared = "minShared";
    // v0.3 — synth
    inline constexpr const char* keyMode = "keyMode";
    inline constexpr const char* magnet = "magnet";
    inline constexpr const char* life = "life";
    inline constexpr const char* glide = "glide";
    inline constexpr const char* pitch = "pitch";
    inline constexpr const char* fshift = "fshift";
    inline constexpr const char* bendRange = "bendRange";
    inline constexpr const char* tilt = "tilt";
    inline constexpr const char* oddEven = "oddEven";
    inline constexpr const char* spread = "spread";
    inline constexpr const char* shimmer = "shimmer";
    inline constexpr const char* shimmerRate = "shimmerRate";
    inline constexpr const char* formFreq = "formFreq";
    inline constexpr const char* formGain = "formGain";
    inline constexpr const char* formWidth = "formWidth";
    inline constexpr const char* noise = "noise";
    inline constexpr const char* filterMode = "filterMode";
    inline constexpr const char* cutoff = "cutoff";
    inline constexpr const char* reso = "reso";
    inline constexpr const char* keyTrack = "keyTrack";
    inline constexpr const char* filterEnv = "filterEnv";
    inline constexpr const char* filterDecay = "filterDecay";
    // v0.3.1 — chaos toys
    inline constexpr const char* chordSize = "chordSize";
    inline constexpr const char* chordStep = "chordStep";
    inline constexpr const char* wander = "wander";
    inline constexpr const char* wanderRate = "wanderRate";
    inline constexpr const char* scramble = "scramble";
    // v0.5 — tempo sync for time-based settings (Hz or BPM)
    inline constexpr const char* shimmerSync = "shimmerSync";
    inline constexpr const char* shimmerDiv = "shimmerDiv";
    inline constexpr const char* wanderSync = "wanderSync";
    inline constexpr const char* wanderDiv = "wanderDiv";

    // v0.6
    inline constexpr const char* baseKey = "baseKey";       // the MIDI key that plays the unison
    inline constexpr const char* scaleLock = "scaleLock";   // freeze the current scale (notes stay when the seed changes)
    inline constexpr const char* edoSteps = "edoSteps";     // EDO / Seed-EDO methods: number of equal steps (1..120)
    inline constexpr const char* partialsLock = "partialsLock"; // v0.7: freeze seed/rolloff/count of the PARTIALS
    inline constexpr const char* shimmerBeat = "shimmerBeat";
    inline constexpr const char* wanderBeat = "wanderBeat";

    // Generated per-item IDs.
    inline juce::String partialLevel (int n) { return "p" + juce::String (n) + "lvl"; }   // n = 1..64
    inline juce::String partialPos (int n) { return "p" + juce::String (n) + "pos"; }     // n = 2..64
    inline juce::String customPartialOn (int k) { return "cp" + juce::String (k) + "on"; } // k = 1..8
    inline juce::String customPartialCents (int k) { return "cp" + juce::String (k) + "c"; }
    inline juce::String customPartialLevel (int k) { return "cp" + juce::String (k) + "lvl"; }
    inline juce::String noteOn (int d) { return "n" + juce::String (d) + "on"; }           // d = 1..32
    inline juce::String noteOffset (int d) { return "n" + juce::String (d) + "off"; }
    inline juce::String customNoteOn (int k) { return "cn" + juce::String (k) + "on"; }    // k = 1..8
    inline juce::String customNoteCents (int k) { return "cn" + juce::String (k) + "c"; }
    inline juce::String partialInScale (int n) { return "p" + juce::String (n) + "scl"; }       // n = 2..64 (v0.4)
    inline juce::String customPartialInScale (int k) { return "cp" + juce::String (k) + "scl"; } // k = 1..8 (v0.4)
    inline juce::String modSync (int m) { return "mod" + juce::String (m) + "sync"; }  // v0.5
    inline juce::String modDiv (int m) { return "mod" + juce::String (m) + "div"; }    // v0.5
    /// FX rack slot parameter (v0.5): fx(2, "mix") -> "fx2mix". Slots 1..kNumFx.
    inline juce::String fx (int slot, const char* name) { return "fx" + juce::String (slot) + name; }
    // v0.7: LFO trigger settings per modulator
    inline juce::String modTrig (int m) { return "mod" + juce::String (m) + "trig"; }  // Never / MIDI / Audio
    inline juce::String modThr (int m) { return "mod" + juce::String (m) + "thr"; }    // audio trigger threshold (dB)
    inline juce::String modSrc (int m) { return "mod" + juce::String (m) + "src"; }    // audio source: Input / Output / Sidechain
    inline juce::String modLoop (int m) { return "mod" + juce::String (m) + "loop"; }  // loop (on) or one-shot (off)
    inline juce::String modBeat (int m) { return "mod" + juce::String (m) + "beat"; }  // v0.6 (replaces modDiv)
    /// 5-band EQ parameter (v0.6): eq(2, "freq") -> "eq2freq". Bands 1..kEqBands.
    inline juce::String eq (int band, const char* name) { return "eq" + juce::String (band) + name; }
    inline juce::String modShape (int m) { return "mod" + juce::String (m) + "shape"; }    // m = 1..4
    inline juce::String modRate (int m) { return "mod" + juce::String (m) + "rate"; }
    inline juce::String modDepth (int m) { return "mod" + juce::String (m) + "depth"; }
    inline juce::String modTarget (int m) { return "mod" + juce::String (m) + "target"; }
} // namespace ids

inline constexpr int kNumMods = 8;     // v0.6: 8 modulators (was 4)
inline constexpr int kLegacyMods = 4;  // mods 1..4 still carry the old single-target / division params
inline constexpr int kNumFx = kBuses;  // v0.6: 12 rack slots (was 5)
inline constexpr int kMaxEditableDegrees = xen::kEditableDegrees;

// FX rack (v0.5). Slot parameter names (appended to "fx<slot>"):
//   common : type, on, in (Chain/Partials), picks (morph over sendRuleNames), mix, pan, width
//   delay  : dTime (ms), dSync, dDiv, dFb, dSpiral, dPing, dTone
//   rough  : rDecay, rRough, rDamp        chirp: cDecay, cChirp, cSize, cDamp
//   squash : kThr, kSlope, kAtt, kRel, kMake
enum FxType { FxOff = 0, FxDelay, FxRough, FxChirp, FxSquash };
inline const juce::StringArray& fxTypeNames()
{
    static const juce::StringArray a { "Off", "Partial Delay", "Rough Reverb", "Chirp Reverb", "Squash Comp" };
    return a;
}
inline const juce::StringArray& fxInputNames()
{
    static const juce::StringArray a { "Chain", "Partials" };
    return a;
}
/// Tempo divisions for BPM sync (v0.6): every base length comes in three flavours,
/// straight / triplet ("t") / dotted ("."), e.g. "4/16", "4/16t", "4/16.". Choice index = base * 3 + flavour.
/// Bases: 1/64, 1/32, then n/16 for n = 1..32, then 48/16 and 64/16 (3 and 4 bars of 4/4).
inline constexpr int kBeatBases = 2 + 32 + 2;
inline constexpr int kBeatChoices = kBeatBases * 3;
inline double beatBaseLength (int base) // in quarter-note beats
{
    if (base == 0) return 0.0625;                 // 1/64 note
    if (base == 1) return 0.125;                  // 1/32 note
    if (base <= 33) return 0.25 * (base - 1);     // n/16 (a sixteenth = 0.25 beat)
    return base == 34 ? 12.0 : 16.0;              // 48/16, 64/16
}
inline juce::String beatBaseName (int base)
{
    if (base == 0) return "1/64";
    if (base == 1) return "1/32";
    if (base <= 33) return juce::String (base - 1) + "/16";
    return base == 34 ? "48/16" : "64/16";
}
inline const juce::StringArray& beatNames()
{
    static const juce::StringArray a = [] {
        juce::StringArray n;
        for (int b = 0; b < kBeatBases; ++b)
            for (const char* f : { "", "t", "." }) n.add (beatBaseName (b) + f);
        return n;
    }();
    return a;
}
/// Length in quarter-note beats of choice index i (flavour: 0 straight, 1 triplet x2/3, 2 dotted x3/2).
inline double beatLength (int i)
{
    i = std::clamp (i, 0, kBeatChoices - 1);
    const double base = beatBaseLength (i / 3);
    return i % 3 == 1 ? base * 2.0 / 3.0 : i % 3 == 2 ? base * 1.5 : base;
}
/// Default for a division: 4/16 straight (one beat), 16/16 straight (one bar).
inline constexpr int kBeatDefaultBeat = 5 * 3, kBeatDefaultBar = 17 * 3;

/// OLD (v0.5) tempo divisions, kept only so the old division parameters still exist in projects.
inline const juce::StringArray& syncDivNames()
{
    static const juce::StringArray a { "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16", "1/32",
                                       "1/2 dot", "1/4 dot", "1/8 dot", "1/2 trip", "1/4 trip", "1/8 trip", "1/16 trip" };
    return a;
}
inline double syncDivBeats (int i)
{
    static const double b[] = { 16, 8, 4, 2, 1, 0.5, 0.25, 0.125, 3, 1.5, 0.75, 4.0 / 3.0, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0 };
    return b[std::clamp (i, 0, 14)];
}

inline const juce::StringArray& methodNames()
{
    static const juce::StringArray a { "Dissonance", "Consonance", "Imported", "EDO", "Seed-EDO" };
    return a;
}
inline const juce::StringArray& keyModeNames()
{
    static const juce::StringArray a { "12-EDO", "Seed-12", "Scale", "Magnet" };
    return a;
}
inline const juce::StringArray& trigNames() { static const juce::StringArray a { "Never", "MIDI", "Audio" }; return a; }
inline const juce::StringArray& trigSourceNames() { static const juce::StringArray a { "Input", "Output", "Sidechain" }; return a; }
inline constexpr int kShapeDrawn = 7;
inline const juce::StringArray& eqSteepNames()
{
    static const juce::StringArray a { "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" };
    return a;
}
inline const juce::StringArray& filterModeNames()
{
    static const juce::StringArray a { "Off", "Low-pass", "Band-pass", "High-pass", "Notch" };
    return a;
}
/// Which partials a send picks (the "delay on specific partials" idea).
inline const juce::StringArray& sendRuleNames()
{
    static const juce::StringArray a { "All", "Odd", "Even", "Low half", "High half", "Every 3rd",
                                       "Fundamental", "All but fund.", "Custom only", "Random pick" };
    return a;
}
inline const juce::StringArray& modShapeNames()
{
    static const juce::StringArray a { "Sine", "Triangle", "Saw", "Square", "S&H", "Smooth rnd", "Drift", "Drawn" };
    return a;
}
/// Modulation targets. Index 0 = none. The ID list must stay aligned with the names.
inline const juce::StringArray& modTargetNames()
{
    static const juce::StringArray a { "None", "Seed", "Fundamental", "Rolloff", "Min dip", "Pitch", "Freq shift",
                                       "Tilt", "Odd/Even", "Spread", "Shimmer", "Formant freq", "Formant gain",
                                       "Noise", "Cutoff", "Resonance", "Filter env", "FX1 mix", "FX1 delay time",
                                       "FX1 feedback", "FX1 spiral", "FX2 mix", "FX2 decay", "FX2 roughness", "Magnet",
                                       "Life", "Glide", "Gain", "Wander", "Scramble", "FX1 picks", "FX2 picks",
                                       "FX3 mix", "FX3 picks", "FX4 mix", "FX5 mix" };
    return a;
}
/// Parameter IDs for the modulation targets, aligned with modTargetNames() ("" = none).
inline const juce::StringArray& modTargetIds()
{
    static const juce::StringArray v { "", ids::seed, ids::fundamental, ids::rolloff, ids::minDip, ids::pitch,
                                       ids::fshift, ids::tilt, ids::oddEven, ids::spread, ids::shimmer,
                                       ids::formFreq, ids::formGain, ids::noise, ids::cutoff, ids::reso,
                                       ids::filterEnv, ids::fx (1, "mix"), ids::fx (1, "dTime"), ids::fx (1, "dFb"),
                                       ids::fx (1, "dSpiral"), ids::fx (2, "mix"), ids::fx (2, "rDecay"), ids::fx (2, "rRough"),
                                       ids::magnet, ids::life, ids::glide, ids::gain, ids::wander, ids::scramble,
                                       ids::fx (1, "picks"), ids::fx (2, "picks"), ids::fx (3, "mix"), ids::fx (3, "picks"),
                                       ids::fx (4, "mix"), ids::fx (5, "mix") };
    return v;
}

/// True if a parameter belongs to the TUNING (calculator) group.
inline bool isTuningParam (const juce::String& id)
{
    static const juce::StringArray core { ids::seed, ids::partials, ids::rolloff, ids::fundamental, ids::minDip,
                                          ids::method, ids::tolerance, ids::minShared, ids::edoSteps, ids::scaleLock, ids::partialsLock };
    if (core.contains (id)) return true;
    if (id.startsWith ("p") && id.endsWith ("scl")) return id.substring (1, id.length() - 3).containsOnly ("0123456789");
    // Generated edit IDs: p{n}lvl, p{n}pos, cp{k}*, n{d}on/off, cn{k}*.
    auto isDigits = [] (const juce::String& s) { return s.isNotEmpty() && s.containsOnly ("0123456789"); };
    if (id.startsWith ("cp") || id.startsWith ("cn")) return isDigits (id.substring (2).retainCharacters ("0123456789").substring (0, 1));
    if (id.startsWith ("p") && (id.endsWith ("lvl") || id.endsWith ("pos"))) return isDigits (id.substring (1, id.length() - 3));
    if (id.startsWith ("n") && (id.endsWith ("on") || id.endsWith ("off")))
        return isDigits (id.substring (1, id.length() - (id.endsWith ("off") ? 3 : 2)));
    return false;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace seed
