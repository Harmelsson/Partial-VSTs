#pragma once
// The Calculator's views, meant to sit on top of every synth in this repo. ArtHar look + ArtHar
// editing (user brief v0.3: "adding custom notes/partials, marked modifications that return to the
// calculator's value when removed").
//
//   CurvePlot    dissonance curve (or, for the consonance method, one bar per found note).
//                Notes: DRAG sideways = move (sticky offset from its home), DOUBLE-CLICK empty
//                space = add a custom note, RIGHT-CLICK = switch a note off/on (custom: remove),
//                MIDDLE-CLICK (or Alt+click) = back to the calculator's value.
//   PartialsPlot loudness vs distance from the fundamental. DRAG up/down = loudness edit,
//                sideways = move it (the axis locks after a few pixels), DOUBLE-CLICK empty space =
//                add a custom partial, RIGHT-CLICK = mute/unmute (custom: remove), MIDDLE-CLICK =
//                remove the edit.
//   ScaleTable   on / note / cents / ratio / Hz / strength. Click the square = switch on/off.
// Hand-modified things are drawn in theme::modified (orange). Numbers shown are exact; only the
// drawing is eased/animated.
#include "ui/Controls.h"
#include "ui/SquareLattice.h"
#include "xen/CalculatorWorker.h"

namespace pvst
{
namespace detail
{
    inline juce::String fmt (double v, int decimals) { return juce::String (v, decimals); }

    /// Draws axes/grid for a plot and returns the inner data area.
    inline juce::Rectangle<float> axes (juce::Graphics& g, juce::Rectangle<float> r, double x0, double x1, double y0, double y1,
                                        double yStep, int yDecimals, const juce::String& xTitle, const juce::String& yTitle)
    {
        auto data = r.withTrimmedLeft (46.0f).withTrimmedBottom (30.0f).withTrimmedTop (4.0f).withTrimmedRight (6.0f);
        g.setFont (theme::sans (11.5f));
        for (double y = y0; y <= y1 + 1e-9; y += yStep)
        {
            const float py = data.getBottom() - (float) ((y - y0) / (y1 - y0)) * data.getHeight();
            g.setColour (theme::grid());
            g.drawHorizontalLine ((int) py, data.getX(), data.getRight());
            g.setColour (theme::dim());
            g.drawText (fmt (y, yDecimals), juce::Rectangle<float> (data.getX() - 40.0f, py - 7.0f, 34.0f, 14.0f), juce::Justification::centredRight);
        }
        const double xs = plot::tickStep (x1 - x0, data.getWidth(), 42.0f);
        for (double x = std::ceil (x0 / xs) * xs; x <= x1 + 1e-9; x += xs)
        {
            const float px = data.getX() + (float) ((x - x0) / (x1 - x0)) * data.getWidth();
            g.setColour (theme::grid());
            g.drawVerticalLine ((int) px, data.getY(), data.getBottom());
            g.setColour (theme::dim());
            g.drawText (juce::String (juce::roundToInt (x)), juce::Rectangle<float> (px - 30.0f, data.getBottom() + 3.0f, 60.0f, 14.0f), juce::Justification::centred);
        }
        g.setColour (theme::text());
        g.drawText (xTitle, juce::Rectangle<float> (data.getX(), data.getBottom() + 16.0f, data.getWidth(), 14.0f), juce::Justification::centred);
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, r.getX() + 8.0f, data.getCentreY()));
        g.drawText (yTitle, juce::Rectangle<float> (r.getX() + 8.0f - 60.0f, data.getCentreY() - 7.0f, 120.0f, 14.0f), juce::Justification::centred);
        g.restoreState();
        return data;
    }

    inline juce::Rectangle<float> plotArea (juce::Rectangle<int> bounds)
    {
        auto inner = bounds.reduced (9, 6).toFloat();
        inner.removeFromTop (18.0f);
        return inner.withTrimmedLeft (46.0f).withTrimmedBottom (30.0f).withTrimmedTop (4.0f).withTrimmedRight (6.0f);
    }
} // namespace detail

// ================================================================================================
class CurvePlot : public juce::Component, public juce::DragAndDropTarget
{
public:
    /// A modulator chip dropped on a scale note (v0.7).
    std::function<void (const xen::FinalNote&, int mod)> onModDrop;
    std::function<void (const xen::FinalNote&)> onDragBegin, onDragEnd, onReset, onToggle;
    std::function<void (const xen::FinalNote&, double cents)> onDrag;
    std::function<void (double cents)> onAdd;

