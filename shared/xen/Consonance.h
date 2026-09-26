#pragma once
// The cross-note consonance method: a scale where EVERY PAIR of notes fits together.
//
// C++ copy of ArtHar's crates/calculator/src/consonance.rs (arthar commit 4185c4b, v1.4).
// Same candidate intervals, coincidence counting, maximum-clique search with a step budget,
// tie-break on total shared partials, and strength = shared-with-unison / shared-with-period.
//
// THE IDEA (ArtHar's words, shortened)
//   Two notes fuse when many of their partials land on the same frequencies. This method lists
//   every interval where partial m of an upper note sits exactly on partial n of a lower note,
//   counts how many partials two notes share (within a tolerance in cents), and keeps the LARGEST
//   set of candidates where every pair (incl. unison and period) shares >= minShared partials.
//   Pure pattern matching on partial positions: loudness and register don't matter. Gives fewer
//   notes than the dissonance curve, chord-like, any combination holds together.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

#include "xen/Dissonance.h" // ScaleNote

namespace xen
{
struct ConsonanceSettings
{
    double toleranceCents = 5.0;
    int minShared = 2;
    uint64_t searchBudget = 100000; // steps before settling for the best set so far
};

/// Coincidences of a note `interval` cents above another, for sorted partial positions (cents).
inline int coincidencesSorted (const std::vector<double>& sorted, double interval, double tol)
{
    int count = 0;
    size_t lo = 0, hi = 0;
    for (double upper : sorted)
    {
        const double target = upper + interval;
        while (lo < sorted.size() && sorted[lo] < target - tol) ++lo;
        hi = std::max (hi, lo);
        while (hi < sorted.size() && sorted[hi] <= target + tol) ++hi;
        count += (int) (hi - lo);
    }
    return count;
}

class ConsonanceCalculator
{
public:
    /// positions: partial positions in cents above their own fundamental. Returns false if
    /// shouldStop() asked to abandon. out.notes = unison, found (rising), period.
    template <typename StopFn>
    bool compute (std::vector<double> positions, double periodCents, const ConsonanceSettings& st,
                  CurveResult& out, bool& searchComplete, StopFn&& shouldStop)
    {
        out = {};
        out.periodCents = periodCents;
        const double period = std::abs (periodCents);
        if (period < 10.0 || positions.empty()) return true;
        std::sort (positions.begin(), positions.end());
        const double tol = st.toleranceCents;
        const int minS = st.minShared;
        auto score = [&] (double interval) { return coincidencesSorted (positions, std::abs (interval), tol); };
        const int periodScore = std::max (1, score (period));
        const double minGap = 2.0 * tol;

        // Candidates: every partial-on-partial interval strictly inside the period, deduplicated,
        // that already agrees with both frame notes.
        std::vector<double> cand;
        for (double m : positions)
            for (double n : positions)
            {
                const double c = m - n;
                if (c > 1e-6 && c < period - 1e-6) cand.push_back (c);
            }
        std::sort (cand.begin(), cand.end());
        cand.erase (std::unique (cand.begin(), cand.end(), [] (double a, double b) { return std::abs (a - b) < 1e-6; }), cand.end());
        nodes.clear();
        for (double c : cand)
            if (c > minGap && period - c > minGap && score (c) >= minS && score (period - c) >= minS)
                nodes.push_back (c);

        const size_t n = nodes.size();
        pair.assign (n * n, 0);
        for (size_t i = 0; i < n; ++i)
        {
            if (shouldStop()) return false;
            for (size_t j = i + 1; j < n; ++j)
                pair[i * n + j] = pair[j * n + i] = score (nodes[j] - nodes[i]);
        }
        frame.resize (n);
        for (size_t i = 0; i < n; ++i) frame[i] = (uint64_t) score (nodes[i]) + (uint64_t) score (period - nodes[i]);

        agreesFn = [this, n, minS, minGap] (size_t i, size_t j) {
            return pair[i * n + j] >= minS && std::abs (nodes[j] - nodes[i]) > minGap;
        };
        std::vector<size_t> order (n);
        for (size_t i = 0; i < n; ++i) order[i] = i;
        std::vector<size_t> degree (n, 0);
        for (size_t i = 0; i < n; ++i)
            for (size_t j = 0; j < n; ++j)
                if (i != j && agreesFn (i, j)) ++degree[i];
        std::stable_sort (order.begin(), order.end(), [&] (size_t a, size_t b) { return degree[a] > degree[b]; });

        best.clear(); bestTotal = 0; steps = 0; budget = st.searchBudget; stopped = false;
        stopFn = [&shouldStop] { return (bool) shouldStop(); };
        std::vector<size_t> group;
        expand (group, order, n);
        if (stopped) return false;
        searchComplete = steps <= budget;

        std::vector<std::pair<double, double>> found;
        for (size_t i : best)
            found.push_back ({ nodes[i], std::min (1.0, (double) score (nodes[i]) / (double) periodScore) });
        std::sort (found.begin(), found.end());
        out.notes.push_back ({ 0.0, 1.0, ScaleNote::Unison });
        for (auto& [c, s] : found) out.notes.push_back ({ c, s, ScaleNote::Found });
        out.notes.push_back ({ period, 1.0, ScaleNote::Period });
        out.valid = true;
        return true;
    }

private:
    uint64_t totalOf (const std::vector<size_t>& set, size_t n) const
    {
        uint64_t t = 0;
        for (size_t k = 0; k < set.size(); ++k)
        {
            t += frame[set[k]];
            for (size_t m = k + 1; m < set.size(); ++m) t += (uint64_t) pair[set[k] * n + set[m]];
        }
        return t;
    }

    /// Branch-and-bound maximum clique (ArtHar's Search::expand).
    void expand (std::vector<size_t>& group, const std::vector<size_t>& open, size_t n)
    {
        if (++steps > budget || stopped) return;
        if ((steps & 255) == 0 && stopFn()) { stopped = true; return; }
        if (open.empty())
        {
            const auto total = totalOf (group, n);
            if (group.size() > best.size() || (group.size() == best.size() && total > bestTotal)) { best = group; bestTotal = total; }
            return;
        }
        for (size_t k = 0; k < open.size(); ++k)
        {
            if (group.size() + (open.size() - k) < best.size()) return;
            const size_t v = open[k];
            std::vector<size_t> rest;
            for (size_t m = k + 1; m < open.size(); ++m)
                if (agreesFn (open[m], v)) rest.push_back (open[m]);
            group.push_back (v);
            expand (group, rest, n);
            group.pop_back();
            if (steps > budget || stopped) return;
        }
        if (group.size() >= best.size())
        {
            const auto total = totalOf (group, n);
            if (group.size() > best.size() || total > bestTotal) { best = group; bestTotal = total; }
        }
    }

    std::vector<double> nodes;
    std::vector<int> pair;
    std::vector<uint64_t> frame;
    std::function<bool (size_t, size_t)> agreesFn;
    std::function<bool()> stopFn;
    std::vector<size_t> best;
    uint64_t bestTotal = 0, steps = 0, budget = 0;
    bool stopped = false;
};
} // namespace xen
