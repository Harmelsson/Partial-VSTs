#pragma once
// Sane's DSP building blocks. Plain C++ (no JUCE), used by the audio engine AND the UI (the UI
// draws the exact same burst / mode positions the engine plays, by calling the same functions).
//
//   Rng            deterministic noise (a seed fully defines a burst, so the UI can redraw it)
//   modes          physical mode tables + the Calc/File/Linked sources + Bessel strike weights
//   BurstGen       the noise exciter (one burst per note)
//   InsertState    per-loop state of one insert slot (filter, fine delay, disperse, ...)
//
// Background for a future agent: docs/DRUM_RESEARCH.md (why feedback loops, why these tables).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace sane::dsp
{
inline constexpr double kPi = 3.14159265358979323846;
inline constexpr int kMaxModes = 16;

// ------------------------------------------------------------------------------------------------
// Small helpers
// ------------------------------------------------------------------------------------------------

/// Cheap tanh (Pade), exact enough for saturation; clamps beyond +-3.
inline float fastTanh (float x) noexcept
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/// sin(2*pi*x) for x in [0, 1): cheap polynomial, plenty for LFOs (error < 0.1 %).
inline float fastSin2Pi (float x) noexcept
{
    x -= std::floor (x);
    const float t = x < 0.5f ? x : x - 0.5f;          // half cycle 0..0.5
    const float y = 16.0f * t * (0.5f - t);             // Bhaskara-like parabola, 0..1
    const float r = y * (0.775f + 0.225f * y);          // refinement
    return x < 0.5f ? r : -r;
}

/// A rotating phasor: e^(i*2*pi*inc*n) without per-sample sin/cos. The rotation is recomputed only when the
/// increment changes (parameter / pitch changes), the magnitude is renormalised every 256 samples.
struct Phasor
{
    float re = 1.0f, im = 0.0f, rr = 1.0f, ri = 0.0f, lastInc = -1e9f;
    int n = 0;
    void step (float inc) noexcept
    {
        if (std::abs (inc - lastInc) > 1e-12f) { lastInc = inc; rr = std::cos (6.2831853f * inc); ri = std::sin (6.2831853f * inc); }
        const float a = re * rr - im * ri, b = re * ri + im * rr;
        re = a; im = b;
        if (++n >= 256) { n = 0; const float m = 1.0f / std::sqrt (re * re + im * im); re *= m; im *= m; }
    }
};

/// The loop guard: transparent below ~1, never above +-kGuard. Every loop passes through it, so even a
/// loop gain > 1 (self-oscillation, resonant filters, folders) can't grow without bound.
inline constexpr float kGuard = 2.5f;
inline float guard (float x) noexcept { return kGuard * fastTanh (x * (1.0f / kGuard)); }

/// xorshift32: fast, deterministic. next() in [-1, 1), uni() in [0, 1).
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    explicit Rng (uint32_t seed = 1) noexcept : s (seed == 0 ? 0x9E3779B9u : seed) {}
    uint32_t bits() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() noexcept { return (float) (bits() >> 8) * (1.0f / 16777216.0f); }
    float next() noexcept { return uni() * 2.0f - 1.0f; }
};

/// Bessel function of the first kind J_m(x) by its power series (fine for x < ~15, which is all we use).
inline double besselJ (int m, double x) noexcept
{
    double fk = 1.0, fkm = 1.0, s = 0.0;
    for (int i = 2; i <= m; ++i) fkm *= i;
    for (int k = 0; k < 40; ++k)
    {
        if (k > 0) { fk *= k; fkm *= (k + m); }
        const double term = ((k % 2) != 0 ? -1.0 : 1.0) / (fk * fkm) * std::pow (x / 2.0, 2 * k + m);
        s += term;
        if (std::abs (term) < 1e-12 && k > 4) break;
    }
    return s;
}