    void setResult (const xen::CalculatorOutput& r)
    {
        result = r;
        const double span = std::abs (r.curve.periodCents) + xen::DissonanceCalculator::kOvershootCents;
        frameMax = std::max (600.0, std::ceil (span / 600.0) * 600.0); // steady frame, 600-cent steps
    }
    void setScale (const xen::FinalScale& s)
    {
        scale = s;
        // A locked scale can be longer than the live curve: keep its notes inside the frame.
        if (s.valid) frameMax = std::max (frameMax, std::ceil ((s.period + xen::DissonanceCalculator::kOvershootCents) / 600.0) * 600.0);
    }
    /// Scale lock (v0.6): the notes drawn are a frozen snapshot, the curve underneath is live.
    void setLocked (bool l) { locked = l; }
    /// Optional label drawn above each note (index = FinalScale index), e.g. the key playing it.
    void setNoteLabels (std::vector<juce::String> l) { labels = std::move (l); }
    /// Where the keyboard's keys land (v0.5, "visualise every scale type"): small labelled
    /// triangles on the curve's floor. show12Edo adds faint 12-EDO guide lines (every 100 c) so
    /// the deviation of a stretched keyboard is visible.
    struct KeyMarker { double cents; juce::String label; };
    void setKeyMarkers (std::vector<KeyMarker> m, bool show12Edo, double edoOffsetCents)
    {
        keyMarkers = std::move (m); showEdo = show12Edo; edoOffset = edoOffsetCents;
    }
    /// 0..1 per FinalScale note: how "lit" it is because it's being played (ArtHar's emphasis).
    void setEmphasis (std::vector<float> e) { emphasis = std::move (e); }
    void setBusy (bool b) { busy = b; }
    void tick (float dt) { lastDt = dt; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const bool consonance = result.settings.method == xen::Method::Consonance;
        const auto method = result.settings.method;
        const juce::String title = consonance ? "Consonance (cross-note)"
                                 : method == xen::Method::Edo ? "Dissonance curve  -  EDO steps"
                                 : method == xen::Method::SeedEdo ? "Dissonance curve  -  equal steps of the seed's octave"
                                 : "Dissonance curve";
        auto inner = theme::drawSection (g, getLocalBounds(), title).toFloat();
        if (locked)
        {
            const float tw = juce::GlyphArrangement::getStringWidth (theme::sans (11.5f).withExtraKerningFactor (0.22f), title.toUpperCase());
            g.setColour (theme::modified);
            g.setFont (theme::sans (11.5f, true).withExtraKerningFactor (0.22f));
            g.drawText ("SCALE LOCKED", juce::Rectangle<float> (9.0f + tw + 16.0f, 6.0f, 140.0f, 18.0f), juce::Justification::centredLeft);
        }
        g.setColour (theme::dim());
        g.setFont (theme::sans (11.5f));
        const juce::String hint = busy ? juce::String ("calculating...")
            : theme::u8 ("drag = move  \xc2\xb7  dbl-click = add  \xc2\xb7  right-click = on/off  \xc2\xb7  middle = home");
        g.drawText (hint, getLocalBounds().reduced (10, 6).removeFromTop (18), juce::Justification::centredRight);

        auto data = detail::axes (g, inner, 0.0, frameMax, 0.0, 100.0, 20.0, 0, "interval above the unison (cents)",
                                  consonance ? "shared with unison %" : "roughness %");
        lattice.setArea (data);
        const int cols = lattice.numCols();
        shown.resize ((size_t) cols, -1.0f);
        const auto& c = result.curve;
        auto colOfCents = [&] (double cents) { return (int) (cents / frameMax * cols); };

        // Targets per column. Dissonance: lowest curve point in the column. Consonance: a bar per note.
        std::vector<float> target ((size_t) cols, -1.0f), near ((size_t) cols, 0.0f);
        if (! consonance && c.valid && c.cents.size() > 2)
        {
            const double peak = std::max (1e-12, *std::max_element (c.dissonance.begin(), c.dissonance.end()));
            for (size_t i = 0; i < c.cents.size(); ++i)
            {
                const int col = colOfCents (c.cents[i]);
                if (col < 0 || col >= cols) continue;
                const float v = (float) (c.dissonance[i] / peak);
                auto& t = target[(size_t) col];
                t = t < 0.0f ? v : juce::jmin (t, v);
            }
        }
        if (consonance && scale.valid)
            for (int i = 0; i < scale.count; ++i)
            {
                const int col = colOfCents (scale.notes[(size_t) i].cents);
                if (col >= 0 && col < cols) target[(size_t) col] = (float) juce::jlimit (0.02, 1.0, scale.notes[(size_t) i].strength);
            }
        for (int i = 0; i < scale.count; ++i)
            for (int d = -3; d <= 3; ++d)
            {
                const int col = colOfCents (scale.notes[(size_t) i].cents) + d;
                if (col >= 0 && col < cols) near[(size_t) col] = juce::jmax (near[(size_t) col], 1.0f - (float) std::abs (d) / 4.0f);
            }

        lattice.beginFrame();
        for (int col = 0; col < cols; ++col)
        {
            auto& s = shown[(size_t) col];
            const float t = target[(size_t) col];
            if (t < 0.0f) { s = -1.0f; continue; }
            s = s < 0.0f ? t : theme::ease (s, t, 0.3f);
            lattice.column (col, s, 1.0f, ! consonance, near[(size_t) col]);
        }
        lattice.paint (g, lastDt);

        if (! scale.valid)
        {
            g.setColour (theme::dim());
            g.setFont (theme::sans (14.0f));
            g.drawText (std::abs (xen::periodCents (result.settings.seed)) < 10.0
                            ? "seed ~ 1: every partial is on one pitch, there is no \"octave\" to search"
                            : "calculating...", data.toNearestInt(), juce::Justification::centred);
            return;
        }

        // 12-EDO reference (faint dashed) and where each key lands (triangles on the floor).
        if (showEdo)
        {
            g.setFont (theme::sans (10.5f));
            for (double ec = edoOffset - 100.0 * std::floor (edoOffset / 100.0); ec <= frameMax; ec += 100.0)
            {
                const float x = xOf (ec);
                g.setColour (theme::mix (0.35f));
                for (float y = data.getY(); y < data.getBottom(); y += 7.0f) g.fillRect (x, y, 1.0f, 3.0f);
            }
            g.setColour (theme::mix (0.5f));
            g.drawText ("dashed = 12-EDO", data.withHeight (14.0f).translated (0.0f, 2.0f).withTrimmedRight (6.0f), juce::Justification::centredRight);
        }
        for (auto& k : keyMarkers)
        {
            if (k.cents < -1.0 || k.cents > frameMax) continue;
            const float x = xOf (k.cents), yb = data.getBottom() - 1.0f;
            juce::Path tri;
            tri.addTriangle (x - 4.0f, yb, x + 4.0f, yb, x, yb - 7.0f);
            g.setColour (theme::mix (0.8f));
            g.fillPath (tri);
            g.setFont (theme::sans (10.0f));
            g.drawText (k.label, juce::Rectangle<float> (x - 14.0f, yb - 20.0f, 28.0f, 12.0f), juce::Justification::centred);
        }

        // Notes: a clear vertical line, a big hollow square on the curve, and the key label on top
        // (user v0.3.1: "notes barely visible"). With many notes (EDO up to 120 steps) the squares
        // shrink to fit between neighbours.
        float noteSide = 14.0f;
        for (int i = 1; i < scale.count; ++i)
            noteSide = juce::jmin (noteSide, juce::jmax (5.0f, (xOf (scale.notes[(size_t) i].cents) - xOf (scale.notes[(size_t) i - 1].cents)) * 0.85f));
        for (int i = 0; i < scale.count; ++i)
        {
            const auto& n = scale.notes[(size_t) i];
            const float x = xOf (n.cents);
            const bool mod = n.moved || n.custom;
            const float e = (size_t) i < emphasis.size() ? emphasis[(size_t) i] : 0.0f; // being played
            const juce::Colour col = ! n.enabled ? theme::mix (0.35f) : mod ? theme::modified : theme::fg;
            g.setColour (col.withAlpha ((n.enabled ? 0.55f : 0.25f) + 0.45f * e));
            g.fillRect (x - 0.5f - e, data.getY(), 1.2f + 2.0f * e, data.getHeight());
            if (n.moved) // faint square at its home, like ArtHar
                SquareLattice::hollowSquare (g, { xOf (n.home), data.getBottom() - 6.0f }, 7.0f, theme::modified.withAlpha (0.6f));
            const int colIdx = juce::jlimit (0, cols - 1, lattice.colAt (x));
            const float h = juce::jmax (0.0f, shown[(size_t) colIdx]);
            const float y = lattice.rowCentreY ((int) std::round (h * (float) (lattice.numRows() - 1)));
            const float side = noteSide + 10.0f * e; // a played note's square grows
            g.setColour (theme::bg);
            g.fillRect (juce::Rectangle<float> (side, side).withCentre ({ x, y }));
            g.setColour (i == dropNote ? theme::modified : col);
            g.drawRect (juce::Rectangle<float> (side, side).withCentre ({ x, y }), i == dropNote ? 3.0f : 2.0f);
            g.fillRect (juce::Rectangle<float> (juce::jmin (4.0f, noteSide * 0.3f) + 8.0f * e, juce::jmin (4.0f, noteSide * 0.3f) + 8.0f * e).withCentre ({ x, y }));
            if (! n.enabled)
            {
                g.drawLine (x - 6, y - 6, x + 6, y + 6, 1.4f);
                g.drawLine (x - 6, y + 6, x + 6, y - 6, 1.4f);
            }
            if ((size_t) i < labels.size() && labels[(size_t) i].isNotEmpty())
            {
                g.setFont (theme::sans (11.5f, true));
                g.setColour (col);
                g.drawText (labels[(size_t) i], juce::Rectangle<float> (x - 20.0f, data.getY() + 1.0f, 40.0f, 13.0f), juce::Justification::centred);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        lastDownWasLeft = e.mods.isLeftButtonDown() && ! e.mods.isAltDown();
        const int i = noteAt (e.position.x);
        if (i < 0) return;
        const auto n = scale.notes[(size_t) i];
        if (n.kind == xen::ScaleNote::Unison || n.kind == xen::ScaleNote::Period) return; // the frame doesn't move
        if (e.mods.isMiddleButtonDown() || e.mods.isAltDown()) { if (onReset) onReset (n); return; }
        if (e.mods.isPopupMenu()) { if (onToggle) onToggle (n); return; }
        dragging = true;
        dragNote = n;
        dragStartX = e.position.x;
        dragStartCents = n.cents;
        if (onDragBegin) onDragBegin (n);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        const double perPx = frameMax / detail::plotArea (getLocalBounds()).getWidth() * (e.mods.isShiftDown() ? 0.1 : 1.0);
        if (onDrag) onDrag (dragNote, dragStartCents + (e.position.x - dragStartX) * perPx);
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging && onDragEnd) onDragEnd (dragNote);
        dragging = false;
    }
    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        if (! d.description.toString().startsWith ("mod:")) return false;
        const int i = noteAt ((float) d.localPosition.x);
        return i >= 0 && scale.notes[(size_t) i].kind == xen::ScaleNote::Found;
    }
    void itemDragEnter (const SourceDetails& d) override { dropNote = noteAt ((float) d.localPosition.x); }
    void itemDragMove (const SourceDetails& d) override { dropNote = noteAt ((float) d.localPosition.x); }
    void itemDragExit (const SourceDetails&) override { dropNote = -1; }
    void itemDropped (const SourceDetails& d) override
    {
        const int i = noteAt ((float) d.localPosition.x);
        dropNote = -1;
        if (i >= 0 && onModDrop) onModDrop (scale.notes[(size_t) i], modOfDrag (d));
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        // Only a LEFT double-click adds (two quick middle-clicks used to add notes by accident).
        if (! lastDownWasLeft) return;
        if (noteAt (e.position.x) >= 0 || ! detail::plotArea (getLocalBounds()).contains (e.position)) return;
        if (onAdd) onAdd (centsAt (e.position.x));
    }

private:
    float xOf (double cents) const { auto d = detail::plotArea (getLocalBounds()); return d.getX() + (float) (cents / frameMax) * d.getWidth(); }
    double centsAt (float x) const { auto d = detail::plotArea (getLocalBounds()); return (x - d.getX()) / d.getWidth() * frameMax; }
    int noteAt (float x) const
    {
        int best = -1; float bestD = 8.0f;
        for (int i = 0; i < scale.count; ++i)
            if (const float d = std::abs (xOf (scale.notes[(size_t) i].cents) - x); d < bestD) { bestD = d; best = i; }
        return best;
    }

