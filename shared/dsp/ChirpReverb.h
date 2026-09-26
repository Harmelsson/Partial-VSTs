#pragma once
// ChirpReverb — a small "spring tank" reverb with a character knob (v0.5, user asked for other,
// characteristic reverb types: "experimental, something characteristic, but just simple").
//
// How it works: each channel is a feedback delay loop (length = Size) with a chain of 16 first-order
// ALL-PASS filters inside it. An all-pass passes every frequency at full level but delays different
// frequencies by different amounts ("dispersion"): high frequencies come out later than low ones.
// A click fed into the loop therefore comes back as a falling "pew" (a chirp), and every trip round
// the loop smears it more — the "boing" of a spring reverb. CHIRP sets how strong the dispersion is
// (0 = plain echoey tank, 1 = cartoon spring). The two channels cross-feed a little for width.
// Safety: loop gain < 1 from the decay time, damping low-pass, tanh in the loop.
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace pvst
{
class ChirpReverb
{
public:
    struct Params
    {
        double decaySec = 2.5, chirp = 0.6, sizeMs = 60.0, damping = 0.4;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& ch : chans) { ch.buf.assign ((size_t) (sr * 0.35) + 8, 0.0f); ch.write = 0; ch.ap.fill ({}); ch.lp = 0.0f; }
        sizeSm = 60.0;
    }

    void reset() { for (auto& ch : chans) { std::fill (ch.buf.begin(), ch.buf.end(), 0.0f); ch.ap.fill ({}); ch.lp = 0.0f; } }

    void process (const float* inL, const float* inR, float* wetL, float* wetR, int n, const Params& p)
    {
        const float a = (float) std::clamp (p.chirp * 0.85, 0.0, 0.85); // all-pass coefficient
        const float damp = (float) (0.95 - 0.8 * std::clamp (p.damping, 0.0, 1.0));
        sizeSm += (std::clamp (p.sizeMs, 10.0, 300.0) - sizeSm) * 0.2; // per block glide
        const float delay = (float) (sizeSm * 0.001 * sr);
        // Loop gain for the decay time: -60 dB after decaySec.
        const float g = (float) std::pow (0.001, (delay / sr) / std::max (0.05, p.decaySec));
        const float* ins[2] = { inL, inR };
        float* outs[2] = { wetL, wetR };
        for (int i = 0; i < n; ++i)
        {
            float y[2];
            for (int c = 0; c < 2; ++c)
            {
                auto& ch = chans[(size_t) c];
                const float d = delay * (c == 0 ? 1.0f : 1.13f); // slightly different sizes = width
                double pos = (double) ch.write - d;
                const int sz = (int) ch.buf.size();
                while (pos < 0) pos += sz;
                const int i0 = (int) pos;
                const float fr = (float) (pos - i0);
                y[c] = ch.buf[(size_t) i0] + fr * (ch.buf[(size_t) ((i0 + 1) % sz)] - ch.buf[(size_t) i0]);
            }
            for (int c = 0; c < 2; ++c)
            {
                auto& ch = chans[(size_t) c];
                // Input + feedback (with a little cross-feed) through the dispersive all-pass chain.
                float x = 0.35f * ins[c][i] + g * (0.85f * y[c] + 0.15f * y[1 - c]);
                for (auto& s : ch.ap)
                {
                    const float out = -a * x + s.x1 + a * s.y1;
                    s.x1 = x; s.y1 = out;
                    x = out;
                }
                ch.lp += damp * (x - ch.lp);
                ch.buf[(size_t) ch.write] = std::tanh (ch.lp);
                ch.write = (ch.write + 1) % (int) ch.buf.size();
                outs[c][i] = y[c];
            }
        }
    }

private:
    struct AP { float x1 = 0.0f, y1 = 0.0f; };
    struct Chan
    {
        std::vector<float> buf;
        int write = 0;
        std::array<AP, 16> ap {};
        float lp = 0.0f;
    };
    std::array<Chan, 2> chans;
    double sr = 48000.0, sizeSm = 60.0;
};

/// SquashCompressor — a stereo-linked compressor whose SLOPE goes past "limiter" into
/// OVER-compression (v0.5, "compressors ... something characteristic").
///   slope 1   = no compression        slope 0.25 = 4:1        slope 0 = limiter (∞:1)
///   slope < 0 = over-compression: the louder the input goes above the threshold, the QUIETER the
///               output gets. Loud hits duck themselves; the sound "breathes backwards".
/// Output = input x gain (the processor's Mix knob blends it with the dry signal = parallel
/// compression).
class SquashCompressor
{
public:
    struct Params
    {
        double thresholdDb = -18.0, slope = 0.25, attackMs = 10.0, releaseMs = 150.0, makeupDb = 0.0;
    };

    void prepare (double sampleRate) { sr = sampleRate; env = 0.0f; }
    void reset() { env = 0.0f; }

    /// Latest gain reduction in dB (for a meter).
    float gainReductionDb() const { return grDb; }

    void process (const float* inL, const float* inR, float* wetL, float* wetR, int n, const Params& p)
    {
        const float att = (float) std::exp (-1.0 / (std::max (0.05, p.attackMs) * 0.001 * sr));
        const float rel = (float) std::exp (-1.0 / (std::max (1.0, p.releaseMs) * 0.001 * sr));
        const float thr = (float) p.thresholdDb, slope = (float) std::clamp (p.slope, -1.0, 1.0);
        const float makeup = (float) std::pow (10.0, p.makeupDb / 20.0);
        if (slope >= 0.999f) // transparent: skip the per-sample maths (just makeup)
        {
            for (int i = 0; i < n; ++i) { wetL[i] = inL[i] * makeup; wetR[i] = inR[i] * makeup; }
            grDb = 0.0f;
            return;
        }
        float gr = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float peak = std::max (std::abs (inL[i]), std::abs (inR[i]));
            env = peak > env ? peak + (env - peak) * att : peak + (env - peak) * rel;
            const float levelDb = 20.0f * std::log10 (std::max (env, 1e-6f));
            const float over = levelDb - thr;
            const float gainDb = over > 0.0f ? over * (slope - 1.0f) : 0.0f; // <= 0
            gr = std::min (gr, gainDb);
            const float gain = std::pow (10.0f, std::max (gainDb, -60.0f) / 20.0f) * makeup;
            wetL[i] = inL[i] * gain;
            wetR[i] = inR[i] * gain;
        }
        grDb = gr;
    }

private:
    double sr = 48000.0;
    float env = 0.0f, grDb = 0.0f;
};
} // namespace pvst
