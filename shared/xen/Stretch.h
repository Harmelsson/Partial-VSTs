#pragma once
// Stretched-harmonics math: the "seed" idea.
//
// Copied (as C++, for real-time use) from the ArtHar calculator's stretch.rs + loudness.rs,
// arthar commit 8bf556f. This is a copy, not a mirror: it may diverge if a plugin needs it to.
//
// THE IDEA
//   A normal string has partials at 1x, 2x, 3x, 4x... the fundamental. Partial 2 sits at 2:1,
//   the octave. The seed bends that series so partial 2 lands on ANY ratio ("seed"), and every
//   other partial follows consistently:
//
//     stretch factor  S = log2(seed)
//     partial n ratio   = n^S           (n = 1, 2, 3, ...)
//
//   Seed 2 -> S = 1 -> ordinary harmonics. Seed 2.5 -> partial 2 at 2.5:1 = 1586.31 cents.
//   The seed also defines the "octave" of that sound universe: period = 1200 * log2(seed) cents.
//   Seeds below 1 make S negative: partials fall BELOW the fundamental (an undertone series at
//   seed 0.5). Seed 1 collapses every partial onto the fundamental. Both are allowed on purpose.
#include <cmath>

namespace xen
{
/// S = log2(seed). Seed must be > 0.
inline double stretchFactor (double seed) noexcept { return std::log2 (seed); }

/// Ratio of partial n (1-based) to the fundamental: n^S.
inline double partialRatio (int n, double stretch) noexcept
{
    return std::pow (static_cast<double> (n), stretch);
}

/// The seed's "octave" (where its scale repeats), in cents: 1200 * log2(seed).
inline double periodCents (double seed) noexcept { return 1200.0 * std::log2 (seed); }

/// Rolloff loudness of partial n: 1 / n^rolloff. 0 = all equal (bright), 1 = 1/n (natural),
/// 2 = 1/n^2 (dark).
inline double rolloffShape (int n, double rolloff) noexcept
{
    return 1.0 / std::pow (static_cast<double> (n), rolloff);
}

inline double ratioToCents (double ratio) noexcept { return 1200.0 * std::log2 (ratio); }
inline double centsToRatio (double cents) noexcept { return std::exp2 (cents / 1200.0); }
} // namespace xen