    xen::CalculatorOutput result;
    xen::FinalScale scale;
    std::vector<juce::String> labels;
    std::vector<KeyMarker> keyMarkers;
    bool showEdo = false;
    double edoOffset = 0.0;
    std::vector<float> emphasis;
    bool lastDownWasLeft = false;
    SquareLattice lattice;
    std::vector<float> shown;
    double frameMax = 1800.0;
    float lastDt = 1.0f / 60.0f;
    bool busy = false, dragging = false, locked = false;
    int dropNote = -1;
    xen::FinalNote dragNote;
    float dragStartX = 0.0f;
    double dragStartCents = 0.0;
};

// ================================================================================================
class PartialsPlot : public juce::Component, public juce::DragAndDropTarget
{
public:
    std::function<void (const xen::ModelPartial&, int mod)> onModDrop; // a modulator chip dropped on a partial (v0.7)
    std::function<void (const xen::ModelPartial&)> onDragBegin, onDragEnd, onReset, onRightClick;
    std::function<void (const xen::ModelPartial&, double cents, double amp)> onDrag;
    std::function<void (double cents, double amp)> onAdd;

    void setLocked (bool l) { locked = l; }
    void setPartials (const std::vector<xen::ModelPartial>& p, double seedValue, double fundamentalHz)
    {
        parts = p; seed = seedValue; fundamental = fundamentalHz;
    }

