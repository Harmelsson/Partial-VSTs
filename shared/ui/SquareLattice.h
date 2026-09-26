#pragma once
// SquareLattice — ArtHar's square-dot plot style (from gui/squares.rs), for JUCE.
//
// A plot is an invisible grid of small squares, one every kPitch pixels. A value is drawn as a
// COLUMN of squares standing on the zero line: the top square (the "line") is solid foreground,
// the squares under it fade towards zero. Two fill styles:
//   solid   : every square under the line, fading (partials, envelopes)
//   scatter : only some squares light up (fixed pseudo-random pattern), dense just under the line
//             and dense all the way down near scale notes (the dissonance curve)
// Motion: every square keeps an AFTERGLOW that fades with a 0.3 s half-life, so moving data
// leaves a ghost trail, like an oscilloscope's phosphor. (Easing of the values themselves is the
// caller's job: ease the numbers, then feed columns here every frame.)
#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/Theme.h"

namespace pvst
{
class SquareLattice
{
public:
    static constexpr float kPitch = 6.0f;      // distance between squares (px)
    static constexpr float kSide = 4.0f;       // square size (px)
    static constexpr float kHalfLife = 0.3f;   // afterglow half-life (s)
    static constexpr float kTrace = 0.5f;      // afterglow starts at this fraction of the square
    static constexpr float kFillMax = 0.6f;    // brightness just under the line
    static constexpr float kInvisible = 0.015f;

    /// Match the lattice to a plot area; resets the afterglow if the size changed.
    void setArea (juce::Rectangle<float> plotArea)
    {
        area = plotArea;
        const int c = juce::jmax (1, (int) std::floor (area.getWidth() / kPitch));
        const int r = juce::jmax (1, (int) std::floor (area.getHeight() / kPitch));
        if (c != cols || r != rows)
        {
            cols = c; rows = r;
            now.assign ((size_t) (cols * rows), 0.0f);
            glow.assign ((size_t) (cols * rows), 0.0f);
        }
    }

    int numCols() const { return cols; }
    int numRows() const { return rows; }
    juce::Rectangle<float> getArea() const { return area; }

    /// Screen x of a column's centre, and the column at a screen x.
    float colCentreX (int c) const { return area.getX() + ((float) c + 0.5f) * kPitch; }
    int colAt (float x) const { return (int) std::floor ((x - area.getX()) / kPitch); }
    float rowCentreY (int r) const { return area.getBottom() - ((float) r + 0.5f) * kPitch; }

    void beginFrame() { std::fill (now.begin(), now.end(), 0.0f); }

    /// One column. `height` is 0..1 of the plot height; dim scales the whole column;
    /// nearNote (0..1) makes the scatter dense all the way down.
    void column (int c, float height, float dimAmount = 1.0f, bool scatter = false, float nearNote = 0.0f)
    {
        if (c < 0 || c >= cols) return;
        const int top = juce::jlimit (0, rows - 1, (int) std::round (height * (float) (rows - 1)));
        const float tall = 0.75f + 0.25f * (float) top / (float) juce::jmax (1, rows - 1); // peaks glow a bit more
        set (c, top, dimAmount);
        for (int r = 0; r < top; ++r)
        {
            const float depth = (float) r / (float) juce::jmax (1, top); // 1 just under the line, 0 at zero
            float b;
            if (scatter)
            {
                const float density = juce::jmax (depth * depth * depth, nearNote * 0.75f);
                b = hash (c, r) < density ? (0.18f + 0.45f * depth) : 0.0f;
            }
            else
                b = kFillMax * std::pow (depth, 1.6f);
            set (c, r, b * tall * dimAmount);
        }
    }

    /// Draws everything and advances the afterglow by dt seconds.
    void paint (juce::Graphics& g, float dt)
    {
        const float decay = std::pow (0.5f, dt / kHalfLife);
        for (int c = 0; c < cols; ++c)
            for (int r = 0; r < rows; ++r)
            {
                const size_t i = (size_t) (r * cols + c);
                glow[i] = juce::jmax (glow[i] * decay, now[i] * kTrace);
                const float b = juce::jmax (now[i], glow[i]);
                if (b < kInvisible) continue;
                g.setColour (theme::mix (b));
                g.fillRect (area.getX() + (float) c * kPitch + (kPitch - kSide) * 0.5f,
                            area.getBottom() - (float) (r + 1) * kPitch + (kPitch - kSide) * 0.5f, kSide, kSide);
            }
    }

    /// True while ghosts are still fading (keep repainting).
    bool isFading() const
    {
        for (float v : glow) if (v > kInvisible) return true;
        return false;
    }

    /// A hollow marker square (ArtHar: scale notes, hand-edited points).
    static void hollowSquare (juce::Graphics& g, juce::Point<float> centre, float size, juce::Colour col)
    {
        g.setColour (theme::bg);
        g.fillRect (juce::Rectangle<float> (size, size).withCentre (centre));
        g.setColour (col);
        g.drawRect (juce::Rectangle<float> (size, size).withCentre (centre), 1.2f);
        g.fillRect (juce::Rectangle<float> (2.0f, 2.0f).withCentre (centre));
    }

private:
    void set (int c, int r, float b)
    {
        auto& v = now[(size_t) (r * cols + c)];
        v = juce::jmax (v, b);
    }

    /// Fixed pseudo-random number per square (stable pattern, not noise).
    static float hash (int c, int r)
    {
        uint32_t h = (uint32_t) c * 374761393u + (uint32_t) r * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return (float) ((h ^ (h >> 16)) & 0xffffu) / 65535.0f;
    }

    juce::Rectangle<float> area;
    int cols = 0, rows = 0;
    std::vector<float> now, glow;
};

/// Plot frame helpers shared by the calculator views: grid, ticks, axis labels.
namespace plot
{
    /// Nice tick step so labels are at least `minPx` apart.
    inline double tickStep (double range, float px, float minPx)
    {
        const double steps[] = { 1, 2, 5, 10, 20, 25, 50, 100, 200, 300, 600, 1200, 2400, 4800 };
        for (double s : steps)
            if (range > 0 && (float) (s / range) * px >= minPx) return s;
        return 4800;
    }

    inline void verticalDotted (juce::Graphics& g, float x, float y0, float y1, juce::Colour c)
    {
        g.setColour (c);
        for (float y = y0; y < y1; y += 5.0f) g.fillRect (x, y, 1.0f, 2.0f);
    }
} // namespace plot
} // namespace pvst
