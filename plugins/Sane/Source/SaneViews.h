#pragma once
// Sane's graph windows (all drawn in the brown "disp" colour with white traces):
//   ExciterView  the current noise burst (+ the last 3 in black: every note gets new noise)
//   StrikePad    the drum head: click = base strike position; shows where pp..ff hits land and the last hits
//   LoopRing     the feedback loop: 7 insert slots on a ring (click = edit, drag onto another = swap), the core in the middle
//   ModesView    where the core's modes sit (level up, decay down), endpoints A ◆ / B ▲ of the morph
//   SlotEditor   the selected slot: type picker, big graph, its three knobs
//   HitView      the last hit's waveform, the drawn pitch drop and the length (all draggable)
//   KeyStrip     on-screen keyboard: lights on press, fades on release; click = play (low on a key = loud)
#include <complex>
#include "PluginProcessor.h"
#include "SaneControls.h"

namespace sane::ui
{
using look::ink; using look::paper; using look::disp;

inline juce::Path polyline (const std::vector<juce::Point<float>>& pts)
{
    juce::Path p;
    for (size_t i = 0; i < pts.size(); ++i)
        if (i == 0) p.startNewSubPath (pts[i]); else p.lineTo (pts[i]);
    return p;
}

/// Draws a slot's character into r (white on the display colour). Used by the ring tiles and the editor.
inline void drawSlotGraph (juce::Graphics& g, juce::Rectangle<float> r, int type, float a, float b, float c, bool big)
{
    const float W = r.getWidth(), H = r.getHeight(), x0 = r.getX(), y0 = r.getY();
    const float lw = big ? 2.0f : 1.3f;
    auto X = [&] (float u) { return x0 + u * W; };
    auto Y = [&] (float v) { return y0 + v * H; };
    std::vector<juce::Point<float>> pts;
    g.setColour (paper);
    switch (type)
    {
        case SlotFilter:
        {
            const double fc = 20.0 * std::pow (1000.0, (double) a), k = 2.0 - 1.9 * b, m = c * 2.0;
            const double wl = std::max (0.0, 1.0 - m), wb = 1.0 - std::abs (m - 1.0), wh = std::max (0.0, m - 1.0);
            for (int i = 0; i <= 120; ++i)
            {
                const double f = 20.0 * std::pow (1000.0, i / 120.0);
                const std::complex<double> s (0.0, f / fc);
                const auto den = s * s + k * s + 1.0;
                const auto h = (wl * 1.0 + wb * k * s + wh * s * s) / den;
                const double db = 20.0 * std::log10 (std::abs (h) + 1e-9);
                pts.push_back ({ X ((float) i / 120.0f), Y ((float) juce::jlimit (0.0, 1.0, 0.45 - db / 48.0)) });
            }
            g.setColour (paper.withAlpha (0.35f)); g.drawHorizontalLine ((int) Y (0.45f), x0, x0 + W);
            g.setColour (paper); g.strokePath (polyline (pts), juce::PathStrokeType (lw));
            break;
        }
        case SlotFineDelay:
        {
            const float d = b > 0.02f ? 0.08f + 0.35f * b : 0.03f + 0.6f * a * a;
            for (int k = 0; k < 8; ++k)
            {
                const float x = 0.03f + k * d;
                if (x > 1.0f) break;
                g.drawLine (X (x), Y (0.96f), X (x), Y (0.96f - 0.85f * std::pow (0.75f, (float) k)), lw);
            }
            g.setColour (paper.withAlpha (0.35f)); g.drawHorizontalLine ((int) Y (0.96f), x0, x0 + W);
            break;
        }
        case SlotDisperse:
        {
            const double ap = (c < 0.5f ? 1.0 : -1.0) * a * 0.9 * (std::abs (c - 0.5) * 2.0 * 0.7 + 0.3);
            const int n = 1 + (int) std::lround (b * 11.0);
            double mx = 1e-9;
            std::vector<double> gd;
            for (int i = 0; i <= 100; ++i)
            {
                const double w = juce::MathConstants<double>::pi * std::pow (10.0, -2.0 + 2.0 * i / 100.0);
                const double v = n * (1.0 - ap * ap) / (1.0 + 2.0 * ap * std::cos (w) + ap * ap);
                gd.push_back (v); mx = std::max (mx, v);
            }
            for (int i = 0; i <= 100; ++i) pts.push_back ({ X (i / 100.0f), Y ((float) (0.92 - 0.82 * gd[(size_t) i] / std::max (mx, 13.0))) });
            g.strokePath (polyline (pts), juce::PathStrokeType (lw));
            break;
        }
        case SlotSaturate:
        case SlotFold:
        {
            g.setColour (paper.withAlpha (0.35f));
            g.drawLine (X (0), Y (1), X (1), Y (0), 1.0f);
            g.drawHorizontalLine ((int) Y (0.5f), x0, x0 + W);
            for (int i = 0; i <= 80; ++i)
            {
                const float u = i / 80.0f, xin = (u * 2.0f - 1.0f) * 1.5f;
                float y;
                if (type == SlotSaturate)
                {
                    const float d = 1.0f + 19.0f * a * a, bi = (b - 0.5f) * 1.2f;
                    y = (dsp::fastTanh (d * xin + bi) - dsp::fastTanh (bi)) / d * 1.0f;
                    y = xin + c * (y - xin);
                }
                else
                {
                    const float d = 1.0f + 9.0f * a, bi = (b - 0.5f) * 1.6f, v = d * xin + bi, t = v * 0.25f + 0.25f;
                    const float fl = -(4.0f * std::abs (t - std::floor (t + 0.5f)) - 1.0f);
                    y = xin + c * (fl / d - xin);
                }
                pts.push_back ({ X (u), Y (juce::jlimit (0.0f, 1.0f, 0.5f - y / 3.0f)) });
            }
            g.setColour (paper); g.strokePath (polyline (pts), juce::PathStrokeType (lw));
            break;
        }
        case SlotFreqShift:
        {
            const float dir = (a - 0.5f) * 2.0f;
            for (int i = 1; i <= 6; ++i)
            {
                const float x = 0.05f + std::log2 ((float) i) / std::log2 (8.0f) * 0.85f, h = 0.85f * std::pow ((float) i, -0.4f);
                g.setColour (paper); g.drawLine (X (x), Y (0.96f), X (x), Y (0.96f - h), lw);
                g.setColour (paper.withAlpha (0.45f));
                for (int e = 1; e <= 3; ++e) { const float xe = x + dir * e * 0.03f; g.drawLine (X (xe), Y (0.96f), X (xe), Y (0.96f - h * std::pow (0.6f, (float) e)), 1.0f); }
            }
            break;
        }
        case SlotChorus:
        {
            const float cyc = 1.0f + 6.0f * b;
            for (int i = 0; i <= 100; ++i) pts.push_back ({ X (i / 100.0f), Y (0.5f - (0.08f + 0.36f * a) * std::sin (i / 100.0f * cyc * 6.2832f)) });
            g.setColour (paper.withAlpha (0.35f)); g.drawHorizontalLine ((int) Y (0.5f), x0, x0 + W);
            g.setColour (paper); g.strokePath (polyline (pts), juce::PathStrokeType (lw));
            break;
        }
        case SlotRingMod:
        {
            const float ratio = 0.25f + 7.75f * a * a;
            auto bar = [&] (float f, float h, float alpha) { const float x = 0.05f + std::log2 (std::max (0.1f, f) * 4.0f) / std::log2 (64.0f) * 0.9f;
                                                             g.setColour (paper.withAlpha (alpha)); g.drawLine (X (x), Y (0.96f), X (x), Y (0.96f - h), lw); };
            bar (1.0f, 0.8f * (1.0f - c * 0.5f), 1.0f);
            bar (1.0f + ratio, 0.45f * c, 1.0f);
            bar (std::abs (1.0f - ratio), 0.45f * c, 1.0f);
            break;
        }
        case SlotFlip:
        {
            dsp::Rng rr (77);
            const int n = big ? 48 : 16;
            for (int i = 0; i < n; ++i)
            {
                const bool flip = rr.uni() < a * 0.5f;
                const float x = (i + 0.5f) / n, h = 0.4f * (0.4f + 0.6f * rr.uni());
                g.setColour (flip ? paper : paper.withAlpha (0.45f));
                g.drawLine (X (x), Y (0.5f), X (x), Y (flip ? 0.5f + h : 0.5f - h), big ? 3.0f : 1.6f);
            }
            break;
        }
        default:
            g.setFont (look::sans (big ? 40.0f : 18.0f));
            g.drawText ("+", r, juce::Justification::centred, false);
            break;
    }
}

// ------------------------------------------------------------------------------------------------
class ExciterView : public juce::Component, private juce::Timer
{
public:
    explicit ExciterView (SaneProcessor& p) : proc (p) { startTimerHz (30); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        look::fillDisp (g, r);
        const float W = (float) r.getWidth(), H = (float) r.getHeight(), cy = H * 0.5f;
        g.setColour (paper.withAlpha (0.3f)); g.drawHorizontalLine ((int) cy, 0.0f, W);
        for (size_t k = 0; k < ghosts.size(); ++k) { g.setColour (ink.withAlpha (0.5f)); g.strokePath (burstPath (ghosts[k], W, H, nullptr), juce::PathStrokeType (1.0f)); }
        juce::Path env;
        auto cur = burstPath (current, W, H, &env);
        g.setColour (paper); g.strokePath (cur, juce::PathStrokeType (1.5f));
        look::strokeDashed (g, env, 1.2f);
        g.setColour (look::dispText);
        g.setFont (look::mono (10.5f));
        g.drawText ("this hit " + look::u8 ("\xc2\xb7 ") + juce::String (current.lengthMs, 1) + " ms", 6, 3, 180, 13, juce::Justification::left, false);
        g.drawText ("black = last 3 hits", r.getWidth() - 136, 3, 130, 13, juce::Justification::right, false);
    }

private:
    juce::Path burstPath (const dsp::BurstSettings& s, float W, float H, juce::Path* env)
    {
        dsp::BurstGen gen;
        constexpr double sr = 48000.0;
        gen.start (s, sr);
        // window follows the burst length (the burst fills ~2/3 of the view)
        const double win = juce::jlimit (0.004, 0.25, (double) currentLength() * 0.001 * 4.0);
        const int total = (int) (win * sr), step = std::max (1, total / (int) W);
        juce::Path p;
        const float cy = H * 0.5f, amp = H * 0.42f;
        const double decay = std::exp (-1.0 / std::max (1.0, 0.001 * s.lengthMs * sr * 0.45));
        double e = 0.0;
        for (int i = 0, k = 0; i < total; ++i)
        {
            float l, rr;
            gen.next (l, rr);
            e = i < 14 ? (i + 1) / 14.0 : e * decay;
            if (i % step != 0) continue;
            const float x = (float) i / (float) total * W;
            const float y = cy - juce::jlimit (-1.0f, 1.0f, l / std::max (0.05f, s.level)) * amp;
            if (k++ == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            if (env != nullptr) { const float ey = cy - (float) e * amp * 1.05f; if (k == 1) env->startNewSubPath (x, ey); else env->lineTo (x, ey); }
        }
        return p;
    }

    float currentLength() const { return std::max (current.lengthMs, 0.3f); }

    void timerCallback() override
    {
        const int c = proc.engine.report.counter.load();
        if (c == seen) return;
        seen = c;
        if (seen > 1) { ghosts.insert (ghosts.begin(), current); if (ghosts.size() > 3) ghosts.pop_back(); }
        auto& rp = proc.engine.report;
        current.seed = rp.seed.load(); current.lengthMs = rp.burstLength.load(); current.color = rp.burstColor.load();
        current.grain = rp.burstGrain.load(); current.level = 1.0f; current.spread = 0.0f;
        repaint();
    }

    SaneProcessor& proc;
    int seen = 0;
    dsp::BurstSettings current;
    std::vector<dsp::BurstSettings> ghosts;
};

// ------------------------------------------------------------------------------------------------
class StrikePad : public juce::Component, private juce::Timer
{
public:
    explicit StrikePad (SaneProcessor& p) : proc (p) { startTimerHz (30); setMouseCursor (juce::MouseCursor::CrosshairCursor); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        look::fillDisp (g, r);
        const auto c = r.getCentre().toFloat();
        const float R = std::min (r.getWidth(), r.getHeight()) * 0.5f - 10.0f;
        auto at = [&] (float rad, float ang) { return juce::Point<float> (c.x + rad * R * std::cos (ang), c.y + rad * R * std::sin (ang)); };
        g.setColour (paper); g.drawEllipse (c.x - R, c.y - R, 2 * R, 2 * R, 2.2f);
        g.setColour (paper.withAlpha (0.34f));
        const float rn = 0.436f * R; // node circle of mode 02
        g.drawEllipse (c.x - rn, c.y - rn, 2 * rn, 2 * rn, 1.0f);
        g.drawHorizontalLine ((int) c.y, c.x - R, c.x + R);      // node line of mode 11

        // where hits land: pp .. ff along the base angle (for the last played key)
        const auto& rp = proc.engine.report;
        const float key = (float) (rp.note.load() - 60) / 36.0f;
        auto pos = [&] (float vel) { return juce::jlimit (0.0f, 0.96f, proc.value (ids::stBase) + proc.value (ids::stVel) * (vel - 0.5f) + proc.value (ids::stKey) * key); };
        const auto pp = at (pos (0.25f), baseAng), ff = at (pos (1.0f), baseAng);
        g.setColour (ink); g.drawLine ({ pp, ff }, 3.0f);
        g.setColour (look::dispText); g.setFont (look::mono (10.0f));
        g.drawText ("pp", (int) pp.x + 5, (int) pp.y - 14, 20, 12, juce::Justification::left, false);
        g.drawText ("ff", (int) ff.x + 5, (int) ff.y + 2, 20, 12, juce::Justification::left, false);
        // base marker
        const auto b = at (proc.value (ids::stBase), baseAng);
        juce::Path bp; bp.addEllipse (b.x - 6, b.y - 6, 12, 12);
        g.setColour (paper); look::strokeDashed (g, bp, 1.2f, 3.0f, 3.0f);
        // last hits
        g.setColour (ink.withAlpha (0.5f));
        for (auto& h : hits) { const auto q = at (h.first, h.second); g.drawRect (q.x - 4, q.y - 4, 8.0f, 8.0f, 1.1f); }
        if (! hits.empty())
        {
            const auto q = at (hits.front().first, hits.front().second);
            g.setColour (ink); g.fillRect (q.x - 4.5f, q.y - 4.5f, 9.0f, 9.0f);
            g.setColour (paper); g.drawRect (q.x - 4.5f, q.y - 4.5f, 9.0f, 9.0f, 1.2f);
        }
        g.setColour (look::dispText);
        g.drawText (look::u8 ("click = base \xe2\x97\x8c"), 6, 3, 120, 12, juce::Justification::left, false);
        g.drawText ("rim", r.getWidth() - 30, r.getHeight() - 15, 24, 12, juce::Justification::right, false);
    }

