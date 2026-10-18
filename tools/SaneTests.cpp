// SaneTests — headless checks for the Sane plugin (feedback drums). Every check prints PASS/FAIL with
// numbers, so a failing run explains itself. Exit code = number of failures (0 = all good).
//
// What is checked, and why:
//   modes     the mode tables / Calc source / morph give the known ratios (membrane 1.594, seed 2 = 1:2:3)
//   pitch     each core rings at the played pitch (Delay fundamental, Partials partials, Membrane modes)
//   decay     FEEDBACK sets the decay time; voices end and free themselves
//   safety    every core x every insert at extreme settings x self-oscillation: no NaN, never above
//             the ceiling, no safety-stage faults
//   dynamic   morphing / changing seed while a note rings glides without clicks
//   hit       per-note random burst, strike position follows velocity, re-strike reuses the voice,
//             drawn pitch drop + tension make the start higher than the tail, LENGTH cuts the note,
//             self-oscillation sustains while held and stops on note-off
//   state     saving and re-loading restores every parameter
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr double kSr = 48000.0;
constexpr int kBlock = 512;
int failures = 0;
using namespace sane;

void check (bool ok, const juce::String& what, const juce::String& detail = {})
{
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << (detail.isEmpty() ? "" : "   [" + detail + "]") << std::endl;
    if (! ok) ++failures;
}

double toneAmp (const float* x, size_t n, double freq)
{
    const double w = 2.0 * juce::MathConstants<double>::pi * freq / kSr;
    const double coeff = 2.0 * std::cos (w);
    double s1 = 0, s2 = 0, wsum = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * (double) i / (double) (n - 1));
        wsum += win;
        const double s0 = x[i] * win + coeff * s1 - s2;
        s2 = s1; s1 = s0;
    }
    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return 2.0 * std::sqrt (std::max (0.0, power)) / wsum;
}

/// Strongest frequency within +-range cents of `around` (0.5 cent scan, parabolic refinement). Returns Hz.
double peakNear (const std::vector<float>& x, size_t from, size_t len, double around, double rangeCents = 80.0)
{
    double best = 0.0, bestC = 0.0;
    std::vector<double> amps;
    for (double c = -rangeCents; c <= rangeCents; c += 0.5)
    {
        const double a = toneAmp (x.data() + from, len, around * std::pow (2.0, c / 1200.0));
        amps.push_back (a);
        if (a > best) { best = a; bestC = c; }
    }
    const int i = (int) std::lround ((bestC + rangeCents) / 0.5);
    if (i > 0 && i + 1 < (int) amps.size())
    {
        const double a = amps[(size_t) i - 1], b = amps[(size_t) i], c = amps[(size_t) i + 1];
        const double den = a - 2.0 * b + c;
        if (std::abs (den) > 1e-12) bestC += 0.5 * 0.5 * (a - c) / den;
    }
    return around * std::pow (2.0, bestC / 1200.0);
}

double cents (double f, double ref) { return 1200.0 * std::log2 (f / ref); }

double rmsDb (const std::vector<float>& x, size_t from, size_t len)
{
    double s = 0;
    len = std::min (len, x.size() - std::min (from, x.size()));
    for (size_t i = 0; i < len; ++i) s += (double) x[from + i] * x[from + i];
    return 10.0 * std::log10 (s / std::max<size_t> (1, len) + 1e-30);
}

struct Rig
{
    std::unique_ptr<SaneProcessor> proc = std::make_unique<SaneProcessor>();
    juce::AudioBuffer<float> buf { 2, kBlock };

    Rig()
    {
        proc->setPlayConfigDetails (0, 2, kSr, kBlock);
        proc->prepareToPlay (kSr, kBlock);
    }

    void set (const juce::String& id, float value)
    {
        auto* p = proc->apvts.getParameter (id);
        jassert (p != nullptr);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    /// A clean, test-friendly patch: no pitch envelope, no tension, no inserts, centre strike.
    void plain (int core)
    {
        set (ids::core, (float) core);
        set (ids::pitchAmt, 0.0f); set (ids::tension, 0.0f);
        for (int s = 1; s <= kNumSlots; ++s) set (ids::slot (s, "type"), 0.0f);
        set (ids::exRandom, 0.0f); set (ids::exVelRandom, 0.0f); set (ids::stRandom, 0.0f);
        set (ids::coupling, 0.0f); set (ids::width, 0.0f);
    }

    /// Renders `seconds`, optionally with a note-on at the start (and note-off at `offAt` seconds). Returns the left channel.
    std::vector<float> render (double seconds, int note = -1, float vel = 0.8f, double offAt = -1.0, std::vector<float>* right = nullptr)
    {
        std::vector<float> out;
        const int blocks = (int) std::ceil (seconds * kSr / kBlock);
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0 && note >= 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, vel), 0);
            if (offAt >= 0.0 && b == (int) (offAt * kSr / kBlock) && note >= 0) midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);
            proc->processBlock (buf, midi);
            for (int i = 0; i < kBlock; ++i)
            {
                out.push_back (buf.getSample (0, i));
                if (right != nullptr) right->push_back (buf.getSample (1, i));
            }
        }
        return out;
    }
};

