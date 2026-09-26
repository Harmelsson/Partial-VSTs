// SeedTests — headless checks for the Seed plugin. Every check prints PASS/FAIL with numbers,
// so a failing run explains itself. Exit code = number of failures (0 = all good).
//
// What is checked, and why:
//   math      the copied stretch math still gives the known values (seed 2 = harmonics, ...)
//   spectrum  seed 2 really produces 440, 880, 1320... with 1/n loudness; seed 2.5 moves them
//   dynamic   changing seed / partial count WHILE a note is held glides without clicks
//   safety    worst cases (64 partials collapsed on one pitch, 16 voices, 0 dB) never exceed
//             the -1 dBFS ceiling and never produce NaN
//   keys      "keys follow seed": 12 keys up from middle C lands on exactly seed x 261.63 Hz
//   drone     drone plays without MIDI and fades out when switched off
//   state     saving and re-loading a project restores every parameter
//   curve     the C++ dissonance-curve copy finds ArtHar's pinned notes (seed 2: 6/5, 5/4, 4/3...)
//   velocity  note volume follows velocity (vel^2 at 100% sensitivity, flat at 0%)
//   envelope  decay settles on the sustain level; sustain 0 ends the note while held
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "xen/Stretch.h"
#include "xen/CalculatorWorker.h"

