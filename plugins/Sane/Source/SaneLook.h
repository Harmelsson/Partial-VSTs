#pragma once
// Sane's look: the "desert" design the user chose (mockup 2, 2026-10-17).
//   ground  desaturated desert yellow (the panels)
//   ink     sharp black: structure, lines, important values
//   paper   white: active / selected / playing things
//   disp    a darker desaturated brown for every graph window (black was "a little intimidating")
// Only clickable things have boxes; section titles are bold capitals with a small black square.
// Every border runs through the whole window (one aligned grid: the user wanted corners to meet).
#include <juce_gui_basics/juce_gui_basics.h>

namespace sane::look
{
inline const juce::Colour ground { 0xffbdae8b };
inline const juce::Colour ink { 0xff121110 };
inline const juce::Colour paper { 0xfff6f3ea };
inline const juce::Colour disp { 0xff7e705a };
inline const juce::Colour dim { 0xff3b3428 };   // secondary text on the ground
inline const juce::Colour dispText = paper.withAlpha (0.88f);

inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

/// Condensed sans (Bahnschrift on Windows, else the default sans squeezed a little).
inline juce::Font sans (float size, bool bold = false)
{
    juce::Font f (juce::FontOptions ("Bahnschrift", size, bold ? juce::Font::bold : juce::Font::plain));
    return f.withHorizontalScale (0.92f);
}
inline juce::Font mono (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, bold ? juce::Font::bold : juce::Font::plain));
}
inline juce::Font caps (float size) { return sans (size, true).withExtraKerningFactor (0.12f); }

/// Section title: a small black square + bold letter-spaced capitals. Returns the width used.
inline int drawTitle (juce::Graphics& g, const juce::String& text, int x, int y)
{
    g.setColour (ink);
    g.fillRect (x, y + 4, 8, 8);
    g.setFont (caps (13.5f).withExtraKerningFactor (0.16f));
    const auto t = text.toUpperCase();
    const int w = (int) std::ceil (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t));
    g.drawText (t, x + 14, y, w + 4, 16, juce::Justification::centredLeft, false);
    return 14 + w + 4;
}

/// Small letter-spaced label (knob names, row labels).
inline void drawLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r, juce::Justification j = juce::Justification::centred,
                       juce::Colour c = dim)
{
    g.setColour (c);
    g.setFont (caps (10.5f));
    g.drawText (text.toUpperCase(), r, j, false);
}

/// A "chip" (clickable box): ink border; white fill when on; dashed + dim when disabled.
inline void drawChip (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool on, bool enabled = true, bool hover = false)
{
    if (on) { g.setColour (paper); g.fillRect (r); }
    else if (hover && enabled) { g.setColour (paper.withAlpha (0.35f)); g.fillRect (r); }
    g.setColour (enabled ? ink : dim);
    if (enabled) g.drawRect (r, 1.5f);
    else
    {
        juce::Path p; p.addRectangle (r.toFloat().reduced (0.75f));
        juce::PathStrokeType st (1.2f);
        const float dash[] = { 3.0f, 3.0f };
        juce::Path d; st.createDashedStroke (d, p, dash, 2);
        g.fillPath (d);
    }
    g.setFont (caps (11.0f));
    g.drawText (text.toUpperCase(), r, juce::Justification::centred, false);
}

inline int chipWidth (const juce::String& text, float size = 11.0f)
{
    return (int) std::ceil (juce::GlyphArrangement::getStringWidth (caps (size), text.toUpperCase())) + 16;
}

/// Graph-window background.
inline void fillDisp (juce::Graphics& g, juce::Rectangle<int> r) { g.setColour (disp); g.fillRect (r); }

/// Dashed path stroke helper.
inline void strokeDashed (juce::Graphics& g, const juce::Path& p, float width, float on = 4.0f, float off = 4.0f)
{
    juce::Path d;
    const float dash[] = { on, off };
    juce::PathStrokeType (width).createDashedStroke (d, p, dash, 2);
    g.fillPath (d);
}

/// The LookAndFeel: rotary knobs = black arc on a faint track + a pointer; bars = boxed fills.
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, ground);
        setColour (juce::PopupMenu::textColourId, ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, ink);
        setColour (juce::PopupMenu::highlightedTextColourId, paper);
        setColour (juce::TooltipWindow::backgroundColourId, paper);
        setColour (juce::TooltipWindow::textColourId, ink);
        setColour (juce::TooltipWindow::outlineColourId, ink);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const auto c = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
        const float R = std::min (w, h) * 0.5f - 4.0f;
        const float av = a0 + pos * (a1 - a0);
        juce::Path track; track.addCentredArc (c.x, c.y, R, R, 0.0f, a0, a1, true);
        g.setColour (ink.withAlpha (0.2f));
        g.strokePath (track, juce::PathStrokeType (3.0f));
        // bipolar parameters fill from the middle
        const bool bi = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float from = bi ? a0 + (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) * (a1 - a0) : a0;
        juce::Path val; val.addCentredArc (c.x, c.y, R, R, 0.0f, std::min (from, av), std::max (from, av) + 0.001f, true);
        g.setColour (ink);
        g.strokePath (val, juce::PathStrokeType (3.0f));
        const juce::Point<float> tip (c.x + (R - 7.0f) * std::sin (av), c.y - (R - 7.0f) * std::cos (av));
        g.drawLine (c.x, c.y, tip.x, tip.y, 1.6f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y + (float) h * 0.5f - 4.5f, (float) w, 9.0f);
        g.setColour (ink);
        g.drawRect (r, 1.5f);
        const bool bi = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float zero = bi ? r.getX() + r.getWidth() * (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum())) : r.getX();
        const float lo = std::min (zero, pos), hi = std::max (zero, pos);
        g.fillRect (juce::Rectangle<float> (lo, r.getY(), std::max (1.5f, hi - lo), r.getHeight()));
        if (bi) g.fillRect (juce::Rectangle<float> (zero - 0.75f, r.getY() - 3.0f, 1.5f, r.getHeight() + 6.0f));
    }

    juce::Font getPopupMenuFont() override { return sans (14.0f); }
};
} // namespace sane::look
