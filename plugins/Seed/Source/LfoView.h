#pragma once
// LFO shape editor (v0.7), in the spirit of PhasePlant / Serum: the curve is DRAWN.
//
//   drag a square          = move that point (the two end points only move up/down)
//   Shift + drag           = snap to the grid (X divisions of the cycle, Y divisions of -1..+1)
//   double-click           = add a point
//   right-click a point    = delete it
//   drag a small dot       = bend the segment (the dot sits on the curve, halfway along it)
//   middle-click / Alt a dot = back to a straight line
//   Presets                = Sine, Triangle, Saw, Square, Pulse, Ramp, Steps, Bounce, Invert, Reverse
// The built-in shapes (Sine, Triangle, Saw, Square) are shown too; touching them turns them into a
// drawn curve. Random shapes (S&H, Smooth rnd, Drift) can't be drawn.
// A moving line + dot show where the modulator is right now.
#include "PluginProcessor.h"
#include "ui/Controls.h"

class LfoEditor : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LfoEditor (SeedProcessor& p) : proc (p)
    {
        setTooltip ("Draw the LFO: drag the squares, double-click to add a point, right-click a point to delete it, drag the small dots to bend a segment. Hold Shift to snap to the grid.");
    }

    void setModulator (int m) { mod = juce::jlimit (1, seed::kNumMods, m); repaint(); }
    void setGrid (int x, int y) { gridX = juce::jlimit (1, 64, x); gridY = juce::jlimit (1, 64, y); repaint(); }
    int gridXValue() const { return gridX; }
    int gridYValue() const { return gridY; }
    void tick() { repaint(); }

    /// The curve to show for a modulator's current shape (random shapes: none).
    static bool shapeToShow (SeedProcessor& proc, int mod, pvst::LfoShape& out)
    {
        auto& sp = proc.param (seed::ids::modShape (mod));
        const int shape = (int) std::lround (sp.convertFrom0to1 (sp.getValue()));
        if (shape == seed::kShapeDrawn) { out = proc.lfoShape (mod); return true; }
        static const int presetOf[] = { 0, 1, 2, 4 }; // Sine, Triangle, Saw, Square
        if (shape >= 0 && shape < 4) { out.preset (presetOf[shape]); return true; }
        return false;
    }

    void paint (juce::Graphics& g) override
    {
        namespace th = pvst::theme;
        const auto area = plotArea();
        g.setColour (th::boxFill()); g.fillRect (getLocalBounds());
        g.setColour (th::frame()); g.drawRect (getLocalBounds(), 1);
        // Grid.
        for (int i = 1; i < gridX; ++i) { g.setColour (th::grid()); g.drawVerticalLine ((int) (area.getX() + area.getWidth() * (float) i / (float) gridX), area.getY(), area.getBottom()); }
        for (int j = 0; j <= gridY; ++j)
        {
            const float y = area.getY() + area.getHeight() * (float) j / (float) gridY;
            g.setColour (j == gridY / 2 && gridY % 2 == 0 ? th::mix (0.22f) : th::grid());
            g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        }
        pvst::LfoShape shape;
        if (! shapeToShow (proc, mod, shape))
        {
            g.setColour (th::dim()); g.setFont (th::sans (13.0f));
            g.drawFittedText ("random shape: nothing to draw.\nPick a preset (or the Drawn shape) to draw your own curve.", getLocalBounds().reduced (12), juce::Justification::centred, 3);
            return;
        }
        juce::Path line, fill;
        const int steps = (int) area.getWidth();
        for (int i = 0; i <= steps; ++i)
        {
            const float p = (float) i / (float) steps, x = area.getX() + p * area.getWidth(), y = yOf (shape.eval (p));
            if (i == 0) { line.startNewSubPath (x, y); fill.startNewSubPath (x, yOf (0.0f)); fill.lineTo (x, y); }
            else { line.lineTo (x, y); fill.lineTo (x, y); }
        }
        fill.lineTo (area.getRight(), yOf (0.0f)); fill.closeSubPath();
        g.setColour (th::mix (0.12f)); g.fillPath (fill);
        g.setColour (th::fg); g.strokePath (line, juce::PathStrokeType (1.6f));
        // Handles.
        for (int i = 0; i < shape.n; ++i)
        {
            const auto c = juce::Point<float> (xOf (shape.x[i]), yOf (shape.y[i]));
            g.setColour (th::bg); g.fillRect (juce::Rectangle<float> (9.0f, 9.0f).withCentre (c));
            g.setColour (i == hoverPoint || i == dragPoint ? th::modified : th::fg);
            g.drawRect (juce::Rectangle<float> (9.0f, 9.0f).withCentre (c), 1.5f);
        }
        for (int i = 0; i + 1 < shape.n; ++i)
        {
            const auto c = midHandle (shape, i);
            g.setColour (i == hoverMid || i == dragMid ? th::modified : th::mix (0.6f));
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (c));
        }
        // Live position.
        const float ph = proc.uiModPhase[(size_t) mod - 1].load(), val = proc.uiModValue[(size_t) mod - 1].load();
        g.setColour (th::modified.withAlpha (0.8f));
        g.drawVerticalLine ((int) xOf (ph), area.getY(), area.getBottom());
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ xOf (ph), yOf (val) }));
        g.setColour (th::dim()); g.setFont (th::sans (10.5f));
        g.drawText ("Shift = snap", getLocalBounds().reduced (5, 2).removeFromBottom (12), juce::Justification::centredRight);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        pvst::LfoShape s;
        int hp = -1, hm = -1;
        if (shapeToShow (proc, mod, s)) { hp = pointAt (s, e.position); if (hp < 0) hm = midAt (s, e.position); }
        if (hp != hoverPoint || hm != hoverMid) { hoverPoint = hp; hoverMid = hm; repaint(); }
    }
    void mouseExit (const juce::MouseEvent&) override { hoverPoint = hoverMid = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        pvst::LfoShape s;
        if (! shapeToShow (proc, mod, s)) return;
        const int p = pointAt (s, e.position), m = p < 0 ? midAt (s, e.position) : -1;
        if (p >= 0 && e.mods.isPopupMenu())
        {
            if (p > 0 && p < s.n - 1) { removePoint (s, p); commit (s); }
            return;
        }
        if (m >= 0 && (e.mods.isMiddleButtonDown() || e.mods.isAltDown())) { s.c[m] = 0.0f; commit (s); return; }
        dragPoint = p; dragMid = m;
        if (p >= 0 || m >= 0) { dragStart = s; dragStartY = e.position.y; }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragPoint < 0 && dragMid < 0) return;
        auto s = dragStart;
        const bool snap = e.mods.isShiftDown();
        if (dragPoint >= 0)
        {
            float px = pxToX (e.position.x), py = pyToY (e.position.y);
            if (snap) { px = std::round (px * (float) gridX) / (float) gridX; py = std::round ((py + 1.0f) * 0.5f * (float) gridY) / (float) gridY * 2.0f - 1.0f; }
            s.y[dragPoint] = std::clamp (py, -1.0f, 1.0f);
            if (dragPoint > 0 && dragPoint < s.n - 1) s.x[dragPoint] = std::clamp (px, s.x[dragPoint - 1] + 1e-3f, s.x[dragPoint + 1] - 1e-3f);
        }
        else
        {
            // Put the handle where the mouse is: solve the bend that makes the curve pass through it halfway along.
            const int i = dragMid;
            const float y0 = s.y[i], y1 = s.y[i + 1], target = pyToY (e.position.y);
            if (std::abs (y1 - y0) > 0.02f)
            {
                const float t = std::clamp ((target - y0) / (y1 - y0), 0.02f, 0.98f);
                const float k = std::log (t) / std::log (0.5f);
                s.c[i] = std::clamp (std::log (k) / std::log (4.0f), -1.0f, 1.0f);
            }
            else s.c[i] = std::clamp (dragStart.c[i] + (dragStartY - e.position.y) * 0.01f, -1.0f, 1.0f);
        }
        commit (s);
    }

    void mouseUp (const juce::MouseEvent&) override { dragPoint = dragMid = -1; }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        pvst::LfoShape s;
        if (! shapeToShow (proc, mod, s) || pointAt (s, e.position) >= 0 || s.n >= pvst::LfoShape::kMaxPoints) return;
        float px = pxToX (e.position.x), py = pyToY (e.position.y);
        if (e.mods.isShiftDown()) { px = std::round (px * (float) gridX) / (float) gridX; py = std::round ((py + 1.0f) * 0.5f * (float) gridY) / (float) gridY * 2.0f - 1.0f; }
        px = std::clamp (px, 0.002f, 0.998f);
        int at = 1;
        while (at < s.n - 1 && s.x[at] < px) ++at;
        for (int k = s.n; k > at; --k) { s.x[k] = s.x[k - 1]; s.y[k] = s.y[k - 1]; s.c[k] = s.c[k - 1]; }
        s.x[at] = px; s.y[at] = std::clamp (py, -1.0f, 1.0f); s.c[at] = 0.0f;
        ++s.n;
        commit (s);
    }

    /// Menu actions (called by the panel's buttons).
    void invert() { pvst::LfoShape s; if (shapeToShow (proc, mod, s)) { for (int i = 0; i < s.n; ++i) { s.y[i] = -s.y[i]; s.c[i] = -s.c[i]; } commit (s); } }
    void reverse()
    {
        pvst::LfoShape s, r;
        if (! shapeToShow (proc, mod, s)) return;
        r.n = s.n;
        for (int i = 0; i < s.n; ++i) { r.x[i] = 1.0f - s.x[s.n - 1 - i]; r.y[i] = s.y[s.n - 1 - i]; }
        for (int i = 0; i + 1 < s.n; ++i) r.c[i] = -s.c[s.n - 2 - i];
        commit (r);
    }