// ------------------------------------------------------------------------------------------------
// Modes: where the core's resonances sit (ratios to the played pitch)
// ------------------------------------------------------------------------------------------------
namespace modes
{
    // Ideal circular membrane: zeros j_mn of the Bessel functions, sorted. ratio = j_mn / j_01.
    // (Order m, zero j) — m decides how a strike position excites the mode (see strikeWeight).
    struct MemMode { int m; double j; };
    inline constexpr std::array<MemMode, kMaxModes> kMembrane { {
        { 0, 2.4048 }, { 1, 3.8317 }, { 2, 5.1356 }, { 0, 5.5201 }, { 3, 6.3802 }, { 1, 7.0156 }, { 4, 7.5883 }, { 2, 8.4172 },
        { 0, 8.6537 }, { 5, 8.7715 }, { 3, 9.7610 }, { 6, 9.9361 }, { 1, 10.1735 }, { 4, 11.0647 }, { 7, 11.0864 }, { 2, 11.6198 } } };
    // Timpani: kettle + air pull the "rocking" modes (11, 21, 31...) near 1 : 1.5 : 2 : 2.5 (Rossing / Munroe).
    inline constexpr std::array<double, kMaxModes> kTimpani { 1, 1.5, 1.98, 2.44, 2.9, 3.36, 3.8, 4.24, 4.68, 5.1, 5.55, 6.0, 6.42, 6.85, 7.3, 7.75 };
    // Tabla / mridangam: the loaded head makes the first modes harmonic (Raman 1920); higher ones drift.
    inline constexpr std::array<double, kMaxModes> kTabla { 1, 2, 3, 4, 5, 5.86, 6.62, 7.4, 8.1, 8.85, 9.6, 10.4, 11.1, 11.9, 12.6, 13.4 };
    // Circular plate (free edge, approximate): dense, cymbal/gong-like.
    inline constexpr std::array<double, kMaxModes> kPlate { 1, 1.73, 2.33, 3.0, 3.91, 4.4, 5.2, 5.9, 6.6, 7.6, 8.4, 9.3, 10.1, 11.0, 12.2, 13.3 };
    // Partial list used by the File source until .scl/partial-list import exists (a gamelan-ish set).
    inline constexpr std::array<double, kMaxModes> kFileDemo { 1, 2.32, 2.76, 3.91, 4.62, 5.43, 6.1, 6.95, 7.6, 8.4, 9.2, 10.1, 11.3, 12.0, 13.1, 14.2 };

    /// Free-free bar (slit drum, marimba, woodblock): 1 : 2.756 : 5.404 : 8.933 ...
    /// beta_n L = 4.7300, 7.8532, 10.9956, then (2n+1) pi / 2; frequency ~ beta^2.
    inline double barRatio (int i) noexcept
    {
        const double beta = i == 0 ? 4.7300 : (i == 1 ? 7.8532 : (2.0 * i + 3.0) * kPi * 0.5);
        return (beta / 4.7300) * (beta / 4.7300);
    }

    /// Mode i of a physical table (0-based).
    inline double physical (int table, int i) noexcept
    {
        i = std::clamp (i, 0, kMaxModes - 1);
        switch (table)
        {
            case 1: return kTimpani[(size_t) i];
            case 2: return kTabla[(size_t) i];
            case 3: return barRatio (i);
            case 4: return kPlate[(size_t) i];
            default: return kMembrane[(size_t) i].j / kMembrane[0].j;
        }
    }

    /// Mode i from a source (Physical / Calc / File / Linked). Linked falls back to Calc until link groups exist.
    inline double fromSource (int source, int table, double calcSeed, int i) noexcept
    {
        switch (source)
        {
            case 0: return physical (table, i);
            case 2: return kFileDemo[(size_t) std::clamp (i, 0, kMaxModes - 1)];
            default: return std::pow ((double) (i + 1), std::log2 (std::max (0.01, calcSeed))); // Calc = Seed's n^log2(seed)
        }
    }

    /// The morph: geometric interpolation (in cents) between endpoint A and B.
    inline double morphed (double a, double b, double t) noexcept
    {
        return std::exp ((1.0 - t) * std::log (std::max (1e-6, a)) + t * std::log (std::max (1e-6, b)));
    }