// ------------------------------------------------------------------------------------------------
void testModes()
{
    std::cout << "modes" << std::endl;
    check (std::abs (dsp::modes::physical (TabMembrane, 1) - 1.5933) < 0.001, "membrane mode 2 = 1.593 x", juce::String (dsp::modes::physical (TabMembrane, 1), 4));
    check (std::abs (dsp::modes::physical (TabBar, 1) - 2.756) < 0.01, "bar mode 2 = 2.756 x", juce::String (dsp::modes::physical (TabBar, 1), 4));
    check (std::abs (dsp::modes::physical (TabTabla, 2) - 3.0) < 1e-9, "tabla mode 3 = 3 x (harmonic)");
    check (std::abs (dsp::modes::fromSource (SrcCalc, 0, 2.0, 2) - 3.0) < 1e-9, "calc seed 2.0: partial 3 = 3 x");
    check (std::abs (cents (dsp::modes::fromSource (SrcCalc, 0, 2.5, 1), 1.0) - 1586.3137) < 0.01, "calc seed 2.5: partial 2 = 1586.31 c (same as Seed)");
    check (std::abs (dsp::modes::morphed (1.0, 4.0, 0.5) - 2.0) < 1e-9, "morph halfway (in cents) between 1x and 4x = 2x");
    check (dsp::modes::strikeWeight (TabMembrane, 1, 0.0) < 0.05, "centre strike does not excite mode 11 (node in the middle)");
    check (dsp::modes::strikeWeight (TabMembrane, 0, 0.0) > 0.99, "centre strike excites mode 01 fully");
}

void testPitch()
{
    std::cout << "pitch" << std::endl;
    {
        Rig r; r.plain (CoreDelay); r.set (ids::feedback, 0.85f); r.set (ids::damp, 0.5f);
        auto x = r.render (1.0, 57, 0.8f);
        const double f = peakNear (x, (size_t) (0.3 * kSr), (size_t) (0.4 * kSr), 220.0);
        check (std::abs (cents (f, 220.0)) < 3.0, "Delay core: A3 rings at 220 Hz", juce::String (f, 2) + " Hz, " + juce::String (cents (f, 220.0), 2) + " c");
    }
    {
        Rig r; r.plain (CoreDelay); r.set (ids::feedback, 0.85f); r.set (ids::damp, 0.0f);
        auto x = r.render (1.0, 81, 0.8f);
        const double f = peakNear (x, (size_t) (0.3 * kSr), (size_t) (0.4 * kSr), 880.0);
        check (std::abs (cents (f, 880.0)) < 4.0, "Delay core: A5 rings at 880 Hz", juce::String (f, 2) + " Hz, " + juce::String (cents (f, 880.0), 2) + " c");
    }
    {
        Rig r; r.plain (CorePartials); r.set (ids::feedback, 0.8f); r.set (ids::srcA, (float) SrcCalc); r.set (ids::calcSeed, 2.0f);
        r.set (ids::stBase, 0.5f); r.set (ids::stVel, 0.0f); r.set (ids::stKey, 0.0f);
        auto x = r.render (1.2, 57, 0.8f);
        for (int n : { 1, 2, 3 })
        {
            const double f = peakNear (x, (size_t) (0.3 * kSr), (size_t) (0.5 * kSr), 220.0 * n, 60.0);
            check (std::abs (cents (f, 220.0 * n)) < 4.0, "Partials core, Calc seed 2: partial " + juce::String (n) + " at " + juce::String (220 * n) + " Hz",
                   juce::String (f, 2) + " Hz, " + juce::String (cents (f, 220.0 * n), 2) + " c");
        }
        r.set (ids::calcSeed, 2.5f);
        auto y = r.render (1.2, 57, 0.8f);
        const double f2 = peakNear (y, (size_t) (0.3 * kSr), (size_t) (0.5 * kSr), 220.0 * 2.5, 60.0);
        check (std::abs (cents (f2, 550.0)) < 5.0, "Partials core, Calc seed 2.5: partial 2 at 2.5 x (550 Hz)", juce::String (f2, 2) + " Hz");
    }
    {
        Rig r; r.plain (CoreMembrane); r.set (ids::feedback, 0.8f); r.set (ids::damp, 0.8f);
        r.set (ids::stBase, 0.5f); r.set (ids::stVel, 0.0f); r.set (ids::stKey, 0.0f);
        auto x = r.render (1.2, 45, 0.8f);
        const double f1 = peakNear (x, (size_t) (0.25 * kSr), (size_t) (0.5 * kSr), 110.0, 40.0);
        const double f2 = peakNear (x, (size_t) (0.25 * kSr), (size_t) (0.5 * kSr), 110.0 * 1.5933, 40.0);
        check (std::abs (cents (f1, 110.0)) < 6.0, "Membrane core: mode 01 at the played pitch (110 Hz)", juce::String (f1, 2) + " Hz");
        check (std::abs (cents (f2, 110.0 * 1.5933)) < 8.0, "Membrane core: mode 11 at 1.593 x", juce::String (f2, 2) + " Hz");
    }
}

