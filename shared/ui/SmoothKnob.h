#pragma once
// SmoothKnob — a gentle rotary control bound to a plugin parameter.
//
// Feel (user asked for "smoother, less intimidating, less aggressive scrubbing"):
//   - Drag up/right = more. 400 px of mouse travel covers the whole range (slow on purpose).
//   - Hold Shift while dragging or scrolling = 10x finer.
//   - Scroll wheel = 1% of the range per notch (integers: 1 step per notch).
//   - Double-click = type an exact value. Middle-click (or Alt+click) = back to default.
//   - The drawn value eases towards the real value, so jumps from automation look like glides.
// Host automation, undo gestures and FL's "link to controller" all work through
// juce::ParameterAttachment.
#include <juce_audio_processors/juce_audio_processors.h>
#include "ui/Theme.h"

namespace pvst
{
class SmoothKnob : public juce::Component
{
public:
    SmoothKnob (juce::RangedAudioParameter& p, juce::String displayName, juce::Colour accentColour = theme::accent)
        : param (p), name (std::move (displayName)), accent (accentColour),
          attachment (p, [this] (float) { repaint(); }, nullptr)
    {
        attachment.sendInitialUpdate();
        shown = param.getValue();
        setRepaintsOnMouseActivity (true);
        editor.setJustification (juce::Justification::centred);
        editor.setColour (juce::TextEditor::backgroundColourId, theme::panel);
        editor.setColour (juce::TextEditor::textColourId, theme::text);
        editor.setColour (juce::TextEditor::outlineColourId, accent.withAlpha (0.6f));
        editor.setColour (juce::TextEditor::focusedOutlineColourId, accent);
        editor.onReturnKey = [this] { commitText(); };
        editor.onEscapeKey = [this] { editor.setVisible (false); };
        editor.onFocusLost = [this] { commitText(); };
        addChildComponent (editor);
    }

    /// Diameter preference is decided by the parent's layout; the label sits under the dial.
    void setValueFontSize (float s) { valueFont = s; }
    void setShowName (bool b) { showName = b; }

