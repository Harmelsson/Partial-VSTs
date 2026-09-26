#pragma once
// SeedEngine — the sound of Seed. Plain C++ (no JUCE), so tools/ can test it headlessly.
//
// WHAT IT IS
//   An additive synth: every voice is a stack of up to 64 sine partials placed at n^S times the
//   note's frequency (S = log2(seed), see shared/xen/Stretch.h). Seed 2 = ordinary harmonics
//   (sounds like a soft sawtooth); other seeds stretch/squeeze the whole series.
//
// "EVERYTHING IS DYNAMIC" (the user's core rule)
//   Every parameter is re-read continuously and applied to notes that are ALREADY sounding.
//   Scrub the seed while holding a chord and every partial of every voice glides live.
//   How that stays click-free:
//     - Oscillators are complex "rotators" (a unit vector rotated by the partial's frequency each
//       sample). Changing frequency only changes the rotation step, never the current position,
//       so the waveform is always phase-continuous. They're also cheap: 4 mul + 2 add per sample.
//     - Parameters are smoothed (one-pole, ~30 ms) at the CONTROL RATE: every 32 samples we
//       recompute all partial frequencies/amplitudes, and amplitudes ramp linearly in between.
//     - Partials never switch on/off abruptly: count changes, the Nyquist limit and the
//       sub-audio limit all fade amplitudes instead.
//
// LOUDNESS
//   Each voice is normalised by 1/sqrt(sum of amp^2), i.e. constant *power* whatever the partial
//   count/rolloff, so scrubbing those doesn't make volume jump around. Coherent cases (seed 1:
//   every partial on the same pitch) can still peak high — the SafetyStage catches that.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "xen/Stretch.h"

namespace seed
{
constexpr int kMaxPartials = 64;
constexpr int kMaxVoices = 16;
constexpr int kControlBlock = 32; // samples between control-rate updates

/// All user-facing settings. The processor copies the plugin parameters in here every block.
struct EngineParams
{
    double seed = 2.0;           // 0.5 .. 4 (below 1 = undertones, 1 = collapse)
    int partials = 16;           // 1 .. 64
    double rolloff = 1.0;        // 0 .. 3, partial n loudness = 1/n^rolloff
    double attackMs = 8.0;
    double decayMs = 600.0;
    double sustain = 0.6;        // 0..1
    double releaseMs = 400.0;
    double velocitySens = 1.0;   // 0 = every note equally loud, 1 = volume follows velocity fully
    double gainDb = -12.0;
    double fundamentalHz = 261.6255653005986; // key 60's pitch when keys follow the seed
    double keysFollowSeed = 0.0; // 0 = normal 12-EDO keys, 1 = 12 keys span the seed's "octave"
    double pitchBendCents = 0.0;
};

class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        // One-pole smoothing coefficient per control tick for a ~30 ms time constant.
        smoothCoef = std::exp (-kControlBlock / (0.030 * sr));
        for (int n = 1; n <= kMaxPartials; ++n)
            lnN[(size_t) n - 1] = std::log ((double) n);
        smoothed = target;
        for (auto& v : voices)
            v = Voice {};
        tickCountdown = 0;
    }

    void setParams (const EngineParams& p) { target = p; }

    // ---- MIDI -------------------------------------------------------------------------------
    void noteOn (int note, float velocity)
    {
        Voice* v = findVoiceForNote (note);   // same key again: re-use its voice (no doubling)
        if (v == nullptr) v = findFreeVoice();
        if (v == nullptr) v = stealVoice();

        const bool wasSilent = ! v->active;
        v->fresh = wasSilent;
        v->note = note;
        v->velocity = std::clamp (velocity, 0.0f, 1.0f);
        v->stage = Voice::Attack;
        v->active = true;
        v->age = ++ageCounter;
        if (wasSilent)
        {
            // Fresh voice: all partials start at phase 0 and amplitude 0 (ramps up at next tick).
            v->env = 0.0f;
            for (int p = 0; p < kMaxPartials; ++p)
            {
                v->re[(size_t) p] = 1.0f; v->im[(size_t) p] = 0.0f;
                v->amp[(size_t) p] = 0.0f; v->ampStep[(size_t) p] = 0.0f;
                v->ampSmoothed[(size_t) p] = 0.0f;
            }
        }
        // A stolen/retriggered voice keeps its phases and current level: the attack continues
        // from where the envelope is, so there is no click.
        tickCountdown = 0; // compute this voice's partials before rendering its first sample
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.active && v.note == note && v.stage != Voice::Release)
                v.stage = Voice::Release;
    }

    void allNotesOff()
    {
        for (auto& v : voices)
            if (v.active) v.stage = Voice::Release;
    }

    int activeVoiceCount() const
    {
        int c = 0;
        for (auto& v : voices) c += v.active ? 1 : 0;
        return c;
    }

    // ---- Audio ------------------------------------------------------------------------------
    /// Adds `numSamples` of mono output to `out` (caller clears it first).
    void render (float* out, int numSamples)
    {
        int i = 0;
        while (i < numSamples)
        {
            if (tickCountdown <= 0)
            {
                controlTick();
                tickCountdown = kControlBlock;
            }
            const int chunk = std::min (tickCountdown, numSamples - i);
            for (auto& v : voices)
                if (v.active)
                    renderVoice (v, out + i, chunk);
            tickCountdown -= chunk;
            i += chunk;
        }
    }

    /// Current smoothed seed (what the audio is actually using right now).
    double currentSeed() const { return smoothed.seed; }