void testDecay()
{
    std::cout << "decay" << std::endl;
    Rig r; r.plain (CoreDelay); r.set (ids::damp, 0.0f);
    r.set (ids::feedback, 0.55f); // T60 ~ 0.84 s
    const double t60 = feedbackToT60 (0.55);
    auto x = r.render (2.0, 57, 0.8f);
    const double a = rmsDb (x, (size_t) (0.2 * kSr), (size_t) (0.05 * kSr));
    const double b = rmsDb (x, (size_t) ((0.2 + t60 / 2.0) * kSr), (size_t) (0.05 * kSr));
    check (std::abs ((a - b) - 30.0) < 6.0, "FEEDBACK 0.55: about -30 dB after half the T60 (" + juce::String (t60, 2) + " s)", juce::String (a - b, 1) + " dB");
    auto y = r.render (6.0);
    check (r.proc->engine.activeVoices() == 0, "the voice ends and frees itself after ringing out");
    check (rmsDb (y, y.size() - 4800, 4800) < -100.0, "silence after the voice ended");
}

void testSafety()
{
    std::cout << "safety" << std::endl;
    double worst = 0.0;
    int nonFinite = 0;
    juce::Random rnd (7);
    for (int core = 0; core < 3; ++core)
        for (int type = 1; type < slotTypeNames().size(); ++type)
        {
            Rig r;
            r.set (ids::core, (float) core);
            r.set (ids::feedback, 1.0f); // self-oscillation
            r.set (ids::tension, 12.0f); r.set (ids::pitchAmt, 24.0f); r.set (ids::exLevel, 12.0f); r.set (ids::gain, 6.0f);
            r.set (ids::damp, 0.0f); r.set (ids::coupling, 1.0f);
            for (int s = 1; s <= kNumSlots; ++s)
            {
                r.set (ids::slot (s, "type"), (float) type);
                r.set (ids::slot (s, "a"), rnd.nextBool() ? 1.0f : rnd.nextFloat());
                r.set (ids::slot (s, "b"), rnd.nextBool() ? 1.0f : 0.0f);
                r.set (ids::slot (s, "c"), 1.0f);
            }
            std::vector<float> R;
            juce::MidiBuffer chord;
            for (int k = 0; k < 6; ++k) chord.addEvent (juce::MidiMessage::noteOn (1, 30 + k * 13, 1.0f), 0);
            r.proc->processBlock (r.buf, chord);
            auto x = r.render (1.0, -1, 1.0f, -1.0, &R);
            for (size_t i = 0; i < x.size(); ++i)
            {
                if (! std::isfinite (x[i]) || ! std::isfinite (R[i])) ++nonFinite;
                worst = std::max (worst, (double) std::max (std::abs (x[i]), std::abs (R[i])));
            }
            if (r.proc->safetyFaults() > 0) ++nonFinite;
        }
    check (nonFinite == 0, "3 cores x 9 inserts x self-oscillation x extreme knobs: no NaN / Inf, no safety faults", juce::String (nonFinite));
    check (worst <= pvst::SafetyStage::kCeiling + 1e-6, "never above the -1 dBFS ceiling", juce::String (worst, 4));
}

