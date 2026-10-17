#include "SaneParams.h"

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

String slotValueText (int type, int knob, float v)
{
    auto p = [&] { return String (roundToInt (v * 100.0f)) + " %"; };
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
            if (knob == 0) return String (1.0 + 19.0 * v * v, 1) + " x";
            if (knob == 1) return String (v, 2);
            return p();
        case SlotFreqShift:
            if (knob == 0) return (v >= 0.5f ? "+" : "") + String ((v - 0.5f) * 2.0f, 2) + " x range";
            if (knob == 1) return String (1.0 + 49.0 * v * v, 1) + " Hz";
            return p();
        case SlotChorus:
            if (knob == 0) return String (1.0 * v * v, 3) + " ms";
            if (knob == 1) return String (0.05 * std::pow (200.0, (double) v), 2) + " Hz";
            return p();
        case SlotFold:
            if (knob == 0) return String (1.0 + 9.0 * v, 1) + " x";
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

    // ---- strike ----
    add<AudioParameterFloat> (L, ParameterID { ids::stBase, v1 }, "Strike position", NormalisableRange<float> (0.0f, 0.96f), 0.4f, fmt (2, ""));
    add<AudioParameterFloat> (L, ParameterID { ids::stVel, v1 }, "Velocity to position", NormalisableRange<float> (-1.0f, 1.0f), -0.6f, bipct());
    add<AudioParameterFloat> (L, ParameterID { ids::stKey, v1 }, "Key to position", NormalisableRange<float> (-1.0f, 1.0f), 0.4f, bipct());
    add<AudioParameterFloat> (L, ParameterID { ids::stRandom, v1 }, "Random position", NormalisableRange<float> (0.0f, 0.3f), 0.06f, fmt (2, ""));

    // ---- loop ----
    add<AudioParameterChoice> (L, ParameterID { ids::core, v1 }, "Core", coreNames(), CoreMembrane);
    add<AudioParameterFloat> (L, ParameterID { ids::feedback, v1 }, "Feedback", NormalisableRange<float> (0.0f, 1.0f), 0.55f,
                              Attr().withStringFromValueFunction ([] (float x, int) {
                                  if (feedbackSelfOsc (x)) return String ("self-osc ") + String (roundToInt ((x - 0.95f) / 0.05f * 100.0f)) + " %";
                                  const double t = feedbackToT60 (x);
                                  return t < 1.0 ? String (roundToInt (t * 1000.0)) + " ms" : String (t, 2) + " s";
                              }));
    add<AudioParameterFloat> (L, ParameterID { ids::damp, v1 }, "Damp", NormalisableRange<float> (0.0f, 1.0f), 0.4f, pct());
    add<AudioParameterFloat> (L, ParameterID { ids::tension, v1 }, "Tension", NormalisableRange<float> (0.0f, 12.0f), 1.5f, fmt (1, "st"));
    add<AudioParameterFloat> (L, ParameterID { ids::coupling, v1 }, "Coupling", NormalisableRange<float> (0.0f, 1.0f), 0.3f, pct());
    add<AudioParameterFloat> (L, ParameterID { ids::width, v1 }, "Width", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct());

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
    add<AudioParameterFloat> (L, ParameterID { ids::gain, v1 }, "Output", NormalisableRange<float> (-36.0f, 6.0f), -6.0f, fmt (1, "dB"));

    // ---- insert slots ----
    // Default ring: a gentle filter, a little dispersion, a touch of saturation, the rest empty.
    const int defType[kNumSlots] = { SlotFilter, SlotDisperse, SlotSaturate, SlotOff, SlotOff, SlotOff, SlotOff };
    const float defA[kNumSlots] = { 0.72f, 0.25f, 0.15f, 0.5f, 0.5f, 0.5f, 0.5f };
    const float defB[kNumSlots] = { 0.15f, 0.35f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    const float defC[kNumSlots] = { 0.0f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    for (int s = 1; s <= kNumSlots; ++s)
    {
        const auto n = "Slot " + String (s) + " ";
        add<AudioParameterChoice> (L, ParameterID { ids::slot (s, "type"), v1 }, n + "type", slotTypeNames(), defType[s - 1]);
        add<AudioParameterFloat> (L, ParameterID { ids::slot (s, "a"), v1 }, n + "A", NormalisableRange<float> (0.0f, 1.0f), defA[s - 1], pct());
        add<AudioParameterFloat> (L, ParameterID { ids::slot (s, "b"), v1 }, n + "B", NormalisableRange<float> (0.0f, 1.0f), defB[s - 1], pct());
        add<AudioParameterFloat> (L, ParameterID { ids::slot (s, "c"), v1 }, n + "C", NormalisableRange<float> (0.0f, 1.0f), defC[s - 1], pct());
    }
    return L;
}
} // namespace sane