    /// How strongly a strike at radius r (0 centre .. 1 rim) excites mode i of a table.
    /// Membrane-like tables use the real mode shapes |J_m(j_mn * r)|; a bar uses a cosine shape along its length.
    inline double strikeWeight (int table, int i, double r) noexcept
    {
        i = std::clamp (i, 0, kMaxModes - 1);
        r = std::clamp (r, 0.0, 0.99);
        double w;
        if (table == 3) // bar: centre = middle of the bar, rim = its end
            w = std::abs (std::cos ((i + 1) * kPi * 0.5 * (1.0 - r) + (i % 2) * kPi * 0.5));
        else if (table == 1) // timpani: rocking modes m = i + 1 (first zero of J_m)
        {
            static constexpr double j1[kMaxModes] = { 3.8317, 5.1356, 6.3802, 7.5883, 8.7715, 9.9361, 11.0864, 12.2251, 13.3543, 14.4755, 15.5898, 16.6983, 17.8014, 18.9000, 19.9944, 21.0851 };
            w = std::abs (besselJ (i + 1, j1[i] * r)) / 0.58;
        }
        else
        {
            const auto& md = kMembrane[(size_t) i];
            w = std::abs (besselJ (md.m, md.j * r)) / (md.m == 0 ? 1.0 : 0.58);
        }
        return std::max (0.03, std::min (1.0, w));
    }
} // namespace modes

// ------------------------------------------------------------------------------------------------
// The noise exciter
// ------------------------------------------------------------------------------------------------

/// Settings of ONE burst (after the per-note randomisation has been applied).
struct BurstSettings
{
    uint32_t seed = 1;
    float lengthMs = 5.0f;  // decay time constant-ish
    float color = 0.45f;    // 0 dark .. 1 white
    float grain = 0.15f;    // crackle density
    float level = 1.0f;     // linear
    float spread = 0.5f;    // 0 = same noise both sides, 1 = independent
};

/// Generates a burst sample by sample: fast attack (0.3 ms), exponential decay. Stereo (two related noises).
class BurstGen
{
public:
    void start (const BurstSettings& s, double sampleRate) noexcept
    {
        set = s;
        rng = Rng (s.seed);
        rng2 = Rng (s.seed * 2654435761u + 7u);
        sr = sampleRate;
        n = 0;
        attackN = std::max (1, (int) (0.0003 * sr));
        decay = (float) std::exp (-1.0 / std::max (1.0, 0.001 * s.lengthMs * sr * 0.45));
        env = 0.0f;
        lp1 = lp2 = 0.0f;
        // colour -> one-pole low-pass coefficient: 0 = very dark (~150 Hz), 1 = white
        coef = (float) (0.02 * std::pow (50.0, std::clamp ((double) s.color, 0.0, 1.0)));
        coef = std::min (1.0f, coef * (float) (48000.0 / sr));
        active = true;
    }

    bool isActive() const noexcept { return active; }

    /// Next stereo sample pair.
    void next (float& l, float& r) noexcept
    {
        if (! active) { l = r = 0.0f; return; }
        if (n < attackN) env = (float) (n + 1) / (float) attackN;
        else env *= decay;
        ++n;
        float a = rng.next(), b = rng2.next();
        if (set.grain > 0.0f && rng.uni() < set.grain * 0.03f) a += (rng.next() > 0.0f ? 1.6f : -1.6f);
        lp1 += coef * (a - lp1);
        lp2 += coef * (b - lp2);
        // darker noise is quieter after the low-pass: make up roughly so colour doesn't change loudness much
        const float makeup = 1.0f / std::sqrt (std::max (0.02f, coef));
        const float m = lp1 * makeup, o = lp2 * makeup;
        const float k = env * set.level * 0.7f;
        l = k * m;
        r = k * (m * (1.0f - set.spread) + o * set.spread);
        if (n > attackN && env < 1e-4f) active = false;
    }

private:
    BurstSettings set;
    Rng rng, rng2;
    double sr = 48000.0;
    int n = 0, attackN = 14;
    float env = 0.0f, decay = 0.99f, coef = 0.5f, lp1 = 0.0f, lp2 = 0.0f;
    bool active = false;
};

