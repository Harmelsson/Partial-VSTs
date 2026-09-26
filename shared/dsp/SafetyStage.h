#pragma once
// SafetyStage — the last thing every plugin's audio passes through. NOT bypassable, by design.
//
// Why: experimental DSP (feedback, resonators, 64 partials collapsing onto one pitch...) can
// produce NaNs, huge DC offsets or runaway levels. The user listens on headphones/laptop audio,
// so hearing safety beats fidelity here. Order:
//   1. NaN/Inf scrub  — a non-finite sample mutes the plugin for ~100 ms and resets the state
//                       (a NaN would otherwise poison every filter downstream forever).
//   2. DC blocker     — one-pole high-pass at ~4 Hz; sub-audio junk wastes headroom.
//   3. Peak limiter   — instant attack, ~120 ms release, channels linked (no stereo image shift).
//                       No lookahead, so sharp transients may distort slightly: that's the price
//                       of zero latency, and a safety net should never add latency.
//   4. Hard ceiling   — absolute clamp at -1 dBFS. Nothing louder can ever leave the plugin.
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace pvst
{
class SafetyStage
{
public:
    static constexpr float kThreshold = 0.794f; // -2 dBFS: limiter starts pulling down here
    static constexpr float kCeiling   = 0.891f; // -1 dBFS: absolute maximum sample value

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        dcR = static_cast<float> (1.0 - (2.0 * juce::MathConstants<double>::pi * 4.0 / sr));
        releaseCoef = static_cast<float> (std::exp (-1.0 / (0.12 * sr)));
        reset();
    }

    void reset()
    {
        dcX1.fill (0.0f);
        dcY1.fill (0.0f);
        gain = 1.0f;
        muteSamplesLeft = 0;
    }

    /// How many times a non-finite sample was caught since load (shown/logged for debugging).
    int faultCount() const noexcept { return faults; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int numCh = std::min (buffer.getNumChannels(), kMaxCh);
        const int n = buffer.getNumSamples();

        // 1. NaN/Inf scrub (whole block: if anything is broken, silence and reset).
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
                if (! std::isfinite (d[i]))
                {
                    ++faults;
                    buffer.clear();
                    reset();
                    muteSamplesLeft = static_cast<int> (0.1 * sr);
                    return;
                }
        }
        if (muteSamplesLeft > 0)
        {
            muteSamplesLeft -= n;
            buffer.clear();
            return;
        }

        for (int i = 0; i < n; ++i)
        {
            // 2. DC blocker per channel, and find the linked peak.
            float peak = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                const float x = d[i];
                const float y = x - dcX1[(size_t) ch] + dcR * dcY1[(size_t) ch];
                dcX1[(size_t) ch] = x;
                dcY1[(size_t) ch] = y;
                d[i] = y;
                peak = std::max (peak, std::abs (y));
            }

            // 3. Limiter: instant attack, exponential release back towards unity gain.
            const float wanted = peak > kThreshold ? kThreshold / peak : 1.0f;
            if (wanted < gain)
                gain = wanted;
            else
                gain = wanted + (gain - wanted) * releaseCoef;

            // 4. Hard ceiling.
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                d[i] = std::clamp (d[i] * gain, -kCeiling, kCeiling);
            }
        }
    }

private:
    static constexpr int kMaxCh = 8;
    double sr = 48000.0;
    float dcR = 0.9995f, releaseCoef = 0.999f, gain = 1.0f;
    std::array<float, kMaxCh> dcX1 {}, dcY1 {};
    int muteSamplesLeft = 0;
    int faults = 0;
};
} // namespace pvst