    void tick (float dt)
    {
        lastDt = dt;
        std::array<bool, kSlots> present {};
        soundOnly.fill (false);
        double lo = 0.0, hi = 0.0;
        for (auto& p : parts)
        {
            const auto i = (size_t) p.index;
            present[i] = true;
            soundOnly[i] = ! p.inScale;
            const double h = std::min (1.0, p.amp);
            cents[i] = first[i] ? p.cents : theme::ease ((float) cents[i], (float) p.cents, 0.3f);
            height[i] = first[i] ? h : theme::ease ((float) height[i], (float) h, 0.3f);
            first[i] = false;
            lo = std::min (lo, p.cents); hi = std::max (hi, p.cents);
        }
        for (size_t i = 0; i < kSlots; ++i)
            if (! present[i]) { height[i] = theme::ease ((float) height[i], 0.0f, 0.3f); if (height[i] < 0.002) first[i] = true; }
        frameMin = std::floor (lo / 1200.0) * 1200.0;
        frameMax = std::max (frameMin + 1200.0, std::ceil ((hi + 60.0) / 1200.0) * 1200.0);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto inner = theme::drawSection (g, getLocalBounds(), "Partials").toFloat();
        if (locked)
        {
            const float tw = juce::GlyphArrangement::getStringWidth (theme::sans (11.5f).withExtraKerningFactor (0.22f), juce::String ("PARTIALS"));
            g.setColour (theme::modified);
            g.setFont (theme::sans (11.5f, true).withExtraKerningFactor (0.22f));
            g.drawText ("PARTIALS LOCKED", juce::Rectangle<float> (9.0f + tw + 16.0f, 6.0f, 180.0f, 18.0f), juce::Justification::centredLeft);
        }
        g.setColour (theme::dim());
        g.setFont (theme::sans (11.5f));
        g.drawText (theme::u8 ("drag = loudness / move  \xc2\xb7  dbl-click = add  \xc2\xb7  right-click = mute  \xc2\xb7  middle = undo edit"),
                    getLocalBounds().reduced (10, 6).removeFromTop (18), juce::Justification::centredRight);
        auto data = detail::axes (g, inner, frameMin, frameMax, 0.0, 1.0, 0.5, 1, "distance from the fundamental (cents)", "loudness");
        lattice.setArea (data);

        const double period = std::abs (xen::periodCents (std::max (1e-6, seed)));
        if (period > 30.0)
            for (double c = std::ceil (frameMin / period) * period; c <= frameMax; c += period)
                if (std::abs (c) > 1e-6) plot::verticalDotted (g, xOf (c), data.getY(), data.getBottom(), theme::mix (0.3f));

        lattice.beginFrame();
        for (size_t i = 0; i < kSlots; ++i)
        {
            if (height[i] < 0.004) continue;
            const double hz = fundamental * std::exp2 (cents[i] / 1200.0);
            // "Sound only" partials (unticked in the Scale column) are drawn dotted, like ArtHar.
            lattice.column (lattice.colAt (xOf (cents[i])), (float) height[i], hz >= 20.0 && hz <= 20000.0 ? 1.0f : 0.35f, soundOnly[i], 0.0f);
        }
        lattice.paint (g, lastDt);

        if (dropPartial >= 0 && (size_t) dropPartial < parts.size())
        {
            const auto& dp = parts[(size_t) dropPartial];
            const float hx = xOf (cents[(size_t) dp.index]), hy = data.getBottom() - (float) juce::jlimit (0.0, 1.0, height[(size_t) dp.index]) * data.getHeight();
            g.setColour (theme::modified);
            g.drawRect (juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ hx, hy }), 2.5f);
        }
        // Markers: hollow orange squares on edited/custom partials, a small x on muted ones.
        for (auto& p : parts)
        {
            const bool muted = p.amp <= 0.0;
            if (! (p.custom || p.moved || p.levelEdited)) continue;
            const float x = xOf (cents[(size_t) p.index]);
            const float y = data.getBottom() - (float) juce::jlimit (0.0, 1.0, height[(size_t) p.index]) * data.getHeight();
            SquareLattice::hollowSquare (g, { x, y - 3.0f }, 10.0f, theme::modified);
            if (muted)
            {
                g.setColour (theme::modified);
                g.drawLine (x - 4, data.getBottom() - 10, x + 4, data.getBottom() - 2, 1.2f);
                g.drawLine (x - 4, data.getBottom() - 2, x + 4, data.getBottom() - 10, 1.2f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        lastDownWasLeft = e.mods.isLeftButtonDown() && ! e.mods.isAltDown();
        const int i = partialAt (e.position.x);
        if (i < 0) return;
        const auto p = parts[(size_t) i];
        if (e.mods.isMiddleButtonDown() || e.mods.isAltDown()) { if (onReset) onReset (p); return; }
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick (p); return; }
        dragging = true; axis = 0;
        dragPartial = p;
        downPos = e.position;
        if (onDragBegin) onDragBegin (p);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        const auto d = e.position - downPos;
        if (axis == 0 && d.getDistanceFromOrigin() > 5.0f) axis = std::abs (d.x) > std::abs (d.y) ? 1 : 2; // lock axis
        const auto area = detail::plotArea (getLocalBounds());
        double c = dragPartial.cents, a = dragPartial.amp;
        if (axis == 1 && (dragPartial.index > 0 || dragPartial.custom))
            c = dragPartial.cents + d.x * (frameMax - frameMin) / area.getWidth() * (e.mods.isShiftDown() ? 0.1 : 1.0);
        if (axis == 2) a = juce::jlimit (0.0, 1.0, (double) ((area.getBottom() - e.position.y) / area.getHeight()));
        if (axis != 0 && onDrag) onDrag (dragPartial, c, a);
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging && onDragEnd) onDragEnd (dragPartial);
        dragging = false;
    }
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("mod:") && partialAt ((float) d.localPosition.x) >= 0; }
    void itemDragEnter (const SourceDetails& d) override { dropPartial = partialAt ((float) d.localPosition.x); }
    void itemDragMove (const SourceDetails& d) override { dropPartial = partialAt ((float) d.localPosition.x); }
    void itemDragExit (const SourceDetails&) override { dropPartial = -1; }
    void itemDropped (const SourceDetails& d) override
    {
        const int i = partialAt ((float) d.localPosition.x);
        dropPartial = -1;
        if (i >= 0 && onModDrop) onModDrop (parts[(size_t) i], modOfDrag (d));
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto area = detail::plotArea (getLocalBounds());
        if (! lastDownWasLeft || partialAt (e.position.x) >= 0 || ! area.contains (e.position)) return;
        const double c = frameMin + (e.position.x - area.getX()) / area.getWidth() * (frameMax - frameMin);
        const double a = juce::jlimit (0.05, 1.0, (double) ((area.getBottom() - e.position.y) / area.getHeight()));
        if (onAdd) onAdd (std::max (1.0, c), a);
    }