void testLevels()
{
    std::cout << "levels" << std::endl;
    for (int core = 0; core < 3; ++core)
    {
        Rig r; r.set (ids::core, (float) core);
        auto x = r.render (1.0, 48, 1.0f);
        float pk = 0.0f;
        for (auto s : x) pk = std::max (pk, std::abs (s));
        const double db = 20.0 * std::log10 (pk + 1e-12);
        check (db > -30.0 && db < -0.5, "default patch, " + coreNames()[core] + " core, full velocity: sensible level", juce::String (db, 1) + " dBFS");
        Rig r2; r2.set (ids::core, (float) core); // fresh instance: no tail of the loud hit
        auto y = r2.render (1.0, 48, 0.25f);
        // loudness = energy over the first 300 ms (a peak reading is misleading when soft rim hits ring more modes)
        const double eLoud = rmsDb (x, 0, (size_t) (0.3 * kSr)), eSoft = rmsDb (y, 0, (size_t) (0.3 * kSr));
        check (eSoft < eLoud - 3.0, "  soft hit is clearly quieter (energy, first 300 ms)", juce::String (eSoft - eLoud, 1) + " dB");
    }
}

void testDynamic()
{
    std::cout << "dynamic" << std::endl;
    Rig r; r.plain (CorePartials); r.set (ids::feedback, 0.85f); r.set (ids::srcA, (float) SrcPhysical); r.set (ids::srcB, (float) SrcCalc);
    r.set (ids::morph, 0.0f);
    auto pre = r.render (0.3, 50, 0.8f);
    float maxStepPre = 0.0f;
    for (size_t i = (size_t) (0.1 * kSr) + 1; i < pre.size(); ++i) maxStepPre = std::max (maxStepPre, std::abs (pre[i] - pre[i - 1]));
    // sweep the morph over 0.2 s while ringing
    float maxStep = 0.0f;
    float last = pre.back();
    for (int b = 0; b < 20; ++b)
    {
        r.set (ids::morph, (float) (b + 1) / 20.0f);
        auto x = r.render ((double) kBlock / kSr);
        for (auto s : x) { maxStep = std::max (maxStep, std::abs (s - last)); last = s; }
    }
    check (maxStep < maxStepPre * 2.5f + 1e-4f, "morph sweep while ringing: no clicks (sample steps stay small)",
           "pre " + juce::String (maxStepPre, 4) + ", during " + juce::String (maxStep, 4));
    auto tail = r.render (0.4);
    const double f0 = 440.0 * std::pow (2.0, (50 - 69) / 12.0);
    const double f2 = peakNear (tail, 0, tail.size(), 2.0 * f0, 60.0);
    check (std::abs (cents (f2, 2.0 * f0)) < 5.0, "after the morph, partial 2 rings at the Calc position (2 x pitch) — on the same ringing note", juce::String (f2, 2) + " Hz");
}

