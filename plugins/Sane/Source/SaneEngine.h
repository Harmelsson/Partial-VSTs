#pragma once
// SaneEngine — the voices. Every core is a bank of tunable delay LOOPS ("lines"):
//
//   Delay     1 line   : Karplus-Strong family. Harmonic (string-like) until an insert breaks it.
//   Membrane  8 lines  : a feedback delay network. Each line is tuned to one membrane mode; a
//                        Householder mix (`coupling`) lets them feed each other, so energy wanders
//                        between modes and each line's own harmonics add a dense, organic top.
//   Partials  16 lines : one line per mode, with a band-pass INSIDE each loop so it rings as one pure
//                        partial at exactly the position shown in Modes.
//
// Per sample, per line:  y = delay(L)  ->  coupling mix  ->  7 insert slots  ->  (band-pass)  ->
//                        damping low-pass  ->  loop gain  ->  guard (soft clip)  ->  + exciter  ->  delay
// Every parameter is read per block and smoothed per sample (delay lengths glide), so all changes are
// heard live on ringing notes. A retriggered note re-strikes its own ringing voice (physical: hitting a
// drum that still rings adds energy to it).
//
// Pitch = played note + tune + bend + drawn pitch envelope (pitchAmt, pitchTime) + TENSION
// (energy-dependent glide, Avanzini/Marogna/Bank: pitch rises with the energy still in the drum).
#include <atomic>
#include <complex>
#include <juce_core/juce_core.h>
#include <memory>
#include "SaneDsp.h"
#include "SaneParams.h"