    void mouseDown (const juce::MouseEvent& e) override { proc.param (ids::stBase).beginChangeGesture(); mouseDrag (e); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto c = getLocalBounds().getCentre().toFloat();
        const float R = std::min (getWidth(), getHeight()) * 0.5f - 10.0f;
        const auto d = e.position - c;
        baseAng = std::atan2 (d.y, d.x);
        auto& p = proc.param (ids::stBase);
        p.setValueNotifyingHost (p.convertTo0to1 (juce::jlimit (0.0f, 0.96f, d.getDistanceFromOrigin() / R)));
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { proc.param (ids::stBase).endChangeGesture(); }

private:
    void timerCallback() override
    {
        const int c = proc.engine.report.counter.load();
        if (c != seen)
        {
            seen = c;
            hits.insert (hits.begin(), { proc.engine.report.strikeR.load(), baseAng + (rnd.nextFloat() - 0.5f) * 0.7f });
            if (hits.size() > 5) hits.pop_back();
        }
        repaint();
    }
    SaneProcessor& proc;
    int seen = 0;
    float baseAng = -0.7f;
    std::vector<std::pair<float, float>> hits;
    juce::Random rnd;
};

// ------------------------------------------------------------------------------------------------
class LoopRing : public juce::Component, private juce::Timer
{
public:
    LoopRing (SaneProcessor& p, std::function<void (int)> select) : proc (p), onSelect (std::move (select))
    {
        for (int s = 0; s < kNumSlots; ++s)
        {
            tiles.push_back (std::make_unique<Tile> (*this, s));
            addAndMakeVisible (*tiles.back());
        }
        startTimerHz (30);
    }
    int selected = 2;

