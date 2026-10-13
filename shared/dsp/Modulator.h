#pragma once
// Modulator — one LFO / random source, output -1..+1, advanced once per audio block.
// Shapes: 0 Sine, 1 Triangle, 2 Saw, 3 Square, 4 S&H (new random value every cycle),
//         5 Smooth random (cosine glide between random values), 6 Drift (bounded random walk),
//         7 Drawn (v0.7: a hand-drawn breakpoint curve, see LfoShape).
// The processor adds depth x output to a parameter's NORMALISED value (0..1) — the parameter
// itself isn't moved, so removing the modulation returns everything to the set value.
//
// Triggering (v0.7): the processor can restart() a modulator (on a MIDI note-on, or when an audio
// level crosses a threshold); `loop = false` makes it a one-shot (plays one cycle, then holds).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace pvst
{
/// A drawn LFO cycle: breakpoints (x 0..1 left to right, y -1..+1) joined by segments whose bend
/// c (-1..+1) curves them: 0 = straight line, +1 = slow start / fast finish, -1 = the opposite.
/// The first point sits at x = 0 and the last at x = 1 (only their heights move).
struct LfoShape
{
    static constexpr int kMaxPoints = 64;
    int n = 0;
    float x[kMaxPoints] {}, y[kMaxPoints] {}, c[kMaxPoints] {}; // c[i] bends the segment from point i to i+1

    LfoShape() { preset (0); }

    /// Value at phase 0..1.
    float eval (double phase) const
    {
        if (n < 2) return 0.0f;
        const float p = (float) std::clamp (phase, 0.0, 1.0);
        int i = 0;
        while (i < n - 2 && p >= x[i + 1]) ++i;
        const float span = std::max (1e-6f, x[i + 1] - x[i]);
        float t = std::clamp ((p - x[i]) / span, 0.0f, 1.0f);
        if (std::abs (c[i]) > 1e-4f) t = std::pow (t, std::pow (4.0f, c[i]));
        return y[i] + (y[i + 1] - y[i]) * t;
    }

    /// 0 Sine, 1 Triangle, 2 Saw up, 3 Saw down, 4 Square, 5 Pulse 25 %, 6 Ramp in (ease), 7 Steps (stairs), 8 Bounce.
    void preset (int which)
    {
        n = 0;
        auto add = [this] (float px, float py, float pc = 0.0f) { if (n < kMaxPoints) { x[n] = px; y[n] = py; c[n] = pc; ++n; } };
        switch (which)
        {
            case 1: add (0, -1); add (0.5f, 1); add (1, -1); break;
            case 2: add (0, -1); add (1, 1); break;
            case 3: add (0, 1); add (1, -1); break;
            case 4: add (0, 1); add (0.4999f, 1); add (0.5f, -1); add (1, -1); break;
            case 5: add (0, 1); add (0.2499f, 1); add (0.25f, -1); add (1, -1); break;
            case 6: add (0, -1, 1.0f); add (1, 1); break;
            case 7: for (int k = 0; k < 4; ++k) { add (k * 0.25f, -1.0f + 2.0f * k / 3.0f); add ((k + 1) * 0.25f - 0.0001f, -1.0f + 2.0f * k / 3.0f); } x[n - 1] = 1.0f; break;
            case 8: add (0, -1); add (0.25f, 1, -0.6f); add (0.5f, -1, 0.6f); add (0.75f, 1, -0.6f); add (1, -1, 0.6f); break;
            default: // sine, 16 segments
                for (int k = 0; k <= 16; ++k) add ((float) k / 16.0f, std::sin (6.2831853f * (float) k / 16.0f));
                break;
        }
    }

    /// Keeps the points valid: first x = 0, last x = 1, x strictly rising, y in range, c in range.
    void sanitize()
    {
        n = std::clamp (n, 2, kMaxPoints);
        x[0] = 0.0f; x[n - 1] = 1.0f;
        for (int i = 1; i < n - 1; ++i) x[i] = std::clamp (x[i], x[i - 1] + 1e-4f, 1.0f - 1e-4f * (float) (n - 1 - i));
        for (int i = 0; i < n; ++i) { y[i] = std::clamp (y[i], -1.0f, 1.0f); c[i] = std::clamp (c[i], -1.0f, 1.0f); }
    }

    /// "x,y,c;x,y,c;..." (for the plugin state and presets).
    std::string toText() const
    {
        std::string s;
        char b[64];
        for (int i = 0; i < n; ++i)
        {
            std::snprintf (b, sizeof b, "%s%.5f,%.5f,%.4f", i ? ";" : "", (double) x[i], (double) y[i], (double) c[i]);
            s += b;
        }
        return s;
    }
    bool fromText (const std::string& s)
    {
        LfoShape t;
        t.n = 0;
        size_t pos = 0;
        while (pos < s.size() && t.n < kMaxPoints)
        {
            const size_t end = s.find (';', pos);
            const std::string part = s.substr (pos, end == std::string::npos ? std::string::npos : end - pos);
            float px, py, pc;
            if (std::sscanf (part.c_str(), "%f,%f,%f", &px, &py, &pc) == 3) { t.x[t.n] = px; t.y[t.n] = py; t.c[t.n] = pc; ++t.n; }
            if (end == std::string::npos) break;
            pos = end + 1;
        }
        if (t.n < 2) return false;
        t.sanitize();
        *this = t;
        return true;
    }
};

class Modulator
{
public:
    static constexpr int kDrawn = 7;

    void reset (uint32_t seed) { rng = seed * 2654435761u + 1u; phase = 0.0; a = rand(); b = rand(); value = 0.0; drift = 0.0; finished = false; }

    /// Starts the cycle again from the beginning (MIDI / audio trigger). Random shapes draw fresh values.
    void restart()
    {
        phase = 0.0; finished = false;
        a = b; b = rand(); held = rand();
    }

    /// Advance by dt seconds at `rateHz`; returns the new output (-1..1). `loop = false`: one cycle, then hold.
    double advance (int shape, double rateHz, double dt, const LfoShape* drawn = nullptr, bool loop = true)
    {
        if (finished) return value;
        phase += rateHz * dt;
        const bool wrapped = phase >= 1.0;
        if (wrapped && ! loop) { finished = true; phase = 0.999999; }
        else phase -= std::floor (phase);
        if (wrapped && loop) { a = b; b = rand(); if (shape == 4) held = rand(); }
        switch (shape)
        {
            case 0: value = std::sin (2.0 * 3.141592653589793 * phase); break;
            case 1: value = 1.0 - 4.0 * std::abs (phase - 0.5); break;
            case 2: value = 2.0 * phase - 1.0; break;
            case 3: value = phase < 0.5 ? 1.0 : -1.0; break;
            case 4: value = held; break;
            case 5: { const double t = 0.5 - 0.5 * std::cos (3.141592653589793 * phase); value = a + (b - a) * t; break; }
            case kDrawn: value = drawn != nullptr ? (double) drawn->eval (phase) : 0.0; break;
            default:
            {
                // Brownian drift: steps scale with rate; soft walls keep it inside -1..1.
                drift += rand() * std::sqrt (std::max (1e-6, rateHz * dt)) * 0.9 - drift * 0.2 * rateHz * dt;
                drift = std::clamp (drift, -1.0, 1.0);
                value = drift;
            }
        }
        return value;
    }

    double current() const { return value; }
    double getPhase() const { return phase; }

private:
    double rand()
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (double) (rng & 0xffffff) / 8388607.5 - 1.0;
    }
    uint32_t rng = 1;
    double phase = 0.0, a = 0.0, b = 0.0, held = 0.0, value = 0.0, drift = 0.0;
    bool finished = false;
};
} // namespace pvst
