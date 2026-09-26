#pragma once
// EnvelopeView — an ADSR drawn in ArtHar's square-dot style, editable by dragging its corners.
//
//   Attack  : linear rise to full level
//   Decay   : exponential fall towards the sustain level (99.9% of the way after "decay" ms)
//   Sustain : the level held while the key is down
//   Release : exponential fall to silence (-60 dB after "release" ms)
// These are the SAME formulas the synth engines use, so the picture is the truth.
//
// Time axis: each segment's width grows with the square root of its time, so both 2 ms and
// 5 s stay visible and draggable. The sustain segment has a fixed width (it lasts as long as
// the key is held).
// Mouse: drag the hollow corners. Attack corner = left/right; decay corner = left/right (decay)
// and up/down (sustain); release corner = left/right. Shift = finer. Middle-click = default.
#include <juce_audio_processors/juce_audio_processors.h>
#include "ui/SquareLattice.h"

namespace pvst
{
class EnvelopeView : public juce::Component
{
public:
    EnvelopeView (juce::RangedAudioParameter& attack, juce::RangedAudioParameter& decay,
                  juce::RangedAudioParameter& sustainPercent, juce::RangedAudioParameter& release)
        : a (attack), d (decay), s (sustainPercent), r (release) {}

    void tick (float dt) { lastDt = dt; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto data = dataArea();
        g.setColour (theme::grid());
        g.drawHorizontalLine ((int) data.getBottom(), data.getX(), data.getRight());
        g.drawHorizontalLine ((int) data.getY(), data.getX(), data.getRight());

        lattice.setArea (data);
        const auto geo = geometry();
        const double S = val (s) / 100.0;
        lattice.beginFrame();
        for (int c = 0; c < lattice.numCols(); ++c)
        {
            const float x = lattice.colCentreX (c);
            double level;
            if (x <= geo.xA)      level = (x - data.getX()) / juce::jmax (1.0f, geo.xA - data.getX());
            else if (x <= geo.xD) { const double u = (x - geo.xA) / juce::jmax (1.0f, geo.xD - geo.xA); level = S + (1.0 - S) * std::pow (0.001, u); }
            else if (x <= geo.xS) level = S;
            else if (x <= geo.xR) { const double u = (x - geo.xS) / juce::jmax (1.0f, geo.xR - geo.xS); level = S * std::pow (0.001, u); }
            else continue;
            lattice.column (c, (float) juce::jlimit (0.0, 1.0, level));
        }
        lattice.paint (g, lastDt);

        // Corners.
        auto yOf = [&] (double level) { return data.getBottom() - (float) level * data.getHeight(); };
        const juce::Point<float> pts[] = { { geo.xA, yOf (1.0) }, { geo.xD, yOf (S) }, { geo.xR, data.getBottom() } };
        for (int i = 0; i < 3; ++i)
            SquareLattice::hollowSquare (g, pts[i], 10.0f, theme::mix (i == hot || i == dragging ? 1.0f : 0.7f));

        g.setColour (theme::dim());
        g.setFont (theme::sans (11.0f));
        g.drawText ("A", juce::Rectangle<float> (data.getX(), data.getBottom() + 2, geo.xA - data.getX(), 12), juce::Justification::centred);
        g.drawText ("D", juce::Rectangle<float> (geo.xA, data.getBottom() + 2, geo.xD - geo.xA, 12), juce::Justification::centred);
        g.drawText ("S", juce::Rectangle<float> (geo.xD, data.getBottom() + 2, geo.xS - geo.xD, 12), juce::Justification::centred);
        g.drawText ("R", juce::Rectangle<float> (geo.xS, data.getBottom() + 2, geo.xR - geo.xS, 12), juce::Justification::centred);
    }

    void mouseMove (const juce::MouseEvent& e) override { const int h = hitCorner (e.position); if (h != hot) { hot = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hot = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int h = hitCorner (e.position);
        if (h < 0) return;
        if (e.mods.isMiddleButtonDown() || e.mods.isAltDown())
        {
            for (auto* p : paramsOf (h)) p->setValueNotifyingHost (p->getDefaultValue());
            return;
        }
        dragging = h;
        last = e.position;
        for (auto* p : paramsOf (h)) p->beginChangeGesture();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging < 0) return;
        const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
        const auto delta = e.position - last;
        last = e.position;
        // Times: multiplicative (1% per pixel), so short and long times feel the same to drag.
        auto scaleTime = [&] (juce::RangedAudioParameter& p) { set (p, val (p) * std::exp (0.012 * delta.x * fine)); };
        if (dragging == 0) scaleTime (a);
        if (dragging == 1)
        {
            scaleTime (d);
            set (s, val (s) - delta.y / dataArea().getHeight() * 100.0 * fine);
        }
        if (dragging == 2) scaleTime (r);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging >= 0)
            for (auto* p : paramsOf (dragging)) p->endChangeGesture();
        dragging = -1;
    }

private:
    struct Geo { float xA, xD, xS, xR; };

    juce::Rectangle<float> dataArea() const { return getLocalBounds().toFloat().reduced (8.0f, 8.0f).withTrimmedBottom (10.0f); }

    Geo geometry() const
    {
        const auto data = dataArea();
        const double wa = std::sqrt (val (a)), wd = std::sqrt (val (d)), wr = std::sqrt (val (r));
        const float sustainW = data.getWidth() * 0.18f;
        const float scale = (data.getWidth() - sustainW - 6.0f) / (float) juce::jmax (1.0, wa + wd + wr);
        Geo g;
        g.xA = data.getX() + (float) wa * scale;
        g.xD = g.xA + (float) wd * scale;
        g.xS = g.xD + sustainW;
        g.xR = g.xS + (float) wr * scale;
        return g;
    }

    int hitCorner (juce::Point<float> p) const
    {
        const auto data = dataArea();
        const auto geo = geometry();
        const double S = val (s) / 100.0;
        const juce::Point<float> pts[] = { { geo.xA, data.getY() }, { geo.xD, data.getBottom() - (float) S * data.getHeight() }, { geo.xR, data.getBottom() } };
        for (int i = 0; i < 3; ++i)
            if (pts[i].getDistanceFrom (p) < 11.0f) return i;
        return -1;
    }

    std::vector<juce::RangedAudioParameter*> paramsOf (int h)
    {
        if (h == 0) return { &a };
        if (h == 1) return { &d, &s };
        return { &r };
    }

    static double val (const juce::RangedAudioParameter& p) { return p.convertFrom0to1 (p.getValue()); }
    static void set (juce::RangedAudioParameter& p, double v)
    {
        const auto& range = p.getNormalisableRange();
        p.setValueNotifyingHost (p.convertTo0to1 ((float) juce::jlimit ((double) range.start, (double) range.end, v)));
    }

    juce::RangedAudioParameter &a, &d, &s, &r;
    SquareLattice lattice;
    float lastDt = 1.0f / 60.0f;
    int hot = -1, dragging = -1;
    juce::Point<float> last;
};
} // namespace pvst
