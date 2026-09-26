#pragma once
// CalculatorWorker — runs the Calculator (dissonance curve OR consonance search) on a background
// thread.
//
// Whoever owns it sends the latest settings whenever something changes (request()). The worker
// always jumps to the NEWEST settings: a calculation in progress is abandoned the moment newer
// settings arrive, so scrubbing never queues up stale work (same idea as ArtHar's gui/worker.rs).
// Results are picked up with fetch().
//
// This is the "Calculator" every synth in this repo is meant to carry on its top panel:
// fundamental + seed + partials (with hand edits) -> dissonance curve / consonance -> scale notes.
#include <juce_core/juce_core.h>
#include "xen/CalcModel.h"
#include "xen/Consonance.h"
#include "xen/Dissonance.h"
#include "xen/Scala.h"
#include "xen/Stretch.h"

namespace xen
{
// Imported (v0.5): the notes come from a loaded Scala file instead of being found; the dissonance
// curve is still drawn (over the imported period) so you can see how well the timbre fits them.
enum class Method { Dissonance = 0, Consonance = 1, Imported = 2 };

struct CalculatorSettings
{
    double fundamentalHz = 261.6255653005986;
    double seed = 2.0;
    int partials = 16;
    double rolloff = 1.0;
    double minStrength = 0.02;      // dissonance: dips weaker than this (0..1) are left out
    Method method = Method::Dissonance;
    double toleranceCents = 5.0;    // consonance
    int minShared = 2;              // consonance
    PartialEdits edits;             // hand edits + custom partials (default = none)
    std::vector<double> imported;   // Imported method: cents, rising, no unison, last = period

    bool operator== (const CalculatorSettings& o) const
    {
        auto eq = [] (double a, double b) { return juce::exactlyEqual (a, b); };
        if (! (eq (fundamentalHz, o.fundamentalHz) && eq (seed, o.seed) && partials == o.partials && eq (rolloff, o.rolloff)
               && eq (minStrength, o.minStrength) && method == o.method && eq (toleranceCents, o.toleranceCents)
               && minShared == o.minShared && imported == o.imported))
            return false;
        for (size_t i = 0; i < edits.levelDb.size(); ++i)
            if (! eq (edits.levelDb[i], o.edits.levelDb[i]) || ! eq (edits.posCents[i], o.edits.posCents[i]) || edits.inScale[i] != o.edits.inScale[i]) return false;
        for (size_t k = 0; k < edits.customOn.size(); ++k)
            if (edits.customOn[k] != o.edits.customOn[k] || edits.customInScale[k] != o.edits.customInScale[k] || ! eq (edits.customCents[k], o.edits.customCents[k])
                || ! eq (edits.customLevelDb[k], o.edits.customLevelDb[k]))
                return false;
        return true;
    }
    bool operator!= (const CalculatorSettings& o) const { return ! (*this == o); }

    /// The partials after edits (for display and for the synth).
    std::vector<ModelPartial> modelPartials() const { return buildPartials (seed, partials, rolloff, edits); }

    /// The partials that SHAPE THE SCALE as (Hz, loudness): silent ones and unticked ("sound
    /// only") ones are left out, like ArtHar.
    std::vector<SinePartial> makePartials() const
    {
        std::vector<SinePartial> p;
        for (auto& m : modelPartials())
            if (m.amp > 0.0 && m.inScale) p.push_back ({ fundamentalHz * centsToRatio (m.cents), std::min (1.0, m.amp) });
        return p;
    }
};

struct CalculatorOutput
{
    CalculatorSettings settings;
    CurveResult curve;          // curve points are empty for the consonance method
    bool searchComplete = true; // consonance: false if the search hit its step budget
    double millis = 0.0;
};

class CalculatorWorker : private juce::Thread
{
public:
    CalculatorWorker() : juce::Thread ("Calculator") { startThread (juce::Thread::Priority::low); }
    ~CalculatorWorker() override { stopThread (2000); }

    /// Ask for a calculation with these settings (cheap; ignored if nothing changed).
    void request (const CalculatorSettings& s)
    {
        {
            const juce::ScopedLock sl (lock);
            if (hasPending ? s == pending : (hasRequested && s == lastRequested)) return;
            pending = s;
            hasPending = true;
            lastRequested = s;
            hasRequested = true;
            newerRequest.store (true); // tells a running calculation to give up
        }
        notify();
    }