private:
    static constexpr size_t kSlots = (size_t) (xen::kSeriesPartials + xen::kCustomPartials);
    float xOf (double c) const { auto d = detail::plotArea (getLocalBounds()); return d.getX() + (float) ((c - frameMin) / (frameMax - frameMin)) * d.getWidth(); }
    int partialAt (float x) const
    {
        int best = -1; float bestD = 7.0f;
        for (size_t i = 0; i < parts.size(); ++i)
            if (const float d = std::abs (xOf (parts[i].cents) - x); d < bestD) { bestD = d; best = (int) i; }
        return best;
    }

    std::vector<xen::ModelPartial> parts;
    double seed = 2.0, fundamental = 261.63;
    SquareLattice lattice;
    std::array<double, kSlots> cents {}, height {};
    std::array<bool, kSlots> soundOnly {};
    bool lastDownWasLeft = false;
    std::array<bool, kSlots> first = [] { std::array<bool, kSlots> a {}; a.fill (true); return a; }();
    double frameMin = 0.0, frameMax = 7200.0;
    float lastDt = 1.0f / 60.0f;
    bool dragging = false, locked = false;
    int dropPartial = -1;
    int axis = 0;
    xen::ModelPartial dragPartial;
    juce::Point<float> downPos;
};

// ================================================================================================
class ScaleTable : public juce::Component, public juce::DragAndDropTarget
{
public:
    std::function<void (const xen::FinalNote&, int mod)> onModDrop; // chip dropped on a row's on/off square (v0.7)
    std::function<void (const xen::FinalNote&)> onToggle, onReset;

