#pragma once
// The dissonance-curve method (William Sethares, "Tuning, Timbre, Spectrum, Scale"):
// finding scale notes from a sound's own partials.
//
// C++ copy of ArtHar's crates/calculator/src/dissonance.rs (arthar commit 8bf556f).
// Same constants, same dip rules, same strength measure, same golden-section refinement.
// Deliberate differences (documented so nobody "fixes" them back by accident):
//   1. Pairs of sines whose roughness is provably negligible (s*diff > 6, i.e. < 1e-9 of a full
//      clash) are skipped after sorting the partials by frequency. Changes values by < 1e-6
//      of the curve's swing; makes 64-partial curves fast enough to recompute while scrubbing.
//   2. The coarse grid step grows with the period so a curve never has more than ~2000 points
//      (ArtHar: always 0.5 cents). Dips are still refined to ~1e-7 cents, so positions match;
//      only ripples narrower than the grid step can be missed.
//   3. Seeds below 1 (negative stretch) use |period| as the search range, so the mirrored
//      (undertone) series gets a scale too. ArtHar leaves that case undefined.
//
// THE IDEA
//   Two close sine tones "beat" and sound rough; Plomp & Levelt measured how rough, Sethares
//   fitted a formula. Play a sound together with a copy of itself transposed by some interval,
//   add up the roughness of every pair of partials: that's how dissonant the interval is FOR
//   THIS SOUND. Sweep the interval from unison to the "octave" and the dips of that curve are
//   the natural scale notes. Harmonic sounds dip at 3/2, 4/3, 5/4...; stretched sounds dip
//   somewhere new.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

#include "xen/Stretch.h" // centsToRatio

namespace xen
{
struct SinePartial
{
    double hz = 0.0;
    double amp = 0.0; // 0..1
};

// Sethares' fitted constants for the Plomp-Levelt curve. Measured hearing: do not tweak.
inline constexpr double kDStar = 0.24;  // roughness peak, as a fraction of the critical band
inline constexpr double kS1 = 0.0207;   // critical band growth with frequency (slope)
inline constexpr double kS2 = 18.96;    // critical band size at low frequencies (Hz)
inline constexpr double kB1 = 3.51;     // rise rate
inline constexpr double kB2 = 5.75;     // fall rate
inline constexpr double kC = 5.0;       // overall scale (only relative values matter)

inline double bandScale (double fMin) noexcept { return kDStar / (kS1 * fMin + kS2); }

/// Roughness of two sines `diff` Hz apart, lower one with band scale `s`, quieter one at `a`.
inline double roughness (double s, double diff, double a) noexcept
{
    const double x = s * diff;
    return a * kC * (std::exp (-kB1 * x) - std::exp (-kB2 * x));
}

/// Roughness between two sines (frequencies in Hz, loudness 0..1).
inline double pairDissonance (double f1, double a1, double f2, double a2) noexcept
{
    return roughness (bandScale (std::min (f1, f2)), std::abs (f2 - f1), std::min (a1, a2));
}

/// One scale note found by the calculator.
struct ScaleNote
{
    enum Kind { Unison, Found, Period };
    double cents = 0.0;
    double strength = 0.0; // 0..1, how clear the dip is (fraction of the curve's total swing)
    Kind kind = Found;
};

/// The whole calculator result for one set of partials.
struct CurveResult
{
    std::vector<double> cents;      // curve x (from 0 to |period| + overshoot)
    std::vector<double> dissonance; // curve y (raw)
    std::vector<ScaleNote> notes;   // unison, found notes (rising), period
    double periodCents = 0.0;
    bool valid = false;             // false if there is no period to search (seed ~ 1)
};

class DissonanceCalculator
{
public:
    /// How far past the period the curve is sampled, so a dip exactly at the period has a rise
    /// on both sides (same as ArtHar).
    static constexpr double kOvershootCents = 50.0;
    static constexpr int kMaxGridPoints = 2000;

    /// Total roughness of the sound played with a copy of itself transposed by `ratio`.
    double dissonanceAt (const std::vector<SinePartial>& partials, double ratio)
    {
        combined.clear();
        for (auto& p : partials) combined.push_back ({ p.hz, p.amp });
        for (auto& p : partials) combined.push_back ({ p.hz * ratio, p.amp });
        std::sort (combined.begin(), combined.end(), [] (auto& a, auto& b) { return a.hz < b.hz; });

        double total = 0.0;
        const size_t n = combined.size();
        for (size_t i = 0; i < n; ++i)
        {
            const double fi = combined[i].hz, ai = combined[i].amp, s = bandScale (fi);
            for (size_t j = i + 1; j < n; ++j)
            {
                const double x = s * (combined[j].hz - fi);
                if (x > 6.0) break; // sorted: every later pair is even further apart
                total += std::min (ai, combined[j].amp) * kC * (std::exp (-kB1 * x) - std::exp (-kB2 * x));
            }
        }
        return total;
    }