    /// Called from the editor's animation timer.
    void tick()
    {
        const float target = param.getValue();
        const float next = theme::ease (shown, target);
        if (! juce::exactlyEqual (next, shown)) { shown = next; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        const auto dial = dialBounds();
        const float r = dial.getWidth() * 0.5f;
        const auto c = dial.getCentre();
        // Angles are clockwise from 12 o'clock (JUCE's arc convention): 7 o'clock -> 5 o'clock.
        constexpr float a0 = -juce::MathConstants<float>::pi * 0.8f;
        constexpr float span = juce::MathConstants<float>::pi * 1.6f;
        const float thickness = juce::jmax (3.0f, r * 0.12f);

        juce::Path trackArc, valueArc;
        trackArc.addCentredArc (c.x, c.y, r - thickness, r - thickness, 0.0f, a0, a0 + span, true);
        valueArc.addCentredArc (c.x, c.y, r - thickness, r - thickness, 0.0f, a0, a0 + span * shown, true);
        const juce::PathStrokeType stroke (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        g.setColour (theme::track);
        g.strokePath (trackArc, stroke);
        const bool hot = isMouseOverOrDragging();
        g.setColour (hot ? accent.brighter (0.15f) : accent);
        g.strokePath (valueArc, stroke);

        // Soft inner disc + indicator dot.
        const float inner = r - thickness * 2.4f;
        g.setColour (theme::panel.brighter (hot ? 0.08f : 0.04f));
        g.fillEllipse (c.x - inner, c.y - inner, inner * 2, inner * 2);
        const float ang = a0 + span * shown;
        const juce::Point<float> dot (c.x + std::sin (ang) * inner * 0.72f, c.y - std::cos (ang) * inner * 0.72f);
        g.setColour (accent);
        const float d = juce::jmax (3.0f, r * 0.09f);
        g.fillEllipse (dot.x - d, dot.y - d, d * 2, d * 2);

        // Value text in the middle, name under the dial.
        g.setColour (theme::text);
        g.setFont (theme::font (valueFont > 0 ? valueFont : juce::jmax (10.0f, r * 0.26f)));
        g.drawFittedText (param.getCurrentValueAsText(), dial.reduced (inner * 0.35f).toNearestInt(), juce::Justification::centred, 1);
        if (showName)
        {
            g.setColour (theme::textDim);
            g.setFont (theme::font (13.0f));
            g.drawText (name, getLocalBounds().removeFromBottom (18), juce::Justification::centred);
        }
    }

    void resized() override
    {
        editor.setBounds (dialBounds().withSizeKeepingCentre (juce::jmax (60.0f, dialBounds().getWidth() * 0.8f), 24.0f).toNearestInt());
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isMiddleButtonDown() || e.mods.isAltDown())
        {
            attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
            return;
        }
        attachment.beginGesture();
        dragging = true;
        dragNorm = param.getValue();
        lastPos = e.position;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        const auto delta = e.position - lastPos;
        lastPos = e.position;
        const float px = delta.x - delta.y; // up or right = more
        const float perPx = (e.mods.isShiftDown() ? 0.1f : 1.0f) / 400.0f;
        dragNorm = juce::jlimit (0.0f, 1.0f, dragNorm + px * perPx);
        attachment.setValueAsPartOfGesture (param.convertFrom0to1 (dragNorm));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging) attachment.endGesture();
        dragging = false;
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        editor.setText (param.getCurrentValueAsText(), false);
        editor.setVisible (true);
        editor.grabKeyboardFocus();
        editor.selectAll();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const float dir = (! juce::exactlyEqual (w.deltaY, 0.0f) ? w.deltaY : w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
        if (juce::exactlyEqual (dir, 0.0f)) return;
        const int steps = param.getNumSteps();
        float step = steps > 1 && steps < 1000 ? 1.0f / (float) (steps - 1) : 0.01f;
        if (e.mods.isShiftDown() && steps >= 1000) step *= 0.1f;
        const float next = juce::jlimit (0.0f, 1.0f, param.getValue() + (dir > 0 ? step : -step));
        attachment.setValueAsCompleteGesture (param.convertFrom0to1 (next));
    }

private:
    juce::Rectangle<float> dialBounds() const
    {
        auto b = getLocalBounds().toFloat();
        if (showName) b.removeFromBottom (20.0f);
        const float size = juce::jmin (b.getWidth(), b.getHeight());
        return b.withSizeKeepingCentre (size, size).reduced (2.0f);
    }

    void commitText()
    {
        if (! editor.isVisible()) return;
        editor.setVisible (false);
        const auto t = editor.getText().trim().replaceCharacter (',', '.');
        if (t.isEmpty()) return;
        // getValueForText understands the parameter's own text format; clamp to the range.
        const float norm = juce::jlimit (0.0f, 1.0f, param.getValueForText (t));
        attachment.setValueAsCompleteGesture (param.convertFrom0to1 (norm));
    }

    juce::RangedAudioParameter& param;
    juce::String name;
    juce::Colour accent;
    juce::ParameterAttachment attachment;
    juce::TextEditor editor;
    float shown = 0.0f, dragNorm = 0.0f, valueFont = 0.0f;
    bool dragging = false, showName = true;
    juce::Point<float> lastPos;
};

/// A pill-shaped on/off switch bound to a bool parameter, with an animated thumb.
class SmoothToggle : public juce::Component
{
public:
    SmoothToggle (juce::RangedAudioParameter& p, juce::String label)
        : param (p), text (std::move (label)), attachment (p, [this] (float) { repaint(); }, nullptr)
    {
        attachment.sendInitialUpdate();
        shown = param.getValue();
        setRepaintsOnMouseActivity (true);
    }

    void tick()
    {
        const float next = theme::ease (shown, param.getValue() > 0.5f ? 1.0f : 0.0f, 0.25f);
        if (! juce::exactlyEqual (next, shown)) { shown = next; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        auto pill = b.removeFromLeft (40.0f).withSizeKeepingCentre (40.0f, 20.0f);
        g.setColour (theme::track.interpolatedWith (theme::accent.withAlpha (0.85f), shown));
        g.fillRoundedRectangle (pill, 10.0f);
        const float x = pill.getX() + 3.0f + shown * (pill.getWidth() - 20.0f);
        g.setColour (theme::text.withAlpha (isMouseOver() ? 1.0f : 0.9f));
        g.fillEllipse (x, pill.getY() + 3.0f, 14.0f, 14.0f);
        g.setColour (shown > 0.5f ? theme::text : theme::textDim);
        g.setFont (theme::font (14.0f));
        g.drawText (text, b.withTrimmedLeft (10.0f), juce::Justification::centredLeft);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains (e.getPosition()))
            attachment.setValueAsCompleteGesture (param.getValue() > 0.5f ? 0.0f : 1.0f);
    }

private:
    juce::RangedAudioParameter& param;
    juce::String text;
    juce::ParameterAttachment attachment;
    float shown = 0.0f;
};
} // namespace pvst