    void setScale (const xen::FinalScale& s, double fundamentalHz, const juce::String& footer)
    {
        scale = s; fundamental = fundamentalHz; footerText = footer; repaint();
    }
    void setEmphasis (std::vector<float> e) { emphasis = std::move (e); }

    void paint (juce::Graphics& g) override
    {
        auto inner = theme::drawSection (g, getLocalBounds(), "Scale");
        const int colX[] = { 0, 24, 88, 150, 212, 272 };
        const char* heads[] = { "", "Note", "Cents", "Ratio", "Hz", "Strength" };
        g.setFont (theme::sans (12.5f));
        g.setColour (theme::text());
        auto head = inner.removeFromTop (20);
        for (int i = 1; i < 6; ++i) g.drawText (heads[i], head.getX() + colX[i], head.getY(), 70, 20, juce::Justification::centredLeft);
        auto foot = inner.removeFromBottom (18);
        g.setColour (theme::dim());
        g.setFont (theme::sans (11.5f));
        g.drawText (footerText, foot, juce::Justification::centredLeft);

        rows.clear();
        maxRows = juce::jmax (1, inner.getHeight() / kRowH);
        first = juce::jlimit (0, juce::jmax (0, scale.count - maxRows), first);
        const int shownRows = juce::jmin (maxRows, scale.count - first);
        int found = 0;
        for (int r = 0; r < shownRows; ++r)
        {
            const int i = first + r;
            const auto& n = scale.notes[(size_t) i];
            auto row = inner.removeFromTop (kRowH);
            rows.push_back ({ row, i });
            const float e = (size_t) i < emphasis.size() ? emphasis[(size_t) i] : 0.0f;
            if (e > 0.01f) { g.setColour (theme::mix (0.22f * e)); g.fillRect (row); } // being played
            if (i == dropRow) { g.setColour (theme::modified); g.drawRect (row, 2); }
            const bool mod = n.moved || n.custom || ! n.enabled;
            const auto textCol = ! n.enabled ? theme::mix (0.35f) : mod ? theme::modified : theme::text();
            if (n.kind == xen::ScaleNote::Found) ++found;
            const juce::String name = n.kind == xen::ScaleNote::Unison ? "unison"
                                    : n.kind == xen::ScaleNote::Period ? "\"octave\""
                                    : n.custom ? "custom " + juce::String (-n.degree)
                                               : juce::String (n.degree) + (n.moved ? " moved" : "");
            // On/off square (found + custom notes only).
            if (n.kind == xen::ScaleNote::Found)
            {
                auto box = juce::Rectangle<float> ((float) row.getX() + 3.0f, (float) row.getCentreY() - 5.0f, 10.0f, 10.0f);
                g.setColour (n.enabled ? theme::mix (0.8f) : theme::mix (0.3f));
                if (n.enabled) g.fillRect (box.reduced (2.0f));
                g.drawRect (box, 1.0f);
            }
            g.setColour (textCol);
            g.setFont (theme::sans (12.5f, e > 0.3f));
            g.drawText (name, row.getX() + colX[1], row.getY(), 64, kRowH, juce::Justification::centredLeft);
            g.setFont (theme::mono (12.5f));
            const double ratio = xen::centsToRatio (n.cents);
            g.drawText (detail::fmt (n.cents, 2), row.getX() + colX[2], row.getY(), 62, kRowH, juce::Justification::centredLeft);
            g.drawText (detail::fmt (ratio, 5), row.getX() + colX[3], row.getY(), 62, kRowH, juce::Justification::centredLeft);
            g.drawText (detail::fmt (fundamental * ratio, 2), row.getX() + colX[4], row.getY(), 62, kRowH, juce::Justification::centredLeft);
            auto bar = juce::Rectangle<float> ((float) (row.getX() + colX[5]), (float) row.getY() + 3.0f,
                                               (float) (row.getRight() - row.getX() - colX[5]), (float) kRowH - 6.0f);
            g.setColour (theme::mix (0.1f));
            g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);
            if (! n.custom)
            {
                g.setColour (theme::mix (0.45f));
                g.fillRoundedRectangle (bar.withWidth (juce::jmax (bar.getHeight(), bar.getWidth() * (float) n.strength)), bar.getHeight() * 0.5f);
                g.setColour (theme::fg);
                g.drawText (detail::fmt (n.strength * 100.0, 1) + "%", bar.reduced (6.0f, 0.0f), juce::Justification::centredLeft);
            }
        }
        if (shownRows < scale.count)
        {
            g.setColour (theme::dim());
            g.setFont (theme::sans (11.0f));
            g.drawText (juce::String (first + 1) + "-" + juce::String (first + shownRows) + " of " + juce::String (scale.count) + "  (scroll)",
                        foot, juce::Justification::centredRight);
        }
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        first = juce::jlimit (0, juce::jmax (0, scale.count - maxRows), first + (w.deltaY < 0 ? 2 : -2));
        repaint();
    }

    int rowAt (juce::Point<int> p) const
    {
        for (auto& [r, i] : rows)
            if (r.contains (p) && (scale.notes[(size_t) i].kind == xen::ScaleNote::Found || scale.notes[(size_t) i].custom)) return i;
        return -1;
    }
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("mod:") && rowAt (d.localPosition) >= 0; }
    void itemDragEnter (const SourceDetails& d) override { dropRow = rowAt (d.localPosition); repaint(); }
    void itemDragMove (const SourceDetails& d) override { const int r = rowAt (d.localPosition); if (r != dropRow) { dropRow = r; repaint(); } }
    void itemDragExit (const SourceDetails&) override { dropRow = -1; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        const int i = rowAt (d.localPosition);
        dropRow = -1; repaint();
        if (i >= 0 && onModDrop) onModDrop (scale.notes[(size_t) i], modOfDrag (d));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (auto& [r, i] : rows)
            if (r.contains (e.getPosition()))
            {
                const auto& n = scale.notes[(size_t) i];
                if (e.mods.isMiddleButtonDown() || e.mods.isAltDown()) { if (onReset) onReset (n); }
                else if (n.kind == xen::ScaleNote::Found || n.custom) { if (onToggle) onToggle (n); }
                return;
            }
    }