void testHit()
{
    std::cout << "hit" << std::endl;
    {
        Rig r;
        r.render (0.1, 60, 0.8f);
        const auto s1 = r.proc->engine.report.seed.load();
        r.render (0.1, 62, 0.8f);
        const auto s2 = r.proc->engine.report.seed.load();
        check (s1 != s2, "every note gets new noise (different burst seeds)");
    }
    {
        Rig r; r.set (ids::stBase, 0.5f); r.set (ids::stVel, -0.6f); r.set (ids::stKey, 0.0f); r.set (ids::stRandom, 0.0f);
        r.render (0.05, 60, 1.0f);
        const float loud = r.proc->engine.report.strikeR.load();
        r.render (0.05, 62, 0.2f);
        const float soft = r.proc->engine.report.strikeR.load();
        check (loud < soft, "velocity -> position: loud hits land nearer the centre", "loud " + juce::String (loud, 2) + ", soft " + juce::String (soft, 2));
    }
    {
        Rig r;
        r.render (0.2, 60, 0.8f);
        r.render (0.2, 60, 0.8f);
        check (r.proc->engine.activeVoices() == 1, "re-striking the same note reuses its ringing voice");
    }
    {
        Rig r; r.plain (CoreDelay); r.set (ids::feedback, 0.85f); r.set (ids::pitchAmt, 7.0f); r.set (ids::pitchTime, 80.0f);
        auto x = r.render (1.2, 57, 0.9f);
        const double early = peakNear (x, (size_t) (0.01 * kSr), (size_t) (0.06 * kSr), 220.0 * std::pow (2.0, 4.0 / 12.0), 500.0);
        const double late = peakNear (x, (size_t) (0.6 * kSr), (size_t) (0.4 * kSr), 220.0, 60.0);
        check (cents (early, late) > 150.0, "drawn pitch drop: the start is higher than the tail", juce::String (cents (early, late), 0) + " c");
        check (std::abs (cents (late, 220.0)) < 5.0, "  and the tail settles on the played pitch", juce::String (late, 2) + " Hz");
    }
    {
        Rig r; r.plain (CoreDelay); r.set (ids::feedback, 0.85f); r.set (ids::tension, 6.0f);
        auto x = r.render (1.2, 57, 1.0f);
        const double early = peakNear (x, (size_t) (0.005 * kSr), (size_t) (0.05 * kSr), 220.0 * std::pow (2.0, 3.0 / 12.0), 500.0);
        const double late = peakNear (x, (size_t) (0.7 * kSr), (size_t) (0.4 * kSr), 220.0, 150.0);
        check (cents (early, late) > 40.0, "tension: a hard hit starts sharp and glides down as energy drains", juce::String (cents (early, late), 0) + " c");
    }
    {
        Rig r; r.set (ids::length, 200.0f);
        auto x = r.render (0.6, 48, 1.0f);
        check (rmsDb (x, (size_t) (0.35 * kSr), (size_t) (0.1 * kSr)) < -70.0, "LENGTH 200 ms: silent by 350 ms",
               juce::String (rmsDb (x, (size_t) (0.35 * kSr), (size_t) (0.1 * kSr)), 1) + " dB");
    }
    {
        Rig r; r.plain (CoreDelay); r.set (ids::feedback, 1.0f);
        auto x = r.render (2.5, 57, 0.8f, 2.0);
        const double held = rmsDb (x, (size_t) (1.8 * kSr), (size_t) (0.1 * kSr));
        const double after = rmsDb (x, x.size() - 4800, 4800);
        check (held > -40.0, "self-oscillation (feedback 100 %) keeps ringing while held", juce::String (held, 1) + " dB");
        check (after < -80.0, "  and stops after note-off", juce::String (after, 1) + " dB");
    }
}


double bandEnergyDb (const std::vector<float>& x, size_t from, size_t len, double fLo, double fHi)
{
    double e = 0.0;
    for (double f = fLo; f <= fHi; f *= 1.06) { const double a = toneAmp (x.data() + from, len, f); e += a * a; }
    return 10.0 * std::log10 (e + 1e-30);
}