    void resized() override
    {
        const auto c = centre();
        for (int s = 0; s < kNumSlots; ++s)
        {
            const float ang = juce::degreesToRadians (-45.0f + 45.0f * (float) s);
            tiles[(size_t) s]->setBounds ((int) (c.x + rx() * std::cos (ang)) - 53, (int) (c.y + ry() * std::sin (ang)) - 31, 106, 62);
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto c = centre();
        g.setColour (ink);
        g.drawEllipse (c.x - rx(), c.y - ry(), 2 * rx(), 2 * ry(), 3.0f);
        for (float deg : { 22.5f, 112.5f, 202.5f, 292.5f }) // direction chevrons (clockwise)
        {
            const float t = juce::degreesToRadians (deg);
            const juce::Point<float> p (c.x + rx() * std::cos (t), c.y + ry() * std::sin (t));
            juce::Point<float> d (-rx() * std::sin (t), ry() * std::cos (t));
            d /= d.getDistanceFromOrigin();
            const juce::Point<float> n (-d.y, d.x);
            juce::Path tri; tri.addTriangle (p + d * 7.0f, p - d * 5.0f + n * 6.0f, p - d * 5.0f - n * 6.0f);
            g.fillPath (tri);
        }
        // junction: the hit goes in, the sound comes out
        const juce::Rectangle<float> j (c.x - 18, c.y - ry() - 11, 36, 22);
        g.fillRect (j);
        g.setColour (paper); g.setFont (look::mono (14.0f, true));
        g.drawText (look::u8 ("\xce\xa3"), j, juce::Justification::centred, false);
        look::drawLabel (g, look::u8 ("hit in \xc2\xb7 sound out"), juce::Rectangle<int> ((int) c.x - 80, (int) j.getY() - 22, 160, 14));
        // energy squares running round while something rings
        if (activity > 0.001f)
        {
            g.setColour (ink.withAlpha (juce::jlimit (0.2f, 1.0f, activity * 8.0f)));
            for (int k = 0; k < 3; ++k)
            {
                const float t = juce::MathConstants<float>::twoPi * (phase + k / 3.0f) - juce::MathConstants<float>::halfPi;
                g.fillRect (c.x + rx() * std::cos (t) - 3.5f, c.y + ry() * std::sin (t) - 3.5f, 7.0f, 7.0f);
            }
        }
        // the core in the middle
        const float R = 58.0f;
        g.setColour (disp); g.fillEllipse (c.x - R, c.y - R, 2 * R, 2 * R);
        g.setColour (ink); g.drawEllipse (c.x - R, c.y - R, 2 * R, 2 * R, 2.0f);
        const int core = (int) proc.value (ids::core);
        g.setColour (paper);
        if (core == CoreMembrane)
        {
            juce::Point<float> nodes[8];
            for (int i = 0; i < 8; ++i) nodes[i] = { c.x + 25.0f * std::cos (i * 0.7854f), c.y - 10 + 25.0f * std::sin (i * 0.7854f) };
            g.setColour (paper.withAlpha (0.35f));
            for (int i = 0; i < 8; ++i) for (int k = i + 1; k < 8; ++k) g.drawLine ({ nodes[i], nodes[k] }, 1.0f);
            g.setColour (paper);
            for (auto& n : nodes) g.fillRect (n.x - 3, n.y - 3, 6.0f, 6.0f);
        }
        else if (core == CorePartials)
            for (int i = 0; i < 7; ++i) g.drawLine (c.x - 24 + i * 8.0f, c.y + 14, c.x - 24 + i * 8.0f, c.y + 14 - 40.0f * std::pow (i + 1.0f, -0.6f), 1.5f);
        else
        {
            g.drawLine (c.x - 26, c.y - 10, c.x + 26, c.y - 10, 1.5f);
            g.drawLine (c.x - 26, c.y - 16, c.x - 26, c.y - 4, 1.5f);
            g.drawLine (c.x + 26, c.y - 16, c.x + 26, c.y - 4, 1.5f);
        }
        g.setFont (look::caps (11.0f));
        const juce::String names[] = { "MEMBRANE " + look::u8 ("\xc2\xb7 8"), "PARTIALS " + look::u8 ("\xc2\xb7 16"), "DELAY " + look::u8 ("\xc2\xb7 1") };
        g.drawText (names[juce::jlimit (0, 2, core)], (int) c.x - 56, (int) c.y + 22, 112, 14, juce::Justification::centred, false);
    }

    /// Swap two slots (type + knobs) — the slots are interchangeable.
    void swapSlots (int a, int b)
    {
        if (a == b) return;
        for (auto* nm : { "type", "a", "b", "c", "place" })
        {
            auto& pa = proc.param (ids::slot (a + 1, nm));
            auto& pb = proc.param (ids::slot (b + 1, nm));
            const float va = pa.getValue(), vb = pb.getValue();
            pa.beginChangeGesture(); pa.setValueNotifyingHost (vb); pa.endChangeGesture();
            pb.beginChangeGesture(); pb.setValueNotifyingHost (va); pb.endChangeGesture();
        }
        select (b);
    }
    void select (int s) { selected = s; for (auto& t : tiles) t->repaint(); if (onSelect) onSelect (s); }
    void removeSlot (int s)
    {
        auto& p = proc.param (ids::slot (s + 1, "type"));
        p.beginChangeGesture(); p.setValueNotifyingHost (p.convertTo0to1 (0.0f)); p.endChangeGesture();
        select (s);
    }

private:
    struct Tile : public juce::Component
    {
        Tile (LoopRing& o, int s) : owner (o), slot (s) {}
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds();
            const int type = (int) owner.proc.value (ids::slot (slot + 1, "type"));
            const bool sel = owner.selected == slot;
            g.setColour (sel ? paper : look::ground); g.fillRect (r);
            g.setColour (ink);
            if (type == SlotOff)
            {
                juce::Path p; p.addRectangle (r.toFloat().reduced (0.75f));
                look::strokeDashed (g, p, 1.5f, 4.0f, 3.0f);
                g.setFont (look::sans (22.0f)); g.drawText ("+", r.withTrimmedBottom (18), juce::Justification::centred, false);
                g.setFont (look::caps (10.5f)); g.drawText (juce::String (slot + 1) + "  ADD FX", r.withTrimmedTop (30), juce::Justification::centred, false);
            }
            else
            {
                g.drawRect (r, 1.5f);
                if (sel) g.drawRect (r.reduced (2), 1.5f);
                auto in = r.reduced (5, 4);
                g.setFont (look::caps (10.5f));
                g.setColour (ink.withAlpha (0.5f));
                g.drawText (look::u8 ("\xe2\x8b\xae\xe2\x8b\xae"), in.getX(), in.getY(), 10, 12, juce::Justification::left, false);
                g.setColour (ink);
                g.drawText (juce::String (slot + 1) + " " + slotTypeNames()[type].toUpperCase(), in.getX() + 10, in.getY(), in.getWidth() - 10, 12, juce::Justification::left, true);
                auto gr = juce::Rectangle<int> (in.getX(), in.getY() + 15, in.getWidth(), 24);
                look::fillDisp (g, gr);
                drawSlotGraph (g, gr.toFloat().reduced (2.0f, 2.0f), type, owner.proc.value (ids::slot (slot + 1, "a")),
                               owner.proc.value (ids::slot (slot + 1, "b")), owner.proc.value (ids::slot (slot + 1, "c")), false);
                g.setColour (ink); g.setFont (look::mono (10.5f, true));
                const int place = (int) owner.proc.value (ids::slot (slot + 1, "place"));
                g.drawText (slotValueText (type, 0, owner.proc.value (ids::slot (slot + 1, "a")), place), in.getX(), in.getBottom() - 12, in.getWidth() - 34, 12, juce::Justification::left, true);
                // where it runs: inside the loop (every pass) or after it (once)
                g.setFont (look::caps (9.5f));
                g.drawText (place == PlaceLoop ? "LOOP" : "AFTER", in.getRight() - 34, in.getBottom() - 12, 34, 12, juce::Justification::right, false);
                if (sel) // remove cross
                {
                    const auto x = removeArea().toFloat().reduced (3.0f);
                    g.drawLine (x.getX(), x.getY(), x.getRight(), x.getBottom(), 1.6f);
                    g.drawLine (x.getRight(), x.getY(), x.getX(), x.getBottom(), 1.6f);
                }
            }
            if (dropHover) { g.setColour (ink); g.drawRect (r, 3); }
        }
        juce::Rectangle<int> removeArea() const { return { getWidth() - 16, 2, 14, 14 }; }
        void mouseDown (const juce::MouseEvent& e) override
        {
            const int type = (int) owner.proc.value (ids::slot (slot + 1, "type"));
            if (owner.selected == slot && type != SlotOff && removeArea().contains (e.getPosition())) { owner.removeSlot (slot); return; }
            owner.select (slot);
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (e.getDistanceFromDragStart() < 6) return;
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            owner.hoverAt (e.getEventRelativeTo (&owner).position, slot);
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            setMouseCursor (juce::MouseCursor::NormalCursor);
            if (e.getDistanceFromDragStart() >= 6) owner.dropAt (e.getEventRelativeTo (&owner).position, slot);
        }
        LoopRing& owner;
        int slot;
        bool dropHover = false;
    };