private:
    static constexpr int kRowH = 19;
    xen::FinalScale scale;
    std::vector<float> emphasis;
    int first = 0, maxRows = 1, dropRow = -1;
    double fundamental = 261.63;
    juce::String footerText;
    std::vector<std::pair<juce::Rectangle<int>, int>> rows;
};
// ================================================================================================
/// The calculator's partials as a table (ArtHar's PARTIALS table), with the SCALE tick column
/// (ArtHar v1.15): ticked = the partial shapes the scale; unticked = "sound only" (played, but the
/// scale ignores it). The fundamental is always in. Mouse wheel scrolls.
class PartialsTable : public juce::Component, public juce::DragAndDropTarget
{
public:
    std::function<void (const xen::ModelPartial&, int mod)> onModDrop; // chip dropped on a row's Scale tick (v0.7)
    std::function<void (const xen::ModelPartial&)> onToggleScale;

    void setPartials (const std::vector<xen::ModelPartial>& p, double fundamentalHz)
    {
        parts = p; fundamental = fundamentalHz;
        first = juce::jlimit (0, juce::jmax (0, (int) parts.size() - visibleRows()), first);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto inner = theme::drawSection (g, getLocalBounds(), "Partials");
        g.setColour (theme::dim());
        g.setFont (theme::sans (11.5f));
        g.drawText ("Scale = shapes the scale", getLocalBounds().reduced (10, 6).removeFromTop (18), juce::Justification::centredRight);
        const int colX[] = { 0, 30, 96, 170, 240, 300 };
        const char* heads[] = { "#", "Hz", "Cents", "Ratio", "Loud", "Scale" };
        g.setColour (theme::text());
        g.setFont (theme::sans (12.5f));
        auto head = inner.removeFromTop (18);
        for (int i = 0; i < 6; ++i) g.drawText (heads[i], head.getX() + colX[i], head.getY(), 70, 18, juce::Justification::centredLeft);
        rows.clear();
        const int n = visibleRows();
        for (int r = 0; r < n && first + r < (int) parts.size(); ++r)
        {
            const auto& p = parts[(size_t) (first + r)];
            auto row = inner.removeFromTop (kRowH);
            rows.push_back ({ row, first + r });
            if (first + r == dropRow) { g.setColour (theme::modified); g.drawRect (row, 2); }
            const bool mod = p.custom || p.moved || p.levelEdited;
            g.setColour (p.amp <= 0.0 ? theme::mix (0.35f) : mod ? theme::modified : theme::text());
            g.setFont (theme::sans (12.5f));
            g.drawText (p.custom ? "c" + juce::String (p.index - xen::kSeriesPartials + 1) : juce::String (p.index + 1),
                        row.getX() + colX[0], row.getY(), 30, kRowH, juce::Justification::centredLeft);
            g.setFont (theme::mono (12.0f));
            const double ratio = xen::centsToRatio (p.cents);
            g.drawText (juce::String (fundamental * ratio, 1), row.getX() + colX[1], row.getY(), 70, kRowH, juce::Justification::centredLeft);
            g.drawText (juce::String (p.cents, 1), row.getX() + colX[2], row.getY(), 70, kRowH, juce::Justification::centredLeft);
            g.drawText (juce::String (ratio, 3), row.getX() + colX[3], row.getY(), 60, kRowH, juce::Justification::centredLeft);
            g.drawText (juce::String (std::min (1.0, p.amp), 3), row.getX() + colX[4], row.getY(), 60, kRowH, juce::Justification::centredLeft);
            auto box = juce::Rectangle<float> ((float) (row.getX() + colX[5]) + 12.0f, (float) row.getCentreY() - 5.0f, 10.0f, 10.0f);
            const bool fixed = ! p.custom && p.index == 0;
            g.setColour (fixed ? theme::mix (0.35f) : p.inScale != (p.custom ? false : true) ? theme::modified : theme::mix (0.8f));
            if (p.inScale) g.fillRect (box.reduced (2.0f));
            g.drawRect (box, 1.0f);
        }
        if ((int) parts.size() > n)
        {
            g.setColour (theme::dim());
            g.setFont (theme::sans (11.0f));
            g.drawText (juce::String (first + 1) + "-" + juce::String (juce::jmin ((int) parts.size(), first + n)) + " of " + juce::String ((int) parts.size()) + "  (scroll)",
                        getLocalBounds().reduced (10, 4).removeFromBottom (14), juce::Justification::centredRight);
        }
    }