private:
    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (8.0f, 8.0f); }
    float xOf (float p) const { auto a = plotArea(); return a.getX() + p * a.getWidth(); }
    float yOf (float v) const { auto a = plotArea(); return a.getCentreY() - v * a.getHeight() * 0.5f; }
    float pxToX (float px) const { auto a = plotArea(); return std::clamp ((px - a.getX()) / a.getWidth(), 0.0f, 1.0f); }
    float pyToY (float py) const { auto a = plotArea(); return std::clamp ((a.getCentreY() - py) / (a.getHeight() * 0.5f), -1.0f, 1.0f); }
    juce::Point<float> midHandle (const pvst::LfoShape& s, int i) const
    {
        const float xm = 0.5f * (s.x[i] + s.x[i + 1]);
        return { xOf (xm), yOf (s.eval (xm)) };
    }
    int pointAt (const pvst::LfoShape& s, juce::Point<float> p) const
    {
        int best = -1; float bestD = 9.0f;
        for (int i = 0; i < s.n; ++i)
            if (const float d = juce::Point<float> (xOf (s.x[i]), yOf (s.y[i])).getDistanceFrom (p); d < bestD) { bestD = d; best = i; }
        return best;
    }
    int midAt (const pvst::LfoShape& s, juce::Point<float> p) const
    {
        int best = -1; float bestD = 8.0f;
        for (int i = 0; i + 1 < s.n; ++i)
            if (const float d = midHandle (s, i).getDistanceFrom (p); d < bestD) { bestD = d; best = i; }
        return best;
    }
    static void removePoint (pvst::LfoShape& s, int p)
    {
        for (int k = p; k + 1 < s.n; ++k) { s.x[k] = s.x[k + 1]; s.y[k] = s.y[k + 1]; s.c[k] = s.c[k + 1]; }
        --s.n;
    }
    void commit (const pvst::LfoShape& s) { proc.setLfoShape (mod, s); repaint(); }

    SeedProcessor& proc;
    int mod = 1, gridX = 8, gridY = 4;
    int hoverPoint = -1, hoverMid = -1, dragPoint = -1, dragMid = -1;
    pvst::LfoShape dragStart;
    float dragStartY = 0.0f;
};