// ------------------------------------------------------------------------------------------------
// Insert slots (run INSIDE each loop, once per loop line)
// ------------------------------------------------------------------------------------------------

/// Per-block coefficients of one slot, shared by all lines of all voices (line-dependent parts are
/// computed by the engine per line, see InsertState::process arguments).
struct SlotCoeffs
{
    int type = 0;
    float a = 0.5f, b = 0.5f, c = 1.0f;     // raw knobs 0..1
    // filter (TPT state-variable)
    float g = 0.1f, k = 1.4f, h = 0.5f, wLp = 1.0f, wBp = 0.0f, wHp = 0.0f;
    // fine delay / chorus
    float delaySamples = 0.0f, followPeriods = 0.0f, wobbleSamples = 0.0f, lfoInc = 0.0f;
    // disperse
    float apCoef = 0.0f; int apStages = 1;
    // saturate / fold
    float drive = 1.0f, bias = 0.0f, biasOut = 0.0f;
    // freq shift
    float shiftInc = 0.0f;
    // ring mod
    float ringRatio = 1.0f; bool ringFollow = true; float ringFixedInc = 0.0f;
    // flip
    float flipChance = 0.0f;
    float mix = 1.0f;
    float spreadAmt = 0.0f;   // in-loop freq shift / chorus / ring mod: how differently each line is detuned
    bool inLoop = true;
};

/// Per-line state of one slot. Buffer memory comes from the engine's pool (no allocation on the audio thread).
struct InsertState
{
    // filter
    float s1 = 0.0f, s2 = 0.0f;
    // disperse: up to 12 first-order all-passes
    std::array<float, 12> ap {};
    // hilbert (freq shift): 2 x 4 second-order sections in z^-2 form + one-sample delay on path 1
    std::array<float, 8> hx1 {}, hx2 {}, hy1 {}, hy2 {};
    std::array<float, 8> hz1 {}, hz2 {}, hw1 {}, hw2 {};
    float hDelay = 0.0f, phase = 0.0f, lfo = 0.0f;
    Phasor rot;   // freq shift / ring mod carrier
    // delay buffer (fine delay / chorus)
    float* buf = nullptr;
    int mask = 0, w = 0;
    Rng rng { 12345 };

    void clear() noexcept
    {
        s1 = s2 = 0.0f;
        ap.fill (0.0f);
        hx1.fill (0.0f); hx2.fill (0.0f); hy1.fill (0.0f); hy2.fill (0.0f);
        hz1.fill (0.0f); hz2.fill (0.0f); hw1.fill (0.0f); hw2.fill (0.0f);
        hDelay = 0.0f;
        rot = Phasor();
        if (buf != nullptr) std::fill (buf, buf + mask + 1, 0.0f);
        w = 0;
    }

    float readLinear (float d) const noexcept
    {
        d = std::clamp (d, 1.0f, (float) (mask - 2));
        const int i = (int) d;
        const float f = d - (float) i;
        const float x0 = buf[(w - i) & mask], x1 = buf[(w - i - 1) & mask];
        return x0 + f * (x1 - x0);
    }

