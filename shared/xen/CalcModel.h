#pragma once
// The Calculator's data model WITH hand edits (ArtHar's "computed value + sticky edit" rule).
//
// Every edit is stored as an OFFSET from what the calculator computed, with a neutral value:
//   partial level   : dB offset on top of the rolloff loudness (0 dB = unmodified, -60 dB = muted)
//   partial position: cents offset from n^S (0 = unmodified). The fundamental can't move.
//   scale note      : on/off per found degree (on = unmodified) + cents offset from its home.
// When the calculation changes (new seed...), edits re-apply to the NEW computed values. Resetting
// an edit to its neutral value = "the calculator decides again". Modified items are flagged so the
// UI can mark them (orange, hollow squares).
// Custom partials / custom notes are extra slots that exist only when switched on.
//
// Plain data + pure functions, no JUCE: used by the audio thread, the UI and the tests alike.
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "xen/Dissonance.h"
#include "xen/Stretch.h"

namespace xen
{
inline constexpr int kSeriesPartials = 64;
inline constexpr int kCustomPartials = 8;
inline constexpr int kEditableDegrees = 32;
inline constexpr int kCustomNotes = 8;
inline constexpr int kMaxRawNotes = 64;
inline constexpr int kMaxFinalNotes = kMaxRawNotes + kCustomNotes;
inline constexpr double kMuteDb = -60.0;

inline double dbToGain (double db) { return db <= kMuteDb + 1e-9 ? 0.0 : std::pow (10.0, db / 20.0); }

struct PartialEdits
{
    std::array<double, kSeriesPartials> levelDb {};  // 0 = unmodified
    std::array<double, kSeriesPartials> posCents {}; // 0 = unmodified
    std::array<bool, kCustomPartials> customOn {};
    std::array<double, kCustomPartials> customCents {}, customLevelDb {};
};

struct ModelPartial
{
    double cents = 0.0;  // above the fundamental
    double amp = 0.0;    // reference loudness (0 = silent: left out of the scale math)
    int index = 0;       // 0..63 series partial (n = index+1), 64..71 custom slot
    bool custom = false, moved = false, levelEdited = false;
};

/// The calculator's partials after edits. Series first (n = 1..count), then custom ones.
inline std::vector<ModelPartial> buildPartials (double seed, int count, double rolloff, const PartialEdits& e)
{
    std::vector<ModelPartial> out;
    const double S = stretchFactor (std::max (1e-9, seed));
    count = std::clamp (count, 1, kSeriesPartials);
    for (int i = 0; i < count; ++i)
    {
        ModelPartial p;
        p.index = i;
        const double pos = i == 0 ? 0.0 : e.posCents[(size_t) i]; // the fundamental stays at 0 c
        p.cents = 1200.0 * S * std::log2 ((double) i + 1) + pos;
        p.amp = std::min (4.0, rolloffShape (i + 1, rolloff) * dbToGain (e.levelDb[(size_t) i]));
        p.moved = std::abs (pos) > 1e-9;
        p.levelEdited = std::abs (e.levelDb[(size_t) i]) > 1e-9;
        out.push_back (p);
    }
    for (int k = 0; k < kCustomPartials; ++k)
        if (e.customOn[(size_t) k])
        {
            ModelPartial p;
            p.index = kSeriesPartials + k;
            p.custom = true;
            p.cents = e.customCents[(size_t) k];
            p.amp = dbToGain (e.customLevelDb[(size_t) k]);
            out.push_back (p);
        }
    return out;
}

/// Raw (unedited) scale as produced by the calculator, in a fixed-size block so it can be copied
/// to the audio thread without allocating.
struct RawScale
{
    std::array<ScaleNote, kMaxRawNotes> notes {};
    int count = 0;
    double period = 1200.0;
    bool valid = false;

    void assign (const CurveResult& r)
    {
        count = 0;
        valid = r.valid;
        period = std::abs (r.periodCents);
        if (! r.valid) return;
        // Keep the unison, as many found notes as fit, and always the period.
        const int n = (int) r.notes.size();
        for (int i = 0; i < n && count < kMaxRawNotes - 1; ++i)
            if (r.notes[(size_t) i].kind != ScaleNote::Period) notes[(size_t) count++] = r.notes[(size_t) i];
        notes[(size_t) count++] = r.notes.back();
    }
};

struct NoteEdits
{
    NoteEdits() { on.fill (true); }
    std::array<bool, kEditableDegrees> on;           // true = unmodified
    std::array<double, kEditableDegrees> offset {};  // cents, 0 = unmodified
    std::array<bool, kCustomNotes> customOn {};
    std::array<double, kCustomNotes> customCents {};
};

struct FinalNote
{
    double cents = 0.0, home = 0.0, strength = 0.0;
    int degree = 0;         // 0 = unison, 1.. = found degree, -1-k = custom slot k, period = count
    ScaleNote::Kind kind = ScaleNote::Found;
    bool enabled = true, moved = false, custom = false;
};

struct FinalScale
{
    std::array<FinalNote, kMaxFinalNotes> notes {};
    int count = 0;          // includes disabled notes and the period (last)
    double period = 1200.0;
    bool valid = false;

    /// Playable notes (enabled, below the period), rising. Returns how many were written.
    int playable (double* cents, double* strength, int maxOut) const
    {
        int n = 0;
        for (int i = 0; i < count && n < maxOut; ++i)
            if (notes[(size_t) i].enabled && notes[(size_t) i].kind != ScaleNote::Period)
            {
                cents[n] = notes[(size_t) i].cents;
                strength[n] = notes[(size_t) i].strength;
                ++n;
            }
        return n;
    }
};

/// Applies the hand edits to a raw scale. Moved notes stay strictly inside (0, period).
inline FinalScale applyNoteEdits (const RawScale& raw, const NoteEdits& e)
{
    FinalScale f;
    f.valid = raw.valid;
    f.period = raw.period;
    if (! raw.valid) return f;
    int degree = 0;
    FinalNote periodNote;
    for (int i = 0; i < raw.count; ++i)
    {
        const auto& r = raw.notes[(size_t) i];
        FinalNote n;
        n.cents = n.home = r.cents;
        n.strength = r.strength;
        n.kind = r.kind;
        if (r.kind == ScaleNote::Unison) { n.degree = 0; }
        else if (r.kind == ScaleNote::Period) { periodNote = n; continue; }
        else
        {
            n.degree = ++degree;
            if (degree <= kEditableDegrees)
            {
                const double off = e.offset[(size_t) degree - 1];
                n.cents = std::clamp (r.cents + off, 0.5, raw.period - 0.5);
                n.moved = std::abs (off) > 1e-9;
                n.enabled = e.on[(size_t) degree - 1];
            }
        }
        f.notes[(size_t) f.count++] = n;
    }
    for (int k = 0; k < kCustomNotes; ++k)
        if (e.customOn[(size_t) k] && f.count < kMaxFinalNotes - 1)
        {
            FinalNote n;
            n.cents = n.home = std::clamp (std::fmod (std::abs (e.customCents[(size_t) k]), raw.period), 0.5, raw.period - 0.5);
            n.strength = 0.5;
            n.degree = -1 - k;
            n.custom = true;
            f.notes[(size_t) f.count++] = n;
        }
    std::sort (f.notes.begin(), f.notes.begin() + f.count, [] (const FinalNote& a, const FinalNote& b) { return a.cents < b.cents; });
    periodNote.degree = degree + 1;
    f.notes[(size_t) f.count++] = periodNote;
    return f;
}
} // namespace xen
