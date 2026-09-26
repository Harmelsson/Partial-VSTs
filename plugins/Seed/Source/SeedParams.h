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
#include "xen/CalcModel.h"

namespace seed
{
inline constexpr const char* kVersion = "0.4.0";

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
    inline constexpr const char* sendA = "sendA";
    inline constexpr const char* ruleA = "ruleA";
    inline constexpr const char* delayTime = "delayTime";
    inline constexpr const char* delayFb = "delayFb";
    inline constexpr const char* spiral = "spiral";
    inline constexpr const char* delayLevel = "delayLevel";
    inline constexpr const char* sendB = "sendB";
    inline constexpr const char* ruleB = "ruleB";
    inline constexpr const char* rvbDecay = "rvbDecay";
    inline constexpr const char* rough = "rough";
    inline constexpr const char* rvbLevel = "rvbLevel";
    // v0.3.1 — chaos toys
    inline constexpr const char* chordSize = "chordSize";
    inline constexpr const char* chordStep = "chordStep";
    inline constexpr const char* wander = "wander";
    inline constexpr const char* wanderRate = "wanderRate";
    inline constexpr const char* scramble = "scramble";

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
    inline juce::String modShape (int m) { return "mod" + juce::String (m) + "shape"; }    // m = 1..4
    inline juce::String modRate (int m) { return "mod" + juce::String (m) + "rate"; }
    inline juce::String modDepth (int m) { return "mod" + juce::String (m) + "depth"; }
    inline juce::String modTarget (int m) { return "mod" + juce::String (m) + "target"; }
} // namespace ids

inline constexpr int kNumMods = 4;

inline const juce::StringArray& keyModeNames()
{
    static const juce::StringArray a { "12-EDO", "Seed-12", "Scale", "Magnet" };
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
    static const juce::StringArray a { "Sine", "Triangle", "Saw", "Square", "S&H", "Smooth rnd", "Drift" };
    return a;
}
/// Modulation targets. Index 0 = none. The ID list must stay aligned with the names.
inline const juce::StringArray& modTargetNames()
{
    static const juce::StringArray a { "None", "Seed", "Fundamental", "Rolloff", "Min dip", "Pitch", "Freq shift",
                                       "Tilt", "Odd/Even", "Spread", "Shimmer", "Formant freq", "Formant gain",
                                       "Noise", "Cutoff", "Resonance", "Filter env", "Send A", "Delay time",
                                       "Delay fb", "Spiral", "Send B", "Rvb decay", "Roughness", "Magnet",
                                       "Life", "Glide", "Gain", "Wander", "Scramble" };
    return a;
}
inline const std::vector<const char*>& modTargetIds()
{
    static const std::vector<const char*> v { "", ids::seed, ids::fundamental, ids::rolloff, ids::minDip, ids::pitch,
                                              ids::fshift, ids::tilt, ids::oddEven, ids::spread, ids::shimmer,
                                              ids::formFreq, ids::formGain, ids::noise, ids::cutoff, ids::reso,
                                              ids::filterEnv, ids::sendA, ids::delayTime, ids::delayFb, ids::spiral,
                                              ids::sendB, ids::rvbDecay, ids::rough, ids::magnet, ids::life,
                                              ids::glide, ids::gain, ids::wander, ids::scramble };
    return v;
}

/// True if a parameter belongs to the TUNING (calculator) group.
inline bool isTuningParam (const juce::String& id)
{
    static const juce::StringArray core { ids::seed, ids::partials, ids::rolloff, ids::fundamental, ids::minDip,
                                          ids::method, ids::tolerance, ids::minShared };
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