    /// One sample through the slot. `period` = this line's period in samples (for "follow pitch"),
    /// `lineInc` = this line's frequency as phase increment (cycles/sample), `lfoOffset` decorrelates lines.
    float process (float x, const SlotCoeffs& c, float period, float lineInc, float spread) noexcept
    {
        float y = x;
        switch (c.type)
        {
            case 1: // filter: TPT SVF, blend LP / BP(normalised) / HP
            {
                const float hp = (x - (c.k + c.g) * s1 - s2) * c.h;
                const float v1 = c.g * hp;
                const float bp = v1 + s1;
                s1 = bp + v1;
                const float v2 = c.g * bp;
                const float lp = v2 + s2;
                s2 = lp + v2;
                y = c.wLp * lp + c.wBp * (c.k * bp) + c.wHp * hp;
                break;
            }
            case 2: // fine delay
            {
                if (buf == nullptr) break;
                buf[w] = x;
                float d = c.followPeriods > 0.0f ? c.followPeriods * period : c.delaySamples;
                lfo += c.lfoInc; if (lfo >= 1.0f) lfo -= 1.0f;
                d += c.wobbleSamples * (0.5f + 0.5f * fastSin2Pi (lfo));
                y = d < 1.0f ? x : readLinear (d);
                w = (w + 1) & mask;
                break;
            }
            case 3: // disperse: chain of first-order all-passes y = a*x + s; s = x - a*y
            {
                const float a = c.apCoef;
                float v = x;
                for (int i = 0; i < c.apStages; ++i)
                {
                    const float o = a * v + ap[(size_t) i];
                    ap[(size_t) i] = v - a * o;
                    v = o;
                }
                y = v;
                break;
            }
            case 4: // saturate. In the loop: small-signal gain 1 (else the loop explodes), loud parts squashed — the loop
                    // level is calibrated to ~1.0, so DRIVE really bites. After the loop: drive pushes the (quieter) output.
                if (c.inLoop) y = (fastTanh (c.drive * x + c.bias) - c.biasOut) / c.drive;
                else y = (fastTanh (c.drive * x * 3.0f + c.bias) - c.biasOut) / (3.0f * std::sqrt (c.drive));
                y = x + c.mix * (y - x);
                break;
            case 5: // freq shift: Hilbert pair (Niemitalo coefficients) + complex rotation
            {
                static constexpr float A1[4] = { 0.4794074f, 0.8762103f, 0.9765983f, 0.9974992f };   // squared coefficients, path 1
                static constexpr float A2[4] = { 0.1617585f, 0.7330289f, 0.9453497f, 0.9905991f };   // path 2
                float p = x, q = x;
                for (int i = 0; i < 4; ++i)
                {
                    const float o = A1[i] * (p + hy2[(size_t) i]) - hx2[(size_t) i];
                    hx2[(size_t) i] = hx1[(size_t) i]; hx1[(size_t) i] = p;
                    hy2[(size_t) i] = hy1[(size_t) i]; hy1[(size_t) i] = o;
                    p = o;
                    const float o2 = A2[i] * (q + hw2[(size_t) i]) - hz2[(size_t) i];
                    hz2[(size_t) i] = hz1[(size_t) i]; hz1[(size_t) i] = q;
                    hw2[(size_t) i] = hw1[(size_t) i]; hw1[(size_t) i] = o2;
                    q = o2;
                }
                const float re = hDelay; hDelay = p;
                rot.step (c.shiftInc * (1.0f + c.spreadAmt * spread));
                const float sh = re * rot.re - q * rot.im;
                y = x + c.mix * (sh - x);
                break;
            }
            case 6: // chorus: moving delay around `depth`
            {
                if (buf == nullptr) break;
                buf[w] = x;
                lfo += c.lfoInc * (1.0f + 0.6f * c.spreadAmt * spread); if (lfo >= 1.0f) lfo -= 1.0f;
                const float d = 1.0f + c.wobbleSamples * (1.0f + fastSin2Pi (lfo));
                const float wet = readLinear (d);
                w = (w + 1) & mask;
                y = x + c.mix * (wet - x);
                break;
            }
            case 7: // fold: triangle folder, scaled back so small signals pass at gain ~1
            {
                const float v = c.drive * x + c.bias;
                const float t = v * 0.25f + 0.25f;
                const float fl = 4.0f * std::abs (t - std::floor (t + 0.5f)) - 1.0f; // triangle in [-1, 1], = v near 0
                const float folded = (-fl - c.biasOut) / c.drive;
                y = x + c.mix * (folded - x);
                break;
            }
            case 8: // ring mod
            {
                rot.step ((c.ringFollow ? lineInc * c.ringRatio : c.ringFixedInc) * (1.0f + 0.1f * c.spreadAmt * spread));
                const float rm = x * rot.re;
                y = x + c.mix * (rm - x);
                break;
            }
            case 9: // flip: random sign (Karplus-Strong drum)
            {
                const float f = rng.uni() < c.flipChance ? -x : x;
                y = x + c.mix * (f - x);
                break;
            }
            default: break;
        }
        return y;
    }
};