namespace
{
constexpr double kSr = 48000.0;
constexpr int kBlock = 512;
int failures = 0;

void check (bool ok, const juce::String& what, const juce::String& detail = {})
{
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << (detail.isEmpty() ? "" : "   [" + detail + "]") << std::endl;
    if (! ok) ++failures;
}

/// Magnitude of one frequency in a signal (Goertzel algorithm with a Hann window),
/// normalised so a full-scale sine of amplitude A reads ~A.
double toneAmp (const std::vector<float>& x, double freq)
{
    const size_t n = x.size();
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

struct Rig
{
    std::unique_ptr<SeedProcessor> proc = std::make_unique<SeedProcessor>();
    juce::AudioBuffer<float> buf { 2, kBlock };

    Rig()
    {
        proc->setPlayConfigDetails (0, 2, kSr, kBlock);
        proc->prepareToPlay (kSr, kBlock);
        set (seed::ids::keyMode, 0.0f); // tests assume normal keys unless they choose a mode
    }

    void set (const juce::String& id, float value)
    {
        auto* p = proc->apvts.getParameter (id);
        jassert (p != nullptr);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    /// Lets the calculator (background thread) catch up with the current settings, and hands its
    /// scale to the audio thread. Renders a few silent-ish blocks so modulated values are published.
    void settleCalculator()
    {
        juce::AudioBuffer<float> tmp (2, kBlock);
        juce::MidiBuffer none;
        for (int i = 0; i < 400; ++i)
        {
            tmp.clear();
            proc->processBlock (tmp, none);
            proc->pumpCalculator();
            if (proc->hasCalculatorResult() && ! proc->calculatorBusy() && i > 3)
            {
                const auto want = proc->currentCalculatorSettings();
                if (proc->calculatorOutput().settings == want) break;
            }
            juce::Thread::sleep (5);
        }
        proc->pumpCalculator();
    }

    /// Renders `seconds` of audio (left channel), sending `midi` at the start of the first block.
    std::vector<float> render (double seconds, juce::MidiBuffer midi = {})
    {
        std::vector<float> out;
        const int blocks = (int) std::ceil (seconds * kSr / kBlock);
        for (int b = 0; b < blocks; ++b)
        {
            buf.clear();
            juce::MidiBuffer m;
            if (b == 0) m = midi;
            proc->processBlock (buf, m);
            out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + kBlock);
        }
        return out;
    }

    static juce::MidiBuffer noteOn (std::initializer_list<int> notes, float vel = 0.8f)
    {
        juce::MidiBuffer m;
        for (int n : notes) m.addEvent (juce::MidiMessage::noteOn (1, n, vel), 0);
        return m;
    }
    static juce::MidiBuffer noteOff (int n)
    {
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::noteOff (1, n), 0);
        return m;
    }
};

std::vector<float> tail (const std::vector<float>& x, double seconds)
{
    const size_t n = std::min (x.size(), (size_t) (seconds * kSr));
    return { x.end() - (long) n, x.end() };
}

float peakOf (const std::vector<float>& x)
{
    float p = 0;
    for (float v : x) p = std::max (p, std::abs (v));
    return p;
}

bool allFinite (const std::vector<float>& x)
{
    return std::all_of (x.begin(), x.end(), [] (float v) { return std::isfinite (v); });
}

/// Largest jump between consecutive samples: a click shows up as an outlier here.
float maxStep (const std::vector<float>& x, size_t from = 1)
{
    float m = 0;
    for (size_t i = std::max<size_t> (from, 1); i < x.size(); ++i) m = std::max (m, std::abs (x[i] - x[i - 1]));
    return m;
}

void testMath()
{
    std::cout << "math" << std::endl;
    bool harmonics = true;
    for (int n = 1; n <= 64; ++n) harmonics &= std::abs (xen::partialRatio (n, xen::stretchFactor (2.0)) - n) < 1e-9;
    check (harmonics, "seed 2 gives ordinary harmonics 1..64");
    const double c2 = xen::ratioToCents (xen::partialRatio (2, xen::stretchFactor (2.5)));
    check (std::abs (c2 - 1586.313713864835) < 1e-6, "seed 2.5: partial 2 at 1586.3137 c", juce::String (c2, 6));
    check (std::abs (xen::periodCents (3.0) - 1901.9550008653874) < 1e-9, "seed 3: period is the tritave 1901.955 c");
    const double expected[] = { 0, 1586, 2514, 3173, 3683, 4101, 4453, 4759 };
    bool hand = true;
    for (int n = 1; n <= 8; ++n)
        hand &= std::abs (xen::ratioToCents (xen::partialRatio (n, xen::stretchFactor (2.5))) - expected[n - 1]) < 0.5;
    check (hand, "seed 2.5 matches the hand-worked cents (0,1586,2514,...,4759)");
}

void testSpectrum()
{
    std::cout << "spectrum" << std::endl;
    {
        Rig r;
        auto x = tail (r.render (1.0, Rig::noteOn ({ 69 })), 0.5); // A4 = 440 Hz
        const double a1 = toneAmp (x, 440.0);
        bool ok = a1 > 0.01;
        juce::String detail;
        for (int n = 2; n <= 8; ++n)
        {
            const double ratio = toneAmp (x, 440.0 * n) / a1;
            detail << n << ":" << juce::String (ratio, 3) << " ";
            ok &= std::abs (ratio - 1.0 / n) < 0.1 / n; // within 10% of 1/n
        }
        check (ok, "seed 2: partials at 440*n with loudness 1/n", detail);
        const double between = toneAmp (x, 660.0) / a1;
        check (between < 0.01, "seed 2: nothing between harmonics (660 Hz)", juce::String (between, 5));
    }
    {
        Rig r;
        r.set (seed::ids::seed, 2.5f);
        auto x = tail (r.render (1.0, Rig::noteOn ({ 69 })), 0.5);
        const double a1 = toneAmp (x, 440.0), a2 = toneAmp (x, 1100.0), plain = toneAmp (x, 880.0);
        check (a2 / a1 > 0.4 && plain / a1 < 0.01, "seed 2.5: partial 2 moved from 880 to 1100 Hz",
               "1100:" + juce::String (a2 / a1, 3) + " 880:" + juce::String (plain / a1, 5));
        const double a3 = toneAmp (x, 440.0 * std::pow (3.0, xen::stretchFactor (2.5)));
        check (a3 / a1 > 0.25, "seed 2.5: partial 3 at 440*3^S", juce::String (a3 / a1, 3));
    }
}

void testDynamic()
{
    std::cout << "dynamic (changes while a note is held)" << std::endl;
    Rig r;
    auto steady = r.render (0.5, Rig::noteOn ({ 57 }));
    const float steadyStep = maxStep (steady, (size_t) (0.1 * kSr));
    r.set (seed::ids::seed, 2.5f);
    auto glide = r.render (0.5);
    const float glideStep = maxStep (glide);
    check (glideStep < steadyStep * 1.6f, "seed 2 -> 2.5 while held: no click",
           "steady max step " + juce::String (steadyStep, 4) + ", during glide " + juce::String (glideStep, 4));
    const double f0 = 220.0;
    auto end = tail (glide, 0.25);
    check (toneAmp (end, f0 * 2.5) / toneAmp (end, f0) > 0.4, "held note ends up on the new seed (partial 2 at 550 Hz)");

    r.set (seed::ids::partials, 64.0f);
    auto more = r.render (0.3);
    r.set (seed::ids::partials, 2.0f);
    auto fewer = r.render (0.3);
    check (maxStep (more) < steadyStep * 2.5f && maxStep (fewer) < steadyStep * 2.5f,
           "partial count 16 -> 64 -> 2 while held: fades, no pops",
           juce::String (maxStep (more), 4) + " / " + juce::String (maxStep (fewer), 4));

    r.set (seed::ids::keyMode, 1.0f); // Seed-12
    auto toggled = r.render (0.3);
    check (maxStep (toggled) < steadyStep * 2.5f, "toggling 'keys follow seed' while held glides", juce::String (maxStep (toggled), 4));
}

void testSafety()
{
    std::cout << "safety" << std::endl;
    struct Case { float seed, rolloff; int partials; };
    for (auto c : { Case { 1.0f, 0.0f, 64 }, Case { 0.5f, 0.0f, 64 }, Case { 4.0f, 0.0f, 64 }, Case { 2.0f, 0.0f, 64 } })
    {
        Rig r;
        r.set (seed::ids::seed, c.seed);
        r.set (seed::ids::rolloff, c.rolloff);
        r.set (seed::ids::partials, (float) c.partials);
        r.set (seed::ids::gain, 0.0f);
        auto x = r.render (1.0, Rig::noteOn ({ 24, 36, 43, 48, 52, 55, 60, 64, 67, 72, 76, 79, 84, 96, 108, 120 }, 1.0f));
        const float pk = peakOf (x);
        check (allFinite (x) && pk <= pvst::SafetyStage::kCeiling + 1e-6f,
               "seed " + juce::String (c.seed, 1) + ", 64 partials, 16 voices, 0 dB: under -1 dBFS ceiling",
               "peak " + juce::String (pk, 4) + ", faults " + juce::String (r.proc->safetyFaults()));
    }
    Rig idle;
    auto silence = idle.render (0.5);
    check (peakOf (silence) <= 0.0f, "no notes = exact silence");
}

void testKeys()
{
    std::cout << "keys follow seed" << std::endl;
    Rig r;
    r.set (seed::ids::seed, 2.5f);
    r.set (seed::ids::keyMode, 1.0f); // Seed-12 (was "keys follow seed")
    auto x = tail (r.render (1.0, Rig::noteOn ({ 72 })), 0.5);
    const double want = 261.6255653005986 * 2.5;
    const double on = toneAmp (x, want), off = toneAmp (x, 523.25);
    check (on > 0.02 && off < on * 0.01, "key 72 = one seed-octave above middle C = 654.06 Hz",
           "at 654.06: " + juce::String (on, 4) + ", at 523.25: " + juce::String (off, 5));
}

void testDrone()
{
    std::cout << "drone" << std::endl;
    Rig r;
    r.set (seed::ids::drone, 1.0f);
    auto on = r.render (0.5);
    check (peakOf (tail (on, 0.2)) > 0.02, "drone sounds without any MIDI", juce::String (peakOf (tail (on, 0.2)), 4));
    auto held = r.render (0.5, Rig::noteOff (48));
    check (peakOf (tail (held, 0.2)) > 0.02, "a note-off on the drone's key does not stop it");
    r.set (seed::ids::drone, 0.0f);
    auto off = r.render (3.0);
    check (peakOf (tail (off, 0.2)) < 1e-4f, "drone switched off fades to silence");
}

void testState()
{
    std::cout << "state" << std::endl;
    Rig a;
    a.set (seed::ids::seed, 2.7182f);
    a.set (seed::ids::partials, 33.0f);
    a.set (seed::ids::rolloff, 0.5f);
    juce::MemoryBlock mb;
    a.proc->getStateInformation (mb);
    Rig b;
    b.proc->setStateInformation (mb.getData(), (int) mb.getSize());
    const float s = b.proc->apvts.getRawParameterValue (seed::ids::seed)->load();
    const float p = b.proc->apvts.getRawParameterValue (seed::ids::partials)->load();
    check (std::abs (s - 2.7182f) < 1e-4f && std::abs (p - 33.0f) < 1e-6f, "save + load restores parameters",
           "seed " + juce::String (s, 4) + ", partials " + juce::String (p));
}

void testCurve()
{
    std::cout << "dissonance curve (C++ copy of ArtHar's method)" << std::endl;
    auto scaleFor = [] (double seed, double fund, double minStrength = 0.02) {
        xen::CalculatorSettings cs; cs.seed = seed; cs.fundamentalHz = fund; cs.minStrength = minStrength;
        xen::DissonanceCalculator calc; xen::CurveResult r;
        calc.compute (cs.makePartials(), xen::periodCents (seed), cs.minStrength, r, [] { return false; });
        return r;
    };
    // ArtHar PROJECT_LOG v0.2 "what to test": 440 Hz, seed 2, 16 partials, rolloff 1, 2%.
    auto r = scaleFor (2.0, 440.0);
    const double want[] = { 315.6, 386.3, 498.0, 582.5, 702.0, 813.7, 884.4, 968.8 };
    bool ok = r.valid && r.notes.size() == 10;
    juce::String got;
    for (size_t i = 1; i + 1 < r.notes.size(); ++i) got << juce::String (r.notes[i].cents, 1) << " ";
    for (size_t i = 0; ok && i < 8; ++i) ok &= std::abs (r.notes[i + 1].cents - want[i]) < 0.06;
    check (ok, "seed 2 @440: 6/5 5/4 4/3 7/5 3/2 8/5 5/3 7/4", got);
    // Strongest found note for seed 2 is the fifth, and seed 2.5's is the stretched fifth 927.9 c.
    auto strongest = [] (const xen::CurveResult& res) {
        const xen::ScaleNote* best = nullptr;
        for (auto& n : res.notes) if (n.kind == xen::ScaleNote::Found && (! best || n.strength > best->strength)) best = &n;
        return best ? best->cents : -1.0;
    };
    check (std::abs (strongest (r) - 701.955) < 0.01, "seed 2: strongest dip is the pure fifth", juce::String (strongest (r), 3));
    auto r25 = scaleFor (2.5, 440.0);
    check (std::abs (strongest (r25) - 927.93) < 0.05, "seed 2.5: strongest dip is the stretched fifth 927.9 c", juce::String (strongest (r25), 3));
    check (std::abs (r25.notes.back().cents - 1586.3137) < 1e-3, "seed 2.5: scale ends on its 'octave' 1586.31 c");
    auto r1 = scaleFor (1.0, 440.0);
    check (! r1.valid, "seed 1: no octave, no scale (and no crash)");
    // Undertones: the loud partials sit in the bass where roughness is smooth, so dips are
    // shallow (the fifth is ~1.5%, under the default 2%). With min strength 0 it must be there.
    auto rUnder = scaleFor (0.5, 440.0, 0.0);
    bool fifth = false;
    for (auto& n : rUnder.notes) fifth |= std::abs (n.cents - 701.955) < 0.1;
    check (rUnder.valid && fifth, "seed 0.5 (undertones): finds the fifth at min strength 0", juce::String ((int) rUnder.notes.size()) + " notes");

    // The background worker delivers a result.
    xen::CalculatorWorker worker;
    xen::CalculatorSettings cs; cs.seed = 2.5;
    worker.request (cs);
    xen::CalculatorOutput out; uint64_t seen = 0;
    for (int i = 0; i < 400 && ! worker.fetch (out, seen); ++i) juce::Thread::sleep (5);
    check (seen > 0 && out.curve.valid && juce::exactlyEqual (out.settings.seed, 2.5), "background worker returns the result",
           juce::String (out.millis, 1) + " ms");
}

void testVelocity()
{
    std::cout << "velocity" << std::endl;
    auto level = [] (float vel, float sens) {
        Rig r;
        r.set (seed::ids::velocity, sens);
        r.set (seed::ids::sustain, 100.0f);
        return toneAmp (tail (r.render (0.6, Rig::noteOn ({ 69 }, vel)), 0.3), 440.0);
    };
    const double full = level (1.0f, 100.0f), half = level (0.5f, 100.0f);
    check (std::abs (half / full - 0.25) < 0.02, "100%: velocity 64 is a quarter of velocity 127 (-12 dB)", juce::String (half / full, 3));
    const double flatFull = level (1.0f, 0.0f), flatHalf = level (0.5f, 0.0f);
    check (std::abs (flatHalf / flatFull - 1.0) < 0.02, "0%: every velocity equally loud", juce::String (flatHalf / flatFull, 3));
}

void testEnvelope()
{
    std::cout << "envelope" << std::endl;
    auto settled = [] (float sustainPct) {
        Rig r;
        r.set (seed::ids::decay, 200.0f);
        r.set (seed::ids::sustain, sustainPct);
        return toneAmp (tail (r.render (1.5, Rig::noteOn ({ 69 })), 0.4), 440.0);
    };
    const double peakAmp = settled (100.0f), sustained = settled (50.0f);
    check (std::abs (sustained / peakAmp - 0.5) < 0.02, "decay settles at the sustain level (50%)", juce::String (sustained / peakAmp, 3));
    Rig r;
    r.set (seed::ids::decay, 200.0f);
    r.set (seed::ids::sustain, 50.0f);
    r.render (1.0, Rig::noteOn ({ 69 }));
    r.set (seed::ids::sustain, 100.0f);
    auto up = r.render (1.0);
    check (toneAmp (tail (up, 0.3), 440.0) / peakAmp > 0.9, "raising sustain while held brings the level up (dynamic rule)");

    Rig z;
    z.set (seed::ids::sustain, 0.0f);
    z.set (seed::ids::decay, 100.0f);
    z.render (1.0, Rig::noteOn ({ 69 }));
    check (z.proc->uiActiveVoices.load() == 0, "sustain 0: the note ends by itself while the key is held");
}

// ================================================================================================
// v0.3 playground
// ================================================================================================
void testConsonance()
{
    std::cout << "consonance method (C++ copy of ArtHar's)" << std::endl;
    auto notesFor = [] (double seed) {
        xen::PartialEdits e;
        std::vector<double> pos;
        for (auto& m : xen::buildPartials (seed, 16, 1.0, e)) pos.push_back (m.cents);
        xen::ConsonanceCalculator c; xen::CurveResult r; bool complete = false;
        c.compute (pos, xen::periodCents (seed), {}, r, complete, [] { return false; });
        juce::String s; for (auto& n : r.notes) s << juce::String (n.cents, 1) << " ";
        return s.trim();
    };
    const auto s2 = notesFor (2.0), s25 = notesFor (2.5);
    check (s2 == "0.0 386.3 702.0 968.8 1200.0", "seed 2: the 4:5:6:7 chord", s2);
    check (s25 == "0.0 510.7 927.9 1280.7 1586.3", "seed 2.5: the stretched 4:5:6:7", s25);
}

void testEdits()
{
    std::cout << "calculator edits (sticky offsets, reset = calculator decides again)" << std::endl;
    Rig r;
    r.settleCalculator();
    auto fin = r.proc->finalScale();
    const int before = fin.count;
    // Switch note 1 off, move note 2 by +10 c, add a custom note at 100 c.
    r.set (seed::ids::noteOn (1), 0.0f);
    r.set (seed::ids::noteOffset (2), 10.0f);
    r.set (seed::ids::customNoteCents (1), 100.0f);
    r.set (seed::ids::customNoteOn (1), 1.0f);
    fin = r.proc->finalScale();
    int disabled = 0, moved = 0, custom = 0;
    for (int i = 0; i < fin.count; ++i) { disabled += fin.notes[(size_t) i].enabled ? 0 : 1; moved += fin.notes[(size_t) i].moved; custom += fin.notes[(size_t) i].custom; }
    check (fin.count == before + 1 && disabled == 1 && moved == 1 && custom == 1, "off / moved / custom notes are marked",
           juce::String (disabled) + " off, " + juce::String (moved) + " moved, " + juce::String (custom) + " custom");
    // Reset = back to the calculator.
    r.set (seed::ids::noteOn (1), 1.0f);
    r.set (seed::ids::noteOffset (2), 0.0f);
    r.set (seed::ids::customNoteOn (1), 0.0f);
    fin = r.proc->finalScale();
    bool clean = fin.count == before;
    for (int i = 0; i < fin.count; ++i) clean &= fin.notes[(size_t) i].enabled && ! fin.notes[(size_t) i].moved && ! fin.notes[(size_t) i].custom;
    check (clean, "removing the edits returns the calculator's scale");

    // Partial level edit: muting partial 2 removes 880 Hz from the sound.
    Rig q;
    q.set (seed::ids::partialLevel (2), -60.0f);
    auto x = tail (q.render (0.8, Rig::noteOn ({ 69 })), 0.4);
    check (toneAmp (x, 880.0) < 1e-3 && toneAmp (x, 1320.0) > 0.01, "muted partial 2 is silent, partial 3 still there");
    // Custom partial at +1900 c (a 'stretched fifth over the octave').
    Rig c;
    c.set (seed::ids::customPartialCents (1), 1900.0f);
    c.set (seed::ids::customPartialLevel (1), -6.0f);
    c.set (seed::ids::customPartialOn (1), 1.0f);
    auto y = tail (c.render (0.8, Rig::noteOn ({ 69 })), 0.4);
    const double fc = 440.0 * std::exp2 (1900.0 / 1200.0);
    check (toneAmp (y, fc) > 0.01, "custom partial sounds at +1900 c", juce::String (toneAmp (y, fc), 4));
    // Partial shift edit moves partial 3.
    Rig s;
    s.set (seed::ids::partialPos (3), 100.0f);
    auto z = tail (s.render (0.8, Rig::noteOn ({ 69 })), 0.4);
    check (toneAmp (z, 1320.0 * std::exp2 (100.0 / 1200.0)) > 0.01 && toneAmp (z, 1320.0) < 2e-3, "partial 3 shifted by +100 c");
}

void testKeyModes()
{
    std::cout << "key modes" << std::endl;
    // Scale mode, seed 2, dissonance @ middle C: key 61 = first found note above the unison.
    Rig r;
    r.set (seed::ids::keyMode, 2.0f);
    r.settleCalculator();
    const auto fin = r.proc->finalScale();
    std::array<double, 80> c {}, st {};
    const int n = fin.playable (c.data(), st.data(), 80);
    auto x = tail (r.render (0.8, Rig::noteOn ({ 61 })), 0.4);
    const double want = 261.6255653 * std::exp2 (c[1] / 1200.0);
    check (n > 3 && toneAmp (x, want) > 0.02, "Scale: key 61 plays scale note 1", juce::String (c[1], 1) + " c = " + juce::String (want, 2) + " Hz");
    // Key 60 + N = one period up.
    Rig o;
    o.set (seed::ids::keyMode, 2.0f);
    o.settleCalculator();
    auto y = tail (o.render (0.8, Rig::noteOn ({ 60 + n })), 0.4);
    check (toneAmp (y, 523.2511) > 0.02, "Scale: key 60 + N = the 'octave'");
    // Switching a note off shifts the keys above it (the keyboard follows the playable notes).
    Rig d;
    d.set (seed::ids::keyMode, 2.0f);
    d.set (seed::ids::noteOn (1), 0.0f);
    d.settleCalculator();
    auto z = tail (d.render (0.8, Rig::noteOn ({ 61 })), 0.4);
    const double want2 = 261.6255653 * std::exp2 (c[2] / 1200.0);
    check (toneAmp (z, want2) > 0.02, "Scale: note 1 off -> key 61 plays note 2");
    // Magnet 100%: every key lands on a scale note.
    Rig m;
    m.set (seed::ids::keyMode, 3.0f);
    m.set (seed::ids::magnet, 100.0f);
    m.settleCalculator();
    auto w = tail (m.render (0.8, Rig::noteOn ({ 64 })), 0.4);   // E4 = 400 c -> nearest note 386.3 (5/4)
    check (toneAmp (w, 261.6255653 * 1.25) > 0.02, "Magnet: key 64 snaps to 5/4 (386.3 c)");
}

void testPitch()
{
    std::cout << "pitch + frequency shift" << std::endl;
    Rig r;
    r.set (seed::ids::pitch, 1200.0f);
    auto x = tail (r.render (0.8, Rig::noteOn ({ 69 })), 0.4);
    check (toneAmp (x, 880.0) > 0.02 && toneAmp (x, 440.0) < 1e-3, "pitch +1200 c: A4 sounds at 880 Hz");
    Rig f;
    f.set (seed::ids::fshift, 100.0f);
    auto y = tail (f.render (0.8, Rig::noteOn ({ 69 })), 0.4);
    check (toneAmp (y, 540.0) > 0.02 && toneAmp (y, 980.0) > 0.01 && toneAmp (y, 880.0) < 2e-3,
           "freq shift +100 Hz: 440->540, 880->980 (inharmonic)");
}

void testChaosSafety()
{
    std::cout << "chaos safety (FX, dice, mods)" << std::endl;
    // Nasty FX settings: max feedback + spiral + max roughness, everything sent, 64 partials.
    Rig r;
    r.set (seed::ids::partials, 64.0f); r.set (seed::ids::rolloff, 0.0f); r.set (seed::ids::gain, 0.0f);
    using seed::ids::fx;
    r.set (fx (1, "mix"), 100.0f); r.set (fx (1, "picks"), 0.0f); r.set (fx (1, "dFb"), 95.0f); r.set (fx (1, "dSpiral"), 700.0f);
    r.set (fx (2, "mix"), 100.0f); r.set (fx (2, "rRough"), 100.0f); r.set (fx (2, "rDecay"), 20.0f);
    r.set (fx (3, "mix"), 100.0f); r.set (fx (3, "cDecay"), 20.0f); r.set (fx (3, "cChirp"), 100.0f);
    r.set (fx (4, "kSlope"), -1.0f); r.set (fx (4, "kMake"), 24.0f); r.set (fx (4, "kThr"), -40.0f);
    r.set (fx (5, "type"), (float) seed::FxChirp); r.set (fx (5, "mix"), 100.0f); r.set (fx (5, "in"), 1.0f);
    r.set (seed::ids::filterMode, 2.0f); r.set (seed::ids::reso, 100.0f); r.set (seed::ids::noise, 0.0f);
    r.settleCalculator();
    auto x = r.render (3.0, Rig::noteOn ({ 36, 48, 60, 64, 67, 72 }, 1.0f));
    check (allFinite (x) && peakOf (x) <= pvst::SafetyStage::kCeiling + 1e-6f, "worst-case FX: finite, under the ceiling",
           "peak " + juce::String (peakOf (x), 4));
    auto tailAfter = r.render (2.0);
    check (allFinite (tailAfter), "FX tail stays finite after notes");

    // Dice everything many times: never NaN, never above the ceiling.
    bool ok = true;
    float worst = 0.0f;
    for (int i = 0; i < 12; ++i)
    {
        Rig d;
        d.proc->randomize (SeedProcessor::Scope::All, 1.0);
        d.settleCalculator();
        auto y = d.render (0.6, Rig::noteOn ({ 48, 60, 67 }, 1.0f));
        ok &= allFinite (y) && peakOf (y) <= pvst::SafetyStage::kCeiling + 1e-6f;
        worst = std::max (worst, peakOf (y));
    }
    check (ok, "12 x Dice (all): finite, under the ceiling", "worst peak " + juce::String (worst, 4));

    // A modulator on the seed actually moves the calculator.
    Rig m;
    m.set (seed::ids::modTarget (1), 1.0f); // Seed
    m.set (seed::ids::modShape (1), 3.0f);  // Square
    m.set (seed::ids::modRate (1), 0.25f);
    m.set (seed::ids::modDepth (1), 100.0f);
    m.render (0.5);
    const double s1 = m.proc->currentCalculatorSettings().seed;
    m.render (2.0);
    const double s2 = m.proc->currentCalculatorSettings().seed;
    check (std::abs (s1 - s2) > 0.5, "mod 1 (square) on Seed swings the calculator's seed", juce::String (s1, 3) + " -> " + juce::String (s2, 3));
}

void testChaosToys()
{
    std::cout << "chaos toys (chord, wander, scramble)" << std::endl;
    Rig r; // 12-EDO chord of 3, step 4 keys: A4 + C#5 + F5
    r.set (seed::ids::chordSize, 3.0f);
    r.set (seed::ids::chordStep, 4.0f);
    auto x = tail (r.render (0.8, Rig::noteOn ({ 69 })), 0.4);
    check (toneAmp (x, 440.0) > 0.01 && toneAmp (x, 554.365) > 0.01 && toneAmp (x, 698.456) > 0.01 && r.proc->uiActiveVoices.load() == 3,
           "chord 3 x 4 keys: one key plays A, C#, F");
    auto off = r.render (1.5, Rig::noteOff (69));
    check (r.proc->uiActiveVoices.load() == 0, "releasing the key releases the whole chord");

    Rig w; // wander pulls partial 2 away from exactly 880 Hz
    w.set (seed::ids::wander, 200.0f);
    w.set (seed::ids::wanderRate, 5.0f);
    auto y = tail (w.render (2.0, Rig::noteOn ({ 69 })), 0.5);
    Rig plain;
    auto z = tail (plain.render (2.0, Rig::noteOn ({ 69 })), 0.5);
    check (toneAmp (y, 880.0) < 0.5 * toneAmp (z, 880.0) && toneAmp (y, 440.0) > 0.5 * toneAmp (z, 440.0),
           "wander moves partials (not the fundamental)", juce::String (toneAmp (y, 880.0) / toneAmp (z, 880.0), 3));

    Rig sc;
    sc.set (seed::ids::scramble, 100.0f);
    sc.set (seed::ids::partials, 32.0f);
    auto q = sc.render (1.0, Rig::noteOn ({ 48, 60 }, 1.0f));
    check (allFinite (q) && peakOf (q) <= pvst::SafetyStage::kCeiling + 1e-6f, "scramble 100%: finite, under the ceiling");
}

void testV04()
{
    std::cout << "v0.4 (scale ticks, reset edits, 0 ms attack, held notes)" << std::endl;
    // Custom partials start "sound only": adding one doesn't change the scale until ticked.
    Rig a;
    a.settleCalculator();
    const auto before = a.proc->finalScale();
    a.set (seed::ids::customPartialCents (1), 1750.0f);
    a.set (seed::ids::customPartialLevel (1), 0.0f);
    a.set (seed::ids::customPartialOn (1), 1.0f);
    a.settleCalculator();
    const auto soundOnly = a.proc->finalScale();
    auto sameScale = [] (const xen::FinalScale& x, const xen::FinalScale& y) {
        if (x.count != y.count) return false;
        for (int i = 0; i < x.count; ++i) if (std::abs (x.notes[(size_t) i].cents - y.notes[(size_t) i].cents) > 1e-6) return false;
        return true;
    };
    check (sameScale (before, soundOnly), "unticked custom partial: scale unchanged");
    auto y = tail (a.render (0.8, Rig::noteOn ({ 69 })), 0.4);
    check (toneAmp (y, 440.0 * std::exp2 (1750.0 / 1200.0)) > 0.01, "...but it sounds");
    a.set (seed::ids::customPartialInScale (1), 1.0f);
    a.settleCalculator();
    check (! sameScale (before, a.proc->finalScale()), "ticked: now it shapes the scale");
    // Unticking a series partial changes the scale too (partial 3 out of the math).
    Rig b;
    b.settleCalculator();
    const auto b0 = b.proc->finalScale();
    b.set (seed::ids::partialInScale (3), 0.0f);
    b.settleCalculator();
    check (! sameScale (b0, b.proc->finalScale()), "unticking partial 3 changes the scale");

    // Reset edits: hand edits AND calculator-aimed modulators go; synth-aimed ones stay.
    Rig r;
    r.set (seed::ids::noteOn (2), 0.0f);
    r.set (seed::ids::partialLevel (4), -20.0f);
    r.set (seed::ids::partialInScale (5), 0.0f);
    r.set (seed::ids::modTarget (1), 1.0f); r.set (seed::ids::modDepth (1), 50.0f);   // -> Seed
    r.set (seed::ids::modTarget (2), 14.0f); r.set (seed::ids::modDepth (2), 50.0f);  // -> Cutoff
    r.proc->resetCalculatorEdits();
    auto v = [&r] (const juce::String& id) { auto* p = r.proc->apvts.getParameter (id); return p->convertFrom0to1 (p->getValue()); };
    check (v (seed::ids::noteOn (2)) > 0.5f && std::abs (v (seed::ids::partialLevel (4))) < 1e-4f && v (seed::ids::partialInScale (5)) > 0.5f
               && v (seed::ids::modTarget (1)) < 0.5f && v (seed::ids::modTarget (2)) > 13.5f,
           "Reset edits: edits + ticks + seed modulator cleared, cutoff modulator kept");

    // 0 ms attack: full level almost at once.
    Rig z;
    z.set (seed::ids::attack, 0.0f);
    auto x = z.render (0.02, Rig::noteOn ({ 69 }, 1.0f));
    float early = 0.0f; for (int i = 0; i < 48; ++i) early = std::max (early, std::abs (x[(size_t) i]));
    check (early > 0.05f, "attack 0 ms: loud within the first millisecond", juce::String (early, 3));

    // Held notes are reported for the UI (Scale mode: key 60 + 2 = scale note 2).
    Rig h;
    h.set (seed::ids::keyMode, 2.0f);
    h.settleCalculator();
    h.render (0.1, Rig::noteOn ({ 62 }));
    const auto fin = h.proc->finalScale();
    std::array<double, 80> c {}, st {};
    fin.playable (c.data(), st.data(), 80);
    const float held = h.proc->uiHeld[0].load();
    check (! std::isnan (held) && std::abs (held - (float) c[2]) < 0.01f, "held key 62 reported as scale note 2", juce::String (held, 2));
}

void testV05()
{
    std::cout << "v0.5 (release on re-press, FX rack, picks morph, BPM sync, squash)" << std::endl;
    using seed::ids::fx;
    // Re-pressing a key while its previous note is still releasing: the tail keeps fading.
    {
        Rig r;
        r.set (seed::ids::release, 2000.0f);
        r.render (0.3, Rig::noteOn ({ 60 }));
        r.render (0.05, Rig::noteOff (60));
        r.render (0.05, Rig::noteOn ({ 60 }));
        check (r.proc->uiActiveVoices.load() == 2, "re-pressed key: old note keeps releasing, new voice starts",
               juce::String (r.proc->uiActiveVoices.load()) + " voices");
    }
    // Picks: slot 1 = delay on partials, no feedback, 200 ms. The note is cut after 50 ms (release
    // 0), so 200-300 ms later only the echo is left: Odd -> 440 but no 880; 1.5 -> both.
    auto echo = [] (float picks) {
        Rig r;
        r.set (seed::ids::release, 0.0f);
        r.set (seed::ids::attack, 0.0f);
        r.set (fx (1, "mix"), 100.0f); r.set (fx (1, "dFb"), 0.0f); r.set (fx (1, "dTime"), 200.0f); r.set (fx (1, "picks"), picks);
        r.set (fx (1, "dPing"), 0.0f);
        r.render (0.4); // let the (deliberately smoothed) picks value settle first
        r.render (0.05, Rig::noteOn ({ 69 }));
        r.render (0.001, Rig::noteOff (69));
        auto x = r.render (0.4);
        const std::vector<float> w (x.begin() + (long) (0.17 * kSr), x.begin() + (long) (0.22 * kSr));
        return std::pair<double, double> { toneAmp (w, 440.0), toneAmp (w, 880.0) };
    };
    const auto odd = echo (1.0f), half = echo (1.5f);
    check (odd.first > 0.01 && odd.second < odd.first * 0.05, "picks = Odd: echo has partial 1, not partial 2",
           juce::String (odd.first, 4) + " / " + juce::String (odd.second, 5));
    // Partial 2 is half as loud as partial 1 (rolloff 1), each at 50% weight -> ratio ~0.5.
    check (half.first > 0.005 && std::abs (half.second / half.first - 0.5) < 0.1, "picks = Odd 50% / Even 50%: both in the echo",
           juce::String (half.first, 4) + " / " + juce::String (half.second, 4));
    // Swap slots.
    {
        Rig r;
        r.set (fx (1, "mix"), 33.0f);
        r.proc->swapFxSlots (1, 2);
        auto v = [&r] (const juce::String& id) { auto* p = r.proc->apvts.getParameter (id); return p->convertFrom0to1 (p->getValue()); };
        check ((int) v (fx (1, "type")) == seed::FxRough && (int) v (fx (2, "type")) == seed::FxDelay && std::abs (v (fx (2, "mix")) - 33.0f) < 0.01f,
               "swapping slots 1 and 2 moves type and settings");
    }
    // BPM sync (no host = 120 BPM): 1/4 = 500 ms echo.
    {
        Rig r;
        r.set (seed::ids::release, 0.0f); r.set (seed::ids::attack, 0.0f);
        r.set (fx (1, "mix"), 100.0f); r.set (fx (1, "dFb"), 0.0f); r.set (fx (1, "dSync"), 1.0f); r.set (fx (1, "dDiv"), 4.0f);
        r.set (fx (1, "picks"), 0.0f); r.set (fx (1, "dPing"), 0.0f);
        r.render (0.05, Rig::noteOn ({ 69 }));
        r.render (0.001, Rig::noteOff (69));
        auto x = r.render (0.8);
        const std::vector<float> before (x.begin() + (long) (0.30 * kSr), x.begin() + (long) (0.40 * kSr));
        const std::vector<float> at (x.begin() + (long) (0.45 * kSr), x.begin() + (long) (0.50 * kSr));
        check (peakOf (before) < 1e-3f && peakOf (at) > 0.01f, "delay synced to 1/4 at 120 BPM echoes after 500 ms");
    }
    // Squash: over-compression makes a loud chord QUIETER than a transparent slot.
    auto level = [] (float slope) {
        Rig r;
        r.set (fx (4, "kThr"), -30.0f); r.set (fx (4, "kSlope"), slope); r.set (fx (4, "kAtt"), 1.0f);
        return (double) peakOf (tail (r.render (0.6, Rig::noteOn ({ 48, 55, 60, 64 }, 1.0f)), 0.3));
    };
    const double plain = level (1.0f), over = level (-1.0f);
    check (over < plain * 0.3, "squash slope -1 (over-compression) turns the loud chord way down",
           juce::String (plain, 3) + " -> " + juce::String (over, 4));
}

void testPresets()
{
    std::cout << "presets" << std::endl;
    auto folder = SeedProcessor::presetFolder();
    const auto name = "zz-test-" + juce::String (juce::Random::getSystemRandom().nextInt (100000));
    Rig a;
    a.set (seed::ids::seed, 2.7f);
    a.set (seed::ids::noteOn (3), 0.0f);
    a.set (seed::ids::cutoff, 555.0f);
    a.set (seed::ids::filterMode, 1.0f);
    check (a.proc->savePreset (name), "save preset");
    const auto file = folder.getChildFile (name + ".seedpreset");

    Rig t; // Tuning only: seed + note edit come in, the filter stays as it was.
    t.set (seed::ids::cutoff, 3000.0f);
    t.proc->loadPreset (file, SeedProcessor::Scope::Tuning);
    auto val = [&t] (const juce::String& id) { auto* p = t.proc->apvts.getParameter (id); return p->convertFrom0to1 (p->getValue()); };
    check (std::abs (val (seed::ids::seed) - 2.7f) < 1e-3 && val (seed::ids::noteOn (3)) < 0.5f && std::abs (val (seed::ids::cutoff) - 3000.0f) < 1.0f,
           "load Tuning only: tuning + edits in, sound untouched");
    Rig s; // Sound only.
    s.proc->loadPreset (file, SeedProcessor::Scope::Sound);
    auto val2 = [&s] (const juce::String& id) { auto* p = s.proc->apvts.getParameter (id); return p->convertFrom0to1 (p->getValue()); };
    check (std::abs (val2 (seed::ids::seed) - 2.0f) < 1e-3 && std::abs (val2 (seed::ids::cutoff) - 555.0f) < 1.0f,
           "load Sound only: filter in, seed untouched");
    file.deleteFile();
}

/// v0.5 Scala: import / export round trip / fit seed / persistence.
void testScala()
{
    std::cout << "scala" << std::endl;
    const juce::String ji = "! test.scl\n!\nJust major pentatonic\n 5\n!\n 9/8\n 5/4 comment after the ratio\n 3/2\n 884.359\n 2/1\n";
    const auto parsed = xen::parseScl (ji.toStdString());
    // A value with a '.' is cents (Scala rule), without one it is a ratio.
    check (parsed.valid() && parsed.cents.size() == 5 && std::abs (parsed.cents[0] - 203.91) < 0.01
               && std::abs (parsed.cents[2] - 701.955) < 0.01 && std::abs (parsed.cents[3] - 884.359) < 1e-9 && std::abs (parsed.period() - 1200.0) < 1e-9,
           "parse .scl (ratios, comments, cents)", juce::String (parsed.cents.size()) + " notes, first " + juce::String (parsed.cents[0], 2));

    Rig r;
    check (! r.proc->importScl ("garbage"), "import rejects a file without notes");
    const juce::String bp = [] {
        juce::String t = "Bohlen-Pierce 13 ED3\n13\n";
        for (int i = 1; i <= 13; ++i) t << juce::String (1901.955 * i / 13.0, 6) << "\n";
        return t;
    }();
    check (r.proc->importScl (bp), "import Bohlen-Pierce");
    r.settleCalculator();
    auto* mp = r.proc->apvts.getParameter (seed::ids::method);
    check (std::lround (mp->convertFrom0to1 (mp->getValue())) == 2, "method switches to Imported");
    auto f = r.proc->finalScale();
    int found = 0;
    double worst = 0.0;
    for (int i = 0; i < f.count; ++i)
        if (f.notes[(size_t) i].kind == xen::ScaleNote::Found)
        {
            ++found;
            worst = std::max (worst, std::abs (f.notes[(size_t) i].cents - 1901.955 * found / 13.0));
        }
    check (found == 12 && std::abs (f.period - 1901.955) < 1e-6 && worst < 1e-6, "imported notes are played exactly",
           juce::String (found) + " notes, period " + juce::String (f.period, 3) + ", worst " + juce::String (worst, 9));

    // The keyboard plays them: Scale key mode, key 61 = degree 1 = 146.30 c above the Fundamental.
    r.set (seed::ids::keyMode, 2.0f);
    r.set (seed::ids::partials, 1.0f);
    r.settleCalculator();
    auto x = r.render (0.6, Rig::noteOn ({ 61 }));
    std::vector<float> tail (x.end() - 16384, x.end());
    const double want = 261.6255653 * std::exp2 (1901.955 / 13.0 / 1200.0);
    check (toneAmp (tail, want) > 5.0 * toneAmp (tail, 261.6255653 * std::exp2 (100.0 / 1200.0)), "key 61 plays BP degree 1",
           juce::String (want, 2) + " Hz");
    r.set (seed::ids::partials, 16.0f);
    r.render (0.2, Rig::noteOff (61));

    // Export round trip (a disabled note is left out of the file).
    r.set (seed::ids::noteOn (3), 0.0f);
    r.settleCalculator();
    const auto back = xen::parseScl (r.proc->exportScl().toStdString());
    check (back.valid() && back.cents.size() == 12 && std::abs (back.period() - 1901.955) < 1e-4
               && std::abs (back.cents[2] - 1901.955 * 4 / 13.0) < 1e-4,
           "export .scl round trip (disabled degree 3 left out)", juce::String (back.cents.size()) + " pitches");
    check (r.proc->exportKbm().contains ("\n12\n"), "export .kbm (12 degrees per period)");
    r.set (seed::ids::noteOn (3), 1.0f);

    // Fit: BP wants seed 3 (the tritave), 12-EDO wants seed 2.
    const double s3 = r.proc->fitSeedToCurrentScale();
    check (std::abs (s3 - 3.0) < 0.02, "fit seed to Bohlen-Pierce = 3", juce::String (s3, 4));
    juce::String edo = "12-EDO\n12\n";
    for (int i = 1; i <= 12; ++i) edo << juce::String (100.0 * i, 1) << "\n";
    r.proc->importScl (edo);
    r.settleCalculator();
    const double s2 = r.proc->fitSeedToCurrentScale();
    check (std::abs (s2 - 2.0) < 0.02, "fit seed to 12-EDO = 2", juce::String (s2, 4));

    // Persistence: the project remembers the imported scale.
    juce::MemoryBlock state;
    r.proc->getStateInformation (state);
    Rig q;
    q.proc->setStateInformation (state.getData(), (int) state.getSize());
    q.settleCalculator();
    check (q.proc->importedScaleName() == "12-EDO" && q.proc->finalScale().count == 13, "imported scale restored with the project",
           q.proc->importedScaleName() + ", " + juce::String (q.proc->finalScale().count) + " notes incl. period");
}

void snapshot (const juce::File& file)
{
    SeedProcessor proc;
    auto setp = [&proc] (const juce::String& id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    setp (seed::ids::seed, 2.5f);
    // Show some edits so the orange markers are visible in the snapshot.
    setp (seed::ids::noteOn (2), 0.0f);
    setp (seed::ids::noteOffset (4), 25.0f);
    setp (seed::ids::customNoteCents (1), 1450.0f); setp (seed::ids::customNoteOn (1), 1.0f);
    setp (seed::ids::partialLevel (3), -12.0f);
    setp (seed::ids::customPartialCents (1), 2100.0f); setp (seed::ids::customPartialOn (1), 1.0f);
    if (auto* tab = std::getenv ("SEED_TAB")) juce::ignoreUnused (tab);
    // Hold a note (Scale mode, key 64 = scale note 4) so the played-note highlight shows.
    setp (seed::ids::keyMode, 2.0f);
    proc.setPlayConfigDetails (0, 2, kSr, kBlock);
    proc.prepareToPlay (kSr, kBlock);
    {
        juce::AudioBuffer<float> buf (2, kBlock);
        for (int i = 0; i < 40; ++i)
        {
            juce::MidiBuffer m;
            if (i == 20) m.addEvent (juce::MidiMessage::noteOn (1, 64, 0.8f), 0);
            proc.pumpCalculator();
            proc.processBlock (buf, m);
            juce::Thread::sleep (5);
        }
    }
    if (auto* km = std::getenv ("SEED_KEYMODE")) setp (seed::ids::keyMode, (float) juce::String (km).getIntValue());
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    if (auto* tab = std::getenv ("SEED_TAB")) static_cast<SeedEditor*> (ed.get())->showTab (juce::String (tab).getIntValue());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (900); // calculator + animations settle
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::FileOutputStream out (file);
    out.setPosition (0); out.truncate();
    juce::PNGImageFormat().writeImageToStream (img, out);
    std::cout << "snapshot written: " << file.getFullPathName() << std::endl;
}
} // namespace

/// --bench: how much CPU a heavy patch costs (1.0 = one core fully busy in real time).
void bench()
{
    struct Case { const char* name; int voices; int partials; bool heavy; };
    for (auto c : { Case { "1 voice, 16 partials, plain", 1, 16, false }, Case { "8 voices, 16 partials, plain", 8, 16, false },
                    Case { "8 voices, 64 partials, filter+sends+FX", 8, 64, true }, Case { "16 voices, 64 partials, filter+sends+FX", 16, 64, true } })
    {
        Rig r;
        r.set (seed::ids::partials, (float) c.partials);
        if (c.heavy)
        {
            r.set (seed::ids::filterMode, 1.0f); r.set (seed::ids::fx (1, "mix"), 50.0f); r.set (seed::ids::fx (2, "mix"), 50.0f);
            r.set (seed::ids::fx (3, "mix"), 50.0f); r.set (seed::ids::spread, 80.0f); r.set (seed::ids::shimmer, 50.0f);
            r.set (seed::ids::fx (1, "dSpiral"), 300.0f);
        }
        juce::MidiBuffer m;
        for (int v = 0; v < c.voices; ++v) m.addEvent (juce::MidiMessage::noteOn (1, 40 + v * 3, 0.8f), 0);
        r.render (0.2, m);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        r.render (10.0);
        const double secs = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        std::cout << "  " << c.name << ": " << juce::String (100.0 * secs / 10.0, 1) << "% of one core" << std::endl;
    }
}

/// --release: prints the level in 25 ms windows after a note-off (debugging the release shape).
void releaseProbe()
{
    Rig r;
    r.set (seed::ids::keyMode, 2.0f);
    r.settleCalculator();
    r.render (0.5, Rig::noteOn ({ 60 }));
    auto x = r.render (0.8, Rig::noteOff (60));
    for (int w = 0; w < 32; ++w)
    {
        double e = 0; for (int i = 0; i < 1200; ++i) e += x[(size_t) (w * 1200 + i)] * x[(size_t) (w * 1200 + i)];
        std::cout << "  " << w * 25 << " ms: " << juce::String (10.0 * std::log10 (e / 1200.0 + 1e-20), 1) << " dB" << std::endl;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    for (int i = 1; i < argc; ++i)
        if (juce::String (argv[i]) == "--release") { releaseProbe(); return 0; }
    for (int i = 1; i < argc; ++i)
        if (juce::String (argv[i]) == "--bench") { bench(); return 0; }
    testMath();
    testSpectrum();
    testDynamic();
    testSafety();
    testKeys();
    testDrone();
    testState();
    testCurve();
    testVelocity();
    testEnvelope();
    testConsonance();
    testEdits();
    testKeyModes();
    testPitch();
    testChaosSafety();
    testPresets();
    testChaosToys();
    testV04();
    testV05();
    testScala();
    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--snapshot")
            snapshot (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]));
    std::cout << (failures == 0 ? "ALL PASSED" : juce::String (failures) + " FAILED") << std::endl;
    return failures;
}
