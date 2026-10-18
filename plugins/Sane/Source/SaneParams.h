#pragma once
// Every Sane parameter in one place: IDs, ranges, defaults.
//
// Sane = feedback-synthesis drums (see docs/DRUM_RESEARCH.md for the why). Sections follow the UI:
//   EXCITER  the noise burst that hits the loop (re-rolled on every note)
//   STRIKE   where on the head the hit lands (velocity / key move it)
//   LOOP     the resonating core (Membrane FDN / Partials / Delay) + 7 insert slots INSIDE the loop
//   MODES    where the core's modes sit: morph between two sources (A, B)
//   HIT      drawn pitch envelope + note length
// Every setting is a host parameter (FL can automate all of it) and every change is heard live,
// also on notes that are already ringing (project rule "everything is dynamic").
//
// ID RULES: never rename or reuse an ID (FL projects and automation clips store them).
#include <juce_audio_processors/juce_audio_processors.h>

namespace sane
{
inline constexpr const char* kVersion = "0.2.0";

inline constexpr int kNumSlots = 7;   // insert slots on the loop ring

namespace ids
{
    // exciter
    inline constexpr const char* exColor = "exColor";         // 0 dark .. 1 bright noise
    inline constexpr const char* exLength = "exLength";       // burst decay (ms)
    inline constexpr const char* exGrain = "exGrain";         // sparse crackle spikes in the burst
    inline constexpr const char* exLevel = "exLevel";         // dB
    inline constexpr const char* exRandom = "exRandom";       // how much each note's burst differs (0..1)
    inline constexpr const char* exVelRandom = "exVelRandom"; // velocity -> more/less randomness (-1..1)
    inline constexpr const char* exVelLength = "exVelLength"; // velocity -> longer/shorter burst (-1..1)
    inline constexpr const char* exKeyColor = "exKeyColor";   // key -> brighter/darker (-1..1)
    inline constexpr const char* exSpread = "exSpread";       // stereo: different noise per side (0..1)
    inline constexpr const char* exDirect = "exDirect";       // v0.2: the dry hit straight to the output (dB, -60 = off)
    // strike
    inline constexpr const char* stBase = "stBase";     // 0 = centre .. 0.96 = rim
    inline constexpr const char* stVel = "stVel";       // velocity -> position (-1..1; negative = loud hits go to the centre)
    inline constexpr const char* stKey = "stKey";       // key -> position (-1..1; positive = high keys go to the rim)
    inline constexpr const char* stRandom = "stRandom"; // random position spread per note (0..0.3)
    // loop
    inline constexpr const char* core = "core";         // Membrane / Partials / Delay
    inline constexpr const char* feedback = "feedback"; // 0..1: decay 30 ms .. 10 s, top 5 % = self-oscillation
    inline constexpr const char* damp = "damp";         // high modes / frequencies die faster
    inline constexpr const char* tension = "tension";   // energy -> pitch (semitones at a full hit)
    inline constexpr const char* coupling = "coupling"; // how strongly the loops feed each other
    inline constexpr const char* width = "width";       // stereo width of the loops
    // v0.2: the loop's TIME (= its length). Keys: the played key sets it (pitched drum). Hz / ms / BPM: a fixed loop
    // like a classic delay; KEY TRACK then bends it with the keys (100 % = keys play semitones again).
    inline constexpr const char* loopMode = "loopMode"; // Keys / Hz / ms / BPM
    inline constexpr const char* loopHz = "loopHz";
    inline constexpr const char* loopMs = "loopMs";
    inline constexpr const char* loopBeat = "loopBeat"; // tempo division (see beatNames)
    inline constexpr const char* keyTrack = "keyTrack";
    // modes
    inline constexpr const char* table = "table";       // physical table: Membrane / Timpani / Tabla / Bar / Plate
    inline constexpr const char* srcA = "srcA";         // endpoint A source: Physical / Calc / File / Linked
    inline constexpr const char* srcB = "srcB";         // endpoint B source
    inline constexpr const char* morph = "morph";       // 0 = A, 1 = B
    inline constexpr const char* calcSeed = "calcSeed"; // the Calc source: partials at n^log2(seed) (same math as Seed)
    inline constexpr const char* rolloff = "rolloff";   // mode n level = n^-rolloff
    // hit
    inline constexpr const char* pitchAmt = "pitchAmt";   // drawn pitch envelope: start offset (st), decays to 0
    inline constexpr const char* pitchTime = "pitchTime"; // its time constant (ms)
    inline constexpr const char* length = "length";       // note length (ms); max = off (rings out naturally)
    // global
    inline constexpr const char* tune = "tune";           // semitones
    inline constexpr const char* fine = "fine";           // cents
    inline constexpr const char* bendRange = "bendRange"; // semitones
    inline constexpr const char* gain = "gain";           // output dB