    void hoverAt (juce::Point<float> p, int from)
    {
        for (int s = 0; s < kNumSlots; ++s)
        {
            const bool h = s != from && tiles[(size_t) s]->getBounds().toFloat().contains (p);
            if (h != tiles[(size_t) s]->dropHover) { tiles[(size_t) s]->dropHover = h; tiles[(size_t) s]->repaint(); }
        }
    }
    void dropAt (juce::Point<float> p, int from)
    {
        for (int s = 0; s < kNumSlots; ++s)
        {
            tiles[(size_t) s]->dropHover = false;
            if (s != from && tiles[(size_t) s]->getBounds().toFloat().contains (p)) swapSlots (from, s);
        }
        for (auto& t : tiles) t->repaint();
    }

    juce::Point<float> centre() const { return { getWidth() * 0.5f, getHeight() * 0.5f + 4.0f }; }
    float rx() const { return getWidth() * 0.5f - 70.0f; }
    float ry() const { return getHeight() * 0.5f - 45.0f; }

    void timerCallback() override
    {
        activity = proc.engine.report.activeVoices.load() > 0 ? 1.0f : activity * 0.9f;
        const float fb = proc.value (ids::feedback);
        phase += 0.004f + 0.02f * fb;
        if (phase > 1.0f) phase -= 1.0f;
        repaint();
        for (auto& t : tiles) t->repaint();
    }

