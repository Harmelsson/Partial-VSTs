#pragma once
// The Calculator's views, meant to sit on top of every synth in this repo (user's vision,
// 2026-09-26): the dissonance curve, the partials, and the scale table. ArtHar look.
//
//   CurvePlot    roughness (% of the curve's own peak) vs interval above the unison (cents).
//                Scattered square columns; hollow squares = scale notes found by the calculator.
//   PartialsPlot loudness vs distance from the fundamental (cents); solid square columns;
//                dashed lines at the seed's "octaves"; partials outside hearing are dimmed.
//   ScaleTable   note / cents / ratio / Hz / strength, like ArtHar's SCALE table.
// All numbers shown are exact; only the drawing is eased/animated.
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

        // Horizontal grid + y labels.
        for (double y = y0; y <= y1 + 1e-9; y += yStep)
        {
            const float py = data.getBottom() - (float) ((y - y0) / (y1 - y0)) * data.getHeight();
            g.setColour (theme::grid());
            g.drawHorizontalLine ((int) py, data.getX(), data.getRight());
            g.setColour (theme::dim());
            g.drawText (fmt (y, yDecimals), juce::Rectangle<float> (data.getX() - 40.0f, py - 7.0f, 34.0f, 14.0f), juce::Justification::centredRight);
        }
        // Vertical grid + x labels.
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
        // Rotated y title.
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, r.getX() + 8.0f, data.getCentreY()));
        g.drawText (yTitle, juce::Rectangle<float> (r.getX() + 8.0f - 60.0f, data.getCentreY() - 7.0f, 120.0f, 14.0f), juce::Justification::centred);
        g.restoreState();
        return data;
    }
} // namespace detail

// ================================================================================================
class CurvePlot : public juce::Component
{
public:
    void setResult (const xen::CalculatorOutput& r)
    {
        result = r;
        const double span = std::abs (r.curve.periodCents) + xen::DissonanceCalculator::kOvershootCents;
        frameMax = std::max (600.0, std::ceil (span / 600.0) * 600.0); // steady frame, 600-cent steps
    }

    void setBusy (bool b) { busy = b; }

    void tick (float dt)
    {
        lastDt = dt;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto inner = theme::drawSection (g, getLocalBounds(), "Dissonance curve").toFloat();
        g.setColour (theme::dim());
        g.setFont (theme::sans (11.5f));
        g.drawText (busy ? juce::String ("calculating...") : theme::u8 ("hollow square = scale note  \xc2\xb7  lower = smoother"),
                    getLocalBounds().reduced (10, 6).removeFromTop (18), juce::Justification::centredRight);

        auto data = detail::axes (g, inner, 0.0, frameMax, 0.0, 100.0, 20.0, 0, "interval above the unison (cents)", "roughness %");
        lattice.setArea (data);
        const int cols = lattice.numCols();
        shown.resize ((size_t) cols, -1.0f);

        const auto& c = result.curve;
        const bool have = c.valid && c.cents.size() > 2;
        double peak = 0.0;
        if (have) peak = *std::max_element (c.dissonance.begin(), c.dissonance.end());

        // Target per column: the LOWEST curve point it covers (so narrow dips never vanish).
        std::vector<float> target ((size_t) cols, -1.0f);
        if (have && peak > 0.0)
        {
            const double centsPerCol = frameMax / (double) cols;
            for (size_t i = 0; i < c.cents.size(); ++i)
            {
                const int col = (int) (c.cents[i] / centsPerCol);
                if (col < 0 || col >= cols) continue;
                const float v = (float) (c.dissonance[i] / peak);
                auto& t = target[(size_t) col];
                t = t < 0.0f ? v : juce::jmin (t, v);
            }
        }

        // Near-note weights for the scatter.
        std::vector<float> near ((size_t) cols, 0.0f);
        auto colOfCents = [&] (double cents) { return (int) (cents / frameMax * cols); };
        if (have)
            for (auto& n : c.notes)
                for (int d = -3; d <= 3; ++d)
                {
                    const int col = colOfCents (n.cents) + d;
                    if (col >= 0 && col < cols) near[(size_t) col] = juce::jmax (near[(size_t) col], 1.0f - (float) std::abs (d) / 4.0f);
                }

        lattice.beginFrame();
        for (int col = 0; col < cols; ++col)
        {
            auto& s = shown[(size_t) col];
            const float t = target[(size_t) col];
            if (t < 0.0f) { s = -1.0f; continue; }
            s = s < 0.0f ? t : theme::ease (s, t, 0.3f);
            lattice.column (col, s, 1.0f, true, near[(size_t) col]);
        }
        lattice.paint (g, lastDt);

        if (! have)
        {
            g.setColour (theme::dim());
            g.setFont (theme::sans (14.0f));
            g.drawText (std::abs (xen::periodCents (result.settings.seed)) < 10.0
                            ? "seed ~ 1: every partial is on one pitch, there is no \"octave\" to search"
                            : "calculating...",
                        data.toNearestInt(), juce::Justification::centred);
            return;
        }

        // Scale notes: dotted line + hollow square on the curve.
        for (auto& n : c.notes)
        {
            const float x = data.getX() + (float) (n.cents / frameMax) * data.getWidth();
            plot::verticalDotted (g, x, data.getY(), data.getBottom(), theme::mix (n.kind == xen::ScaleNote::Found ? 0.3f : 0.45f));
            const int col = lattice.colAt (x);
            const float h = col >= 0 && col < cols ? juce::jmax (0.0f, shown[(size_t) col]) : 0.0f;
            const int row = (int) std::round (h * (float) (lattice.numRows() - 1));
            SquareLattice::hollowSquare (g, { lattice.colCentreX (col), lattice.rowCentreY (row) }, 10.0f,
                                         theme::mix (0.55f + 0.45f * (float) n.strength * 2.0f));
        }
    }

private:
    xen::CalculatorOutput result;
    SquareLattice lattice;
    std::vector<float> shown;
    double frameMax = 1800.0;
    float lastDt = 1.0f / 60.0f;
    bool busy = false;
};