    int tickRowAt (juce::Point<int> p) const
    {
        for (auto& [r, i] : rows)
            if (r.contains (p) && ! (! parts[(size_t) i].custom && parts[(size_t) i].index == 0)) return i; // the fundamental is always in
        return -1;
    }
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("mod:") && tickRowAt (d.localPosition) >= 0; }
    void itemDragEnter (const SourceDetails& d) override { dropRow = tickRowAt (d.localPosition); repaint(); }
    void itemDragMove (const SourceDetails& d) override { const int r = tickRowAt (d.localPosition); if (r != dropRow) { dropRow = r; repaint(); } }
    void itemDragExit (const SourceDetails&) override { dropRow = -1; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        const int i = tickRowAt (d.localPosition);
        dropRow = -1; repaint();
        if (i >= 0 && onModDrop) onModDrop (parts[(size_t) i], modOfDrag (d));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (auto& [r, i] : rows)
            if (r.contains (e.getPosition()) && e.position.x > (float) (r.getX() + 290))
            {
                const auto& p = parts[(size_t) i];
                if (! p.custom && p.index == 0) return; // the fundamental is always in
                if (onToggleScale) onToggleScale (p);
                return;
            }
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        first = juce::jlimit (0, juce::jmax (0, (int) parts.size() - visibleRows()), first + (w.deltaY < 0 ? 2 : -2));
        repaint();
    }

private:
    static constexpr int kRowH = 17;
    int visibleRows() const { return juce::jmax (1, (getHeight() - 6 - 18 - 18 - 16) / kRowH); }
    std::vector<xen::ModelPartial> parts;
    double fundamental = 261.63;
    int first = 0, dropRow = -1;
    std::vector<std::pair<juce::Rectangle<int>, int>> rows;
};

} // namespace pvst
