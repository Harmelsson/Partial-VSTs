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
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "xen/Stretch.h"

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

    Rig() { proc->setPlayConfigDetails (0, 2, kSr, kBlock); proc->prepareToPlay (kSr, kBlock); }

    void set (const char* id, float value)
    {
        auto* p = proc->apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
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

    r.set (seed::ids::keysFollow, 1.0f);
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
    check (peakOf (silence) == 0.0f, "no notes = exact silence");
}

void testKeys()
{
    std::cout << "keys follow seed" << std::endl;
    Rig r;
    r.set (seed::ids::seed, 2.5f);
    r.set (seed::ids::keysFollow, 1.0f);
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
    check (std::abs (s - 2.7182f) < 1e-4f && p == 33.0f, "save + load restores parameters",
           "seed " + juce::String (s, 4) + ", partials " + juce::String (p));
}

void snapshot (const juce::File& file)
{
    SeedProcessor proc;
    proc.apvts.getParameter (seed::ids::seed)->setValueNotifyingHost (proc.apvts.getParameter (seed::ids::seed)->convertTo0to1 (2.5f));
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (600); // let the animations settle
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::FileOutputStream out (file);
    out.setPosition (0); out.truncate();
    juce::PNGImageFormat().writeImageToStream (img, out);
    std::cout << "snapshot written: " << file.getFullPathName() << std::endl;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    testMath();
    testSpectrum();
    testDynamic();
    testSafety();
    testKeys();
    testDrone();
    testState();
    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String (argv[i]) == "--snapshot")
            snapshot (juce::File::getCurrentWorkingDirectory().getChildFile (argv[i + 1]));
    std::cout << (failures == 0 ? "ALL PASSED" : juce::String (failures) + " FAILED") << std::endl;
    return failures;
}
