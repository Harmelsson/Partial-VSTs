#pragma once
// Shared visual constants. Soft, warm, dark — "friendly instrument", not "lab equipment".
// Plugins may pick their own accent; everything else should come from here so the family of
// plugins feels related.
#include <juce_gui_basics/juce_gui_basics.h>

namespace pvst::theme
{
inline const juce::Colour bgTop    { 0xff1b1e24 };
inline const juce::Colour bgBottom { 0xff121418 };
inline const juce::Colour panel    { 0xff22262e };
inline const juce::Colour panelEdge{ 0xff2c313a };
inline const juce::Colour text     { 0xffebe6de };
inline const juce::Colour textDim  { 0xff8d8a86 };
inline const juce::Colour track    { 0xff343943 };
inline const juce::Colour accent   { 0xffe8b87c }; // warm amber
inline const juce::Colour accent2  { 0xff86c1b5 }; // soft sea-green, for secondary info

inline juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

/// Frame-rate independent-ish easing: move `current` a fraction of the way to `target`.
inline float ease (float current, float target, float amount = 0.22f)
{
    const float next = current + (target - current) * amount;
    return std::abs (next - target) < 1e-5f ? target : next;
}
} // namespace pvst::theme