    /// Insert slot parameter: slot(3, "type") -> "s3type". Slots 1..kNumSlots.
    /// Each slot has a type, three generic knobs a/b/c whose meaning depends on the type, and (v0.2) "place":
    /// In loop (runs on every pass) / After loop (runs once on the voice's output).
    inline juce::String slot (int s, const char* name) { return "s" + juce::String (s) + name; }
} // namespace ids

enum Core { CoreMembrane = 0, CorePartials, CoreDelay };

enum LoopMode { LoopKeys = 0, LoopHz, LoopMs, LoopBpm };
inline const juce::StringArray& loopModeNames() { static const juce::StringArray a { "Keys", "Hz", "ms", "BPM" }; return a; }

enum Place { PlaceLoop = 0, PlaceAfter };
inline const juce::StringArray& placeNames() { static const juce::StringArray a { "In loop", "After loop" }; return a; }

/// Tempo divisions: 8 lengths x (straight / triplet "T" / dotted "."). Index = base * 3 + flavour.
inline constexpr int kBeatBases = 8;
inline const juce::StringArray& beatNames()
{
    static const juce::StringArray a = [] {
        juce::StringArray r;
        const char* base[kBeatBases] = { "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars" };
        for (auto* b : base) { r.add (b); r.add (juce::String (b) + " T"); r.add (juce::String (b) + " ."); }
        return r;
    }();
    return a;
}
/// Length of a beat division in seconds at `bpm` (4/4: a bar = 4 quarter notes).
inline double beatSeconds (int index, double bpm)
{
    const int base = juce::jlimit (0, kBeatBases - 1, index / 3), flavour = index % 3;
    const double wholeNotes = std::pow (2.0, base - 6);   // 1/64 .. 2 bars
    double s = wholeNotes * 4.0 * 60.0 / std::max (20.0, bpm);
    if (flavour == 1) s *= 2.0 / 3.0;
    if (flavour == 2) s *= 1.5;
    return s;
}
inline const juce::StringArray& coreNames() { static const juce::StringArray a { "Membrane", "Partials", "Delay" }; return a; }

enum Table { TabMembrane = 0, TabTimpani, TabTabla, TabBar, TabPlate };
inline const juce::StringArray& tableNames() { static const juce::StringArray a { "Membrane", "Timpani", "Tabla", "Bar", "Plate" }; return a; }

enum Source { SrcPhysical = 0, SrcCalc, SrcFile, SrcLinked };
inline const juce::StringArray& sourceNames() { static const juce::StringArray a { "Physical", "Calc", "File", "Linked" }; return a; }

// Insert types. APPEND ONLY (the state stores the index).
enum SlotType { SlotOff = 0, SlotFilter, SlotFineDelay, SlotDisperse, SlotSaturate, SlotFreqShift, SlotChorus, SlotFold, SlotRingMod, SlotFlip };
inline const juce::StringArray& slotTypeNames()
{
    static const juce::StringArray a { "Off", "Filter", "Fine Delay", "Disperse", "Saturate", "Freq Shift", "Chorus", "Fold", "Ring Mod", "Flip" };
    return a;
}

/// What the generic knobs a/b/c mean for each slot type (label + how the 0..1 value is shown).
struct SlotKnob { const char* label; };
struct SlotInfo { SlotKnob a, b, c; const char* hint; };
inline const SlotInfo& slotInfo (int type)
{
    static const SlotInfo info[] = {
        { { "-" }, { "-" }, { "-" }, "empty slot: choose an effect" },
        { { "cutoff" }, { "resonance" }, { "mode" }, "low-pass .. band-pass .. high-pass inside the loop: every pass filters again" },
        { { "time" }, { "follow pitch" }, { "wobble" }, "extra delay in the loop: lowers the modes and adds a comb" },
        { { "amount" }, { "stages" }, { "colour" }, "all-pass chain: delay per frequency stretches the partials apart" },
        { { "drive" }, { "asymmetry" }, { "mix" }, "louder = brighter; also holds a self-oscillating loop in place" },
        { { "shift" }, { "range" }, { "mix" }, "each pass shifts every frequency by a few Hz: spirals up or down" },
        { { "depth" }, { "rate" }, { "mix" }, "tiny moving delay: two heads beating against each other" },
        { { "drive" }, { "bias" }, { "mix" }, "wave folding inside the loop: metallic, wild" },
        { { "ratio" }, { "follow pitch" }, { "mix" }, "multiplies the loop by a tone: bell-like sidebands" },
        { { "chance" }, { "-" }, { "mix" }, "flips the sign of samples at random (Karplus-Strong drum): 50 % = noise drum" },
    };
    return info[juce::jlimit (0, (int) std::size (info) - 1, type)];
}

/// Inside the loop a dry/wet MIX of a time-varying effect (freq shift, chorus, ring mod) interferes with itself on every
/// pass and thins the tail out (user report on v0.1). So in the loop those effects are always fully wet and knob C
/// becomes SPREAD (each loop line gets a slightly different setting); after the loop C is a normal mix.
inline bool slotCIsSpread (int type, int place) { return place == PlaceLoop && (type == SlotFreqShift || type == SlotChorus || type == SlotRingMod); }
inline const char* slotKnobLabel (int type, int knob, int place)
{
    if (knob == 2 && slotCIsSpread (type, place)) return "spread";
    const auto& in = slotInfo (type);
    return knob == 0 ? in.a.label : (knob == 1 ? in.b.label : in.c.label);
}

/// Display text for a slot knob value (0..1) of the given type and knob (0 = a, 1 = b, 2 = c).
juce::String slotValueText (int type, int knob, float v, int place = PlaceLoop);

/// Good starting knob values (a, b, c) for a slot type: applied when the user switches a slot's type, so knobs
/// don't carry over a meaning from the previous effect (e.g. a filter's resonance becoming a saturator's asymmetry).
struct SlotDefaults { float a, b, c; };
inline SlotDefaults slotDefaults (int type)
{
    switch (type)
    {
        case SlotFilter:    return { 0.85f, 0.15f, 0.0f };
        case SlotFineDelay: return { 0.3f, 0.0f, 0.0f };
        case SlotDisperse:  return { 0.35f, 0.35f, 0.75f };
        case SlotSaturate:  return { 0.35f, 0.5f, 1.0f };
        case SlotFreqShift: return { 0.6f, 0.15f, 0.3f };
        case SlotChorus:    return { 0.4f, 0.3f, 0.5f };
        case SlotFold:      return { 0.25f, 0.5f, 0.6f };
        case SlotRingMod:   return { 0.4f, 1.0f, 0.3f };
        case SlotFlip:      return { 0.3f, 0.5f, 1.0f };
        default:            return { 0.5f, 0.5f, 1.0f };
    }
}

/// One-line hover help for a parameter ID (empty if none).
juce::String helpText (const juce::String& paramId);

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/// FEEDBACK knob -> decay time (T60, seconds) at the fundamental, and the extra loop gain above 0.95.
inline double feedbackToT60 (double fb) { return 0.03 * std::pow (10.0 / 0.03, juce::jlimit (0.0, 0.95, fb) / 0.95); }
inline double feedbackOverdrive (double fb) { return fb > 0.95 ? 1.0 + (fb - 0.95) / 0.05 * 0.03 : 1.0; }
inline bool feedbackSelfOsc (double fb) { return fb > 0.95; }

/// LENGTH: the top of the range means "off" (the note rings out on its own).
inline constexpr float kLengthOff = 4000.0f;
} // namespace sane
