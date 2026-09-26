#pragma once
// Scala (.scl / .kbm) import + export, and "fit a timbre to a scale" (v0.5, user: "import and export
// scales? And if we import a scale, could it be calculated what partial settings would fit it
// nicely based on consonance and beating?").
//
// .scl format (Manuel Op de Coul's Scala): '!' lines are comments; first real line = description;
// second = number of notes; then one pitch per line, rising, NOT including the unison: a value
// with a '.' is in cents, otherwise a ratio "a/b" or a whole number. The last pitch is the period
// (usually 2/1 = 1200 c).
//
// FIT SEED (the Sethares idea, turned around): for a candidate seed we build the stretched
// partials and ask the dissonance model how rough each imported note is against the unison.
// The seed whose partials make the imported notes the SMOOTHEST — relative to how rough that timbre
// is on average (otherwise "all partials on one pitch" would always win) — fits the scale best.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "xen/Dissonance.h"
#include "xen/Stretch.h"

namespace xen
{
struct ScalaScale
{
    std::string description;
    std::vector<double> cents; // rising, WITHOUT the unison; last = period
    double period() const { return cents.empty() ? 1200.0 : cents.back(); }
    bool valid() const { return cents.size() >= 1 && period() > 1.0; }
};

/// Parses .scl text. Invalid lines are skipped; returns an invalid scale if nothing usable.
inline ScalaScale parseScl (const std::string& text)
{
    ScalaScale s;
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size())
    {
        size_t end = text.find ('\n', start);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr (start, end - start);
        if (! line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] != '!') lines.push_back (line);
        start = end + 1;
    }
    if (lines.size() < 2) return s;
    s.description = lines[0];
    int count = 0;
    try { count = std::stoi (lines[1]); } catch (...) { return s; }
    for (size_t i = 2; i < lines.size() && (int) s.cents.size() < count; ++i)
    {
        // First token of the line.
        const auto first = lines[i].find_first_not_of (" \t");
        if (first == std::string::npos) continue;
        const auto last = lines[i].find_first_of (" \t", first);
        const std::string tok = lines[i].substr (first, last == std::string::npos ? std::string::npos : last - first);
        try
        {
            double c;
            if (tok.find ('.') != std::string::npos) c = std::stod (tok);
            else if (const auto slash = tok.find ('/'); slash != std::string::npos)
            {
                const double a = std::stod (tok.substr (0, slash)), b = std::stod (tok.substr (slash + 1));
                if (a <= 0 || b <= 0) continue;
                c = 1200.0 * std::log2 (a / b);
            }
            else
            {
                const double a = std::stod (tok);
                if (a <= 0) continue;
                c = 1200.0 * std::log2 (a);
            }
            s.cents.push_back (c);
        }
        catch (...) {}
    }
    return s;
}

/// .scl text for notes (cents above the unison, rising, unison excluded) + the period.
inline std::string writeScl (const std::string& description, const std::vector<std::string>& commentLines,
                             const std::vector<double>& notes, double period)
{
    std::string out = "! Exported by Seed (Partial-VSTs)\n";
    for (auto& c : commentLines) out += "! " + c + "\n";
    out += description + "\n " + std::to_string (notes.size() + 1) + "\n!\n";
    char buf[64];
    for (double c : notes) { std::snprintf (buf, sizeof (buf), " %.6f\n", c); out += buf; }
    std::snprintf (buf, sizeof (buf), " %.6f\n", period);
    out += buf;
    return out;
}

/// .kbm keyboard mapping: linear mapping, key 60 plays degree 0 at fundamentalHz.
inline std::string writeKbm (int scaleSize, double fundamentalHz)
{
    char buf[512];
    std::snprintf (buf, sizeof (buf),
                   "! Exported by Seed (Partial-VSTs): key 60 plays the unison at the Fundamental\n"
                   "! Map size (0 = linear)\n0\n! First / last MIDI note\n0\n127\n! Middle note (degree 0)\n60\n"
                   "! Reference note and its frequency\n60\n%.6f\n! Scale degree of the formal octave\n%d\n",
                   fundamentalHz, scaleSize);
    return buf;
}

/// How badly this set of partials suits the scale (lower = better). Each note should sit in a
/// VALLEY of the dissonance curve: we measure how much rougher the curve is a little to each side
/// of the note (at +-15 and +-35 cents) than at the note itself, relative to the curve's overall
/// swing. Notes on a slope or a peak score badly. (v0.5 first attempt compared notes with the
/// timbre's AVERAGE roughness — that picked seed 3.5 for 12-EDO, so it was replaced.)
inline double scaleRoughness (DissonanceCalculator& calc, const std::vector<SinePartial>& partials,
                              const std::vector<double>& notes, double period)
{
    if (notes.empty() || period <= 1.0) return 1e9;
    auto D = [&] (double c) { return calc.dissonanceAt (partials, centsToRatio (c)); };
    double lo = 1e18, hi = -1e18;
    for (int i = 1; i <= 40; ++i) { const double d = D (period * i / 41.0); lo = std::min (lo, d); hi = std::max (hi, d); }
    const double swing = std::max (hi - lo, 1e-12);
    double score = 0.0, weights = 0.0;
    auto valley = [&] (double c, double w) {
        const double here = D (c);
        const double around = 0.25 * (D (c - 15.0) + D (c + 15.0) + D (c - 35.0) + D (c + 35.0));
        score -= w * (around - here) / swing; // deeper valley under the note = better (more negative)
        weights += w;
    };
    for (double c : notes)
        if (std::abs (c - period) > 0.01) valley (c, 1.0);
    // The period is where the scale repeats: it defines the whole system, so it counts 4 times.
    valley (period, 4.0);
    return score / weights;
}

/// Searches seeds in [lo, hi] for the one whose stretched partials fit the scale best.
inline double fitSeedToScale (const std::vector<double>& notes, double period, double fundamentalHz, int partials,
                              double rolloff, double lo = 1.5, double hi = 4.0)
{
    DissonanceCalculator calc;
    double best = 2.0, bestScore = 1e18;
    auto partialsFor = [&] (double seed) {
        std::vector<SinePartial> p;
        const double S = stretchFactor (seed);
        for (int n = 1; n <= partials; ++n) p.push_back ({ fundamentalHz * partialRatio (n, S), rolloffShape (n, rolloff) });
        return p;
    };
    // Coarse grid, then refine around the winner.
    for (double s = lo; s <= hi; s += 0.01)
        if (const double sc = scaleRoughness (calc, partialsFor (s), notes, period); sc < bestScore) { bestScore = sc; best = s; }
    const double c = best;
    for (double s = std::max (lo, c - 0.01); s <= std::min (hi, c + 0.01); s += 0.0005)
        if (const double sc = scaleRoughness (calc, partialsFor (s), notes, period); sc < bestScore) { bestScore = sc; best = s; }
    return best;
}
} // namespace xen
