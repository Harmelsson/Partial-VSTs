#include "SaneParams.h"
#include <map>

namespace sane
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

    Attr pct()
    {
        return Attr().withLabel ("%").withStringFromValueFunction ([] (float x, int) { return String (juce::roundToInt (x * 100.0f)) + " %"; });
    }

    Attr bipct()
    {
        return Attr().withLabel ("%").withStringFromValueFunction ([] (float x, int) {
            const int v = juce::roundToInt (x * 100.0f);
            return (v > 0 ? "+" : "") + String (v) + " %";
        });
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

String slotValueText (int type, int knob, float v, int place)
{
    auto p = [&] { return String (roundToInt (v * 100.0f)) + " %"; };
    if (knob == 2 && slotCIsSpread (type, place)) return p();
    switch (type)
    {
        case SlotFilter:
            if (knob == 0) return String (20.0 * std::pow (1000.0, (double) v), 0) + " Hz";
            if (knob == 1) return String (v, 2);
            return v < 0.33f ? "low-pass" : (v < 0.66f ? "band-pass" : "high-pass");
        case SlotFineDelay:
            if (knob == 0) return String (5.0 * v * v, 3) + " ms";
            if (knob == 1) return v < 0.02f ? "off" : String (v * 2.0f, 2) + " x period";
            return String (0.2 * v, 3) + " ms";
        case SlotDisperse:
            if (knob == 0) return String (v * 0.9f, 2);
            if (knob == 1) return String (1 + roundToInt (v * 11.0f)) + " stages";
            return v < 0.5f ? "dark " + p() : "bright " + p();
        case SlotSaturate:
            if (knob == 0) return String (1.0 + 39.0 * v * v, 1) + " x";
            if (knob == 1) return String (v, 2);
            return p();
        case SlotFreqShift:
            if (knob == 0) return (v >= 0.5f ? "+" : "") + String ((v - 0.5f) * 2.0f, 2) + " x range";
            if (knob == 1) return String (1.0 + 199.0 * v * v, 1) + " Hz";
            return p();
        case SlotChorus:
            if (knob == 0) return String (1.0 * v * v, 3) + " ms";
            if (knob == 1) return String (0.05 * std::pow (200.0, (double) v), 2) + " Hz";
            return p();
        case SlotFold:
            if (knob == 0) return String (1.0 + 19.0 * v, 1) + " x";
            if (knob == 1) return String ((v - 0.5f) * 2.0f, 2);
            return p();
        case SlotRingMod:
            if (knob == 0) return String (0.25 + 7.75 * v * v, 2) + " x";
            if (knob == 1) return v < 0.5f ? "fixed 110 Hz" : "follows pitch";
            return p();
        case SlotFlip:
            if (knob == 0) return String (v * 50.0f, 1) + " %";
            if (knob == 1) return "-";
            return p();
        default: return "-";
    }
}

String helpText (const String& id)
{
    static const std::map<String, String> h = {
        { ids::exColor, "EXCITER: brightness of the noise burst that hits the loop (dark thud .. bright crack)." },
        { ids::exLength, "EXCITER: how long the noise burst lasts. Short = click / stick, long = brush / scrape." },
        { ids::exGrain, "EXCITER: random crackle spikes inside the burst (gritty, rattly hits)." },
        { ids::exLevel, "EXCITER: how hard the burst drives INTO the loop (more = louder ring, saturators bite harder)." },
        { ids::exDirect, "EXCITER: the dry burst straight to the output, bypassing the loop (stick attack). Off = only the loop rings." },
        { ids::exRandom, "EXCITER: how different each note's noise is (0 = similar every hit)." },
        { ids::exVelRandom, "EXCITER: velocity changes the randomness (+ = hard hits vary more)." },
        { ids::exVelLength, "EXCITER: velocity changes the burst length (- = hard hits are shorter / snappier)." },
        { ids::exKeyColor, "EXCITER: higher keys get a brighter (+) or darker (-) burst." },
        { ids::exSpread, "EXCITER: stereo: 0 = same noise left and right, 100 % = independent noise per side." },
        { ids::stBase, "STRIKE: where the hit lands: 0 = centre (thump, few overtones) .. rim (ringy, many overtones)." },
        { ids::stVel, "STRIKE: velocity moves the hit (- = hard hits go to the centre, like a real drummer)." },
        { ids::stKey, "STRIKE: higher keys move the hit toward the rim (+) or centre (-)." },
        { ids::stRandom, "STRIKE: random position change per hit (humanising)." },
        { ids::core, "LOOP: what resonates. Membrane = 8 coupled loops (drum skin), Partials = one pure loop per mode, Delay = one loop (string / echo)." },
        { ids::feedback, "LOOP: how long the loop rings (decay time). The very top = self-oscillation (rings while the key is held)." },
        { ids::damp, "LOOP: high overtones die faster (felt, hand on the skin). 0 = bright and metallic." },
        { ids::tension, "LOOP: a hard hit starts sharp and glides down as the energy drains (real drumhead physics)." },
        { ids::coupling, "LOOP: how strongly the loops pass energy to each other (Membrane / Partials): beating, blooming." },
        { ids::width, "LOOP: stereo spread of the loops." },
        { ids::loopMode, "LOOP TIME: Keys = the played key sets the loop length (pitched). Hz / ms / BPM = a fixed loop length, like a delay." },
        { ids::loopHz, "LOOP TIME: the loop's frequency (1 / its length) when the mode is Hz." },
        { ids::loopMs, "LOOP TIME: the loop's length in milliseconds when the mode is ms. Long = echo-like (Delay core)." },
        { ids::loopBeat, "LOOP TIME: the loop's length as a tempo division (follows the host tempo) when the mode is BPM." },
        { ids::keyTrack, "LOOP TIME: how much the keys bend a fixed loop time (0 = all keys the same, 100 % = semitones)." },
        { ids::table, "MODES: the physical mode set used by the Physical source (real instrument ratios)." },
        { ids::srcA, "MODES: where endpoint A takes its mode positions from." },
        { ids::srcB, "MODES: where endpoint B takes its mode positions from." },
        { ids::morph, "MODES: slides every mode from its A position to its B position (live, also on ringing notes)." },
        { ids::calcSeed, "MODES: the Calc source = partials at n^log2(seed), exactly like Seed. 2.0 = harmonic." },
        { ids::rolloff, "MODES: how much quieter the higher modes are." },
        { ids::pitchAmt, "HIT: the pitch at the start of the hit, gliding back to the note (below 0 = rising)." },
        { ids::pitchTime, "HIT: how fast the drawn pitch drop glides back." },
        { ids::length, "HIT: the note is muted (a hand on the skin) after this time. Far right = off." },
        { ids::tune, "Tune in semitones." }, { ids::fine, "Fine tune in cents." }, { ids::bendRange, "Pitch-bend range in semitones." },
        { ids::gain, "Output level." },
    };
    if (id.startsWith ("s") && id.length() >= 3 && juce::CharacterFunctions::isDigit (id[1]))
    {
        if (id.endsWith ("type")) return "SLOT: the effect in this slot (click a type; Off removes it).";
        if (id.endsWith ("place")) return "SLOT: In loop = runs on EVERY pass (shapes the ring itself). After loop = runs once on the output (a normal effect).";
        return "SLOT: a setting of this slot's effect (its meaning is shown above the knob).";
    }
    auto it = h.find (id);
    return it != h.end() ? it->second : String();
}

AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout L;
    const int v1 = 1; // VST3 version hint: the Sane version that ADDED the param

    // ---- exciter ----
    add<AudioParameterFloat> (L, ParameterID { ids::exColor, v1 }, "Exciter colour", NormalisableRange<float> (0.0f, 1.0f), 0.45f, pct());
    add<AudioParameterFloat> (L, ParameterID { ids::exLength, v1 }, "Exciter length", skewed (0.3f, 120.0f, 8.0f), 5.0f, fmt (1, "ms"));
    add<AudioParameterFloat> (L, ParameterID { ids::exGrain, v1 }, "Exciter grain", NormalisableRange<float> (0.0f, 1.0f), 0.15f, pct());
    add<AudioParameterFloat> (L, ParameterID { ids::exLevel, v1 }, "Exciter level", NormalisableRange<float> (-36.0f, 12.0f), 0.0f, fmt (1, "dB"));
    add<AudioParameterFloat> (L, ParameterID { ids::exRandom, v1 }, "Random per note", NormalisableRange<float> (0.0f, 1.0f), 0.35f, pct());
    add<AudioParameterFloat> (L, ParameterID { ids::exVelRandom, v1 }, "Velocity to random", NormalisableRange<float> (-1.0f, 1.0f), 0.2f, bipct());
    add<AudioParameterFloat> (L, ParameterID { ids::exVelLength, v1 }, "Velocity to length", NormalisableRange<float> (-1.0f, 1.0f), -0.3f, bipct());
    add<AudioParameterFloat> (L, ParameterID { ids::exKeyColor, v1 }, "Key to colour", NormalisableRange<float> (-1.0f, 1.0f), 0.4f, bipct());
    add<AudioParameterFloat> (L, ParameterID { ids::exSpread, v1 }, "Exciter stereo spread", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct());
    const int v2 = 2;
    add<AudioParameterFloat> (L, ParameterID { ids::exDirect, v2 }, "Direct (dry hit)", NormalisableRange<float> (-60.0f, 0.0f), -30.0f,
                              Attr().withStringFromValueFunction ([] (float x, int) { return x <= -59.9f ? String ("off") : String (x, 1) + " dB"; }));

    // ---- strike ----
    add<AudioParameterFloat> (L, ParameterID { ids::stBase, v1 }, "Strike position", NormalisableRange<float> (0.0f, 0.96f), 0.4f, fmt (2, ""));
    add<AudioParameterFloat> (L, ParameterID { ids::stVel, v1 }, "Velocity to position", NormalisableRange<float> (-1.0f, 1.0f), -0.35f, bipct());
    add<AudioParameterFloat> (L, ParameterID { ids::stKey, v1 }, "Key to position", NormalisableRange<float> (-1.0f, 1.0f), 0.4f, bipct());
    add<AudioParameterFloat> (L, ParameterID { ids::stRandom, v1 }, "Random position", NormalisableRange<float> (0.0f, 0.3f), 0.06f, fmt (2, ""));

    // ---- loop ----
    add<AudioParameterChoice> (L, ParameterID { ids::core, v1 }, "Core", coreNames(), CoreMembrane);
    add<AudioParameterFloat> (L, ParameterID { ids::feedback, v1 }, "Feedback", NormalisableRange<float> (0.0f, 1.0f), 0.65f,
                              Attr().withStringFromValueFunction ([] (float x, int) {
                                  if (feedbackSelfOsc (x)) return String ("self-osc ") + String (roundToInt ((x - 0.95f) / 0.05f * 100.0f)) + " %";
                                  const double t = feedbackToT60 (x);
                                  return t < 1.0 ? String (roundToInt (t * 1000.0)) + " ms" : String (t, 2) + " s";
                              }));
    add<AudioParameterFloat> (L, ParameterID { ids::damp, v1 }, "Damp", NormalisableRange<float> (0.0f, 1.0f), 0.4f, pct());
    add<AudioParameterFloat> (L, ParameterID { ids::tension, v1 }, "Tension", NormalisableRange<float> (0.0f, 12.0f), 1.5f, fmt (1, "st"));
    add<AudioParameterFloat> (L, ParameterID { ids::coupling, v1 }, "Coupling", NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct());
    add<AudioParameterFloat> (L, ParameterID { ids::width, v1 }, "Width", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct());
    add<AudioParameterChoice> (L, ParameterID { ids::loopMode, v2 }, "Loop time mode", loopModeNames(), LoopKeys);
    add<AudioParameterFloat> (L, ParameterID { ids::loopHz, v2 }, "Loop time (Hz)", skewed (0.5f, 2000.0f, 110.0f), 110.0f, fmt (2, "Hz"));
    add<AudioParameterFloat> (L, ParameterID { ids::loopMs, v2 }, "Loop time (ms)", skewed (0.5f, 2000.0f, 20.0f), 10.0f, fmt (2, "ms"));
    add<AudioParameterChoice> (L, ParameterID { ids::loopBeat, v2 }, "Loop time (BPM)", beatNames(), 9);
    add<AudioParameterFloat> (L, ParameterID { ids::keyTrack, v2 }, "Key track", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct());

    // ---- modes ----
    add<AudioParameterChoice> (L, ParameterID { ids::table, v1 }, "Mode table", tableNames(), TabMembrane);
    add<AudioParameterChoice> (L, ParameterID { ids::srcA, v1 }, "Modes A", sourceNames(), SrcPhysical);
    add<AudioParameterChoice> (L, ParameterID { ids::srcB, v1 }, "Modes B", sourceNames(), SrcCalc);
    add<AudioParameterFloat> (L, ParameterID { ids::morph, v1 }, "Morph A to B", NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct());
    add<AudioParameterFloat> (L, ParameterID { ids::calcSeed, v1 }, "Calc seed", NormalisableRange<float> (0.5f, 4.0f), 2.0f, fmt (3, ""));
    add<AudioParameterFloat> (L, ParameterID { ids::rolloff, v1 }, "Rolloff", NormalisableRange<float> (0.0f, 2.0f), 0.35f, fmt (2, ""));

    // ---- hit ----
    add<AudioParameterFloat> (L, ParameterID { ids::pitchAmt, v1 }, "Pitch drop", NormalisableRange<float> (-12.0f, 24.0f), 3.0f, fmt (1, "st"));
    add<AudioParameterFloat> (L, ParameterID { ids::pitchTime, v1 }, "Pitch drop time", skewed (2.0f, 1000.0f, 80.0f), 60.0f, fmt (0, "ms"));
    add<AudioParameterFloat> (L, ParameterID { ids::length, v1 }, "Length", skewed (20.0f, kLengthOff, 600.0f), kLengthOff,
                              Attr().withStringFromValueFunction ([] (float x, int) { return x >= kLengthOff - 0.5f ? String ("off") : String (roundToInt (x)) + " ms"; }));

    // ---- global ----
    add<AudioParameterFloat> (L, ParameterID { ids::tune, v1 }, "Tune", NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f, fmt (0, "st"));
    add<AudioParameterFloat> (L, ParameterID { ids::fine, v1 }, "Fine", NormalisableRange<float> (-100.0f, 100.0f), 0.0f, fmt (1, "c"));
    add<AudioParameterFloat> (L, ParameterID { ids::bendRange, v1 }, "Bend range", NormalisableRange<float> (0.0f, 24.0f, 1.0f), 2.0f, fmt (0, "st"));
    add<AudioParameterFloat> (L, ParameterID { ids::gain, v1 }, "Output", NormalisableRange<float> (-36.0f, 6.0f), -3.0f, fmt (1, "dB"));

    // ---- insert slots ----
    // Default ring: a gentle filter, a little dispersion, a touch of saturation, the rest empty.
    const int defType[kNumSlots] = { SlotFilter, SlotDisperse, SlotSaturate, SlotOff, SlotOff, SlotOff, SlotOff };
    const float defA[kNumSlots] = { 0.85f, 0.25f, 0.10f, 0.5f, 0.5f, 0.5f, 0.5f };
    const float defB[kNumSlots] = { 0.15f, 0.35f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    const float defC[kNumSlots] = { 0.0f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    for (int s = 1; s <= kNumSlots; ++s)
    {
        const auto n = "Slot " + String (s) + " ";
        add<AudioParameterChoice> (L, ParameterID { ids::slot (s, "type"), v1 }, n + "type", slotTypeNames(), defType[s - 1]);
        add<AudioParameterFloat> (L, ParameterID { ids::slot (s, "a"), v1 }, n + "A", NormalisableRange<float> (0.0f, 1.0f), defA[s - 1], pct());
        add<AudioParameterFloat> (L, ParameterID { ids::slot (s, "b"), v1 }, n + "B", NormalisableRange<float> (0.0f, 1.0f), defB[s - 1], pct());
        add<AudioParameterFloat> (L, ParameterID { ids::slot (s, "c"), v1 }, n + "C", NormalisableRange<float> (0.0f, 1.0f), defC[s - 1], pct());
        add<AudioParameterChoice> (L, ParameterID { ids::slot (s, "place"), v2 }, n + "place", placeNames(), PlaceLoop);
    }
    return L;
}
} // namespace sane