    SaneProcessor& proc;
    std::function<void (int)> onSelect;
    std::vector<std::unique_ptr<Tile>> tiles;
    float phase = 0.0f, activity = 0.0f;
};

// ------------------------------------------------------------------------------------------------
class ModesView : public juce::Component, private juce::Timer
{
public:
    explicit ModesView (SaneProcessor& p) : proc (p) { startTimerHz (20); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        look::fillDisp (g, r);
        const auto p = proc.readParams();
        const float W = (float) r.getWidth(), H = (float) r.getHeight(), base = H * 0.6f;
        const double maxR = 14.0;
        auto X = [&] (double ratio) { return 14.0f + (float) (std::log2 (std::max (1.0, ratio)) / std::log2 (maxR)) * (W - 28.0f); };
        // grid at whole ratios + 12-EDO semitone ticks on top (the scale link replaces these in v0.2)
        g.setColour (paper.withAlpha (0.18f));
        for (int k = 1; k <= 14; ++k) g.drawVerticalLine ((int) X (k), 24.0f, H - 22.0f);
        g.setColour (paper);
        for (int st = 0; st <= 46; ++st) { const float x = X (std::pow (2.0, st / 12.0)); g.drawLine (x, st % 12 == 0 ? 4.0f : 9.0f, x, st % 12 == 0 ? 20.0f : 16.0f, 1.2f); }
        g.setColour (paper.withAlpha (0.34f)); g.drawHorizontalLine ((int) base, 14.0f, W - 14.0f);

        const auto ratios = computeRatios (p);
        const int n = linesForCore (p.core);
        const float rr = proc.engine.report.strikeR.load();
        const double t60 = feedbackToT60 (p.feedback);
        juce::ignoreUnused (t60);
        float mx = 1e-6f;
        std::vector<float> lv ((size_t) kMaxLines);
        for (int i = 0; i < kMaxLines; ++i)
        {
            lv[(size_t) i] = (float) (dsp::modes::strikeWeight (p.table, i, rr) * std::pow (i + 1.0, -(double) p.rolloff));
            if (i < n) mx = std::max (mx, lv[(size_t) i]);
        }
        for (int i = 0; i < kMaxLines; ++i)
        {
            const double a = dsp::modes::fromSource (p.srcA, p.table, p.calcSeed, i), b = dsp::modes::fromSource (p.srcB, p.table, p.calcSeed, i);
            const bool used = i < n;
            const double ratio = p.core == CoreDelay ? (double) (i + 1) : ratios[(size_t) i];
            if (ratio > maxR) continue;
            const float x = X (ratio);
            const float lvl = p.core == CoreDelay ? 0.9f * std::pow ((float) (i + 1), -1.0f) : lv[(size_t) i] / mx;
            const float y = base - lvl * (base - 30.0f);
            const float dec = (float) (1.0 / (1.0 + p.damp * 2.5 * std::max (0.0, ratio - 1.0)));
            if (p.core != CoreDelay && used)
            {
                g.setColour (ink.withAlpha (0.6f));
                if (a <= maxR && b <= maxR) { juce::Path tr; tr.startNewSubPath (X (a), base); tr.lineTo (X (b), base); look::strokeDashed (g, tr, 1.4f, 3.0f, 3.0f); }
                g.setColour (ink);
                if (a <= maxR) { juce::Path d; const float xa = X (a); d.addTriangle (xa, base - 5, xa + 5, base, xa, base + 5); d.addTriangle (xa, base - 5, xa - 5, base, xa, base + 5); g.fillPath (d); }
                g.setColour (paper);
                if (b <= maxR) { juce::Path t; const float xb = X (b); t.addTriangle (xb, base - 14, xb + 4, base - 7, xb - 4, base - 7); g.fillPath (t); }
            }
            g.setColour (used || p.core == CoreDelay ? paper : paper.withAlpha (0.25f));
            g.drawLine (x, base, x, y, 2.2f);
            g.fillRect (x - 3.5f, y - 3.5f, 7.0f, 7.0f);
            g.setColour (used ? ink : ink.withAlpha (0.3f));
            g.drawLine (x, base + 6, x, base + 6 + dec * (H - base - 30.0f), 1.6f);
            if (p.core == CoreDelay && i >= 9) break;
        }
        g.setColour (look::dispText);
        g.setFont (look::mono (10.0f));
        g.drawText (look::u8 ("level \xe2\x96\xb2"), 6, 24, 60, 12, juce::Justification::left, false);
        g.drawText (look::u8 ("decay \xe2\x96\xbc"), 6, (int) H - 34, 60, 12, juce::Justification::left, false);
        g.drawText ("ticks = 12-EDO", (int) W - 106, 24, 100, 12, juce::Justification::right, false);
        for (int k : { 1, 2, 3, 4, 6, 8, 12 })
            g.drawText (juce::String (k) + look::u8 ("\xc3\x97"), (int) X (k) - 3, (int) H - 15, 30, 12, juce::Justification::left, false);
        if (p.core == CoreDelay) g.drawText ("delay core: one loop = harmonic series", 70, (int) H - 34, 260, 12, juce::Justification::left, false);
    }

private:
    void timerCallback() override { repaint(); }
    SaneProcessor& proc;
};

// ------------------------------------------------------------------------------------------------
class SlotEditor : public juce::Component, private juce::Timer
{
public:
    explicit SlotEditor (SaneProcessor& p) : proc (p),
        typeBtn (look::u8 ("type \xe2\x96\xbe"), [this] { picker = ! picker; layoutPicker(); repaint(); }),
        removeBtn (look::u8 ("\xc3\x97 remove"), [this] { setType (SlotOff); })
    {
        typeBtn.setTooltip ("Choose the effect in this slot.");
        removeBtn.setTooltip ("Remove the effect from this slot (the slot becomes empty).");
        for (auto* b : { &typeBtn, &removeBtn }) addAndMakeVisible (*b);
        for (int s = 0; s < kNumSlots; ++s)   // one placement switch per slot; only the selected slot's is shown
        {
            placeChips.push_back (std::make_unique<Chips> (p.param (ids::slot (s + 1, "place")), juce::StringArray { "in loop", "after loop" }));
            addChildComponent (*placeChips.back());
        }
        for (int k = 0; k < 3; ++k)
        {
            knobs[(size_t) k] = std::make_unique<Knob> (p.param (ids::slot (1, k == 0 ? "a" : (k == 1 ? "b" : "c"))), "-");
            addAndMakeVisible (*knobs[(size_t) k]);
        }
        for (int t = 0; t < slotTypeNames().size(); ++t)
        {
            auto b = std::make_unique<Button> (t == 0 ? look::u8 ("off \xc2\xb7 remove") : slotTypeNames()[t], [this, t] { setType (t); });
            addChildComponent (*b);
            typeButtons.push_back (std::move (b));
        }
        setSlot (2);
        startTimerHz (10);
    }