    /// Copies the newest finished result into `out`; returns true if it's newer than `seenId`.
    bool fetch (CalculatorOutput& out, uint64_t& seenId)
    {
        const juce::ScopedLock sl (lock);
        if (resultId == seenId) return false;
        out = result;
        seenId = resultId;
        return true;
    }

    bool isBusy() const { return busy.load(); }

private:
    void run() override
    {
        DissonanceCalculator dis;
        ConsonanceCalculator con;
        while (! threadShouldExit())
        {
            CalculatorSettings job;
            bool gotJob = false;
            {
                // Taking the job and clearing the "newer" flag happen together under the lock,
                // so a request can never slip in between and get lost.
                const juce::ScopedLock sl (lock);
                if (hasPending) { job = pending; hasPending = false; newerRequest.store (false); gotJob = true; }
            }
            if (! gotJob) { wait (-1); continue; }

            busy = true;
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            CalculatorOutput out;
            out.settings = job;
            auto stop = [this] { return threadShouldExit() || newerRequest.load(); };
            bool finished;
            if (job.method == Method::Consonance)
            {
                std::vector<double> positions;
                for (auto& m : job.modelPartials())
                    if (m.amp > 0.0 && m.inScale) positions.push_back (m.cents);
                ConsonanceSettings cs;
                cs.toleranceCents = job.toleranceCents;
                cs.minShared = job.minShared;
                finished = con.compute (positions, periodCents (job.seed), cs, out.curve, out.searchComplete, stop);
            }
            else if (job.method == Method::Imported)
                finished = computeImported (dis, job, out.curve, stop);
            else
                finished = dis.compute (job.makePartials(), periodCents (job.seed), job.minStrength, out.curve, stop);
            out.millis = juce::Time::getMillisecondCounterHiRes() - t0;
            busy = false;
            if (finished)
            {
                const juce::ScopedLock sl (lock);
                result = std::move (out);
                ++resultId;
            }
        }
    }

    /// Imported method: the curve over the imported period (no dips kept), then the imported notes
    /// with a strength = how deep a dissonance valley each one sits in (1 if there are no partials).
    template <typename StopFn>
    static bool computeImported (DissonanceCalculator& dis, const CalculatorSettings& job, CurveResult& out, StopFn&& stop)
    {
        out = {};
        if (job.imported.empty() || job.imported.back() <= 1.0) return true; // nothing loaded: invalid scale
        const double period = job.imported.back();
        const auto partials = job.makePartials();
        if (! dis.compute (partials, period, 2.0 /* > any strength: keep no dips */, out, stop)) return false;
        out.periodCents = period;
        out.notes.clear();
        out.notes.push_back ({ 0.0, importedStrength (dis, partials, 0.0), ScaleNote::Unison });
        for (size_t i = 0; i + 1 < job.imported.size(); ++i)
            if (job.imported[i] > 0.5 && job.imported[i] < period - 0.5)
                out.notes.push_back ({ job.imported[i], importedStrength (dis, partials, job.imported[i]), ScaleNote::Found });
        out.notes.push_back ({ period, importedStrength (dis, partials, period), ScaleNote::Period });
        out.valid = true;
        return true;
    }

    static double importedStrength (DissonanceCalculator& dis, const std::vector<SinePartial>& partials, double c)
    {
        if (partials.empty()) return 1.0;
        auto D = [&] (double x) { return dis.dissonanceAt (partials, centsToRatio (x)); };
        const double here = D (c), around = 0.5 * (D (c - 25.0) + D (c + 25.0));
        const double scale = std::max (std::abs (around) + std::abs (here), 1e-12);
        return std::clamp (0.1 + 2.0 * (around - here) / scale, 0.05, 1.0); // on a slope/peak = faint
    }

    juce::CriticalSection lock;
    CalculatorSettings pending, lastRequested;
    bool hasPending = false, hasRequested = false;
    std::atomic<bool> newerRequest { false }, busy { false };
    CalculatorOutput result;
    uint64_t resultId = 0;
};
} // namespace xen