// ================================================================================================
class PartialsPlot : public juce::Component
{
public:
    void setSettings (const xen::CalculatorSettings& s) { settings = s; }

    void tick (float dt)
    {
        lastDt = dt;
        const double S = xen::stretchFactor (std::max (1e-6, settings.seed));
        double lo = 0.0, hi = 0.0;
        for (int p = 0; p < kMax; ++p)
        {
            const double c = 1200.0 * S * std::log2 ((double) p + 1);
            const double h = p < settings.partials ? xen::rolloffShape (p + 1, settings.rolloff) : 0.0;
            cents[(size_t) p] = first ? c : theme::ease ((float) cents[(size_t) p], (float) c, 0.3f);
            height[(size_t) p] = first ? h : theme::ease ((float) height[(size_t) p], (float) h, 0.3f);
            if (p < settings.partials) { lo = std::min (lo, c); hi = std::max (hi, c); }
        }
        // Steady frame in 1200-cent steps, so the grid stays still while the partials move.
        frameMin = std::floor (lo / 1200.0) * 1200.0;
        frameMax = std::max (frameMin + 1200.0, std::ceil ((hi + 60.0) / 1200.0) * 1200.0);
        first = false;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto inner = theme::drawSection (g, getLocalBounds(), "Partials").toFloat();
        g.setColour (theme::dim());
        g.setFont (theme::sans (11.5f));
        g.drawText (theme::u8 ("dashed = the seed's \"octaves\"  \xc2\xb7  dimmed = outside hearing"),
                    getLocalBounds().reduced (10, 6).removeFromTop (18), juce::Justification::centredRight);
        auto data = detail::axes (g, inner, frameMin, frameMax, 0.0, 1.0, 0.5, 1, "distance from the fundamental (cents)", "loudness");
        lattice.setArea (data);
        auto xOf = [&] (double c) { return data.getX() + (float) ((c - frameMin) / (frameMax - frameMin)) * data.getWidth(); };

        const double period = std::abs (xen::periodCents (std::max (1e-6, settings.seed)));
        if (period > 30.0)
            for (double c = std::ceil (frameMin / period) * period; c <= frameMax; c += period)
                if (std::abs (c) > 1e-6) plot::verticalDotted (g, xOf (c), data.getY(), data.getBottom(), theme::mix (0.3f));

        lattice.beginFrame();
        for (int p = 0; p < kMax; ++p)
        {
            if (height[(size_t) p] < 0.004) continue;
            const double hz = settings.fundamentalHz * std::exp2 (cents[(size_t) p] / 1200.0);
            const bool audible = hz >= 20.0 && hz <= 20000.0;
            lattice.column (lattice.colAt (xOf (cents[(size_t) p])), (float) height[(size_t) p], audible ? 1.0f : 0.35f);
        }
        lattice.paint (g, lastDt);
    }

private:
    static constexpr int kMax = 64;
    xen::CalculatorSettings settings;
    SquareLattice lattice;
    std::array<double, kMax> cents {}, height {};
    double frameMin = 0.0, frameMax = 7200.0;
    float lastDt = 1.0f / 60.0f;
    bool first = true;
};