    void setSlot (int s)
    {
        slot = s;
        for (int k = 0; k < kNumSlots; ++k) placeChips[(size_t) k]->setVisible (k == s);
        for (int k = 0; k < 3; ++k)
            knobs[(size_t) k]->attach (proc.param (ids::slot (s + 1, k == 0 ? "a" : (k == 1 ? "b" : "c"))));
        lastType = -1;
        timerCallback();
        picker = currentType() == SlotOff;
        layoutPicker();
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (22);
        for (auto& pc : placeChips) pc->setBounds (getWidth() - pc->preferredWidth(), 0, pc->preferredWidth(), 22);
        graph = r.removeFromTop (164).withTrimmedTop (6);
        r.removeFromTop (4);
        const int kw = 90;
        for (int k = 0; k < 3; ++k) knobs[(size_t) k]->setBounds (r.removeFromLeft (kw).withHeight (71));
        layoutPicker();
    }

    void paint (juce::Graphics& g) override
    {
        const int type = currentType();
        const int w = look::drawTitle (g, "slot " + juce::String (slot + 1) + look::u8 (" \xc2\xb7 ") + (type == SlotOff ? juce::String ("empty") : slotTypeNames()[type]), 0, 3);
        typeBtn.setBounds (w + 6, 0, typeBtn.preferredWidth(), 22);
        removeBtn.setBounds (typeBtn.getRight() + 4, 0, removeBtn.preferredWidth(), 22);
        removeBtn.setVisible (type != SlotOff);
        const int px = placeChips[(size_t) slot]->getX();
        look::drawLabel (g, "runs", juce::Rectangle<int> (px - 50, 0, 46, 22), juce::Justification::centredRight);
        look::fillDisp (g, graph);
        if (! picker && type != SlotOff)
        {
            drawSlotGraph (g, graph.toFloat().reduced (10.0f, 22.0f), type, val ("a"), val ("b"), val ("c"), true);
            g.setColour (look::dispText); g.setFont (look::mono (10.5f));
            g.drawText (slotInfo (type).hint, graph.getX() + 8, graph.getY() + 5, graph.getWidth() - 16, 13, juce::Justification::left, true);
        }
        else
        {
            g.setColour (look::dispText); g.setFont (look::mono (10.5f));
            g.drawText ("choose an effect for slot " + juce::String (slot + 1) + look::u8 (" \xc2\xb7 it runs inside the loop, on every pass"),
                        graph.getX() + 8, graph.getY() + 5, graph.getWidth() - 16, 13, juce::Justification::left, true);
        }
        auto info = getLocalBounds().withTop (graph.getBottom() + 8).withTrimmedLeft (290);
        g.setColour (look::dim); g.setFont (look::sans (13.0f));
        const bool inLoop = currentPlace() == PlaceLoop;
        g.drawFittedText (type == SlotOff ? "An empty slot passes the sound through unchanged."
                          : (inLoop ? "IN LOOP: runs on every pass, so it shapes the ring itself. Small settings already change a lot."
                                    : "AFTER LOOP: runs once on the output, like a normal effect (C = dry/wet mix)."),
                          info.reduced (4, 6), juce::Justification::topLeft, 4);
    }

private:
    int currentType() const { return (int) proc.value (ids::slot (slot + 1, "type")); }
    int currentPlace() const { return (int) proc.value (ids::slot (slot + 1, "place")); }
    float val (const char* k) const { return proc.value (ids::slot (slot + 1, k)); }

    void setType (int t)
    {
        auto set = [this] (const char* nm, float v) {
            auto& p = proc.param (ids::slot (slot + 1, nm));
            p.beginChangeGesture(); p.setValueNotifyingHost (p.convertTo0to1 (v)); p.endChangeGesture();
        };
        set ("type", (float) t);
        // fresh knob values that make sense for the new effect (no meaning carried over from the old one)
        const auto d = slotDefaults (t);
        set ("a", d.a); set ("b", d.b); set ("c", d.c);
        picker = false;
        layoutPicker();
        repaint();
    }

