#pragma once
// Eq.h — maths for Seed's 5-band EQ (plain C++, no JUCE: the engine, the UI's response curve and
// the tests all use the same code, so the picture is the truth).
//
// Every band is one or more "SVF sections": Andrew Simper's (Cytomic) linear-trapezoidal
// state-variable filter. Why this one: it stays stable and click-free when its coefficients change
// every few samples (key tracking, modulation, dragging a node while notes ring), which is exactly
// what a per-voice, key-tracked EQ does. One section computes
//     out = m0 * in + m1 * bandpass + m2 * lowpass
// and the mixing constants (m0, m1, m2) turn it into bell / shelf / cut / notch / band-pass.
//
// STEEPNESS: Low cut and High cut are Butterworth filters of 1..4 cascaded sections
// (12 / 24 / 36 / 48 dB per octave). The user's Q multiplies the resonance of the highest-Q
// section (Q = 0.707 with one section is the classic flat response). For the other types
// steepness has no meaning and is ignored.
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

namespace pvst::eq
{
enum Type { Bell = 0, LowShelf, HighShelf, LowCut, HighCut, Notch, BandPass, kNumTypes };
inline constexpr const char* kTypeNames[kNumTypes] = { "Bell", "Low shelf", "High shelf", "Low cut", "High cut", "Notch", "Band-pass" };
inline constexpr int kMaxSections = 4;
inline constexpr double kPi = 3.141592653589793;

inline bool usesGain (int type) { return type == Bell || type == LowShelf || type == HighShelf; }
inline bool usesSteepness (int type) { return type == LowCut || type == HighCut; }

/// What the user sets for one band.
struct Band
{
    bool on = false;
    int type = Bell;
    double freq = 1000.0;  // Hz (centre / corner), before key tracking
    double gainDb = 0.0;   // bell + shelves
    double q = 0.7071;
    int steep = 1;         // 1..4 sections (cuts only)
    /// Bell/shelf at ~0 dB does nothing: skipping it saves CPU.
    bool neutral() const { return usesGain (type) && std::abs (gainDb) < 0.01; }
    bool active() const { return on && ! neutral(); }
};

/// One state-variable section's coefficients.
struct Section
{
    float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, m0 = 1.0f, m1 = 0.0f, m2 = 0.0f;
    double g = 0.0, k = 1.0, dm0 = 1.0, dm1 = 0.0, dm2 = 0.0; // double copies, for the response curve
};

struct Coefs
{
    int n = 0;
    std::array<Section, kMaxSections> s {};
};

struct State { float ic1 = 0.0f, ic2 = 0.0f; };

inline Section makeSection (int type, double fc, double q, double gainDb, double sr)
{
    fc = std::clamp (fc, 10.0, 0.49 * sr);
    q = std::clamp (q, 0.1, 30.0);
    const double A = std::pow (10.0, gainDb / 40.0);
    double g = std::tan (kPi * fc / sr), k = 1.0 / q, m0 = 1.0, m1 = 0.0, m2 = 0.0;
    switch (type)
    {
        case Bell:      k = 1.0 / (q * A); m1 = k * (A * A - 1.0); break;
        case LowShelf:  g /= std::sqrt (A); m1 = k * (A - 1.0); m2 = A * A - 1.0; break;
        case HighShelf: g *= std::sqrt (A); m0 = A * A; m1 = k * (1.0 - A) * A; m2 = 1.0 - A * A; break;
        case LowCut:    m1 = -k; m2 = -1.0; break;
        case HighCut:   m0 = 0.0; m2 = 1.0; break;
        case Notch:     m1 = -k; break;
        default:        m0 = 0.0; m1 = k; break; // band-pass, unity gain at the centre
    }
    Section s;
    const double a1 = 1.0 / (1.0 + g * (g + k));
    s.a1 = (float) a1; s.a2 = (float) (g * a1); s.a3 = (float) (g * g * a1);
    s.m0 = (float) m0; s.m1 = (float) m1; s.m2 = (float) m2;
    s.g = g; s.k = k; s.dm0 = m0; s.dm1 = m1; s.dm2 = m2;
    return s;
}

/// All sections of one band at frequency `freq` (already key-tracked). n = 0 if the band does nothing.
inline Coefs makeCoefs (const Band& b, double freq, double sr)
{
    Coefs c;
    if (! b.active()) return c;
    if (usesSteepness (b.type))
    {
        const int n = std::clamp (b.steep, 1, kMaxSections);
        for (int i = 0; i < n; ++i)
        {
            // Butterworth: section i has Q = 1 / (2 cos((2i+1) pi / 4n)); the user's Q scales the sharpest.
            double q = 1.0 / (2.0 * std::cos ((2.0 * i + 1.0) * kPi / (4.0 * n)));
            if (i == n - 1) q *= b.q / 0.7071;
            c.s[(size_t) i] = makeSection (b.type, freq, q, 0.0, sr);
        }
        c.n = n;
    }
    else
    {
        c.s[0] = makeSection (b.type, freq, b.q, b.gainDb, sr);
        c.n = 1;
    }
    return c;
}

inline float tick (const Section& c, State& st, float x)
{
    const float v3 = x - st.ic2;
    const float v1 = c.a1 * st.ic1 + c.a2 * v3;
    const float v2 = st.ic2 + c.a2 * st.ic1 + c.a3 * v3;
    st.ic1 = 2.0f * v1 - st.ic1;
    st.ic2 = 2.0f * v2 - st.ic2;
    return c.m0 * x + c.m1 * v1 + c.m2 * v2;
}

/// Linear gain of a band's sections at frequency f (for drawing the curve).
inline double magnitude (const Coefs& c, double f, double sr)
{
    double mag = 1.0;
    const double t = std::tan (kPi * std::clamp (f, 1.0, 0.499 * sr) / sr);
    for (int i = 0; i < c.n; ++i)
    {
        const auto& s = c.s[(size_t) i];
        const std::complex<double> j (0.0, t / s.g);          // the analog prototype's "s" at this frequency
        const auto d = j * j + s.k * j + 1.0;
        mag *= std::abs (s.dm0 + (s.dm1 * j + s.dm2) / d);
    }
    return mag;
}

/// Where key tracking puts a band: freq * 2^(track * cents / 1200). `cents` = the note's distance
/// above the Fundamental (the unison), so the band sits at its set frequency on the base key.
inline double trackedFreq (double freq, double track, double cents) { return freq * std::exp2 (track * cents / 1200.0); }
} // namespace pvst::eq