/// A tiny picture of a modulator's shape for the list.
class ShapeThumb : public juce::Component
{
public:
    ShapeThumb (SeedProcessor& p, int modNumber) : proc (p), mod (modNumber) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics& g) override
    {
        namespace th = pvst::theme;
        g.setColour (th::boxFill()); g.fillRect (getLocalBounds());
        g.setColour (th::frame()); g.drawRect (getLocalBounds(), 1);
        pvst::LfoShape s;
        auto a = getLocalBounds().toFloat().reduced (4.0f, 4.0f);
        if (! LfoEditor::shapeToShow (proc, mod, s))
        {
            g.setColour (th::dim()); g.setFont (th::sans (11.0f));
            g.drawText ("random", getLocalBounds(), juce::Justification::centred);
            return;
        }
        juce::Path path;
        for (int i = 0; i <= 40; ++i)
        {
            const float p = (float) i / 40.0f, x = a.getX() + p * a.getWidth(), y = a.getCentreY() - s.eval (p) * a.getHeight() * 0.5f;
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        g.setColour (th::mix (0.85f)); g.strokePath (path, juce::PathStrokeType (1.2f));
        const float ph = proc.uiModPhase[(size_t) mod - 1].load();
        g.setColour (th::modified.withAlpha (0.7f));
        g.drawVerticalLine ((int) (a.getX() + ph * a.getWidth()), a.getY(), a.getBottom());
    }
    void tick() { repaint(); }
private:
    SeedProcessor& proc;
    int mod;
};
