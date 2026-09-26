#pragma once
// Shared look for all plugins: ArtHar's dark design (user's direction, 2026-09-26).
//
// A TWO-COLOUR palette: a near-black background and a warm off-white foreground. Every other
// colour is a mix of the two (`mix(t)`: 0 = background, 1 = foreground), exactly like ArtHar's
// gui/theme.rs. Thin 1px frames, square corners, letter-spaced capital section titles,
// monospace numbers. Accent colours are deliberately absent; use brightness for emphasis.
#include <juce_gui_basics/juce_gui_basics.h>

namespace pvst::theme
{
inline const juce::Colour bg { 0xff0c0c0c };
inline const juce::Colour fg { 0xffeceae4 };

/// A colour `t` of the way from the background (0) to the foreground (1).
inline juce::Colour mix (float t) { return bg.interpolatedWith (fg, juce::jlimit (0.0f, 1.0f, t)); }

// Named mixes, so every panel uses the same steps (values follow ArtHar's).
inline juce::Colour frame()     { return mix (0.22f); } // section frames, box outlines
inline juce::Colour grid()      { return mix (0.10f); } // plot grid lines
inline juce::Colour dim()       { return mix (0.55f); } // secondary labels
inline juce::Colour text()      { return mix (0.92f); } // normal text
inline juce::Colour boxFill()   { return mix (0.04f); } // number boxes, inactive buttons
inline juce::Colour selected()  { return mix (0.28f); } // active segment of a toggle
/// The ONE accent colour: hand-modified values (ArtHar v1.1 marks moved notes orange).
inline const juce::Colour modified { 0xffe8923a };

/// UTF-8 text literal -> juce::String. Plain `const char*` is read as Latin-1 by JUCE, which turns
/// "·" into "Â·" — always wrap non-ASCII literals in this.
inline juce::String u8 (const char* utf8) { return juce::String::fromUTF8 (utf8); }

/// The " · " separator used in info lines.
inline juce::String dot() { return u8 ("   \xc2\xb7   "); }

inline juce::Font sans (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

inline juce::Font mono (float size)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain));
}

/// Letter-spaced capitals for section titles ("DISSONANCE CURVE").
inline void drawSectionTitle (juce::Graphics& g, const juce::String& title, juce::Rectangle<int> area)
{
    g.setColour (text());
    g.setFont (sans (11.5f).withExtraKerningFactor (0.22f));
    g.drawText (title.toUpperCase(), area, juce::Justification::centredLeft);
}

/// A section frame (ArtHar: thin line, square corners) with its title in the top-left corner.
/// Returns the area inside the frame, under the title.
inline juce::Rectangle<int> drawSection (juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title)
{
    g.setColour (frame());
    g.drawRect (bounds, 1);
    auto inner = bounds.reduced (9, 6);
    if (title.isNotEmpty())
        drawSectionTitle (g, title, inner.removeFromTop (18));
    return inner;
}

/// Frame-rate independent easing towards a target.
inline float ease (float current, float target, float amount = 0.25f)
{
    const float next = current + (target - current) * amount;
    return std::abs (next - target) < 1e-5f ? target : next;
}
} // namespace pvst::theme