/// Fill the per-block coefficients of a slot from its knobs.
inline SlotCoeffs makeSlotCoeffs (int type, float a, float b, float c, double sr, bool inLoop = true) noexcept
{
    SlotCoeffs k;
    k.type = type; k.a = a; k.b = b; k.c = c;
    k.inLoop = inLoop;
    const double nyq = 0.5 * sr;
    switch (type)
    {
        case 1:
        {
            const double fc = std::min (0.45 * sr, 20.0 * std::pow (1000.0, (double) a));
            k.g = (float) std::tan (kPi * fc / sr);
            k.k = 2.0f - 1.9f * b;                // resonance: k 2 (none) .. 0.1 (sharp peak)
            k.h = 1.0f / (1.0f + k.k * k.g + k.g * k.g);
            const float m = std::clamp (c, 0.0f, 1.0f) * 2.0f; // 0 LP .. 1 BP .. 2 HP
            k.wLp = std::max (0.0f, 1.0f - m);
            k.wBp = 1.0f - std::abs (m - 1.0f);
            k.wHp = std::max (0.0f, m - 1.0f);
            break;
        }
        case 2:
            k.delaySamples = (float) (0.005 * a * a * sr);
            k.followPeriods = b < 0.02f ? 0.0f : b * 2.0f;
            k.wobbleSamples = (float) (0.0002 * c * sr);
            k.lfoInc = (float) (0.7 / sr);
            break;
        case 3:
            k.apCoef = (c < 0.5f ? 1.0f : -1.0f) * a * 0.9f * (std::abs (c - 0.5f) * 2.0f * 0.7f + 0.3f);
            k.apStages = 1 + (int) std::lround (b * 11.0f);
            break;
        case 4:
            k.drive = 1.0f + 39.0f * a * a;
            k.bias = (b - 0.5f) * 1.2f;
            k.biasOut = fastTanh (k.bias);
            k.mix = c;
            break;
        case 5:
        {
            const double range = 1.0 + 199.0 * b * b;
            k.shiftInc = (float) (((a - 0.5) * 2.0) * range / sr);
            k.mix = inLoop ? 1.0f : c;        // in the loop: always fully wet, C = spread (see slotCIsSpread)
            k.spreadAmt = inLoop ? c : 0.0f;
            break;
        }
        case 6:
            k.wobbleSamples = (float) (0.001 * a * a * sr);
            k.lfoInc = (float) (0.05 * std::pow (200.0, (double) b) / sr);
            k.mix = inLoop ? 1.0f : c;
            k.spreadAmt = inLoop ? c : 0.0f;
            break;
        case 7:
        {
            k.drive = 1.0f + 19.0f * a;
            k.bias = (b - 0.5f) * 1.6f;
            const float t = k.bias * 0.25f + 0.25f;
            k.biasOut = -(4.0f * std::abs (t - std::floor (t + 0.5f)) - 1.0f);
            k.mix = c;
            break;
        }
        case 8:
            k.ringRatio = 0.25f + 7.75f * a * a;
            k.ringFollow = b >= 0.5f;
            k.ringFixedInc = (float) (110.0 * k.ringRatio / sr);
            k.mix = inLoop ? 1.0f : c;
            k.spreadAmt = inLoop ? c : 0.0f;
            break;
        case 9:
            k.flipChance = a * 0.5f;
            k.mix = c;
            break;
        default: break;
    }
    (void) nyq;
    return k;
}
} // namespace sane::dsp