namespace sane
{
inline constexpr int kMaxVoices = 12;
inline constexpr int kMaxLines = dsp::kMaxModes;    // 16
inline constexpr int kCtrl = 32;                    // control-rate block (samples)
inline constexpr int kScopeBins = 400;              // UI scope: bins over kScopeSeconds of the last hit
inline constexpr double kScopeSeconds = 1.2;

inline int linesForCore (int core) { return core == CoreDelay ? 1 : (core == CorePartials ? 16 : 8); }

/// Everything the engine needs from the parameters, read once per block.
struct EngineParams
{
    float exColor = 0.45f, exLength = 5.0f, exGrain = 0.15f, exLevelDb = 0.0f, exRandom = 0.35f, exVelRandom = 0.2f,
          exVelLength = -0.3f, exKeyColor = 0.4f, exSpread = 0.5f;
    float stBase = 0.4f, stVel = -0.6f, stKey = 0.4f, stRandom = 0.06f;
    int core = CoreMembrane;
    float feedback = 0.65f, damp = 0.4f, tension = 1.5f, coupling = 0.3f, width = 0.5f;
    int table = TabMembrane, srcA = SrcPhysical, srcB = SrcCalc;
    float morph = 0.0f, calcSeed = 2.0f, rolloff = 0.35f;
    float pitchAmt = 3.0f, pitchTime = 60.0f, lengthMs = kLengthOff;
    float tune = 0.0f, fine = 0.0f, bendRange = 2.0f, gainDb = -3.0f;
    std::array<int, kNumSlots> slotType {};
    std::array<float, kNumSlots> slotA {}, slotB {}, slotC {};
};

/// Mode ratios after the A/B morph (shared by all voices).
inline std::array<double, kMaxLines> computeRatios (const EngineParams& p)
{
    std::array<double, kMaxLines> r {};
    for (int i = 0; i < kMaxLines; ++i)
        r[(size_t) i] = dsp::modes::morphed (dsp::modes::fromSource (p.srcA, p.table, p.calcSeed, i),
                                             dsp::modes::fromSource (p.srcB, p.table, p.calcSeed, i), p.morph);
    return r;
}

/// What the UI wants to know about the latest hit (written by the audio thread).
struct HitReport
{
    std::atomic<int> counter { 0 };
    std::atomic<int> note { 60 };
    std::atomic<float> velocity { 0.8f }, strikeR { 0.3f };
    std::atomic<uint32_t> seed { 1 };
    std::atomic<float> burstLength { 5.0f }, burstColor { 0.45f }, burstGrain { 0.15f };
    std::array<std::atomic<float>, kScopeBins> scope {};
    std::atomic<int> scopeFill { 0 };
    std::array<std::atomic<uint64_t>, 2> held {};
    std::atomic<int> activeVoices { 0 };
};

class SaneEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        // A line must hold one period of the lowest pitch (16 Hz) plus headroom for downward glides.
        lineSize = 1;
        while (lineSize < (int) (sr / 12.0) + 8) lineSize <<= 1;
        insSize = 1;
        while (insSize < (int) (0.012 * sr) + 8) insSize <<= 1;
        linePool.assign ((size_t) (kMaxVoices * kMaxLines * lineSize), 0.0f);
        insPool.assign ((size_t) (kMaxVoices * kMaxLines * kNumSlots * insSize), 0.0f);
        excPool.assign ((size_t) (kMaxVoices * lineSize), 0.0f);
        for (int v = 0; v < kMaxVoices; ++v)
        {
            auto& vc = voices[(size_t) v];
            vc.excBuf = excPool.data() + (size_t) (v * lineSize);
            for (int l = 0; l < kMaxLines; ++l)
            {
                auto& ln = vc.lines[(size_t) l];
                ln.buf = linePool.data() + (size_t) ((v * kMaxLines + l) * lineSize);
                for (int s = 0; s < kNumSlots; ++s)
                {
                    auto& is = ln.ins[(size_t) s];
                    is.buf = insPool.data() + (size_t) (((v * kMaxLines + l) * kNumSlots + s) * insSize);
                    is.mask = insSize - 1;
                    is.rng = dsp::Rng ((uint32_t) (v * 7919 + l * 104729 + s * 13 + 1));
                }
            }
            vc.reset (lineSize);
        }
        scopeVoice = -1;
        envAtt = (float) (1.0 - std::exp (-1.0 / (0.001 * sr)));
        envRel = (float) std::exp (-1.0 / (0.03 * sr));
    }

    void setParams (const EngineParams& p) { prm = p; }
    const EngineParams& params() const { return prm; }

    void noteOn (int note, float velocity)
    {
        // Re-strike the voice already ringing this note, else a free one, else the quietest.
        int idx = -1;
        for (int i = 0; i < kMaxVoices; ++i)
            if (voices[(size_t) i].active && voices[(size_t) i].note == note && ! voices[(size_t) i].muting) { idx = i; break; }
        if (idx < 0)
            for (int i = 0; i < kMaxVoices; ++i)
                if (! voices[(size_t) i].active) { idx = i; break; }
        bool fresh = true;
        if (idx < 0)
        {
            float best = 1e9f;
            for (int i = 0; i < kMaxVoices; ++i)
                if (voices[(size_t) i].env < best) { best = voices[(size_t) i].env; idx = i; }
            voices[(size_t) idx].reset (lineSize);
        }
        else if (voices[(size_t) idx].active)
            fresh = false;
        auto& v = voices[(size_t) idx];
        if (fresh) v.reset (lineSize);
        strike (v, note, velocity, fresh);
        scopeVoice = idx;
        scopeN = 0;
        for (auto& b : report.scope) b.store (0.0f, std::memory_order_relaxed);
        report.scopeFill.store (0);
        setHeld (note, true);
    }

    void noteOff (int note)
    {
        setHeld (note, false);
        // Drums ring out on their own. Only a self-oscillating loop is stopped by note-off.
        if (! feedbackSelfOsc (prm.feedback)) return;
        for (auto& v : voices)
            if (v.active && v.note == note) v.muting = true;
    }

    void allNotesOff()
    {
        for (auto& v : voices) if (v.active) v.muting = true;
        report.held[0].store (0); report.held[1].store (0);
    }

    void setPitchBend (float semis) { bendSemis = semis; }

    /// Adds the voices into out (stereo). out must be cleared by the caller.
    void render (float* outL, float* outR, int numSamples)
    {
        const auto ratios = computeRatios (prm);
        // Per-block slot coefficients (shared by every line of every voice).
        std::array<dsp::SlotCoeffs, kNumSlots> sc;
        int active[kNumSlots], nActive = 0;
        for (int s = 0; s < kNumSlots; ++s)
        {
            sc[(size_t) s] = dsp::makeSlotCoeffs (prm.slotType[(size_t) s], prm.slotA[(size_t) s], prm.slotB[(size_t) s], prm.slotC[(size_t) s], sr);
            if (prm.slotType[(size_t) s] != SlotOff) active[nActive++] = s;
        }
        const float outGain = (float) std::pow (10.0, prm.gainDb / 20.0);
        for (int vi = 0; vi < kMaxVoices; ++vi)
        {
            auto& v = voices[(size_t) vi];
            if (! v.active) continue;
            renderVoice (v, ratios, sc, active, nActive, outL, outR, numSamples, outGain, vi == scopeVoice);
        }
        report.activeVoices.store (activeVoices(), std::memory_order_relaxed);
    }

    HitReport report;

    // ---- for tests ----
    int activeVoices() const { int n = 0; for (auto& v : voices) n += v.active ? 1 : 0; return n; }
    double sampleRate() const { return sr; }
    float voiceLineLength (int voice, int line) const { return voices[(size_t) voice].lines[(size_t) line].L; }
    juce::String debugVoice (int voice) const
    {
        const auto& v = voices[(size_t) voice];
        return "g " + juce::String (v.lines[0].g, 4) + " gov " + juce::String (v.gov, 3) + " mute " + juce::String (v.muteGain, 3)
             + " env " + juce::String (v.env, 4) + " L " + juce::String (v.lines[0].L, 2) + " comp " + juce::String (v.lines[0].lpComp, 3);
    }

