#pragma once
// PartialDelay — a ping-pong delay meant to be fed only SOME partials of a sound (the synth's send
// bus picks them: odd, even, high half...). Creative twist: SPIRAL re-pitches every repeat by
// `spiralCents` (a two-tap "doppler" pitch shifter inside the feedback loop), so echoes climb or
// fall like a barber-pole.
//
// Safety: feedback is capped (95%) and soft-clipped (tanh) inside the loop, so even extreme
// settings can't run away; the plugin's SafetyStage is still the final guard.
// Changing the delay time glides the read head (tape-style), never clicks.
#include <cmath>
#include <vector>

namespace pvst
{
class PartialDelay
{
public:
    struct Params
    {
        double timeMs = 350.0, feedback = 0.45, spiralCents = 0.0, level = 1.0;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = (int) (sr * 2.6) + 8;
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        write = 0;
        delaySm = 0.35 * sr;
        phase = 0.0;
        timeCoef = std::exp (-1.0 / (0.08 * sr));
    }

    void reset() { std::fill (bufL.begin(), bufL.end(), 0.0f); std::fill (bufR.begin(), bufR.end(), 0.0f); }

    /// Mono in, ADDS stereo out.
    void process (const float* in, float* outL, float* outR, int n, const Params& p)
    {
        const double targetDelay = std::clamp (p.timeMs * 0.001 * sr, 1.0, (double) size - kWindow - 4.0);
        const double ratio = std::exp2 (p.spiralCents / 1200.0);
        const bool shifting = std::abs (p.spiralCents) > 0.05;
        const float fb = (float) std::clamp (p.feedback, 0.0, 0.95);
        const float lvl = (float) p.level;
        for (int i = 0; i < n; ++i)
        {
            delaySm = targetDelay + (delaySm - targetDelay) * timeCoef;
            float rl, rr;
            if (shifting)
            {
                phase += (1.0 - ratio) / kWindow;
                phase -= std::floor (phase);
                const double p2 = phase + 0.5 - std::floor (phase + 0.5);
                const float g1 = (float) (1.0 - std::abs (2.0 * phase - 1.0));
                const float g2 = (float) (1.0 - std::abs (2.0 * p2 - 1.0));
                rl = g1 * read (bufL, delaySm + phase * kWindow) + g2 * read (bufL, delaySm + p2 * kWindow);
                rr = g1 * read (bufR, delaySm + phase * kWindow) + g2 * read (bufR, delaySm + p2 * kWindow);
            }
            else
            {
                rl = read (bufL, delaySm);
                rr = read (bufR, delaySm);
            }
            // Ping-pong: input enters left, repeats bounce L -> R -> L.
            bufL[(size_t) write] = std::tanh (in[i] + fb * rr);
            bufR[(size_t) write] = std::tanh (fb * rl);
            write = (write + 1) % size;
            outL[i] += lvl * rl;
            outR[i] += lvl * rr;
        }
    }

private:
    static constexpr double kWindow = 2400.0; // pitch-shifter grain, samples

    float read (const std::vector<float>& b, double delay) const
    {
        double pos = (double) write - delay;
        while (pos < 0) pos += size;
        const int i0 = (int) pos;
        const float f = (float) (pos - i0);
        const int i1 = (i0 + 1) % size;
        return b[(size_t) i0] + f * (b[(size_t) i1] - b[(size_t) i0]);
    }

    double sr = 48000.0, delaySm = 0.0, phase = 0.0, timeCoef = 0.999;
    std::vector<float> bufL, bufR;
    int size = 1, write = 0;
};
} // namespace pvst
