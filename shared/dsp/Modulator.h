#pragma once
// Modulator — one LFO / random source, output -1..+1, advanced once per audio block.
// Shapes: 0 Sine, 1 Triangle, 2 Saw, 3 Square, 4 S&H (new random value every cycle),
//         5 Smooth random (cosine glide between random values), 6 Drift (bounded random walk).
// The processor adds depth x output to a parameter's NORMALISED value (0..1) — the parameter
// itself isn't moved, so removing the modulation returns everything to the set value.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace pvst
{
class Modulator
{
public:
    void reset (uint32_t seed) { rng = seed * 2654435761u + 1u; phase = 0.0; a = rand(); b = rand(); value = 0.0; drift = 0.0; }

    /// Advance by dt seconds at `rateHz`; returns the new output (-1..1).
    double advance (int shape, double rateHz, double dt)
    {
        phase += rateHz * dt;
        const bool wrapped = phase >= 1.0;
        phase -= std::floor (phase);
        if (wrapped) { a = b; b = rand(); if (shape == 4) held = rand(); }
        switch (shape)
        {
            case 0: value = std::sin (2.0 * 3.141592653589793 * phase); break;
            case 1: value = 1.0 - 4.0 * std::abs (phase - 0.5); break;
            case 2: value = 2.0 * phase - 1.0; break;
            case 3: value = phase < 0.5 ? 1.0 : -1.0; break;
            case 4: value = held; break;
            case 5: { const double t = 0.5 - 0.5 * std::cos (3.141592653589793 * phase); value = a + (b - a) * t; break; }
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

private:
    double rand()
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (double) (rng & 0xffffff) / 8388607.5 - 1.0;
    }
    uint32_t rng = 1;
    double phase = 0.0, a = 0.0, b = 0.0, held = 0.0, value = 0.0, drift = 0.0;
};
} // namespace pvst
