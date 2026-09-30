#pragma once
// SeedEngine — the SYNTH half of Seed. Plain C++ (no JUCE), so tools/ can test it headlessly.
//
// It never decides tuning itself: it reads the CALCULATOR's partials (with hand edits) and the
// calculator's playable scale (setScale), and turns them into sound. (User: "the synth should be
// separate from the calculator".)
//
// Per voice: up to 64 series partials + 8 custom partials as complex "rotator" oscillators
// (phase-continuous, so every change glides — the user's "everything is dynamic" rule), a noise
// layer, a stereo state-variable filter with its own decay envelope, and an ADSR.
// Outputs dry L/R plus one MONO "partial bus" per FX-rack slot (v0.5): each carries only the
// partials that slot picks (odd, even, high half..., or a smooth morph between two rules), so an
// FX can act on specific partials only.
//
// Control rate: every 32 samples all partial frequencies/amplitudes, pans, sends and filter
// coefficients are recomputed; amplitudes ramp linearly in between; parameters are smoothed.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "dsp/Eq.h"
#include "xen/CalcModel.h"
#include "xen/Stretch.h"

namespace seed
{
constexpr int kSeries = xen::kSeriesPartials;             // 64
constexpr int kOsc = xen::kSeriesPartials + xen::kCustomPartials; // 72 oscillators per voice
constexpr int kMaxVoices = 16;
constexpr int kControlBlock = 32;
constexpr int kMaxPlayable = xen::kMaxFinalNotes;
constexpr double kC4 = 261.6255653005986;
constexpr int kBuses = 12; // one partial bus per FX rack slot (= kNumFx)
constexpr int kEqBands = 5;
/// Below this envelope level (-100 dB) a voice is finished. The end is a fade to (practically)
/// nothing, not a cut: -100 dB of a voice is ~-112 dBFS, far under anything a DAC can reproduce.
/// (v0.6: the old threshold was -60 dB, a step the user heard as "the release just cuts off".)
constexpr float kSilence = 1e-5f;

enum KeyMode { Keys12 = 0, KeysSeed12 = 1, KeysScale = 2, KeysMagnet = 3 };
enum FilterMode { FilterOff = 0, FilterLP, FilterBP, FilterHP, FilterNotch };
enum SendRule { RuleAll = 0, RuleOdd, RuleEven, RuleLow, RuleHigh, RuleThird, RuleFund, RuleNotFund, RuleCustom, RuleRandom };

/// Everything the engine reads each block. The processor fills it (already modulated).
struct EngineParams
{
    // Calculator (tuning) inputs.
    double seed = 2.0, rolloff = 1.0, fundamentalHz = kC4;
    int partials = 16;
    xen::PartialEdits edits;
    // Envelope + level.
    double attackMs = 8.0, decayMs = 600.0, sustain = 0.6, releaseMs = 400.0, velocitySens = 1.0, gainDb = -12.0;
    // Keys + pitch.
    int keyMode = Keys12;
    double magnet = 1.0, life = 0.0, glideMs = 40.0, pitchCents = 0.0, fshiftHz = 0.0, bendCents = 0.0;
    int baseKey = 60; // the key that plays the unison (Scale / Magnet / Seed-12 modes)
    // Additive timbre tools.
    double tiltDbOct = 0.0, oddEven = 0.0, spread = 0.0, shimmer = 0.0, shimmerRate = 1.5;
    double formFreq = 1000.0, formGainDb = 0.0, formWidth = 1.0;
    // Subtractive tools.
    double noiseDb = -60.0;
    int filterMode = FilterOff;
    double cutoff = 4000.0, reso = 0.2, keyTrack = 0.0, filterEnvOct = 0.0, filterDecayMs = 300.0;
    // 5-band EQ, per voice, key-trackable (keyTrack 0..1 per band: 1 = the band follows the note).
    std::array<pvst::eq::Band, kEqBands> eq {};
    std::array<double, kEqBands> eqKeyTrack {};
    // Partial buses (one per FX slot): used or not, and which partials they pick. picks is a
    // continuous position over the rules (1.5 = halfway between Odd and Even).
    std::array<bool, kBuses> busOn {};
    std::array<double, kBuses> busPicks {};
    // Chaos toys (synth-side only: the calculator never sees them).
    double wanderCents = 0.0, wanderRate = 0.5, scramble = 0.0;
};

class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        tickSec = kControlBlock / sr;
        smoothCoef = std::exp (-tickSec / 0.030);
        for (int n = 1; n <= kSeries; ++n) log2N[(size_t) n - 1] = std::log2 ((double) n);
        s = target;
        for (auto& v : voices) v = Voice {};
        tickCountdown = 0;
    }

    void setParams (const EngineParams& p) { target = p; }

    /// The calculator's playable notes (cents above the unison, rising, unison first) + period.
    void setScale (const double* cents, const double* strength, int count, double period)
    {
        numPlayable = std::clamp (count, 0, kMaxPlayable);
        for (int i = 0; i < numPlayable; ++i) { playCents[(size_t) i] = cents[i]; playStrength[(size_t) i] = strength[i]; }
        scalePeriod = period;
        maxStrength = 1e-9;
        for (int i = 1; i < numPlayable; ++i) maxStrength = std::max (maxStrength, playStrength[(size_t) i]);
    }

    // ---- MIDI -------------------------------------------------------------------------------
    void noteOn (int note, float velocity)
    {
        Voice* v = findVoiceForNote (note);
        if (v == nullptr) v = findFreeVoice();
        if (v == nullptr) v = stealVoice();
        const bool wasSilent = ! v->active;
        v->fresh = wasSilent;
        v->note = note;
        v->velocity = std::clamp (velocity, 0.0f, 1.0f);
        v->stage = Voice::Attack;
        v->active = true;
        v->age = ++ageCounter;
        v->fenv = 1.0f;
        if (numHits < (int) pendingHits.size()) pendingHits[(size_t) numHits++] = note; // for the UI's "a note was played" latch
        if (wasSilent)
        {
            v->env = 0.0f;
            for (int p = 0; p < kOsc; ++p)
            {
                const auto i = (size_t) p;
                v->re[i] = 1.0f; v->im[i] = 0.0f; v->amp[i] = 0.0f; v->ampStep[i] = 0.0f; v->ampSm[i] = 0.0f;
                v->shimPhase[i] = (float) (hash (p, 7) * 6.2831853);
            }
            v->f1L = v->f2L = v->f1R = v->f2R = 0.0f;
            for (auto& band : v->eqSt) for (auto& sec : band) for (auto& ch : sec) ch = pvst::eq::State {};
        }
        tickCountdown = 0; // compute this voice before its first sample
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.active && v.note == note && v.stage != Voice::Release) v.stage = Voice::Release;
    }

    /// For tests: how many voices are fading out right now.
    int releasingVoiceCount() const
    {
        int c = 0;
        for (auto& v : voices) c += (v.active && v.stage == Voice::Release) ? 1 : 0;
        return c;
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

    /// Held notes for the UI: each held voice's key pitch in cents above the Fundamental (before
    /// pitch shift / bend), so the UI can light up the scale note being played. Returns count.
    int heldNoteCents (float* out, int maxOut) const
    {
        int n = 0;
        for (auto& v : voices)
            if (v.active && v.stage != Voice::Release && n < maxOut) out[n++] = (float) v.keyRelCents;
        return n;
    }

    /// Notes struck since the last call, as cents above the Fundamental (same meaning as
    /// heldNoteCents). Lets the UI light up notes that were shorter than one screen refresh.
    int takeHits (float* out, int maxOut)
    {
        int n = 0;
        const double anchor = 1200.0 * std::log2 (s.fundamentalHz / kC4);
        for (int i = 0; i < numHits && n < maxOut; ++i)
        {
            double cents, strength;
            keyTarget (pendingHits[(size_t) i], cents, strength);
            out[n++] = (float) (cents - anchor);
        }
        numHits = 0;
        return n;
    }

    /// Current pitch (cents from middle C) of the first sounding voice, for the UI (or NaN).
    double firstVoiceCents() const
    {
        for (auto& v : voices) if (v.active) return v.cents;
        return std::nan ("");
    }

    // ---- Audio ------------------------------------------------------------------------------
    /// ADDS n samples to dry L/R and to the kBuses partial buses (caller clears them first).
    void render (float* dryL, float* dryR, float* const* buses, int n)
    {
        int i = 0;
        while (i < n)
        {
            if (tickCountdown <= 0) { controlTick(); tickCountdown = kControlBlock; }
            const int chunk = std::min (tickCountdown, n - i);
            for (auto& v : voices)
                if (v.active)
                {
                    if (hasSends) renderVoice<true> (v, dryL + i, dryR + i, buses, i, chunk);
                    else renderVoice<false> (v, dryL + i, dryR + i, buses, i, chunk);
                }
            tickCountdown -= chunk;
            i += chunk;
        }
    }

private:
    struct Voice
    {
        enum Stage { Attack, Decay, Release };
        bool active = false, fresh = false;
        int note = -1;
        float velocity = 1.0f, velGain = 1.0f, lifeGain = 1.0f, env = 0.0f, fenv = 0.0f;
        Stage stage = Attack;
        uint64_t age = 0;
        double cents = 0.0; // current (glided) pitch, cents from middle C
        double keyRelCents = 0.0; // the key's note, cents above the Fundamental (for the UI)
        int numActive = 0;
        uint32_t rng = 12345u;
        float fg = 0.1f, fk = 1.4f, fa1 = 0, fa2 = 0, fa3 = 0; // SVF coefficients
        float f1L = 0, f2L = 0, f1R = 0, f2R = 0;              // SVF states
        float noiseAmp = 0.0f;
        alignas (32) std::array<float, kOsc> re {}, im {}, c {}, s {};
        alignas (32) std::array<float, kOsc> amp {}, ampStep {}, ampSm {}, panL {}, panR {}, shimPhase {};
        alignas (32) std::array<std::array<float, kOsc>, kBuses> w {}; // partial-bus weights
        // 5-band EQ: coefficients for this note (key tracking) and filter states [band][section][channel].
        std::array<pvst::eq::Coefs, kEqBands> eqc {};
        std::array<std::array<std::array<pvst::eq::State, 2>, pvst::eq::kMaxSections>, kEqBands> eqSt {};
        std::array<int, kEqBands> eqIdx {};
        int eqNum = 0;
    };

    static double hash (int a, int b)
    {
        uint32_t h = (uint32_t) a * 374761393u + (uint32_t) b * 668265263u + 0x9e3779b9u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return (double) ((h ^ (h >> 16)) & 0xffffu) / 65535.0;
    }

    double smooth (double cur, double tgt) const
    {
        const double next = tgt + (cur - tgt) * smoothCoef;
        return std::abs (next - tgt) < 1e-7 * std::max (1.0, std::abs (tgt)) ? tgt : next;
    }
    double smoothLog (double cur, double tgt) const { return std::exp (smooth (std::log (std::max (cur, 1e-9)), std::log (std::max (tgt, 1e-9)))); }

    void smoothAll()
    {
        auto& t = target;
        s.seed = smooth (s.seed, t.seed);
        s.rolloff = smooth (s.rolloff, t.rolloff);
        s.fundamentalHz = smoothLog (s.fundamentalHz, t.fundamentalHz);
        s.partials = t.partials;
        for (size_t i = 0; i < t.edits.levelDb.size(); ++i)
        {
            s.edits.levelDb[i] = smooth (s.edits.levelDb[i], t.edits.levelDb[i]);
            s.edits.posCents[i] = smooth (s.edits.posCents[i], t.edits.posCents[i]);
        }
        for (size_t k = 0; k < t.edits.customOn.size(); ++k)
        {
            s.edits.customOn[k] = t.edits.customOn[k];
            s.edits.customCents[k] = smooth (s.edits.customCents[k], t.edits.customCents[k]);
            s.edits.customLevelDb[k] = smooth (s.edits.customLevelDb[k], t.edits.customLevelDb[k]);
        }
        s.attackMs = t.attackMs; s.decayMs = t.decayMs; s.releaseMs = t.releaseMs;
        s.sustain = smooth (s.sustain, t.sustain);
        s.velocitySens = smooth (s.velocitySens, t.velocitySens);
        s.gainDb = smooth (s.gainDb, t.gainDb);
        s.keyMode = t.keyMode;
        s.magnet = smooth (s.magnet, t.magnet);
        s.life = smooth (s.life, t.life);
        s.glideMs = t.glideMs;
        s.pitchCents = smooth (s.pitchCents, t.pitchCents);
        s.fshiftHz = smooth (s.fshiftHz, t.fshiftHz);
        s.bendCents = smooth (s.bendCents, t.bendCents);
        s.tiltDbOct = smooth (s.tiltDbOct, t.tiltDbOct);
        s.oddEven = smooth (s.oddEven, t.oddEven);
        s.spread = smooth (s.spread, t.spread);
        s.shimmer = smooth (s.shimmer, t.shimmer);
        s.shimmerRate = t.shimmerRate;
        s.formFreq = smoothLog (s.formFreq, t.formFreq);
        s.formGainDb = smooth (s.formGainDb, t.formGainDb);
        s.formWidth = smooth (s.formWidth, t.formWidth);
        s.noiseDb = smooth (s.noiseDb, t.noiseDb);
        s.filterMode = t.filterMode;
        s.cutoff = smoothLog (s.cutoff, t.cutoff);
        s.reso = smooth (s.reso, t.reso);
        s.keyTrack = smooth (s.keyTrack, t.keyTrack);
        s.filterEnvOct = smooth (s.filterEnvOct, t.filterEnvOct);
        s.filterDecayMs = t.filterDecayMs;
        for (int b = 0; b < kBuses; ++b)
        {
            s.busOn[(size_t) b] = t.busOn[(size_t) b];
            s.busPicks[(size_t) b] = smooth (s.busPicks[(size_t) b], t.busPicks[(size_t) b]); // glides, no clicks
        }
        s.baseKey = t.baseKey;
        for (int b = 0; b < kEqBands; ++b)
        {
            auto& sb = s.eq[(size_t) b]; const auto& tb = t.eq[(size_t) b];
            sb.on = tb.on; sb.type = tb.type; sb.steep = tb.steep;
            sb.freq = smoothLog (sb.freq, tb.freq);
            sb.gainDb = smooth (sb.gainDb, tb.gainDb);
            sb.q = smoothLog (sb.q, tb.q);
            s.eqKeyTrack[(size_t) b] = smooth (s.eqKeyTrack[(size_t) b], t.eqKeyTrack[(size_t) b]);
        }
        s.wanderCents = smooth (s.wanderCents, t.wanderCents);
        s.wanderRate = t.wanderRate;
        s.scramble = smooth (s.scramble, t.scramble);
    }

    /// Where a key sits (cents from middle C, before pitch shift/bend) and its note strength.
    void keyTarget (int note, double& cents, double& strength01) const
    {
        const double period = xen::periodCents (std::max (1e-9, s.seed));
        const double anchor = 1200.0 * std::log2 (s.fundamentalHz / kC4); // key 60 = Fundamental
        strength01 = 1.0;
        const int d = note - s.baseKey; // keys counted from the base key (default C4 = 60)
        const double absPeriod = numPlayable > 0 ? scalePeriod : std::abs (period);
        switch (s.keyMode)
        {
            case KeysScale:
                if (numPlayable > 0)
                {
                    const int N = numPlayable;
                    const int o = (d >= 0) ? d / N : -((-d + N - 1) / N);
                    const int i = d - o * N;
                    cents = anchor + o * absPeriod + playCents[(size_t) i];
                    strength01 = i == 0 ? 1.0 : std::clamp (playStrength[(size_t) i] / maxStrength, 0.0, 1.0);
                    return;
                }
                break;
            case KeysMagnet:
                if (numPlayable > 0)
                {
                    const double base = d * absPeriod / 12.0;
                    const double oct = std::floor (base / absPeriod);
                    const double pc = base - oct * absPeriod;
                    double best = playCents[0], bestDist = 1e18, bestS = 1.0;
                    for (int i = 0; i <= numPlayable; ++i)
                    {
                        const double c = i < numPlayable ? playCents[(size_t) i] : absPeriod; // the period too
                        if (std::abs (c - pc) < bestDist)
                        {
                            bestDist = std::abs (c - pc); best = c;
                            bestS = (i == 0 || i == numPlayable) ? 1.0 : std::clamp (playStrength[(size_t) i] / maxStrength, 0.0, 1.0);
                        }
                    }
                    cents = anchor + base + s.magnet * (best - pc);
                    strength01 = 1.0 + s.magnet * (bestS - 1.0);
                    return;
                }
                break;
            case Keys12:
                cents = (note - 60) * 100.0; // normal tuning: independent of the base key
                return;
            default: break;
        }
        cents = anchor + d * period / 12.0; // Seed-12 (and the fallback while no scale exists yet)
    }

    double sendRule (int rule, int idx, bool custom) const
    {
        const int n = idx + 1;
        switch (rule)
        {
            case RuleAll: return 1.0;
            case RuleOdd: return (! custom && n % 2 == 1) ? 1.0 : 0.0;
            case RuleEven: return (! custom && n % 2 == 0) ? 1.0 : 0.0;
            case RuleLow: return (! custom && n <= std::max (1, s.partials / 2)) ? 1.0 : 0.0;
            case RuleHigh: return (! custom && n > s.partials / 2) ? 1.0 : 0.0;
            case RuleThird: return (! custom && n % 3 == 0) ? 1.0 : 0.0;
            case RuleFund: return (! custom && n == 1) ? 1.0 : 0.0;
            case RuleNotFund: return (custom || n > 1) ? 1.0 : 0.0;
            case RuleCustom: return custom ? 1.0 : 0.0;
            case RuleRandom: return hash (idx, 99) > 0.5 ? 1.0 : 0.0;
            default: return 0.0;
        }
    }

    void controlTick()
    {
        smoothAll();
        const double S = xen::stretchFactor (std::max (1e-9, s.seed));
        const double twoPiOverSr = 2.0 * 3.141592653589793 / sr;
        const double fadeTop = 0.45 * sr, fadeBottom = 0.40 * sr;

        masterGain = (float) std::pow (10.0, s.gainDb / 20.0);
        // Times may be 0 ms (user request): then the stage lasts one sample (a click is accepted).
        attackStep = (float) (1.0 / std::max (1.0, s.attackMs * 0.001 * sr));
        decayCoef = s.decayMs <= 0.0 ? 0.0f : (float) std::exp (std::log (0.001) / std::max (1.0, s.decayMs * 0.001 * sr));
        releaseCoef = s.releaseMs <= 0.0 ? 0.0f : (float) std::exp (std::log (0.001) / std::max (1.0, s.releaseMs * 0.001 * sr));
        sustainLevel = (float) std::clamp (s.sustain, 0.0, 1.0);
        const double glideCoef = s.glideMs <= 0.5 ? 0.0 : std::exp (-tickSec / (s.glideMs * 0.001));
        const float fenvCoef = (float) std::exp (-tickSec / std::max (0.001, s.filterDecayMs * 0.001));
        const float noiseAmp = s.noiseDb <= -59.9 ? 0.0f : (float) (0.5 * std::pow (10.0, s.noiseDb / 20.0));
        numActiveBus = 0;
        for (int b = 0; b < kBuses; ++b)
            if (s.busOn[(size_t) b]) activeBus[(size_t) numActiveBus++] = b;
        hasSends = numActiveBus > 0;

        // EQ: bands that do something; key-tracked ones are recomputed per voice, the rest once.
        std::array<pvst::eq::Coefs, kEqBands> eqStatic {};
        std::array<bool, kEqBands> eqLive {};
        for (int b = 0; b < kEqBands; ++b)
        {
            const auto ub = (size_t) b;
            eqLive[ub] = s.eq[ub].active();
            if (eqLive[ub]) eqStatic[ub] = pvst::eq::makeCoefs (s.eq[ub], s.eq[ub].freq, sr);
        }
        const double eqAnchor = 1200.0 * std::log2 (s.fundamentalHz / kC4);

        // Wander: every partial walks its own bounded random path (shared by all voices).
        if (s.wanderCents > 1e-3)
            for (int i = 1; i < kOsc; ++i)
            {
                auto& w = wanderState[(size_t) i];
                const double r = s.wanderRate * (0.5 + hash (i, 11));
                w += (double) rnd (wanderRng) * std::sqrt (r * tickSec) * 1.2 - w * r * tickSec * 0.6;
                w = std::clamp (w, -1.0, 1.0);
            }

        // Shared (voice-independent) partial table: the calculator's edited partials.
        std::array<double, kOsc> cents {}, calcAmp {}, panL {}, panR {};
        std::array<std::array<double, kOsc>, kBuses> busW {};
        std::array<bool, kOsc> isCustom {};
        for (int i = 0; i < kOsc; ++i)
        {
            const auto ii = (size_t) i;
            if (i < kSeries)
            {
                const double pos = i == 0 ? 0.0 : s.edits.posCents[ii];
                cents[ii] = 1200.0 * S * log2N[ii] + pos;
                calcAmp[ii] = i < s.partials ? std::min (4.0, xen::rolloffShape (i + 1, s.rolloff) * xen::dbToGain (s.edits.levelDb[ii])) : 0.0;
            }
            else
            {
                const auto k = ii - kSeries;
                isCustom[ii] = true;
                cents[ii] = s.edits.customCents[k];
                calcAmp[ii] = s.edits.customOn[k] ? xen::dbToGain (s.edits.customLevelDb[k]) : 0.0;
            }
            // Synth-side shaping independent of pitch: odd/even balance.
            if (! isCustom[ii] && std::abs (s.oddEven) > 1e-6 && i > 0)
            {
                const bool odd = (i + 1) % 2 == 1;
                if (s.oddEven > 0 && ! odd) calcAmp[ii] *= 1.0 - s.oddEven;
                if (s.oddEven < 0 && odd) calcAmp[ii] *= 1.0 + s.oddEven;
            }
            if (i > 0) cents[ii] += s.wanderCents * wanderState[ii];
            for (int b = 0; b < kBuses; ++b)
                if (s.busOn[(size_t) b])
                {
                    // Morph between two neighbouring rules (e.g. Odd -> Even).
                    const double x = std::clamp (s.busPicks[(size_t) b], 0.0, 9.0);
                    const int r0 = (int) std::floor (x);
                    const double f = x - r0;
                    busW[(size_t) b][ii] = (1.0 - f) * sendRule (r0, i, isCustom[ii]) + (f > 0.0 ? f * sendRule (std::min (r0 + 1, 9), i, isCustom[ii]) : 0.0);
                }
            // Stereo spread: each partial gets a fixed random place in the stereo field.
            const double pan = s.spread * (hash (i, 3) * 2.0 - 1.0);
            const double ang = (pan + 1.0) * 0.25 * 3.141592653589793;
            panL[ii] = std::cos (ang) * 1.41421356;
            panR[ii] = std::sin (ang) * 1.41421356;
        }

        // Scramble: blend each series partial's loudness with another partial's (fixed shuffle).
        if (s.scramble > 1e-4)
        {
            std::array<double, kSeries> orig {};
            for (int i = 0; i < kSeries; ++i) orig[(size_t) i] = calcAmp[(size_t) i];
            const int n = std::max (1, s.partials);
            for (int i = 1; i < n; ++i)
            {
                const int j = 1 + (int) (hash (i, 23) * (n - 1)) % std::max (1, n - 1);
                calcAmp[(size_t) i] = orig[(size_t) i] + s.scramble * (orig[(size_t) std::min (j, n - 1)] - orig[(size_t) i]);
            }
        }

        for (auto& v : voices)
        {
            if (! v.active) continue;
            double keyCents, strength;
            keyTarget (v.note, keyCents, strength);
            v.keyRelCents = keyCents - 1200.0 * std::log2 (s.fundamentalHz / kC4);
            const double targetCents = keyCents + s.pitchCents + s.bendCents;
            v.cents = (v.fresh || glideCoef <= 0.0) ? targetCents : targetCents + (v.cents - targetCents) * glideCoef;
            const double f0 = kC4 * std::exp2 (v.cents / 1200.0);
            v.velGain = (float) std::pow (std::max (1e-4, (double) v.velocity), 2.0 * s.velocitySens);
            const float lifeTarget = (float) (1.0 - s.life * (1.0 - std::sqrt (strength)));
            v.lifeGain = v.fresh ? lifeTarget : v.lifeGain + (lifeTarget - v.lifeGain) * (float) (1.0 - smoothCoef);
            v.noiseAmp = noiseAmp;

            std::array<double, kOsc> tgt {};
            double power = 0.0;
            for (int p = 0; p < kOsc; ++p)
            {
                const auto ii = (size_t) p;
                double a = calcAmp[ii];
                double f = f0 * std::exp2 (cents[ii] / 1200.0) + s.fshiftHz;
                if (a > 0.0)
                {
                    const double af = std::abs (f);
                    if (std::abs (s.tiltDbOct) > 1e-6) a *= std::pow (10.0, s.tiltDbOct * (cents[ii] / 1200.0) / 20.0);
                    if (std::abs (s.formGainDb) > 1e-6)
                    {
                        const double x = std::log2 (std::max (af, 1.0) / s.formFreq) / std::max (0.05, s.formWidth);
                        a *= std::pow (10.0, s.formGainDb * std::exp (-0.5 * x * x) / 20.0);
                    }
                    if (s.shimmer > 1e-6)
                    {
                        v.shimPhase[ii] += (float) (6.2831853 * s.shimmerRate * (0.6 + 0.8 * hash (p, 5)) * tickSec);
                        if (v.shimPhase[ii] > 6.2831853f) v.shimPhase[ii] -= 6.2831853f;
                        a *= 1.0 - s.shimmer * (0.5 + 0.5 * std::sin ((double) v.shimPhase[ii]));
                    }
                    a *= std::clamp ((fadeTop - af) / (fadeTop - fadeBottom), 0.0, 1.0); // Nyquist guard
                    a *= std::clamp ((af - 10.0) / 10.0, 0.0, 1.0);                      // sub-audio fade
                }
                tgt[ii] = a;
                power += a * a;
                const double w = std::clamp (f * twoPiOverSr, -3.141592653589793, 3.141592653589793); // negative = through-zero
                v.c[ii] = (float) std::cos (w);
                v.s[ii] = (float) std::sin (w);
                v.panL[ii] = (float) panL[ii];
                v.panR[ii] = (float) panR[ii];
                for (int k = 0; k < numActiveBus; ++k) { const auto b = (size_t) activeBus[(size_t) k]; v.w[b][ii] = (float) busW[b][ii]; }
            }
            const double norm = power > 1e-12 ? 1.0 / std::sqrt (power) : 0.0;

            int highest = 0;
            for (int p = 0; p < kOsc; ++p)
            {
                const auto ii = (size_t) p;
                v.ampSm[ii] = v.fresh ? (float) (tgt[ii] * norm) : (float) smooth (v.ampSm[ii], tgt[ii] * norm);
                if (v.ampSm[ii] < 1e-6f && tgt[ii] <= 0.0) v.ampSm[ii] = 0.0f;
                v.ampStep[ii] = (v.ampSm[ii] - v.amp[ii]) / (float) kControlBlock;
                if (v.ampSm[ii] > 0.0f || v.amp[ii] > 0.0f) highest = p + 1;
                const float mag = std::sqrt (v.re[ii] * v.re[ii] + v.im[ii] * v.im[ii]);
                if (mag > 0.0f) { v.re[ii] /= mag; v.im[ii] /= mag; } else { v.re[ii] = 1.0f; v.im[ii] = 0.0f; }
            }
            v.numActive = highest;

            // Filter: cutoff follows key (keyTrack) and its own decaying envelope.
            if (s.filterMode != FilterOff)
            {
                double fc = s.cutoff * std::exp2 (s.keyTrack * v.cents / 1200.0) * std::exp2 (s.filterEnvOct * v.fenv);
                fc = std::clamp (fc, 20.0, 0.45 * sr);
                v.fg = (float) std::tan (3.141592653589793 * fc / sr);
                v.fk = (float) (2.0 - 1.96 * std::clamp (s.reso, 0.0, 1.0));
                v.fa1 = 1.0f / (1.0f + v.fg * (v.fg + v.fk));
                v.fa2 = v.fg * v.fa1;
                v.fa3 = v.fg * v.fa2;
            }
            // EQ: the band's frequency follows the note by keyTrack (0 = fixed, 1 = one octave up per octave).
            v.eqNum = 0;
            for (int b = 0; b < kEqBands; ++b)
            {
                const auto ub = (size_t) b;
                if (! eqLive[ub])
                {
                    for (auto& sec : v.eqSt[ub]) for (auto& ch : sec) ch = pvst::eq::State {}; // no stale state when re-enabled
                    continue;
                }
                v.eqc[ub] = s.eqKeyTrack[ub] > 1e-4
                              ? pvst::eq::makeCoefs (s.eq[ub], pvst::eq::trackedFreq (s.eq[ub].freq, s.eqKeyTrack[ub], v.cents - eqAnchor), sr)
                              : eqStatic[ub];
                v.eqIdx[(size_t) v.eqNum++] = b;
            }
            v.fenv *= fenvCoef;
            v.fresh = false;
        }
    }

    float svf (float x, float& ic1, float& ic2, const Voice& v) const
    {
        const float v3 = x - ic2;
        const float v1 = v.fa1 * ic1 + v.fa2 * v3;
        const float v2 = ic2 + v.fa2 * ic1 + v.fa3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        switch (s.filterMode)
        {
            case FilterLP: return v2;
            case FilterBP: return v1;
            case FilterHP: return x - v.fk * v1 - v2;
            case FilterNotch: return x - v.fk * v1;
            default: return x;
        }
    }

    template <bool Sends>
    void renderVoice (Voice& v, float* dL, float* dR, float* const* buses, int offset, int n)
    {
        const int P = v.numActive;
        float* re = v.re.data(); float* im = v.im.data();
        const float* c = v.c.data(); const float* s_ = v.s.data();
        float* amp = v.amp.data(); const float* step = v.ampStep.data();
        const float* pl = v.panL.data(); const float* pr = v.panR.data();
        const float gain = v.velGain * v.lifeGain * masterGain;
        const bool filt = s.filterMode != FilterOff;
        alignas (32) float xs[kOsc];

        for (int i = 0; i < n; ++i)
        {
            if (v.stage == Voice::Attack)
            {
                v.env += attackStep;
                if (v.env >= 1.0f) { v.env = 1.0f; v.stage = Voice::Decay; }
            }
            else if (v.stage == Voice::Decay)
            {
                v.env = sustainLevel + (v.env - sustainLevel) * decayCoef;
                if (sustainLevel <= 0.0f && v.env < kSilence) { v.active = false; v.env = 0.0f; return; }
            }
            else
            {
                v.env *= releaseCoef;
                if (v.env < kSilence) { v.active = false; v.env = 0.0f; return; }
            }

            float sl = 0.0f, sr_ = 0.0f;
            for (int p = 0; p < P; ++p)
            {
                const float r = re[p] * c[p] - im[p] * s_[p];
                im[p] = re[p] * s_[p] + im[p] * c[p];
                re[p] = r;
                const float x = amp[p] * im[p];
                xs[p] = x;
                sl += x * pl[p];
                sr_ += x * pr[p];
                amp[p] += step[p];
            }
            if (v.noiseAmp > 0.0f) { sl += v.noiseAmp * rnd (v.rng); sr_ += v.noiseAmp * rnd (v.rng); }
            if (filt) { sl = svf (sl, v.f1L, v.f2L, v); sr_ = svf (sr_, v.f1R, v.f2R, v); }
            for (int e = 0; e < v.eqNum; ++e)
            {
                const auto b = (size_t) v.eqIdx[(size_t) e];
                const auto& cf = v.eqc[b];
                for (int k = 0; k < cf.n; ++k)
                {
                    sl = pvst::eq::tick (cf.s[(size_t) k], v.eqSt[b][(size_t) k][0], sl);
                    sr_ = pvst::eq::tick (cf.s[(size_t) k], v.eqSt[b][(size_t) k][1], sr_);
                }
            }
            const float g = v.env * gain;
            dL[i] += sl * g;
            dR[i] += sr_ * g;
            if constexpr (Sends)
                for (int k = 0; k < numActiveBus; ++k)
                {
                    const int bus = activeBus[(size_t) k];
                    const float* w = v.w[(size_t) bus].data();
                    float acc = 0.0f;
                    for (int p = 0; p < P; ++p) acc += xs[p] * w[p];
                    buses[bus][offset + i] += acc * g;
                }
        }
    }

    static float rnd (uint32_t& st)
    {
        st ^= st << 13; st ^= st >> 17; st ^= st << 5;
        return (float) (st & 0xffffff) / 8388608.0f - 1.0f;
    }

    /// A key pressed again while its voice is still HELD re-uses that voice. A voice that is
    /// already releasing is left alone to fade out (v0.5 fix: re-using it cut the release off
    /// instantly, which the user heard as notes "just stopping").
    Voice* findVoiceForNote (int note)
    {
        for (auto& v : voices) if (v.active && v.note == note && v.stage != Voice::Release) return &v;
        return nullptr;
    }
    Voice* findFreeVoice()
    {
        for (auto& v : voices) if (! v.active) return &v;
        return nullptr;
    }
    Voice* stealVoice()
    {
        Voice* best = nullptr;
        for (auto& v : voices) if (v.stage == Voice::Release && (best == nullptr || v.age < best->age)) best = &v;
        if (best) return best;
        for (auto& v : voices) if (best == nullptr || v.age < best->age) best = &v;
        return best;
    }

    double sr = 48000.0, tickSec = kControlBlock / 48000.0, smoothCoef = 0.98;
    EngineParams target, s;
    std::array<double, kSeries> log2N {};
    std::array<double, kOsc> wanderState {};
    uint32_t wanderRng = 777u;
    std::array<Voice, kMaxVoices> voices {};
    std::array<double, kMaxPlayable> playCents {}, playStrength {};
    int numPlayable = 0;
    double scalePeriod = 1200.0, maxStrength = 1.0;
    uint64_t ageCounter = 0;
    int tickCountdown = 0;
    bool hasSends = false;
    std::array<int, kBuses> activeBus {};
    int numActiveBus = 0;
    std::array<int, 32> pendingHits {};
    int numHits = 0;
    float masterGain = 0.25f, attackStep = 0.001f, decayCoef = 0.999f, releaseCoef = 0.999f, sustainLevel = 0.6f;
};
} // namespace seed
