#pragma once
// CalculatorWorker — runs the dissonance-curve calculator on a background thread.
//
// The UI sends the latest settings whenever something changes (request()). The worker always
// jumps to the NEWEST settings: a calculation in progress is abandoned the moment newer settings
// arrive, so scrubbing never queues up stale work (same idea as ArtHar's gui/worker.rs).
// The UI polls fetch() from its timer to pick up finished results.
//
// This is the "Calculator" every synth in this repo is meant to carry on its top panel:
// fundamental + seed + partial count + rolloff -> partials -> dissonance curve -> scale notes.
#include <juce_core/juce_core.h>
#include "xen/Dissonance.h"
#include "xen/Stretch.h"

namespace xen
{
struct CalculatorSettings
{
    double fundamentalHz = 261.6255653005986;
    double seed = 2.0;
    int partials = 16;
    double rolloff = 1.0;
    double minStrength = 0.02; // 0..1

    bool operator== (const CalculatorSettings& o) const
    {
        return juce::exactlyEqual (fundamentalHz, o.fundamentalHz) && juce::exactlyEqual (seed, o.seed)
            && partials == o.partials && juce::exactlyEqual (rolloff, o.rolloff)
            && juce::exactlyEqual (minStrength, o.minStrength);
    }
    bool operator!= (const CalculatorSettings& o) const { return ! (*this == o); }

    /// The partials these settings describe (stretched harmonics, rolloff loudness).
    std::vector<SinePartial> makePartials() const
    {
        std::vector<SinePartial> p;
        const double S = stretchFactor (seed);
        for (int n = 1; n <= partials; ++n)
            p.push_back ({ fundamentalHz * partialRatio (n, S), rolloffShape (n, rolloff) });
        return p;
    }
};

struct CalculatorOutput
{
    CalculatorSettings settings;
    CurveResult curve;
    double millis = 0.0; // how long it took (shown for curiosity / performance tracking)
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
        DissonanceCalculator calc;
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
            const bool finished = calc.compute (job.makePartials(), periodCents (job.seed), job.minStrength, out.curve,
                                                [this] { return threadShouldExit() || newerRequest.load(); });
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

    juce::CriticalSection lock;
    CalculatorSettings pending, lastRequested;
    bool hasPending = false, hasRequested = false;
    std::atomic<bool> newerRequest { false }, busy { false };
    CalculatorOutput result;
    uint64_t resultId = 0;
};
} // namespace xen