    void layoutPicker()
    {
        const int cols = 4, gap = 6;
        auto area = graph.reduced (10).withTrimmedTop (18);
        const int bw = (area.getWidth() - gap * (cols - 1)) / cols, bh = 28;
        for (size_t i = 0; i < typeButtons.size(); ++i)
        {
            auto& b = *typeButtons[i];
            b.setBounds (area.getX() + (int) (i % cols) * (bw + gap), area.getY() + (int) (i / cols) * (bh + gap), bw, bh);
            b.on = (int) i == currentType();
            b.setVisible (picker);
        }
    }

    void timerCallback() override
    {
        const int t = currentType(), pl = currentPlace();
        if (t != lastType || pl != lastPlace)
        {
            lastType = t; lastPlace = pl;
            for (int k = 0; k < 3; ++k)
            {
                knobs[(size_t) k]->setLabel (slotKnobLabel (t, k, pl));
                knobs[(size_t) k]->valueText = [t, k, pl] (float v) { return slotValueText (t, k, v, pl); };
                knobs[(size_t) k]->setVisible (t != SlotOff);
            }
            layoutPicker();
            repaint();
        }
        repaint (graph);
    }

    SaneProcessor& proc;
    int slot = 2, lastType = -1, lastPlace = -1;
    bool picker = false;
    juce::Rectangle<int> graph;
    Button typeBtn, removeBtn;
    std::vector<std::unique_ptr<Chips>> placeChips;
    std::array<std::unique_ptr<Knob>, 3> knobs;
    std::vector<std::unique_ptr<Button>> typeButtons;
};

// ------------------------------------------------------------------------------------------------
class HitView : public juce::Component, private juce::Timer
{
public:
    explicit HitView (SaneProcessor& p) : proc (p) { startTimerHz (30); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        look::fillDisp (g, r);
        const float W = (float) r.getWidth(), H = (float) r.getHeight(), cy = H * 0.5f;
        const double T = kScopeSeconds;
        g.setColour (paper.withAlpha (0.18f));
        for (int ms = 300; ms < 1200; ms += 300) g.drawVerticalLine ((int) (ms / 1200.0 * W), 0.0f, H);
        g.drawHorizontalLine ((int) cy, 0.0f, W);
        // waveform of the last hit (peak per bin, mirrored)
        auto& rp = proc.engine.report;
        const int fill = rp.scopeFill.load();
        float mx = 1e-4f;
        for (int b = 0; b < fill; ++b) mx = std::max (mx, rp.scope[(size_t) b].load());
        juce::Path wave, env;
        for (int b = 0; b < fill; ++b)
        {
            const float x = (float) b / kScopeBins * W, a = rp.scope[(size_t) b].load() / mx * (cy - 8);
            if (b == 0) { wave.startNewSubPath (x, cy - a); env.startNewSubPath (x, cy - a); } else { wave.lineTo (x, cy - a); env.lineTo (x, cy - a); }
        }
        for (int b = fill - 1; b >= 0; --b) wave.lineTo ((float) b / kScopeBins * W, cy + rp.scope[(size_t) b].load() / mx * (cy - 8));
        wave.closeSubPath();
        g.setColour (paper.withAlpha (0.26f)); g.fillPath (wave);
        g.setColour (paper); g.strokePath (env, juce::PathStrokeType (1.4f));

        // pitch drop (black) + zero line
        const float amt = proc.value (ids::pitchAmt), tau = proc.value (ids::pitchTime) * 0.001f;
        juce::Path zero; zero.startNewSubPath (0, pitchY (0.0f)); zero.lineTo (W, pitchY (0.0f));
        g.setColour (paper.withAlpha (0.4f)); look::strokeDashed (g, zero, 1.0f, 2.0f, 3.0f);
        juce::Path pc;
        for (int i = 0; i <= 160; ++i) { const float t = (float) (i / 160.0 * T), x = t / (float) T * W, y = pitchY (amt * std::exp (-t / std::max (0.002f, tau)));
                                         if (i == 0) pc.startNewSubPath (x, y); else pc.lineTo (x, y); }
        g.setColour (ink); g.strokePath (pc, juce::PathStrokeType (2.2f));
        // length
        const float len = proc.value (ids::length);
        if (len < kLengthOff - 0.5f)
        {
            const float lx = std::min (W, len * 0.001f / (float) T * W);
            g.setColour (ink.withAlpha (0.32f)); g.fillRect (lx, 0.0f, W - lx, H);
            g.setColour (paper); g.drawLine (lx, 0.0f, lx, H, 2.2f);
        }
        // handles
        g.setColour (paper); const auto lh = lenHandle(); g.fillRect (lh);
        for (auto h : { amtHandle(), tauHandle() }) { g.setColour (ink); g.fillRect (h); g.setColour (paper); g.drawRect (h, 1.2f); }
        // playhead
        if (sinceHit < T) { g.setColour (paper); g.fillRect ((float) (sinceHit / T) * W, 0.0f, 2.0f, H); }
        g.setColour (look::dispText); g.setFont (look::mono (10.0f));
        const auto a = amtHandle();
        g.drawText ("pitch " + proc.param (ids::pitchAmt).getCurrentValueAsText(), (int) a.getRight() + 6, (int) a.getY() - 2, 120, 12, juce::Justification::left, false);
        const auto t = tauHandle();
        g.drawText (look::u8 ("\xcf\x84 ") + proc.param (ids::pitchTime).getCurrentValueAsText(), (int) t.getRight() + 5, (int) t.getY() - 2, 90, 12, juce::Justification::left, false);
        g.drawText ("0 st", 6, (int) pitchY (0.0f) + 2, 40, 12, juce::Justification::left, false);
        g.drawText ("length " + proc.param (ids::length).getCurrentValueAsText(), (int) lh.getX() - 112, 4, 106, 12, juce::Justification::right, false);
        for (int ms = 0; ms < 1200; ms += 300) g.drawText (juce::String (ms) + " ms", (int) (ms / 1200.0 * W) + 4, (int) H - 14, 60, 12, juce::Justification::left, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        drag = -1;
        if (amtHandle().expanded (6).contains (e.position)) drag = 0;
        else if (tauHandle().expanded (6).contains (e.position)) drag = 1;
        else if (lenHandle().expanded (6).contains (e.position) || std::abs (e.position.x - lenX()) < 6) drag = 2;
        if (drag >= 0) param().beginChangeGesture();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag < 0) return;
        auto& p = param();
        const float W = (float) getWidth();
        float v = 0.0f;
        if (drag == 0) v = juce::jmap (e.position.y, pitchY (0.0f), pitchY (12.0f), 0.0f, 12.0f);
        if (drag == 1) v = std::max (2.0f, e.position.x / W * (float) kScopeSeconds * 1000.0f);
        if (drag == 2) { v = e.position.x / W * (float) kScopeSeconds * 1000.0f; if (e.position.x > W - 4) v = kLengthOff; }
        p.setValueNotifyingHost (p.convertTo0to1 (p.getNormalisableRange().snapToLegalValue (v)));
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { if (drag >= 0) param().endChangeGesture(); drag = -1; }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool over = amtHandle().expanded (6).contains (e.position) || tauHandle().expanded (6).contains (e.position) || lenHandle().expanded (6).contains (e.position);
        setMouseCursor (over ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }

private:
    juce::RangedAudioParameter& param() { return proc.param (drag == 0 ? ids::pitchAmt : (drag == 1 ? ids::pitchTime : ids::length)); }
    // pitch view: -6 .. +12 semitones (values outside are pinned to the edge; the label shows the real value)
    float pitchY (float st) const { return juce::jmap (juce::jlimit (-6.0f, 12.0f, st), -6.0f, 12.0f, (float) getHeight() - 20.0f, 14.0f); }
    float lenX() const { const float len = proc.value (ids::length); return len >= kLengthOff - 0.5f ? (float) getWidth() - 7.0f : std::min ((float) getWidth() - 7.0f, len * 0.001f / (float) kScopeSeconds * (float) getWidth()); }
    juce::Rectangle<float> amtHandle() const { return { 1.0f, pitchY (proc.value (ids::pitchAmt)) - 5.5f, 11.0f, 11.0f }; }
    juce::Rectangle<float> tauHandle() const
    {
        const float tau = proc.value (ids::pitchTime) * 0.001f;
        const float x = std::min ((float) getWidth() - 6.0f, tau / (float) kScopeSeconds * (float) getWidth());
        return { x - 5.5f, pitchY (proc.value (ids::pitchAmt) * 0.3679f) - 5.5f, 11.0f, 11.0f };
    }
    juce::Rectangle<float> lenHandle() const { return { lenX() - 6.0f, 4.0f, 12.0f, 12.0f }; }

    void timerCallback() override
    {
        const int c = proc.engine.report.counter.load();
        if (c != seen) { seen = c; hitTime = juce::Time::getMillisecondCounterHiRes(); }
        sinceHit = seen > 0 ? (juce::Time::getMillisecondCounterHiRes() - hitTime) * 0.001 : 99.0;
        repaint();
    }

    SaneProcessor& proc;
    int drag = -1, seen = 0;
    double hitTime = 0.0, sinceHit = 99.0;
};

// ------------------------------------------------------------------------------------------------
class KeyStrip : public juce::Component, private juce::Timer
{
public:
    static constexpr int kFirst = 36, kKeys = 36; // C2 .. B4
    explicit KeyStrip (SaneProcessor& p) : proc (p) { light.fill (0.0f); startTimerHz (30); }