// ================================================================================================
class ScaleTable : public juce::Component
{
public:
    void setResult (const xen::CalculatorOutput& r) { result = r; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto inner = theme::drawSection (g, getLocalBounds(), "Scale");
        const auto& notes = result.curve.notes;
        const int colX[] = { 0, 64, 128, 192, 252 };
        const char* heads[] = { "Note", "Cents", "Ratio", "Hz", "Strength" };
        g.setFont (theme::sans (12.5f));
        g.setColour (theme::text());
        auto head = inner.removeFromTop (20);
        for (int i = 0; i < 5; ++i)
            g.drawText (heads[i], head.getX() + colX[i], head.getY(), 70, 20, juce::Justification::centredLeft);

        const int rowH = 19;
        const int maxRows = juce::jmax (1, inner.getHeight() / rowH);
        const int shownRows = (int) notes.size() > maxRows ? maxRows - 1 : (int) notes.size();
        for (int i = 0; i < shownRows; ++i)
        {
            const auto& n = notes[(size_t) i];
            auto row = inner.removeFromTop (rowH);
            const double ratio = xen::centsToRatio (n.cents);
            const juce::String name = n.kind == xen::ScaleNote::Unison ? "0 unison"
                                    : n.kind == xen::ScaleNote::Period ? juce::String (i) + " \"octave\""
                                                                        : juce::String (i);
            g.setFont (theme::sans (12.5f));
            g.setColour (theme::text());
            g.drawText (name, row.getX() + colX[0], row.getY(), 70, rowH, juce::Justification::centredLeft);
            g.setFont (theme::mono (12.5f));
            g.drawText (detail::fmt (n.cents, 2), row.getX() + colX[1], row.getY(), 70, rowH, juce::Justification::centredLeft);
            g.drawText (detail::fmt (ratio, 5), row.getX() + colX[2], row.getY(), 70, rowH, juce::Justification::centredLeft);
            g.drawText (detail::fmt (result.settings.fundamentalHz * ratio, 2), row.getX() + colX[3], row.getY(), 76, rowH, juce::Justification::centredLeft);

            auto bar = juce::Rectangle<float> ((float) (row.getX() + colX[4]), (float) row.getY() + 3.0f,
                                               (float) (row.getRight() - row.getX() - colX[4]), (float) rowH - 6.0f);
            g.setColour (theme::mix (0.1f));
            g.fillRoundedRectangle (bar, bar.getHeight() * 0.5f);
            g.setColour (theme::mix (0.45f));
            g.fillRoundedRectangle (bar.withWidth (juce::jmax (bar.getHeight(), bar.getWidth() * (float) n.strength)), bar.getHeight() * 0.5f);
            g.setColour (theme::fg);
            g.drawText (detail::fmt (n.strength * 100.0, 1) + "%", bar.reduced (6.0f, 0.0f), juce::Justification::centredLeft);
        }
        if (shownRows < (int) notes.size())
        {
            g.setColour (theme::dim());
            g.setFont (theme::sans (12.0f));
            g.drawText ("+ " + juce::String ((int) notes.size() - shownRows) + " more (raise min dip strength)",
                        inner.removeFromTop (rowH), juce::Justification::centredLeft);
        }
    }

private:
    xen::CalculatorOutput result;
};
} // namespace pvst