void testV02()
{
    std::cout << "v0.2: loop time, direct, placement, calibration" << std::endl;
    {
        Rig r; r.plain (CoreDelay); r.set (ids::feedback, 0.85f); r.set (ids::loopMode, (float) LoopHz); r.set (ids::loopHz, 150.0f); r.set (ids::keyTrack, 0.0f);
        auto a = r.render (1.0, 48, 0.8f);
        Rig r2; r2.plain (CoreDelay); r2.set (ids::feedback, 0.85f); r2.set (ids::loopMode, (float) LoopHz); r2.set (ids::loopHz, 150.0f); r2.set (ids::keyTrack, 0.0f);
        auto b = r2.render (1.0, 64, 0.8f);
        const double fa = peakNear (a, 14400, 19200, 150.0), fb = peakNear (b, 14400, 19200, 150.0);
        check (std::abs (cents (fa, 150.0)) < 4.0 && std::abs (cents (fb, 150.0)) < 4.0, "loop time 150 Hz, key track 0: every key rings 150 Hz",
               juce::String (fa, 2) + " / " + juce::String (fb, 2) + " Hz");
        Rig r3; r3.plain (CoreDelay); r3.set (ids::feedback, 0.85f); r3.set (ids::loopMode, (float) LoopHz); r3.set (ids::loopHz, 150.0f); r3.set (ids::keyTrack, 1.0f);
        auto c = r3.render (1.0, 72, 0.8f);
        const double fc = peakNear (c, 14400, 19200, 300.0);
        check (std::abs (cents (fc, 300.0)) < 4.0, "key track 100 %: an octave above C4 rings 300 Hz", juce::String (fc, 2) + " Hz");
    }
    {
        Rig r; r.plain (CoreDelay); r.set (ids::feedback, 0.85f); r.set (ids::loopMode, (float) LoopMs); r.set (ids::loopMs, 5.0f);
        auto a = r.render (1.0, 60, 0.8f);
        const double f = peakNear (a, 14400, 19200, 200.0);
        check (std::abs (cents (f, 200.0)) < 4.0, "loop time 5 ms rings at 200 Hz", juce::String (f, 2) + " Hz");
    }
    {
        Rig r; r.plain (CoreDelay); r.set (ids::feedback, 0.9f); r.set (ids::damp, 0.0f); r.set (ids::loopMode, (float) LoopBpm);
        r.set (ids::loopBeat, 4.0f * 3.0f); // 1/4 at 120 BPM = 0.5 s
        r.set (ids::exDirect, -60.0f);
        auto a = r.render (1.4, 60, 1.0f);
        const double gap = rmsDb (a, (size_t) (0.2 * kSr), (size_t) (0.25 * kSr)), echo = rmsDb (a, (size_t) (0.49 * kSr), (size_t) (0.06 * kSr));
        check (echo - gap > 20.0, "BPM mode, Delay core, 1/4 at 120 BPM: the hit comes back after 0.5 s (echo-like loop)",
               "gap " + juce::String (gap, 1) + " dB, echo " + juce::String (echo, 1) + " dB");
    }
    {
        // in-loop filter shapes the hit from the first pass; with DIRECT off the raw noise is never heard
        Rig a; a.plain (CoreDelay); a.set (ids::exDirect, -60.0f);
        auto open = a.render (0.05, 60, 1.0f);
        Rig b; b.plain (CoreDelay); b.set (ids::exDirect, -60.0f); b.set (ids::slot (1, "type"), (float) SlotFilter); b.set (ids::slot (1, "a"), 0.25f); b.set (ids::slot (1, "c"), 0.0f);
        auto dark = b.render (0.05, 60, 1.0f);
        const double hiOpen = bandEnergyDb (open, 0, 960, 4000.0, 12000.0), hiDark = bandEnergyDb (dark, 0, 960, 4000.0, 12000.0);
        check (hiOpen - hiDark > 20.0, "the in-loop filter darkens the hit itself (first 20 ms) — no raw noise passes", juce::String (hiOpen - hiDark, 1) + " dB less highs");
        Rig c; c.plain (CoreDelay); c.set (ids::exDirect, 0.0f); c.set (ids::slot (1, "type"), (float) SlotFilter); c.set (ids::slot (1, "a"), 0.25f); c.set (ids::slot (1, "c"), 0.0f);
        auto withDirect = c.render (0.05, 60, 1.0f);
        check (bandEnergyDb (withDirect, 0, 960, 4000.0, 12000.0) - hiDark > 20.0, "  DIRECT brings the dry hit back on purpose");
    }
    {
        // freq shift in the loop: the tail no longer thins out between 0 and 100 % (C = spread, always fully wet)
        double tails[3];
        int k = 0;
        for (float cval : { 0.0f, 0.5f, 1.0f })
        {
            Rig r; r.plain (CoreMembrane); r.set (ids::feedback, 0.8f);
            r.set (ids::slot (1, "type"), (float) SlotFreqShift); r.set (ids::slot (1, "a"), 0.55f); r.set (ids::slot (1, "b"), 0.2f); r.set (ids::slot (1, "c"), cval);
            auto x = r.render (1.0, 48, 0.9f);
            tails[k++] = rmsDb (x, (size_t) (0.6 * kSr), (size_t) (0.2 * kSr));
        }
        const double spread = std::max ({ tails[0], tails[1], tails[2] }) - std::min ({ tails[0], tails[1], tails[2] });
        check (spread < 6.0, "freq shift in the loop: tail level about the same at C = 0 / 50 / 100 % (no thin tail)",
               juce::String (tails[0], 1) + " / " + juce::String (tails[1], 1) + " / " + juce::String (tails[2], 1) + " dB");
    }
    {
        // calibration: an in-loop saturator at full drive clearly changes the ring (it used to do almost nothing)
        Rig a; a.plain (CoreMembrane); auto clean = a.render (0.6, 48, 1.0f);
        Rig b; b.plain (CoreMembrane); b.set (ids::slot (1, "type"), (float) SlotSaturate); b.set (ids::slot (1, "a"), 1.0f); b.set (ids::slot (1, "b"), 0.5f); b.set (ids::slot (1, "c"), 1.0f);
        auto hot = b.render (0.6, 48, 1.0f);
        double diff = 0.0, ref = 0.0;
        for (size_t i = 0; i < clean.size(); ++i) { diff += (hot[i] - clean[i]) * (hot[i] - clean[i]); ref += clean[i] * clean[i]; }
        const double rel = 10.0 * std::log10 (diff / (ref + 1e-30) + 1e-30);
        check (rel > -6.0, "in-loop Saturate at full drive changes the sound a lot (calibrated loop level)", juce::String (rel, 1) + " dB difference");
    }
    {
        // after-loop saturator adds overtones once (classic distortion)
        Rig a; a.plain (CoreDelay); a.set (ids::damp, 0.9f); a.set (ids::gain, 0.0f); auto clean = a.render (0.5, 48, 1.0f);
        Rig b; b.plain (CoreDelay); b.set (ids::damp, 0.9f); b.set (ids::gain, 0.0f); b.set (ids::slot (1, "type"), (float) SlotSaturate); b.set (ids::slot (1, "place"), (float) PlaceAfter);
        b.set (ids::slot (1, "a"), 1.0f); b.set (ids::slot (1, "b"), 0.5f); b.set (ids::slot (1, "c"), 1.0f);
        auto hot = b.render (0.5, 48, 1.0f);
        auto crest = [] (const std::vector<float>& x, size_t from, size_t len) {
            double pk = 0.0, e = 0.0;
            for (size_t i = from; i < from + len; ++i) { pk = std::max (pk, (double) std::abs (x[i])); e += (double) x[i] * x[i]; }
            return 20.0 * std::log10 (pk / std::sqrt (e / (double) len + 1e-30) + 1e-30);
        };
        const double c0 = crest (clean, 1440, 1200), c1 = crest (hot, 1440, 1200);
        check (c0 - c1 > 1.5, "Saturate AFTER the loop flattens the waveform's peaks (classic distortion)", "crest " + juce::String (c0, 1) + " -> " + juce::String (c1, 1) + " dB");
    }
}