    /// Computes the curve and the scale. `minStrength` 0..1 (ArtHar default 0.02).
    /// `shouldStop` is polled so a newer request can abandon an old calculation (returns false).
    template <typename StopFn>
    bool compute (const std::vector<SinePartial>& partials, double periodCents, double minStrength,
                  CurveResult& out, StopFn&& shouldStop)
    {
        out = {};
        out.periodCents = periodCents;
        const double period = std::abs (periodCents);
        if (period < 10.0 || partials.empty()) return true; // no "octave" to search: seed ~ 1

        const double to = period + kOvershootCents;
        const double step = std::max (0.5, to / (double) kMaxGridPoints);
        const int steps = (int) std::round (to / step);

        out.cents.resize ((size_t) steps + 1);
        out.dissonance.resize ((size_t) steps + 1);
        for (int i = 0; i <= steps; ++i)
        {
            if ((i & 63) == 0 && shouldStop()) return false;
            const double c = i * step;
            out.cents[(size_t) i] = c;
            out.dissonance[(size_t) i] = dissonanceAt (partials, centsToRatio (c));
        }
        const auto& d = out.dissonance;
        const auto [mnIt, mxIt] = std::minmax_element (d.begin(), d.end());
        const double span = std::max (*mxIt - *mnIt, 1e-300);

        // 1. Grid points lower than both neighbours are dips (index 0 counts if the curve rises).
        std::vector<size_t> dips;
        for (size_t i = 0; i < d.size(); ++i)
        {
            const bool leftOk = i == 0 || d[i] < d[i - 1];
            const bool rightOk = i + 1 < d.size() && d[i] <= d[i + 1];
            if (leftOk && rightOk) dips.push_back (i);
        }

        // 2. Strength: how far the curve climbs before the next dip, on the lower side,
        //    relative to the whole curve's swing.
        auto strengthOf = [&] (size_t k) {
            const size_t i = dips[k];
            const size_t lo = k == 0 ? 0 : dips[k - 1];
            const size_t hi = k + 1 < dips.size() ? dips[k + 1] : d.size() - 1;
            const double leftPeak = *std::max_element (d.begin() + (long) lo, d.begin() + (long) i + 1);
            const double rightPeak = *std::max_element (d.begin() + (long) i, d.begin() + (long) hi + 1);
            const double peak = i == 0 ? rightPeak : std::min (leftPeak, rightPeak);
            return std::clamp ((peak - d[i]) / span, 0.0, 1.0);
        };

        // 3. Refine the dips that could be kept, then sort into unison / found / period.
        std::vector<std::pair<double, double>> found; // (cents, strength)
        double periodStrength = 0.0;
        for (size_t k = 0; k < dips.size(); ++k)
        {
            const size_t i = dips[k];
            if (i == 0) continue;
            const double strength = strengthOf (k);
            const bool nearPeriod = std::abs (out.cents[i] - period) <= step * 4.0;
            if (strength < minStrength && ! nearPeriod) continue;
            if (shouldStop()) return false;
            const double c = refineMinimum (partials, out.cents[i - 1], out.cents[std::min (i + 1, d.size() - 1)]);
            if (std::abs (c - period) <= step * 2.0) periodStrength = strength;
            else if (c < period && strength >= minStrength) found.push_back ({ c, strength });
        }
        std::sort (found.begin(), found.end());

        const double unisonStrength = ! dips.empty() && dips.front() == 0 ? strengthOf (0) : 0.0;
        out.notes.push_back ({ 0.0, unisonStrength, ScaleNote::Unison });
        for (auto& [c, s] : found) out.notes.push_back ({ c, s, ScaleNote::Found });
        out.notes.push_back ({ period, periodStrength, ScaleNote::Period });
        out.valid = true;
        return true;
    }

private:
    /// Golden-section search for the exact lowest point between lo and hi (cents).
    double refineMinimum (const std::vector<SinePartial>& partials, double lo, double hi)
    {
        auto f = [&] (double c) { return dissonanceAt (partials, centsToRatio (c)); };
        const double g = (std::sqrt (5.0) - 1.0) / 2.0;
        double x1 = hi - g * (hi - lo), x2 = lo + g * (hi - lo);
        double f1 = f (x1), f2 = f (x2);
        while (hi - lo > 1e-7)
        {
            if (f1 <= f2) { hi = x2; x2 = x1; f2 = f1; x1 = hi - g * (hi - lo); f1 = f (x1); }
            else          { lo = x1; x1 = x2; f1 = f2; x2 = lo + g * (hi - lo); f2 = f (x2); }
        }
        return (lo + hi) / 2.0;
    }

    std::vector<SinePartial> combined;
};
} // namespace xen