    void paint (juce::Graphics& g) override
    {
        const float ww = (float) getWidth() / 21.0f, H = (float) getHeight();
        int wi = 0;
        for (int k = 0; k < kKeys; ++k)
        {
            if (isBlack (k)) continue;
            auto r = juce::Rectangle<float> (wi * ww, 0.0f, ww, H);
            g.setColour (look::ground.interpolatedWith (paper, light[(size_t) k])); g.fillRect (r);
            g.setColour (ink); g.drawRect (r, 1.5f);
            if (k % 12 == 0)
            {
                g.setFont (look::mono (10.0f, true));
                g.drawText ("C" + juce::String ((kFirst + k) / 12 - 1), r.withTrimmedBottom (3), juce::Justification::centredBottom, false);
            }
            ++wi;
        }
        wi = 0;
        for (int k = 0; k < kKeys; ++k)
        {
            if (! isBlack (k)) { ++wi; continue; }
            auto r = juce::Rectangle<float> (wi * ww - 8.0f, 0.0f, 16.0f, H * 0.58f);
            g.setColour (ink.interpolatedWith (paper, light[(size_t) k])); g.fillRect (r);
            g.setColour (ink); g.drawRect (r, 1.5f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { play (e); }
    void mouseDrag (const juce::MouseEvent& e) override { if (keyAt (e.position) != down) play (e); }
    void mouseUp (const juce::MouseEvent&) override { release(); }

private:
    static bool isBlack (int k) { const int n = k % 12; return n == 1 || n == 3 || n == 6 || n == 8 || n == 10; }
    int keyAt (juce::Point<float> p) const
    {
        const float ww = (float) getWidth() / 21.0f, H = (float) getHeight();
        if (p.y < H * 0.58f)
        {
            int wi = 0;
            for (int k = 0; k < kKeys; ++k)
            {
                if (! isBlack (k)) { ++wi; continue; }
                if (std::abs (p.x - wi * ww) < 8.0f) return k;
            }
        }
        const int w = juce::jlimit (0, 20, (int) (p.x / ww));
        int wi = 0;
        for (int k = 0; k < kKeys; ++k) if (! isBlack (k)) { if (wi == w) return k; ++wi; }
        return -1;
    }
    void play (const juce::MouseEvent& e)
    {
        release();
        down = keyAt (e.position);
        if (down < 0) return;
        const float vel = juce::jlimit (0.15f, 1.0f, 0.25f + 0.75f * e.position.y / (float) getHeight());
        proc.uiKeys.noteOn (1, kFirst + down, vel);
    }
    void release() { if (down >= 0) proc.uiKeys.noteOff (1, kFirst + down, 0.0f); down = -1; }

    void timerCallback() override
    {
        auto& rp = proc.engine.report;
        bool changed = false;
        for (int k = 0; k < kKeys; ++k)
        {
            const int n = kFirst + k;
            const bool held = ((rp.held[(size_t) (n >> 6)].load() >> (n & 63)) & 1ull) != 0;
            float& l = light[(size_t) k];
            const float nl = held ? 1.0f : l * 0.85f;   // press = instant white, release = fade (~0.5 s)
            if (std::abs (nl - l) > 0.002f || (nl < 0.002f && l > 0.0f)) { l = nl < 0.002f ? 0.0f : nl; changed = true; }
        }
        if (changed) repaint();
    }

    SaneProcessor& proc;
    std::array<float, kKeys> light {};
    int down = -1;
};
} // namespace sane::ui