private:
    struct Voice
    {
        // Decay also covers sustain: the level keeps gliding towards the (live) sustain level.
        enum Stage { Attack, Decay, Release };
        bool active = false;
        int note = -1;
        float velocity = 1.0f;  // raw MIDI velocity 0..1
        float velGain = 1.0f;   // velocity after the sensitivity curve (control rate)
        float env = 0.0f;
        Stage stage = Attack;
        uint64_t age = 0;
        int numActive = 0; // partials [0, numActive) have non-zero amplitude somewhere this tick
        bool fresh = false; // first tick after a note-on from silence: skip amp smoothing
        alignas (32) std::array<float, kMaxPartials> re {}, im {}, c {}, s {};
        alignas (32) std::array<float, kMaxPartials> amp {}, ampStep {}, ampSmoothed {};
    };

    static double smoothTowards (double current, double tgt, double coef)
    {
        const double next = tgt + (current - tgt) * coef;
        // Snap when practically there: exact targets matter (e.g. sustain 0 must reach 0).
        return std::abs (next - tgt) < 1e-7 * std::max (1.0, std::abs (tgt)) ? tgt : next;
    }

    void controlTick()
    {
        // Smooth every continuous parameter. (partials is an int; its change is smoothed per
        // partial amplitude below, so partials fade in/out instead of popping.)
        smoothed.seed = smoothTowards (smoothed.seed, target.seed, smoothCoef);
        smoothed.rolloff = smoothTowards (smoothed.rolloff, target.rolloff, smoothCoef);
        smoothed.gainDb = smoothTowards (smoothed.gainDb, target.gainDb, smoothCoef);
        smoothed.sustain = smoothTowards (smoothed.sustain, target.sustain, smoothCoef);
        smoothed.velocitySens = smoothTowards (smoothed.velocitySens, target.velocitySens, smoothCoef);
        // Fundamental glides in pitch (log) space, so it sounds like a smooth portamento.
        smoothed.fundamentalHz = std::exp (smoothTowards (std::log (smoothed.fundamentalHz), std::log (target.fundamentalHz), smoothCoef));
        smoothed.keysFollowSeed = smoothTowards (smoothed.keysFollowSeed, target.keysFollowSeed, smoothCoef);
        smoothed.pitchBendCents = smoothTowards (smoothed.pitchBendCents, target.pitchBendCents, smoothCoef);
        smoothed.partials = target.partials;
        smoothed.attackMs = target.attackMs;
        smoothed.decayMs = target.decayMs;
        smoothed.releaseMs = target.releaseMs;

        const double seedNow = std::max (smoothed.seed, 1e-6);
        const double S = xen::stretchFactor (seedNow);
        const double period = xen::periodCents (seedNow);
        const double twoPiOverSr = 2.0 * 3.141592653589793 / sr;
        const double fadeTop = 0.45 * sr, fadeBottom = 0.40 * sr; // Nyquist guard band
        const double ampCoef = smoothCoef;

        masterGain = (float) std::pow (10.0, smoothed.gainDb / 20.0);
        attackStep = (float) (1.0 / std::max (1.0, smoothed.attackMs * 0.001 * sr));
        decayCoef = (float) std::exp (std::log (0.001) / std::max (1.0, smoothed.decayMs * 0.001 * sr));
        releaseCoef = (float) std::exp (std::log (0.001) / std::max (1.0, smoothed.releaseMs * 0.001 * sr));
        sustainLevel = (float) std::clamp (smoothed.sustain, 0.0, 1.0);

        for (auto& v : voices)
        {
            if (! v.active) continue;

            // Voice fundamental, measured in cents from key 60.
            // keysFollowSeed blends between normal keys (100 c per key, key 60 = middle C) and
            // seed keys (period/12 per key, key 60 = the calculator's Fundamental), so toggling
            // it glides instead of jumping.
            const double k = smoothed.keysFollowSeed;
            const double centsPerKey = 100.0 + k * (period / 12.0 - 100.0);
            const double anchorCents = k * 1200.0 * std::log2 (smoothed.fundamentalHz / 261.6255653005986);
            const double cents = anchorCents + (v.note - 60) * centsPerKey + smoothed.pitchBendCents;
            const double f0 = 261.6255653005986 * std::exp2 (cents / 1200.0);
            // Velocity -> volume: velocity^(2*sens). At full sensitivity velocity 64 is ~-12 dB.
            v.velGain = (float) std::pow (std::max (1e-4, (double) v.velocity), 2.0 * smoothed.velocitySens);

            // Pass 1: raw target amplitudes (rolloff x count mask x audible-band fades).
            std::array<double, kMaxPartials> tgt {};
            double power = 0.0;
            for (int p = 0; p < kMaxPartials; ++p)
            {
                const double f = f0 * std::exp (S * lnN[(size_t) p]);
                double a = (p < smoothed.partials) ? xen::rolloffShape (p + 1, smoothed.rolloff) : 0.0;
                // Fade partials approaching Nyquist (they would alias into garbage above it).
                a *= std::clamp ((fadeTop - f) / (fadeTop - fadeBottom), 0.0, 1.0);
                // Fade sub-audio partials (10-20 Hz): inaudible, they'd only eat headroom.
                a *= std::clamp ((f - 10.0) / 10.0, 0.0, 1.0);
                tgt[(size_t) p] = a;
                power += a * a;

                const double w = std::min (f * twoPiOverSr, 3.141592653589793);
                v.c[(size_t) p] = (float) std::cos (w);
                v.s[(size_t) p] = (float) std::sin (w);
            }
            const double norm = power > 1e-12 ? 1.0 / std::sqrt (power) : 0.0;

            // Pass 2: smooth amplitudes, set the per-sample ramp, renormalise the rotators
            // (float rounding makes their length drift slowly; this pins it back to 1).
            int highest = 0;
            for (int p = 0; p < kMaxPartials; ++p)
            {
                const auto idx = (size_t) p;
                // A fresh note takes its amplitudes directly (the envelope does its fade-in);
                // a sounding note glides towards new amplitudes.
                v.ampSmoothed[idx] = v.fresh ? (float) (tgt[idx] * norm)
                                             : (float) smoothTowards (v.ampSmoothed[idx], tgt[idx] * norm, ampCoef);
                if (v.ampSmoothed[idx] < 1e-6f && tgt[idx] <= 0.0) v.ampSmoothed[idx] = 0.0f;
                v.ampStep[idx] = (v.ampSmoothed[idx] - v.amp[idx]) / (float) kControlBlock;
                if (v.ampSmoothed[idx] > 0.0f || v.amp[idx] > 0.0f) highest = p + 1;

                const float mag = std::sqrt (v.re[idx] * v.re[idx] + v.im[idx] * v.im[idx]);
                if (mag > 0.0f) { v.re[idx] /= mag; v.im[idx] /= mag; }
                else { v.re[idx] = 1.0f; v.im[idx] = 0.0f; }
            }
            v.numActive = highest;
            v.fresh = false;
        }
    }

    void renderVoice (Voice& v, float* out, int n)
    {
        const int P = v.numActive;
        float* re = v.re.data(); float* im = v.im.data();
        const float* c = v.c.data(); const float* s = v.s.data();
        float* amp = v.amp.data(); const float* step = v.ampStep.data();
        const float gain = v.velGain * masterGain;

        for (int i = 0; i < n; ++i)
        {
            // Envelope (same formulas as shared/ui/EnvelopeView.h draws):
            // linear attack; decay glides exponentially towards the sustain level (99.9% of the way
            // after "decay" ms) and keeps following it live; release falls to -60 dB in "release" ms.
            if (v.stage == Voice::Attack)
            {
                v.env += attackStep;
                if (v.env >= 1.0f) { v.env = 1.0f; v.stage = Voice::Decay; }
            }
            else if (v.stage == Voice::Decay)
            {
                v.env = sustainLevel + (v.env - sustainLevel) * decayCoef;
                // Fully decayed to a zero sustain: the note is over even if the key is still down.
                if (sustainLevel <= 0.0f && v.env < 0.001f) { v.active = false; v.env = 0.0f; return; }
            }
            else if (v.stage == Voice::Release)
            {
                v.env *= releaseCoef;
                if (v.env < 0.001f) { v.active = false; v.env = 0.0f; return; }
            }

            float sum = 0.0f;
            for (int p = 0; p < P; ++p)
            {
                const float r = re[p] * c[p] - im[p] * s[p];
                im[p] = re[p] * s[p] + im[p] * c[p];
                re[p] = r;
                sum += amp[p] * im[p];
                amp[p] += step[p];
            }
            out[i] += sum * v.env * gain;
        }
    }

    Voice* findVoiceForNote (int note)
    {
        for (auto& v : voices)
            if (v.active && v.note == note) return &v;
        return nullptr;
    }

    Voice* findFreeVoice()
    {
        for (auto& v : voices)
            if (! v.active) return &v;
        return nullptr;
    }

    Voice* stealVoice()
    {
        // Prefer the oldest releasing voice, else the oldest voice overall.
        Voice* best = nullptr;
        for (auto& v : voices)
            if (v.stage == Voice::Release && (best == nullptr || v.age < best->age)) best = &v;
        if (best) return best;
        for (auto& v : voices)
            if (best == nullptr || v.age < best->age) best = &v;
        return best;
    }

    double sr = 48000.0;
    double smoothCoef = 0.98;
    EngineParams target, smoothed;
    std::array<double, kMaxPartials> lnN {};
    std::array<Voice, kMaxVoices> voices {};
    uint64_t ageCounter = 0;
    int tickCountdown = 0;
    float masterGain = 0.25f, attackStep = 0.001f, decayCoef = 0.999f, releaseCoef = 0.999f, sustainLevel = 0.6f;
};
} // namespace seed