void testState()
{
    std::cout << "state" << std::endl;
    Rig a;
    a.set (ids::core, (float) CorePartials); a.set (ids::morph, 0.37f); a.set (ids::slot (5, "type"), (float) SlotChorus); a.set (ids::feedback, 0.71f);
    juce::MemoryBlock mb;
    a.proc->getStateInformation (mb);
    Rig b;
    b.proc->setStateInformation (mb.getData(), (int) mb.getSize());
    bool same = true;
    for (auto* p : a.proc->getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (std::abs (rp->getValue() - b.proc->param (rp->getParameterID()).getValue()) > 1e-6f) same = false;
    check (same, "save + load restores every parameter");
}

void benchOne (bool worst)
{
    Rig r;
    if (worst)
    {
        r.set (ids::core, (float) CorePartials); r.set (ids::feedback, 0.9f);
        r.set (ids::slot (4, "type"), (float) SlotFreqShift); r.set (ids::slot (5, "type"), (float) SlotChorus);
    }
    juce::MidiBuffer chord;
    for (int k = 0; k < kMaxVoices; ++k) chord.addEvent (juce::MidiMessage::noteOn (1, 36 + k * 3, 0.9f), 0);
    r.proc->processBlock (r.buf, chord);
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    const int blocks = (int) (5.0 * kSr / kBlock);
    juce::MidiBuffer none;
    for (int b = 0; b < blocks; ++b) r.proc->processBlock (r.buf, (b % 40 == 0) ? chord : none); // re-strike all 12 every ~0.4 s
    const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
    std::cout << "bench: " << kMaxVoices << (worst ? " voices, Partials core (16 loops), 5 inserts: " : " voices, default patch (Membrane 8 loops, 3 inserts): ")
              << juce::String (ms / 5000.0 * 100.0, 1) << " % of one core (" << r.proc->engine.activeVoices() << " voices active)" << std::endl;
}

void bench() { benchOne (false); benchOne (true); }

void writeWav (const juce::File& f, const std::vector<float>& L, const std::vector<float>& R)
{
    juce::AudioBuffer<float> b (2, (int) L.size());
    for (int i = 0; i < (int) L.size(); ++i) { b.setSample (0, i, L[(size_t) i]); b.setSample (1, i, R[(size_t) i]); }
    f.deleteFile();
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (f);
    auto w = fmt.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (kSr).withNumChannels (2).withBitsPerSample (24));
    if (w != nullptr) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

void wavs (const juce::File& dir)
{
    dir.createDirectory();
    for (int core = 0; core < 3; ++core)
        for (int note : { 40, 52 })
        {
            Rig r; r.set (ids::core, (float) core);
            std::vector<float> R;
            auto L = r.render (2.0, note, 0.9f, -1.0, &R);
            writeWav (dir.getChildFile (coreNames()[core] + "_" + juce::String (note) + ".wav"), L, R);
        }
    std::cout << "wrote WAVs to " << dir.getFullPathName() << std::endl;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    for (int i = 1; i < argc; ++i)
        if (juce::String (argv[i]) == "--bench") { bench(); return 0; }
    for (int i = 1; i < argc; ++i)
        if (juce::String (argv[i]) == "--probe")
        {
            for (int core = 0; core < 3; ++core)
                for (float base : { 0.05f, 0.7f })
                    for (float vel : { 0.25f, 1.0f })
                    {
                        Rig r; r.set (ids::core, (float) core); for (int s = 1; s <= kNumSlots; ++s) r.set (ids::slot (s, "type"), 0.0f);
                        r.set (ids::stVel, 0.0f); r.set (ids::stKey, 0.0f); r.set (ids::stRandom, 0.0f); r.set (ids::stBase, base);
                        r.set (ids::exRandom, 0.0f); r.set (ids::exVelRandom, 0.0f); r.set (ids::exVelLength, 0.0f);
                        auto x = r.render (0.5, 48, vel);
                        std::cout << coreNames()[core] << " pos " << base << " vel " << vel << ": energy " << rmsDb (x, 0, 14400) << " dB" << std::endl;
                    }
            for (int slotsOn = 0; slotsOn < 0; ++slotsOn)
                for (float vel : { 0.25f, 1.0f })
                {
                    Rig r; r.set (ids::core, (float) CorePartials);
                    if (! slotsOn) for (int s = 1; s <= kNumSlots; ++s) r.set (ids::slot (s, "type"), 0.0f);
                    auto x = r.render (1.0, 48, vel);
                    float pk = 0; size_t at = 0; for (size_t j = 0; j < x.size(); ++j) if (std::abs (x[j]) > pk) { pk = std::abs (x[j]); at = j; }
                    std::cout << "partials slots " << slotsOn << " vel " << vel << ": peak " << 20 * std::log10 (pk) << " dB at " << at / kSr * 1000 << " ms, strike " << r.proc->engine.report.strikeR.load() << std::endl;
                }
            for (int core = 0; core < 3; ++core)
                for (float vel : { 0.3f, 1.0f })
                {
                    Rig r; r.set (ids::core, (float) core); for (int s = 1; s <= kNumSlots; ++s) r.set (ids::slot (s, "type"), 0.0f);
                    r.proc->engine.dbgLinePeak = 0.0f;
                    r.render (0.5, 48, vel);
                    std::cout << coreNames()[core] << " vel " << vel << ": loop-internal peak " << r.proc->engine.dbgLinePeak << std::endl;
                }
            for (int variant = 0; variant < 0; ++variant)
            {
                Rig r; r.set (ids::core, (float) CoreDelay);
                if (variant >= 1 && variant <= 3) r.set (ids::slot (variant, "type"), 0.0f);
                if (variant == 4) for (int s = 1; s <= 3; ++s) r.set (ids::slot (s, "type"), 0.0f);
                if (variant == 5) { for (int s = 1; s <= 3; ++s) r.set (ids::slot (s, "type"), 0.0f); r.set (ids::damp, 0.0f); r.set (ids::pitchAmt, 0.0f); r.set (ids::tension, 0.0f); }
                auto x = r.render (1.0, 40, 0.9f);
                std::cout << "variant " << variant << ": 0.1s " << rmsDb (x, 4800, 2400) << "  0.3s " << rmsDb (x, 14400, 2400) << "  0.6s " << rmsDb (x, 28800, 2400) << std::endl;
            }
            return 0;
        }
    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--wav") { wavs (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1])); return 0; }
    testModes();
    testPitch();
    testDecay();
    testSafety();
    testLevels();
    testDynamic();
    testHit();
    testV02();
    testState();
    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--snapshot")
        {
            Rig r;
            r.render (0.3, 45, 0.9f);   // a hit, so the views have something to show
            r.render (0.2, 52, 0.6f);
            std::unique_ptr<juce::AudioProcessorEditor> ed (r.proc->createEditor());
            for (int k = 0; k < 20; ++k) juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f = juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]);
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
            std::cout << "snapshot: " << f.getFullPathName() << std::endl;
        }
    std::cout << (failures == 0 ? "ALL PASSED" : juce::String (failures) + " FAILED") << std::endl;
    return failures;
}
