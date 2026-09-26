#pragma once
// RoughReverb — a reverb that works in UNCOMFORTABLE ways (user's brief).
//
// Instead of a smooth diffuse room, the tail is a bank of comb resonators tuned to the CURRENT
// SCALE's notes (so the room "rings in the tuning"). Every resonator has a TWIN, detuned by
// `roughness` x the distance where two sines clash the most (the peak of the Plomp-Levelt
// roughness curve that the calculator's dissonance method uses). Roughness 0 = twins in unison
// (a pure, eerie ring); 100% = every note paired with its most dissonant neighbour. Twins sit on
// the OPPOSITE ear from their partner, so the beating also happens between the ears.
// Retuning (new seed, new notes) glides the resonators instead of clicking.
//
// Safety: comb gains stay < 1 (set from the decay time), feedback is damped by a one-pole
// low-pass, and the input is scaled down by the number of resonators.
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace pvst
{
class RoughReverb
{
public:
    static constexpr int kMaxNotes = 8;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& c : combs) { c.buf.assign (kBuf, 0.0f); c.write = 0; c.delay = c.target = sr / 220.0; c.lp = 0.0f; }
        glideCoef = std::exp (-1.0 / (0.15 * sr));
    }

    void reset() { for (auto& c : combs) { std::fill (c.buf.begin(), c.buf.end(), 0.0f); c.lp = 0.0f; } }

    /// Sets the note frequencies (Hz). `roughness` 0..1.
    void setTuning (const double* hz, int count, double roughness)
    {
        count = std::clamp (count, 0, kMaxNotes);
        active = count * 2;
        for (int i = 0; i < count; ++i)
        {
            const double f = std::clamp (hz[i], 30.0, 4000.0);
            // Peak roughness distance: x* = ln(B2/B1)/(B2-B1) = 0.2204 in Sethares' units,
            // divided by the band scale 0.24/(0.0207 f + 18.96).
            const double clash = 0.2204 * (0.0207 * f + 18.96) / 0.24;
            combs[(size_t) (2 * i)].target = sr / f;
            combs[(size_t) (2 * i + 1)].target = sr / (f + roughness * clash);
        }
    }

    /// Mono in, ADDS stereo out.
    void process (const float* in, float* outL, float* outR, int n, double decaySec, double level)
    {
        if (active == 0) return;
        const float inScale = 0.6f / (float) active;
        const float lvl = (float) level;
        const double t60 = std::max (0.05, decaySec);
        for (int k = 0; k < active; ++k)
        {
            auto& c = combs[(size_t) k];
            // Even resonators left, odd (the twins) right: partners beat across the ears.
            float* out = (k % 2 == 0) ? outL : outR;
            for (int i = 0; i < n; ++i)
            {
                c.delay = c.target + (c.delay - c.target) * glideCoef;
                const float g = (float) std::pow (0.001, c.delay / (sr * t60));
                double pos = (double) c.write - c.delay;
                while (pos < 0) pos += kBuf;
                const int i0 = (int) pos;
                const float fr = (float) (pos - i0);
                const float y = c.buf[(size_t) i0] + fr * (c.buf[(size_t) ((i0 + 1) % kBuf)] - c.buf[(size_t) i0]);
                c.lp += 0.35f * (y - c.lp); // damping: highs die first, like a real room
                c.buf[(size_t) c.write] = in[i] * inScale + g * c.lp;
                c.write = (c.write + 1) % kBuf;
                out[i] += lvl * y;
            }
        }
    }

private:
    static constexpr int kBuf = 8192;
    struct Comb
    {
        std::vector<float> buf;
        int write = 0;
        double delay = 200.0, target = 200.0;
        float lp = 0.0f;
    };
    std::array<Comb, kMaxNotes * 2> combs;
    int active = 0;
    double sr = 48000.0, glideCoef = 0.999;
};
} // namespace pvst
