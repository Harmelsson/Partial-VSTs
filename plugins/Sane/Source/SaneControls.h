#pragma once
// Parameter-bound controls in the desert look:
//   Knob     rotary knob with its label above and the value below (double-click = default)
//   Bar      horizontal bar (bipolar fills from the middle) with label left and value right
//   Chips    a row of chips for a choice parameter (click = select); items can be disabled
//   Button   a plain chip that runs a callback
#include <juce_audio_processors/juce_audio_processors.h>
#include "SaneLook.h"

namespace sane::ui
{
/// Keeps a slider and a parameter in sync, with default-on-double-click.
inline void bindDefaults (juce::Slider& s, juce::RangedAudioParameter& p)
{
    s.setDoubleClickReturnValue (true, p.convertFrom0to1 (p.getDefaultValue()));
    s.setTooltip (p.getName (64));
}

class Knob : public juce::Component, private juce::Timer
{
public:
    Knob (juce::RangedAudioParameter& p, const juce::String& label) : param (&p), name (label)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (220);
        addAndMakeVisible (slider);
        attach (p);
        startTimerHz (15);
    }

    /// Rebind to another parameter (the slot editor reuses its three knobs for whichever slot is selected).
    void attach (juce::RangedAudioParameter& p, const juce::String& label = {})
    {
        att.reset();
        param = &p;
        if (label.isNotEmpty()) name = label;
        att = std::make_unique<juce::SliderParameterAttachment> (p, slider);
        bindDefaults (slider, p);
        repaint();
    }

    /// Optional custom value text (e.g. slot knobs whose meaning depends on the slot type).
    std::function<juce::String (float)> valueText;
    void setLabel (const juce::String& l) { name = l; repaint(); }

    void resized() override { slider.setBounds (getLocalBounds().withTrimmedTop (13).withTrimmedBottom (14).withSizeKeepingCentre (44, 44)); }

    void paint (juce::Graphics& g) override
    {
        look::drawLabel (g, name, getLocalBounds().removeFromTop (13));
        g.setColour (look::ink);
        g.setFont (look::mono (11.5f, true));
        const auto txt = valueText ? valueText (param->getValue()) : param->getCurrentValueAsText();
        g.drawText (txt, getLocalBounds().removeFromBottom (14), juce::Justification::centred, false);
    }

private:
    void timerCallback() override
    {
        const float v = param->getValue();
        if (std::abs (v - shown) > 1e-6f) { shown = v; repaint(); }
    }
    juce::RangedAudioParameter* param;
    juce::String name;
    juce::Slider slider;
    std::unique_ptr<juce::SliderParameterAttachment> att;
    float shown = -1.0f;
};

class Bar : public juce::Component, private juce::Timer
{
public:
    Bar (juce::RangedAudioParameter& p, const juce::String& label) : param (p), name (label)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (slider);
        att = std::make_unique<juce::SliderParameterAttachment> (p, slider);
        bindDefaults (slider, p);
        startTimerHz (15);
    }
    int labelWidth = 120, valueWidth = 58;
    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromLeft (labelWidth);
        r.removeFromRight (valueWidth);
        slider.setBounds (r);
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        look::drawLabel (g, name, r.removeFromLeft (labelWidth), juce::Justification::centredLeft);
        g.setColour (look::ink);
        g.setFont (look::mono (11.0f, true));
        g.drawText (param.getCurrentValueAsText(), r.removeFromRight (valueWidth), juce::Justification::centredRight, false);
    }

private:
    void timerCallback() override
    {
        const float v = param.getValue();
        if (std::abs (v - shown) > 1e-6f) { shown = v; repaint(); }
    }
    juce::RangedAudioParameter& param;
    juce::String name;
    juce::Slider slider;
    std::unique_ptr<juce::SliderParameterAttachment> att;
    float shown = -1.0f;
};

class Chips : public juce::Component
{
public:
    Chips (juce::RangedAudioParameter& p, juce::StringArray labels, float fontSize = 11.0f)
        : param (p), items (std::move (labels)), size (fontSize),
          att (p, [this] (float) { repaint(); if (onChange) onChange(); }, nullptr)
    {
        enabled.resize ((size_t) items.size(), true);
        att.sendInitialUpdate();
    }
    void setItemEnabled (int i, bool e) { if (i >= 0 && i < (int) enabled.size()) enabled[(size_t) i] = e; repaint(); }
    std::function<void()> onChange;
    int gap = 4;

    int preferredWidth() const
    {
        int w = 0;
        for (auto& s : items) w += look::chipWidth (s, size) + gap;
        return w - gap;
    }
    int selected() const { return juce::roundToInt (param.convertFrom0to1 (param.getValue())); }

    void paint (juce::Graphics& g) override
    {
        int x = 0;
        for (int i = 0; i < items.size(); ++i)
        {
            const int w = look::chipWidth (items[i], size);
            auto r = juce::Rectangle<int> (x, 0, w, getHeight());
            look::drawChip (g, r, items[i], i == selected(), enabled[(size_t) i], i == hover);
            x += w + gap;
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = itemAt (e.x); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = itemAt (e.x);
        if (i < 0 || ! enabled[(size_t) i]) return;
        att.setValueAsCompleteGesture ((float) i);
    }

private:
    int itemAt (int px) const
    {
        int x = 0;
        for (int i = 0; i < items.size(); ++i)
        {
            const int w = look::chipWidth (items[i], size);
            if (px >= x && px < x + w) return i;
            x += w + gap;
        }
        return -1;
    }
    juce::RangedAudioParameter& param;
    juce::StringArray items;
    float size;
    std::vector<bool> enabled;
    int hover = -1;
    juce::ParameterAttachment att;
};

class Button : public juce::Component
{
public:
    Button (juce::String t, std::function<void()> click) : text (std::move (t)), onClick (std::move (click)) {}
    bool on = false, isEnabled = true;
    juce::String text;
    std::function<void()> onClick;
    void paint (juce::Graphics& g) override { look::drawChip (g, getLocalBounds(), text, on, isEnabled, hover); }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { if (isEnabled && onClick && getLocalBounds().contains (e.getPosition())) onClick(); }
    int preferredWidth() const { return look::chipWidth (text); }

private:
    bool hover = false;
};
} // namespace sane::ui