private:
    struct Line
    {
        float* buf = nullptr;
        int w = 0;
        float L = 100.0f, Lstep = 0.0f;      // current delay (samples) and per-sample ramp
        float g = 0.9f, gStep = 0.0f;        // loop gain
        float lpCoef = 1.0f, lpState = 0.0f, lpComp = 1.0f;
        float hpR = 0.9995f, hpX1 = 0.0f, hpY1 = 0.0f;  // DC guard inside the loop
        float bpB0 = 0.0f, bpA1 = 0.0f, bpA2 = 0.0f, bpX1 = 0.0f, bpX2 = 0.0f, bpY1 = 0.0f, bpY2 = 0.0f;
        float inj = 0.0f, panL = 0.707f, panR = 0.707f;
        float inc = 0.0f;                    // line frequency as cycles/sample (ring mod "follow")
        std::array<dsp::InsertState, kNumSlots> ins;
    };

    struct Voice
    {
        bool active = false, muting = false, jump = true;
        int note = 60;
        float vel = 0.8f, strikeR = 0.3f;
        double t = 0.0;                       // seconds since the last strike
        float env = 0.0f, peak = 1e-6f;       // output envelope follower, its max since the strike
        float muteGain = 1.0f, gov = 1.0f;
        int quietCtrl = 0;
        std::array<float, kMaxLines> weights {};
        dsp::BurstGen burst;
        float* excBuf = nullptr;              // strike comb delay line (mono)
        int excW = 0;
        float combDelay = 0.0f;
        int ctrlLeft = 0;
        // output high-pass (2nd-order Butterworth, half an octave+ below the lowest mode): removes sub-bass outside the loop
        float ob0 = 1.0f, ob1 = 0.0f, ob2 = 0.0f, oa1 = 0.0f, oa2 = 0.0f;
        std::array<float, 2> ox1 {}, ox2 {}, oy1 {}, oy2 {};
        std::array<Line, kMaxLines> lines;

        void reset (int lineSize)
        {
            active = muting = false;
            jump = true;
            env = 0.0f; peak = 1e-6f; muteGain = 1.0f; gov = 1.0f; quietCtrl = 0; t = 0.0; ctrlLeft = 0;
            for (auto& ln : lines)
            {
                if (ln.buf != nullptr) std::fill (ln.buf, ln.buf + lineSize, 0.0f);
                ln.w = 0; ln.lpState = 0.0f; ln.hpX1 = ln.hpY1 = 0.0f; ln.bpX1 = ln.bpX2 = ln.bpY1 = ln.bpY2 = 0.0f; ln.Lstep = 0.0f; ln.gStep = 0.0f;
                for (auto& is : ln.ins) is.clear();
            }
            if (excBuf != nullptr) std::fill (excBuf, excBuf + lineSize, 0.0f);
            excW = 0;
            ox1.fill (0.0f); ox2.fill (0.0f); oy1.fill (0.0f); oy2.fill (0.0f);
        }
    };

    void setHeld (int note, bool on)
    {
        if (note < 0 || note > 127) return;
        auto& a = report.held[(size_t) (note >> 6)];
        const uint64_t bit = 1ull << (note & 63);
        if (on) a.fetch_or (bit); else a.fetch_and (~bit);
    }

    void strike (Voice& v, int note, float velocity, bool fresh)
    {
        const auto& p = prm;
        v.active = true;
        v.muting = false;
        v.muteGain = 1.0f;
        v.note = note;
        v.vel = velocity;
        v.t = 0.0;
        v.peak = std::max (1e-6f, v.env);
        v.quietCtrl = 0;
        if (fresh) { v.ctrlLeft = 0; v.jump = true; }

        // ---- strike position: base + velocity + key + random ----
        const float key = (float) (note - 60) / 36.0f;
        float r = p.stBase + p.stVel * (velocity - 0.5f) + p.stKey * key + p.stRandom * rng.next();
        r = std::clamp (r, 0.0f, 0.96f);
        v.strikeR = r;
        for (int i = 0; i < kMaxLines; ++i)
            v.weights[(size_t) i] = (float) dsp::modes::strikeWeight (p.table, i, r);

        // ---- the burst: new noise on every note, varied by the randomness settings ----
        const float rnd = std::clamp (p.exRandom + p.exVelRandom * (velocity - 0.5f) * 2.0f, 0.0f, 1.0f);
        dsp::BurstSettings b;
        b.seed = rng.bits() | 1u;
        b.lengthMs = p.exLength * std::pow (2.0f, p.exVelLength * (velocity - 0.5f) * 2.0f) * (1.0f + rnd * 0.8f * rng.next());
        b.lengthMs = std::clamp (b.lengthMs, 0.2f, 200.0f);
        b.color = std::clamp (p.exColor + p.exKeyColor * key * 0.5f + 0.25f * (velocity - 0.6f) + rnd * 0.3f * rng.next(), 0.0f, 1.0f);
        b.grain = std::clamp (p.exGrain * (1.0f + rnd * rng.next()), 0.0f, 1.0f);
        b.level = (float) std::pow (10.0, p.exLevelDb / 20.0) * (0.15f + 0.85f * velocity * velocity) * (1.0f + rnd * 0.25f * rng.next());
        b.spread = p.exSpread;
        v.burst.start (b, sr);
        // pluck-position comb on the exciter (Delay + Membrane cores): delay = r/2 of the fundamental's period
        v.combDelay = 0.0f;

        report.note.store (note);
        report.velocity.store (velocity);
        report.strikeR.store (r);
        report.seed.store (b.seed);
        report.burstLength.store (b.lengthMs);
        report.burstColor.store (b.color);
        report.burstGrain.store (b.grain);
        report.counter.fetch_add (1);
    }

    /// Control-rate update: pitch, delay targets, gains, filters. Runs every kCtrl samples per voice.
    void control (Voice& v, const std::array<double, kMaxLines>& ratios, int n)
    {
        const auto& p = prm;
        const int nLines = linesForCore (p.core);
        // ---- pitch ----
        const double drawn = p.pitchAmt * std::exp (-v.t / std::max (0.002, (double) p.pitchTime * 0.001));
        const double e = std::clamp ((double) (v.env / std::max (1e-6f, v.peak)), 0.0, 1.0);
        const double tens = p.tension * v.vel * e * e;
        const double semis = (v.note - 69) + p.tune + p.fine * 0.01 + bendSemis + drawn + tens;
        const double f0 = std::clamp (440.0 * std::pow (2.0, semis / 12.0), 16.0, 0.45 * sr);

        // ---- decay ----
        double t60 = feedbackToT60 (p.feedback);
        const double over = feedbackOverdrive (p.feedback);
        const bool selfOsc = feedbackSelfOsc (p.feedback);

        // ---- length / mute: a hand on the skin (fast extra damping) ----
        if (p.lengthMs < kLengthOff - 0.5f && v.t * 1000.0 > p.lengthMs) v.muting = true;
        if (v.muting) v.muteGain *= 0.82f; // ~ -60 dB in ~35 control blocks (~25 ms at 48k)

        // comb on the exciter for Delay / Membrane: pluck at r/2 of the period
        v.combDelay = (p.core == CorePartials) ? 0.0f : (float) std::clamp (v.strikeR * 0.5 * sr / f0, 0.0, (double) lineSize - 4.0);

        {
            double minRatio = 1.0;
            if (p.core != CoreDelay) for (int i = 0; i < nLines; ++i) minRatio = std::min (minRatio, ratios[(size_t) i]);
            const double fc = std::clamp (0.5 * f0 * minRatio, 10.0, 0.2 * sr);
            const double w0 = 2.0 * dsp::kPi * fc / sr, al = std::sin (w0) / (2.0 * 0.70710678), cw = std::cos (w0), a0 = 1.0 + al;
            v.ob0 = (float) ((1.0 + cw) * 0.5 / a0); v.ob1 = (float) (-(1.0 + cw) / a0); v.ob2 = v.ob0;
            v.oa1 = (float) (-2.0 * cw / a0); v.oa2 = (float) ((1.0 - al) / a0);
        }

        for (int i = 0; i < kMaxLines; ++i)
        {
            auto& ln = v.lines[(size_t) i];
            if (i >= nLines) { ln.inj = 0.0f; ln.gStep = (0.0f - ln.g) / (float) n; continue; }
            const double ratio = (p.core == CoreDelay) ? 1.0 : ratios[(size_t) i];
            const double f = f0 * ratio;
            const bool ok = f < 0.42 * sr;
            const double w = 2.0 * dsp::kPi * std::min (f, 0.42 * sr) / sr;

            // damping low-pass (Delay / Membrane): cutoff tied to the line's frequency. Its gain and phase
            // at f are compensated, so the mode itself keeps its pitch and decay; only the line's harmonics die.
            double lpPhaseDelay = 0.0, comp = 1.0, a = 1.0;
            if (p.core != CorePartials)
            {
                const double rel = 1.0 + std::pow (1.0 - p.damp, 2.0) * 40.0;
                const double fc = std::min (0.45 * sr, f * rel);
                a = 1.0 - std::exp (-2.0 * dsp::kPi * fc / sr);
                const double re = 1.0 - (1.0 - a) * std::cos (w), im = (1.0 - a) * std::sin (w);
                const double mag = a / std::sqrt (re * re + im * im);
                comp = 1.0 / std::max (0.05, mag);
                lpPhaseDelay = std::atan2 (im, re) / std::max (1e-9, w);
            }
            // DC guard inside the loop: a gentle one-pole high-pass far below f (max 5 Hz). Without it a loop gain > 1
            // (self-oscillation, biased saturation) latches onto DC. Its tiny gain loss and phase lead at f are compensated.
            // (Any high-pass in a loop also creates a weak "phantom" mode far below the pitch; the per-voice output
            // high-pass below removes it, outside the loop, where it can't touch tuning or stability.)
            {
                const double fc = std::min (5.0, f * 0.03);
                const double R = std::exp (-2.0 * dsp::kPi * fc / sr);
                const std::complex<double> z1 = std::polar (1.0, -w);
                const auto h = (1.0 - z1) / (1.0 - R * z1);
                comp /= std::max (0.5, std::abs (h));
                lpPhaseDelay -= std::arg (h) / std::max (1e-9, w);
                ln.hpR = (float) R;
            }
            ln.lpCoef = (float) a;
            ln.lpComp = (float) comp;

            // band-pass inside the loop (Partials): zero phase at f, so no pitch error.
            if (p.core == CorePartials && ok)
            {
                const double q = 9.0;
                const double alpha = std::sin (w) / (2.0 * q), a0 = 1.0 + alpha;
                ln.bpB0 = (float) (alpha / a0);
                ln.bpA1 = (float) (-2.0 * std::cos (w) / a0);
                ln.bpA2 = (float) ((1.0 - alpha) / a0);
            }

            const double Ltarget = std::clamp (sr / f - lpPhaseDelay, 2.5, (double) lineSize - 6.0);
            ln.Lstep = (float) ((Ltarget - ln.L) / n);
            if (v.jump) { ln.L = (float) Ltarget; ln.Lstep = 0.0f; } // first block of a fresh voice: jump

            // per-line decay: higher modes die faster with DAMP
            const double t60i = t60 / (1.0 + p.damp * 2.5 * std::max (0.0, ratio - 1.0));
            double g = selfOsc ? over : std::pow (10.0, -3.0 * (Ltarget / sr) / std::max (0.005, t60i));
            g *= v.gov * v.muteGain;
            if (! ok) g = 0.0;
            ln.gStep = (float) ((g - ln.g) / n);
            if (v.jump) { ln.g = (float) g; ln.gStep = 0.0f; }

            // injection: strike weight x rolloff, normalised by line count
            const double roll = std::pow ((double) (i + 1), -(double) p.rolloff);
            const double w8 = (p.core == CoreDelay) ? 1.0 : v.weights[(size_t) i] * roll;
            // makeup: a band-passed (Partials) or 8-way split (Membrane) burst carries much less energy per line
            const double makeup = p.core == CorePartials ? 20.0 : (p.core == CoreMembrane ? 6.0 : 1.0);
            ln.inj = ok ? (float) (makeup * w8 / std::sqrt ((double) nLines)) : 0.0f;
            ln.inc = (float) (f / sr);

            // stereo: lines alternate left / right, spread by WIDTH
            const double side = nLines == 1 ? 0.0 : ((i % 2 == 0) ? -1.0 : 1.0) * (0.35 + 0.65 * (double) (i + 1) / nLines);
            const double pan = 0.5 + 0.5 * p.width * side;
            ln.panL = (float) std::cos (pan * dsp::kPi * 0.5);
            ln.panR = (float) std::sin (pan * dsp::kPi * 0.5);
        }
        v.t += (double) n / sr;
        v.jump = false;
    }

    void renderVoice (Voice& v, const std::array<double, kMaxLines>& ratios, const std::array<dsp::SlotCoeffs, kNumSlots>& sc,
                      const int* active, int nActive, float* outL, float* outR, int numSamples, float outGain, bool scope)
    {
        const int nLines = linesForCore (prm.core);
        const bool partials = prm.core == CorePartials;
        // coupling = lossless mixing: Givens rotations between neighbouring lines, in two staggered stages
        // ((0,1),(2,3)... then (1,2),(3,4)...,(N-1,0)). Orthogonal, so it only MOVES energy between modes, never removes it
        // (a plain I - c*Householder mix would leak energy for 0 < c < 1 and kill the ring).
        const float theta = (float) (nLines > 1 ? prm.coupling * (partials ? 0.15 : 1.0) * dsp::kPi * 0.25 : 0.0);
        const float rc = std::cos (theta), rs = std::sin (theta);
        // output level: lines add up, so normalise gently (N^0.25) — keeps the three cores at similar loudness
        const float norm = outGain / std::sqrt (std::sqrt ((float) nLines));
        const int mask = lineSize - 1;
        float y[kMaxLines];

        for (int i = 0; i < numSamples; ++i)
        {
            if (v.ctrlLeft <= 0)
            {
                control (v, ratios, kCtrl);
                v.ctrlLeft = kCtrl;
            }
            --v.ctrlLeft;

            // exciter (+ pluck comb)
            float eL, eR;
            v.burst.next (eL, eR);
            if (v.combDelay >= 1.0f)
            {
                const float m = 0.5f * (eL + eR);
                v.excBuf[v.excW] = m;
                const int di = (int) v.combDelay;
                const float fr = v.combDelay - (float) di;
                const float d0 = v.excBuf[(v.excW - di) & mask], d1 = v.excBuf[(v.excW - di - 1) & mask];
                const float del = d0 + fr * (d1 - d0);
                v.excW = (v.excW + 1) & mask;
                eL -= 0.8f * del;
                eR -= 0.8f * del;
            }

            // read all lines (3rd-order Lagrange fractional delay)
            for (int l = 0; l < nLines; ++l)
            {
                auto& ln = v.lines[(size_t) l];
                ln.L += ln.Lstep;
                const float D = ln.L;
                const int di = (int) D;
                const float p = D - (float) di + 1.0f; // position relative to tap (di - 1)
                const float x0 = ln.buf[(ln.w - (di - 1)) & mask], x1 = ln.buf[(ln.w - di) & mask];
                const float x2 = ln.buf[(ln.w - (di + 1)) & mask], x3 = ln.buf[(ln.w - (di + 2)) & mask];
                const float pm1 = p - 1.0f, pm2 = p - 2.0f, pm3 = p - 3.0f;
                const float out = -x0 * pm1 * pm2 * pm3 * (1.0f / 6.0f) + x1 * p * pm2 * pm3 * 0.5f
                                  - x2 * p * pm1 * pm3 * 0.5f + x3 * p * pm1 * pm2 * (1.0f / 6.0f);
                y[l] = out;
            }
            float u[kMaxLines];
            for (int l = 0; l < nLines; ++l) u[l] = y[l];
            if (nLines > 1 && theta > 0.0f)
            {
                for (int l = 0; l + 1 < nLines; l += 2)
                {
                    const float a = u[l], b = u[l + 1];
                    u[l] = rc * a - rs * b; u[l + 1] = rs * a + rc * b;
                }
                for (int l = 1; l < nLines; l += 2)
                {
                    const int m = (l + 1) % nLines;
                    const float a = u[l], b = u[m];
                    u[l] = rc * a - rs * b; u[m] = rs * a + rc * b;
                }
            }

            float oL = 0.0f, oR = 0.0f;
            for (int l = 0; l < nLines; ++l)
            {
                auto& ln = v.lines[(size_t) l];
                float s = u[l];
                const float period = ln.L;
                for (int k = 0; k < nActive; ++k)
                    s = ln.ins[(size_t) active[k]].process (s, sc[(size_t) active[k]], period, ln.inc);
                {
                    const float hp = s - ln.hpX1 + ln.hpR * ln.hpY1;
                    ln.hpX1 = s; ln.hpY1 = hp;
                    s = hp;
                }
                if (partials)
                {
                    const float b = ln.bpB0 * (s - ln.bpX2) - ln.bpA1 * ln.bpY1 - ln.bpA2 * ln.bpY2;
                    ln.bpX2 = ln.bpX1; ln.bpX1 = s; ln.bpY2 = ln.bpY1; ln.bpY1 = b;
                    s = b * ln.lpComp;
                }
                else
                {
                    ln.lpState += ln.lpCoef * (s - ln.lpState);
                    s = ln.lpState * ln.lpComp;
                }
                ln.g += ln.gStep;
                s = dsp::guard (s * ln.g);
                const float e = (l % 2 == 0) ? eL : eR;
                ln.buf[ln.w] = s + e * ln.inj;
                ln.w = (ln.w + 1) & mask;
                oL += y[l] * ln.panL;
                oR += y[l] * ln.panR;
            }
            oL *= norm; oR *= norm;
            {
                auto hp = [&v] (float x, int c) {
                    const float o = v.ob0 * x + v.ob1 * v.ox1[(size_t) c] + v.ob2 * v.ox2[(size_t) c] - v.oa1 * v.oy1[(size_t) c] - v.oa2 * v.oy2[(size_t) c];
                    v.ox2[(size_t) c] = v.ox1[(size_t) c]; v.ox1[(size_t) c] = x; v.oy2[(size_t) c] = v.oy1[(size_t) c]; v.oy1[(size_t) c] = o;
                    return o;
                };
                oL = hp (oL, 0); oR = hp (oR, 1);
            }
            outL[i] += oL;
            outR[i] += oR;

            const float a = std::max (std::abs (oL), std::abs (oR));
            v.env = a > v.env ? v.env + (a - v.env) * envAtt : v.env * envRel;
            v.peak = std::max (v.peak, v.env);

            if (scope)
            {
                const int bin = (int) ((double) scopeN / (kScopeSeconds * sr) * kScopeBins);
                if (bin < kScopeBins)
                {
                    auto& sb = report.scope[(size_t) bin];
                    if (a > sb.load (std::memory_order_relaxed)) sb.store (a, std::memory_order_relaxed);
                    report.scopeFill.store (bin + 1, std::memory_order_relaxed);
                }
                ++scopeN;
            }

            // end of a control block: governor + end-of-voice check
            if (v.ctrlLeft == 0)
            {
                // governor: a voice may not stay louder than ~0.85 at the output. Pulls the loop gain down gently
                // (never below 0.6, or a self-oscillating loop would die instead of settling) and recovers slowly.
                if (v.env > 0.85f) v.gov = std::max (0.6f, v.gov * 0.995f);
                else if (v.env < 0.6f) v.gov = std::min (1.0f, v.gov + 0.001f);
                const bool quiet = v.env < 1e-5f && ! v.burst.isActive();
                v.quietCtrl = quiet ? v.quietCtrl + 1 : 0;
                if (v.quietCtrl > (int) (0.05 * sr / kCtrl) || (v.muting && v.muteGain < 1e-5f))
                {
                    v.active = false;
                    return;
                }
            }
        }
    }

    double sr = 48000.0;
    int lineSize = 4096, insSize = 1024;
    std::vector<float> linePool, insPool, excPool;
    std::array<Voice, kMaxVoices> voices;
    EngineParams prm;
    dsp::Rng rng { 0xC0FFEEu };
    float bendSemis = 0.0f;
    float envAtt = 0.2f, envRel = 0.9995f;
    int scopeVoice = -1;
    long scopeN = 0;
};
} // namespace sane
