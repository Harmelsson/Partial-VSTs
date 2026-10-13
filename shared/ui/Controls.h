#pragma once
// ArtHar-style controls bound to plugin parameters.
//
// ScrubBox — a number box where EACH DIGIT is scrubbable (ArtHar's core interaction):
//   - Drag a digit up/down: changes the value by that digit's place value (tens digit = +-10,
//     third decimal = +-0.001). Carry/borrow happen live. Up = more.
//   - GENTLER than ArtHar (user asked, Seed is a synth): 18 px of drag per step (ArtHar: 10),
//     Shift = 3x slower still.
//   - Scroll wheel over a digit: one step of that digit per notch.
//   - Small arrows on the right: +-arrowStep.
//   - Double-click: type an exact value. Middle-click (or Alt+click): back to default.
// SegmentedToggle — ArtHar's "Seed | Hand" style two-way switch for a bool parameter.
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include "ui/Theme.h"

namespace pvst
{
/// Implemented by the editor (which is also the DragAndDropContainer). Controls find it by walking up
/// their parents: a modulator chip dropped on a ScrubBox links the modulator to that parameter, and
/// linked boxes ask which modulators drive them and how far they currently move them.
struct ModDropHandler
{
    virtual ~ModDropHandler() = default;
    virtual void modDroppedOn (juce::RangedAudioParameter& p, int mod) = 0;          // mod = 1..N
    virtual unsigned modMaskFor (const juce::RangedAudioParameter& p) = 0;           // bit m-1 = modulator m
    virtual float modOffsetNorm (const juce::RangedAudioParameter& p) = 0;           // live, normalised units
    virtual void modRemoveFrom (juce::RangedAudioParameter& p, int mod) = 0;
    virtual bool canModulate (const juce::RangedAudioParameter& p) = 0;               // every editable parameter, except the two locks
};

/// A control bound to parameter(s): lets the editor find "the control for parameter X" (click a modulator
/// link -> switch to the tab and light the control up).
struct ParamBound
{
    virtual ~ParamBound() = default;
    virtual bool usesParam (const juce::String& id) const = 0;
};
inline ModDropHandler* findModHandler (juce::Component* c)
{
    return dynamic_cast<ModDropHandler*> (juce::DragAndDropContainer::findParentDragContainerFor (c));
}

/// Mixin for every control that accepts a modulator chip dropped on it: highlights while a chip hovers,
/// links on drop, and shows which modulators drive it (orange "~1 3" tag + a bar where they push it).
/// `modParamAt` says which parameter a drop at that point means (controls with several parameters return different ones).
class ModAware : public juce::DragAndDropTarget, private juce::Timer
{
public:
    explicit ModAware (juce::Component* ownerComponent) : owner (*ownerComponent) { startTimerHz (20); }

    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        if (! d.description.toString().startsWith ("mod:")) return false;
        auto* p = modParamAt (d.localPosition);
        if (p == nullptr) return false;
        auto* h = findModHandler (&owner);
        return h == nullptr || h->canModulate (*p);
    }
    void itemDragEnter (const SourceDetails& d) override { hover = modParamAt (d.localPosition); owner.repaint(); }
    void itemDragMove (const SourceDetails& d) override { auto* p = modParamAt (d.localPosition); if (p != hover) { hover = p; owner.repaint(); } }
    void itemDragExit (const SourceDetails&) override { hover = nullptr; owner.repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        hover = nullptr;
        if (auto* p = modParamAt (d.localPosition))
            if (auto* h = findModHandler (&owner))
                h->modDroppedOn (*p, d.description.toString().fromFirstOccurrenceOf ("mod:", false, false).getIntValue());
        owner.repaint();
    }

protected:
    virtual juce::RangedAudioParameter* modParamAt (juce::Point<int> localPos) = 0;
    /// The parameter whose modulation is shown on the control (default: the one under its centre).
    virtual juce::RangedAudioParameter* primaryModParam() { return modParamAt (owner.getLocalBounds().getCentre()); }

    /// Call at the end of paint(): `box` = the control's drawn area.
    void paintModOverlay (juce::Graphics& g, juce::Rectangle<int> box, bool showNumbers = true)
    {
        if (hover != nullptr) { g.setColour (theme::modified); g.drawRect (box, 2); }
        if (shownMask == 0) return;
        if (showNumbers)
        {
            juce::String nums;
            for (int m = 0; m < 16; ++m) if (shownMask & (1u << m)) nums << (nums.isEmpty() ? "" : " ") << (m + 1);
            g.setColour (theme::modified);
            g.setFont (theme::mono (10.5f));
            g.drawText ("~" + nums, box.withHeight (12).translated (0, -13), juce::Justification::centredRight, false);
        }
        if (auto* p = primaryModParam())
        {
            const float base = p->getValue(), now = juce::jlimit (0.0f, 1.0f, base + shownOffset);
            auto bar = juce::Rectangle<float> ((float) box.getX() + 1.0f, (float) box.getBottom() - 4.0f, (float) box.getWidth() - 2.0f, 3.0f);
            g.setColour (theme::mix (0.15f)); g.fillRect (bar);
            g.setColour (theme::modified.withAlpha (0.85f));
            g.fillRect (juce::Rectangle<float> (bar.getX() + bar.getWidth() * juce::jmin (base, now), bar.getY(), juce::jmax (1.5f, bar.getWidth() * std::abs (now - base)), bar.getHeight()));
        }
    }

    juce::Component& owner;
    juce::RangedAudioParameter* hover = nullptr;

private:
    void timerCallback() override
    {
        if (! owner.isVisible()) return;
        auto* p = primaryModParam();
        auto* h = findModHandler (&owner);
        const unsigned mask = (p != nullptr && h != nullptr) ? h->modMaskFor (*p) : 0u;
        const float off = mask != 0 ? h->modOffsetNorm (*p) : 0.0f;
        if (mask != shownMask || std::abs (off - shownOffset) > 1e-4f) { shownMask = mask; shownOffset = off; owner.repaint(); }
    }
    unsigned shownMask = 0;
    float shownOffset = 0.0f;
};

/// A dropped modulator that could mean several parameters (an envelope corner, an EQ node, a partial...):
/// ask which one, then link it.
inline void chooseAndLink (juce::Component& c, std::vector<std::pair<juce::String, juce::RangedAudioParameter*>> options, int mod)
{
    juce::PopupMenu m;
    m.addSectionHeader ("Modulator " + juce::String (mod) + " controls:");
    for (size_t i = 0; i < options.size(); ++i) m.addItem ((int) i + 1, options[i].first);
    juce::Component::SafePointer<juce::Component> safe (&c);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe, options, mod] (int r) {
        if (safe == nullptr || r <= 0) return;
        if (auto* h = findModHandler (safe.getComponent())) h->modDroppedOn (*options[(size_t) r - 1].second, mod);
    });
}
inline int modOfDrag (const juce::DragAndDropTarget::SourceDetails& d)
{
    return d.description.toString().fromFirstOccurrenceOf ("mod:", false, false).getIntValue();
}

/// Label line above a control (unless compact); trims it off `b`.
inline void drawControlLabel (juce::Graphics& g, juce::Rectangle<int>& b, const juce::String& label, bool compact)
{
    if (compact) return;
    g.setColour (theme::dim());
    g.setFont (theme::sans (12.5f));
    g.drawText (label, b.removeFromTop (16), juce::Justification::bottomLeft);
    b.removeFromTop (3);
}

class ScrubBox : public juce::Component, public juce::DragAndDropTarget, public juce::SettableTooltipClient, public ParamBound, private juce::Timer
{
public:
    ScrubBox (juce::RangedAudioParameter& p, juce::String labelText, int integerDigits, int decimalPlaces, double arrowStepSize = 1.0)
        : param (p), label (std::move (labelText)), intDigits (integerDigits), decimals (decimalPlaces), arrowStep (arrowStepSize),
          attachment (p, [this] (float) { repaint(); }, nullptr)
    {
        attachment.sendInitialUpdate();
        const auto& r = param.getNormalisableRange();
        lo = r.start; hi = r.end;
        signed_ = lo < 0.0;
        setRepaintsOnMouseActivity (true);
        editor.setJustification (juce::Justification::centredLeft);
        editor.setFont (theme::mono (16.0f));
        editor.setColour (juce::TextEditor::backgroundColourId, theme::mix (0.08f));
        editor.setColour (juce::TextEditor::textColourId, theme::text());
        editor.setColour (juce::TextEditor::outlineColourId, theme::mix (0.5f));
        editor.setColour (juce::TextEditor::focusedOutlineColourId, theme::mix (0.7f));
        editor.setColour (juce::TextEditor::highlightColourId, theme::mix (0.3f));
        editor.onReturnKey = [this] { commitText(); };
        editor.onEscapeKey = [this] { editor.setVisible (false); };
        editor.onFocusLost = [this] { commitText(); };
        addChildComponent (editor);
        startTimerHz (20);
    }

    /// No label line above the box (used in dense panels, e.g. the modulator list).
    void setCompact (bool c) { compact = c; resized(); repaint(); }

    // ---- modulation: a modulator chip dropped here links it to this parameter -------------------
    bool usesParam (const juce::String& id) const override { return param.getParameterID() == id; }
    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        if (! d.description.toString().startsWith ("mod:")) return false;
        auto* h = findModHandler (this);
        return h == nullptr || h->canModulate (param);
    }
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        dropHover = false;
        if (auto* h = findModHandler (this)) h->modDroppedOn (param, d.description.toString().fromFirstOccurrenceOf ("mod:", false, false).getIntValue());
        repaint();
    }

    /// Optional text drawn dim to the right of the box (e.g. a note name).
    std::function<juce::String (double)> suffix;

    /// Width this box wants (label above may be wider; the parent decides).
    int preferredWidth() const { return (int) std::ceil (charW() * (float) numChars() + 16.0f + kArrowW); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        if (! compact)
        {
            g.setColour (theme::dim());
            g.setFont (theme::sans (12.5f));
            g.drawText (label, b.removeFromTop (kLabelH), juce::Justification::bottomLeft);
            b.removeFromTop (3);
        }
        auto box = boxArea();

        g.setColour (theme::boxFill());
        g.fillRect (box);
        g.setColour (dropHover ? theme::modified : isMouseOverOrDragging() ? theme::mix (0.4f) : theme::frame());
        g.drawRect (box, dropHover ? 2 : 1);

        // Modulation: which modulators drive this box (numbers) and where they currently push it.
        if (shownMask != 0)
        {
            juce::String nums;
            for (int m = 0; m < 16; ++m) if (shownMask & (1u << m)) nums << (nums.isEmpty() ? "" : " ") << (m + 1);
            g.setColour (theme::modified);
            g.setFont (theme::mono (11.0f));
            if (! compact)
            {   // right after the label text (so a long label isn't overprinted)
                const int lw = (int) std::ceil (juce::GlyphArrangement::getStringWidth (theme::sans (12.5f), label)) + 8;
                g.drawText ("~" + nums, juce::Rectangle<int> (box.getX() + lw, getLocalBounds().getY(), 60, kLabelH), juce::Justification::centredLeft, false);
            }
            auto bar = juce::Rectangle<float> ((float) box.getX() + 1.0f, (float) box.getBottom() - 4.0f, (float) (box.getWidth() - kArrowW - 2), 3.0f);
            const float base = param.getValue(), now = juce::jlimit (0.0f, 1.0f, base + shownOffset);
            g.setColour (theme::mix (0.15f));
            g.fillRect (bar);
            g.setColour (theme::modified.withAlpha (0.85f));
            g.fillRect (juce::Rectangle<float> (bar.getX() + bar.getWidth() * juce::jmin (base, now), bar.getY(),
                                                juce::jmax (1.5f, bar.getWidth() * std::abs (now - base)), bar.getHeight()));
        }

        // Digits.
        const auto str = valueString();
        g.setFont (theme::mono (17.0f));
        const float cw = charW();
        const float x0 = (float) box.getX() + 7.0f;
        for (int i = 0; i < str.length(); ++i)
        {
            const bool hot = i == hoverChar && placeOf (i) != kNoPlace;
            g.setColour (hot ? theme::fg : theme::text());
            g.drawText (juce::String::charToString (str[i]), juce::Rectangle<float> (x0 + cw * (float) i, (float) box.getY(), cw, (float) box.getHeight()),
                        juce::Justification::centred, false);
            if (hot)
                g.fillRect (x0 + cw * (float) i + 1.0f, (float) box.getBottom() - 5.0f, cw - 2.0f, 1.0f);
        }

        // Arrows.
        auto arrows = arrowArea();
        g.setColour (theme::frame());
        g.drawVerticalLine (arrows.getX(), (float) box.getY() + 1, (float) box.getBottom() - 1);
        auto up = arrows.removeFromTop (arrows.getHeight() / 2).toFloat().reduced (3.5f, 4.5f);
        auto dn = arrows.toFloat().reduced (3.5f, 4.5f);
        juce::Path pu, pd;
        pu.addTriangle (up.getCentreX(), up.getY(), up.getRight(), up.getBottom(), up.getX(), up.getBottom());
        pd.addTriangle (dn.getX(), dn.getY(), dn.getRight(), dn.getY(), dn.getCentreX(), dn.getBottom());
        g.setColour (theme::mix (hoverArrow == 1 ? 0.9f : 0.45f));
        g.fillPath (pu);
        g.setColour (theme::mix (hoverArrow == -1 ? 0.9f : 0.45f));
        g.fillPath (pd);

        if (suffix)
        {
            g.setColour (theme::dim());
            g.setFont (theme::sans (13.0f));
            g.drawText (suffix (value()), box.getRight() + 8, box.getY(), 60, box.getHeight(), juce::Justification::centredLeft);
        }
    }

    void resized() override { editor.setBounds (boxArea().withTrimmedRight (kArrowW)); }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int c = charAt (e.position.x), a = arrowAt (e.position);
        if (c != hoverChar || a != hoverArrow) { hoverChar = c; hoverArrow = a; repaint(); }
    }
    void mouseExit (const juce::MouseEvent&) override { hoverChar = -1; hoverArrow = 0; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            if (shownMask == 0) return;
            juce::PopupMenu menu;
            for (int m = 0; m < 16; ++m) if (shownMask & (1u << m)) menu.addItem (m + 1, "Remove modulator " + juce::String (m + 1));
            juce::Component::SafePointer<ScrubBox> safe (this);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r) {
                if (safe != nullptr && r > 0 && safe->handler != nullptr) safe->handler->modRemoveFrom (safe->param, r);
            });
            return;
        }
        if (e.mods.isMiddleButtonDown() || e.mods.isAltDown())
        {
            attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
            return;
        }
        if (const int a = arrowAt (e.position); a != 0)
        {
            setValue (value() + a * arrowStep, true);
            return;
        }
        dragPlace = placeOf (nearestDigit (charAt (e.position.x)));
        if (dragPlace == kNoPlace) return;
        attachment.beginGesture();
        dragging = true;
        dragStartValue = value();
        dragAccum = 0.0f;
        lastY = e.position.y;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        const float pxPerStep = e.mods.isShiftDown() ? 54.0f : 18.0f;
        dragAccum += (lastY - e.position.y) / pxPerStep;
        lastY = e.position.y;
        const double steps = std::trunc (dragAccum);
        setValue (dragStartValue + steps * std::pow (10.0, dragPlace), false);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging) attachment.endGesture();
        dragging = false;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (arrowAt (e.position) != 0 || ! e.mods.isLeftButtonDown()) return;
        editor.setText (juce::String (value(), decimals), false);
        editor.setVisible (true);
        editor.grabKeyboardFocus();
        editor.selectAll();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const float d = (juce::exactlyEqual (w.deltaY, 0.0f) ? w.deltaX : w.deltaY) * (w.isReversed ? -1.0f : 1.0f);
        if (juce::exactlyEqual (d, 0.0f)) return;
        int place = placeOf (nearestDigit (charAt (e.position.x)));
        if (place == kNoPlace) place = -decimals;
        setValue (value() + (d > 0 ? 1.0 : -1.0) * std::pow (10.0, place), true);
    }

private:
    static constexpr int kLabelH = 16, kArrowW = 14, kNoPlace = 1000;

    void timerCallback() override
    {
        if (! isVisible()) return;
        if (handler == nullptr) handler = findModHandler (this);
        const unsigned mask = handler != nullptr ? handler->modMaskFor (param) : 0u;
        const float off = mask != 0 ? handler->modOffsetNorm (param) : 0.0f;
        if (mask != shownMask || std::abs (off - shownOffset) > 1e-4f) { shownMask = mask; shownOffset = off; repaint(); }
    }
    void parentHierarchyChanged() override { handler = nullptr; }

    juce::Rectangle<int> boxArea() const
    {
        auto b = compact ? getLocalBounds() : getLocalBounds().withTrimmedTop (kLabelH + 3);
        return b.withWidth (juce::jmin (b.getWidth(), preferredWidth())).withHeight (juce::jmin (b.getHeight(), 28));
    }
    juce::Rectangle<int> arrowArea() const { return boxArea().removeFromRight (kArrowW); }

    float charW() const { return juce::GlyphArrangement::getStringWidth (theme::mono (17.0f), "0"); }
    int numChars() const { return (signed_ ? 1 : 0) + intDigits + (decimals > 0 ? 1 + decimals : 0); }

    double value() const { return param.convertFrom0to1 (param.getValue()); }

    void setValue (double v, bool asCompleteGesture)
    {
        v = juce::jlimit (lo, hi, v);
        const float norm = param.convertTo0to1 ((float) v);
        if (asCompleteGesture) attachment.setValueAsCompleteGesture (param.convertFrom0to1 (norm));
        else attachment.setValueAsPartOfGesture (param.convertFrom0to1 (norm));
    }

    /// Fixed-width text: sign slot, integer digits padded with spaces, decimals.
    juce::String valueString() const
    {
        const double v = value();
        auto s = juce::String (std::abs (v), decimals);
        if (decimals == 0) s = juce::String (juce::roundToInt (std::abs (v)));
        const int intLen = decimals > 0 ? s.indexOfChar ('.') : s.length();
        s = juce::String::repeatedString (" ", juce::jmax (0, intDigits - intLen)) + s;
        if (signed_) s = (v < -1e-12 ? "-" : " ") + s;
        return s;
    }

    /// Power of ten of the character at index i, or kNoPlace for sign/point/padding.
    int placeOf (int i) const
    {
        if (i < 0 || i >= numChars()) return kNoPlace;
        const int first = signed_ ? 1 : 0;
        if (i < first) return kNoPlace;
        const int k = i - first;
        if (k < intDigits) return intDigits - 1 - k;
        if (decimals == 0 || k == intDigits) return kNoPlace; // past the end / the decimal point
        const int d = k - intDigits - 1;
        return d < decimals ? -(d + 1) : kNoPlace;
    }

    /// Clicking a space/sign/point picks the nearest real digit instead.
    int nearestDigit (int i) const
    {
        if (i < 0) return -1;
        for (int off = 0; off < numChars(); ++off)
        {
            if (placeOf (i + off) != kNoPlace) return i + off;
            if (placeOf (i - off) != kNoPlace) return i - off;
        }
        return -1;
    }

    int charAt (float x) const
    {
        const auto box = boxArea();
        if (x >= (float) (box.getRight() - kArrowW) || x < (float) box.getX()) return -1;
        return juce::jlimit (0, numChars() - 1, (int) std::floor ((x - (float) box.getX() - 7.0f) / charW()));
    }

    int arrowAt (juce::Point<float> p) const
    {
        const auto a = arrowArea();
        if (! a.toFloat().contains (p)) return 0;
        return p.y < (float) a.getCentreY() ? 1 : -1;
    }

    void commitText()
    {
        if (! editor.isVisible()) return;
        editor.setVisible (false);
        const auto t = editor.getText().trim().replaceCharacter (',', '.');
        if (t.containsAnyOf ("0123456789")) setValue (t.getDoubleValue(), true);
    }

    juce::RangedAudioParameter& param;
    juce::String label;
    int intDigits, decimals;
    double arrowStep, lo = 0.0, hi = 1.0;
    bool signed_ = false;
    juce::ParameterAttachment attachment;
    juce::TextEditor editor;
    int hoverChar = -1, hoverArrow = 0, dragPlace = kNoPlace;
    bool compact = false, dropHover = false;
    ModDropHandler* handler = nullptr;
    unsigned shownMask = 0;
    float shownOffset = 0.0f;
    bool dragging = false;
    double dragStartValue = 0.0;
    float dragAccum = 0.0f, lastY = 0.0f;
};

class SegmentedToggle : public juce::Component, public juce::SettableTooltipClient, public ModAware, public ParamBound
{
public:
    void setCompact (bool c) { compact = c; repaint(); }
    bool usesParam (const juce::String& id) const override { return param.getParameterID() == id; }
    SegmentedToggle (juce::RangedAudioParameter& p, juce::String labelText, juce::String offText, juce::String onText)
        : ModAware (this), param (p), label (std::move (labelText)), texts { std::move (offText), std::move (onText) },
          attachment (p, [this] (float) { repaint(); }, nullptr)
    {
        attachment.sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        drawControlLabel (g, b, label, compact);
        b = b.withHeight (juce::jmin (b.getHeight(), 28));
        const auto overlayBox = b;
        const bool on = param.getValue() > 0.5f;
        for (int i = 0; i < 2; ++i)
        {
            auto cell = i == 0 ? b.removeFromLeft (b.getWidth() / 2) : b;
            const bool sel = (i == 1) == on;
            g.setColour (sel ? theme::selected() : theme::boxFill());
            g.fillRect (cell);
            g.setColour (theme::frame());
            g.drawRect (cell, 1);
            g.setColour (sel ? theme::fg : theme::dim());
            g.setFont (theme::sans (14.0f));
            g.drawText (texts[i], cell, juce::Justification::centred);
        }
        paintModOverlay (g, overlayBox);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const bool clickedOn = e.position.x > (float) getWidth() * 0.5f;
        attachment.setValueAsCompleteGesture (clickedOn ? 1.0f : 0.0f);
    }

protected:
    juce::RangedAudioParameter* modParamAt (juce::Point<int>) override { return &param; }
private:
    bool compact = false;
    juce::RangedAudioParameter& param;
    juce::String label;
    juce::String texts[2];
    juce::ParameterAttachment attachment;
};
/// N-way segmented switch for a choice (or int) parameter: ArtHar's "Seed | Hand" style.
class SegmentedChoice : public juce::Component, public juce::SettableTooltipClient, public ModAware, public ParamBound
{
public:
    void setCompact (bool c) { compact = c; repaint(); }
    bool usesParam (const juce::String& id) const override { return param.getParameterID() == id; }
    SegmentedChoice (juce::RangedAudioParameter& p, juce::String labelText, juce::StringArray optionNames)
        : ModAware (this), param (p), label (std::move (labelText)), names (std::move (optionNames)),
          attachment (p, [this] (float) { repaint(); }, nullptr)
    {
        attachment.sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        drawControlLabel (g, b, label, compact);
        b = b.withHeight (juce::jmin (b.getHeight(), 28));
        const auto overlayBox = b;
        const int sel = index();
        const int n = names.size();
        for (int i = 0; i < n; ++i)
        {
            auto cell = b.withX (b.getX() + b.getWidth() * i / n).withWidth (b.getWidth() / n + (i == n - 1 ? b.getWidth() % n : 0));
            g.setColour (i == sel ? theme::selected() : theme::boxFill());
            g.fillRect (cell);
            g.setColour (theme::frame());
            g.drawRect (cell, 1);
            g.setColour (i == sel ? theme::fg : theme::dim());
            g.setFont (theme::sans (13.0f));
            g.drawFittedText (names[i], cell.reduced (3, 0), juce::Justification::centred, 1, 0.8f);
        }
        paintModOverlay (g, overlayBox);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isMiddleButtonDown() || e.mods.isAltDown()) { attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue())); return; }
        const int n = names.size();
        const int i = juce::jlimit (0, n - 1, (int) (e.position.x * (float) n / (float) juce::jmax (1, getWidth())));
        attachment.setValueAsCompleteGesture ((float) (param.getNormalisableRange().start + (float) i));
    }

protected:
    juce::RangedAudioParameter* modParamAt (juce::Point<int>) override { return &param; }
private:
    int index() const { return juce::roundToInt (param.convertFrom0to1 (param.getValue()) - param.getNormalisableRange().start); }
    bool compact = false;
    juce::RangedAudioParameter& param;
    juce::String label;
    juce::StringArray names;
    juce::ParameterAttachment attachment;
};

/// Dropdown for a choice parameter with many options (ArtHar combo style: "Dissonance curve ▼").
class ChoiceBox : public juce::Component, public juce::SettableTooltipClient, public ModAware, public ParamBound
{
public:
    void setCompact (bool c) { compact = c; repaint(); }
    bool usesParam (const juce::String& id) const override { return param.getParameterID() == id; }
    ChoiceBox (juce::RangedAudioParameter& p, juce::String labelText, juce::StringArray optionNames)
        : ModAware (this), param (p), label (std::move (labelText)), names (std::move (optionNames)),
          attachment (p, [this] (float) { repaint(); }, nullptr)
    {
        attachment.sendInitialUpdate();
        setRepaintsOnMouseActivity (true);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        drawControlLabel (g, b, label, compact);
        b = b.withHeight (juce::jmin (b.getHeight(), 28));
        const auto overlayBox = b;
        g.setColour (theme::boxFill());
        g.fillRect (b);
        g.setColour (isMouseOver() ? theme::mix (0.4f) : theme::frame());
        g.drawRect (b, 1);
        g.setColour (theme::text());
        g.setFont (theme::sans (13.5f));
        g.drawFittedText (names[index()], b.reduced (8, 0).withTrimmedRight (14), juce::Justification::centredLeft, 1, 0.8f);
        juce::Path tri;
        const auto a = b.removeFromRight (18).toFloat().withSizeKeepingCentre (8.0f, 5.0f);
        tri.addTriangle (a.getX(), a.getY(), a.getRight(), a.getY(), a.getCentreX(), a.getBottom());
        g.setColour (theme::mix (0.6f));
        g.fillPath (tri);
        paintModOverlay (g, overlayBox);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isMiddleButtonDown() || e.mods.isAltDown()) { attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue())); return; }
        juce::PopupMenu m;
        for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == index());
        juce::Component::SafePointer<ChoiceBox> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r) {
            if (safe != nullptr && r > 0)
                safe->attachment.setValueAsCompleteGesture ((float) (safe->param.getNormalisableRange().start + (float) (r - 1)));
        });
    }

protected:
    juce::RangedAudioParameter* modParamAt (juce::Point<int>) override { return &param; }
private:
    int index() const { return juce::jlimit (0, names.size() - 1, juce::roundToInt (param.convertFrom0to1 (param.getValue()) - param.getNormalisableRange().start)); }
    bool compact = false;
    juce::RangedAudioParameter& param;
    juce::String label;
    juce::StringArray names;
    juce::ParameterAttachment attachment;
};

/// Plain ArtHar-style text button ("Light", "Reset", "Help & findings").
class FlatButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit FlatButton (juce::String t) : text (std::move (t)) { setRepaintsOnMouseActivity (true); }
    std::function<void()> onClick;
    void setText (const juce::String& t) { if (t != text) { text = t; repaint(); } }
    /// A lit button (e.g. "scale locked"): filled, in the accent colour.
    void setActive (bool a) { if (a != active) { active = a; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        g.setColour (active ? theme::modified.withAlpha (0.28f) : isMouseButtonDown() ? theme::selected() : isMouseOver() ? theme::mix (0.12f) : theme::boxFill());
        g.fillRect (b);
        g.setColour (active ? theme::modified : isMouseOver() ? theme::mix (0.45f) : theme::frame());
        g.drawRect (b, 1);
        g.setColour (active ? theme::modified : theme::text());
        g.setFont (theme::sans (13.0f));
        g.drawFittedText (text, b.reduced (4, 0), juce::Justification::centred, 1, 0.8f);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
    }
private:
    juce::String text;
    bool active = false;
};

/// ONE control for "how fast": Hz (or ms) OR a tempo division, switched by a small Hz|BPM toggle
/// (v0.6, user: "only one showing up if we change from Hz to BPM"). In BPM mode the box shows a
/// scrollable division, each with three versions: straight "4/16", triplet "4/16t", dotted "4/16.".
/// Wheel or drag up/down over the division steps through the list; the three flavour cells pick the version.
/// Bound to three parameters: sync (bool), the Hz/ms value (float), the division (choice; index = base*3 + flavour).
class RateControl : public juce::Component, public juce::SettableTooltipClient, public ModAware, public ParamBound
{
public:
    /// `divisionNames` = the choice parameter's names (base names are derived as "every third, without the t / .").
    RateControl (juce::RangedAudioParameter& syncParam, juce::RangedAudioParameter& valueParam, juce::RangedAudioParameter& beatParam,
                 juce::String labelText, int valueIntDigits, int valueDecimals, double valueStep, juce::String unit = "Hz", bool compactLayout = false)
        : ModAware (this), sync (syncParam), beat (beatParam), label (std::move (labelText)), unitText (std::move (unit)), compact (compactLayout),
          valueBox (valueParam, "", valueIntDigits, valueDecimals, valueStep),
          syncAttachment (syncParam, [this] (float) { updateMode(); }, nullptr),
          beatAttachment (beatParam, [this] (float) { repaint(); }, nullptr)
    {
        valueBox.setCompact (true);
        addAndMakeVisible (valueBox);
        syncAttachment.sendInitialUpdate();
        beatAttachment.sendInitialUpdate();
        setRepaintsOnMouseActivity (true);
        updateMode();
    }

    /// Tooltip for the whole control (the Hz/ms box too).
    void setHelp (const juce::String& t) { setTooltip (t); valueBox.setTooltip (t); }

    void resized() override { valueBox.setBounds (mainArea()); }
    bool usesParam (const juce::String& id) const override
    {
        return sync.getParameterID() == id || beat.getParameterID() == id || valueBox.usesParam (id);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds();
        drawControlLabel (g, b, label, compact);
        // Hz | BPM toggle.
        const bool bpm = isBpm();
        auto t = toggleArea();
        for (int i = 0; i < 2; ++i)
        {
            auto cell = i == 0 ? t.withWidth (t.getWidth() / 2) : t.withTrimmedLeft (t.getWidth() / 2);
            const bool sel = (i == 1) == bpm;
            g.setColour (sel ? theme::selected() : theme::boxFill());
            g.fillRect (cell);
            g.setColour (theme::frame());
            g.drawRect (cell, 1);
            g.setColour (sel ? theme::fg : theme::dim());
            g.setFont (theme::sans (12.5f));
            g.drawText (i == 0 ? unitText : "BPM", cell, juce::Justification::centred);
        }
        paintModOverlay (g, toggleArea(), false);
        if (! bpm) return;
        // Division box.
        auto box = mainArea();
        g.setColour (theme::boxFill()); g.fillRect (box);
        g.setColour (isMouseOverOrDragging() ? theme::mix (0.4f) : theme::frame()); g.drawRect (box, 1);
        auto flavours = box.removeFromRight (kFlavourW * 3);
        g.setColour (theme::text());
        g.setFont (theme::mono (17.0f));
        g.drawText (baseText(), box.reduced (6, 0), juce::Justification::centredLeft, false);
        static const char* marks[3] = { "-", "t", "." };
        for (int f = 0; f < 3; ++f)
        {
            auto cell = flavours.removeFromLeft (kFlavourW);
            const bool sel = f == flavour();
            g.setColour (sel ? theme::selected() : theme::boxFill()); g.fillRect (cell);
            g.setColour (theme::frame()); g.drawRect (cell, 1);
            g.setColour (sel ? theme::fg : theme::dim());
            g.setFont (theme::sans (15.0f, sel));
            g.drawText (marks[f], cell, juce::Justification::centred);
        }
        if (hover == &beat) { g.setColour (theme::modified); g.drawRect (mainArea(), 2); }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (toggleArea().contains (e.getPosition()))
        {
            syncAttachment.setValueAsCompleteGesture (e.position.x > (float) toggleArea().getCentreX() ? 1.0f : 0.0f);
            return;
        }
        if (! isBpm() || ! mainArea().contains (e.getPosition())) return;
        if (e.mods.isMiddleButtonDown() || e.mods.isAltDown()) { beatAttachment.setValueAsCompleteGesture (beat.convertFrom0to1 (beat.getDefaultValue())); return; }
        const auto box = mainArea();
        if (e.position.x >= (float) (box.getRight() - kFlavourW * 3))
        {
            const int f = juce::jlimit (0, 2, (int) ((e.position.x - (float) (box.getRight() - kFlavourW * 3)) / (float) kFlavourW));
            setIndex (base() * 3 + f, true);
            return;
        }
        dragging = true; dragAccum = 0.0f; lastY = e.position.y; dragStartBase = base();
        beatAttachment.beginGesture();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        dragAccum += (lastY - e.position.y) / (e.mods.isShiftDown() ? 40.0f : 14.0f);
        lastY = e.position.y;
        setIndex (juce::jlimit (0, numBases() - 1, dragStartBase + (int) std::trunc (dragAccum)) * 3 + flavour(), false);
    }
    void mouseUp (const juce::MouseEvent&) override { if (dragging) beatAttachment.endGesture(); dragging = false; }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (! isBpm()) { valueBox.mouseWheelMove (e.getEventRelativeTo (&valueBox), w); return; }
        const float d = (juce::exactlyEqual (w.deltaY, 0.0f) ? w.deltaX : w.deltaY) * (w.isReversed ? -1.0f : 1.0f);
        if (juce::exactlyEqual (d, 0.0f)) return;
        setIndex (juce::jlimit (0, numBases() - 1, base() + (d > 0 ? 1 : -1)) * 3 + flavour(), true);
    }

protected:
    /// The BPM switch = the sync parameter; the division box (in BPM mode) = the division parameter.
    juce::RangedAudioParameter* modParamAt (juce::Point<int> p) override
    {
        if (toggleArea().contains (p)) return &sync;
        return isBpm() && mainArea().contains (p) ? &beat : nullptr;
    }
    juce::RangedAudioParameter* primaryModParam() override { return isBpm() ? &beat : &sync; }

private:
    static constexpr int kFlavourW = 17, kToggleW = 50;
    bool isBpm() const { return sync.getValue() > 0.5f; }
    int index() const { return juce::roundToInt (beat.convertFrom0to1 (beat.getValue())); }
    int base() const { return index() / 3; }
    int flavour() const { return index() % 3; }
    int numBases() const { return (int) (beat.getNumSteps() > 1 ? beat.getNumSteps() : 3) / 3; }
    juce::String baseText() const
    {
        const auto* c = dynamic_cast<juce::AudioParameterChoice*> (&beat);
        const auto name = c != nullptr ? c->choices[base() * 3] : juce::String (base());
        return name;
    }
    void setIndex (int i, bool complete)
    {
        i = juce::jlimit (0, numBases() * 3 - 1, i);
        if (complete) beatAttachment.setValueAsCompleteGesture ((float) i); else beatAttachment.setValueAsPartOfGesture ((float) i);
    }
    juce::Rectangle<int> rowArea() const { auto b = compact ? getLocalBounds() : getLocalBounds().withTrimmedTop (19); return b.withHeight (juce::jmin (28, b.getHeight())); }
    juce::Rectangle<int> toggleArea() const { return rowArea().withWidth (kToggleW); }
    juce::Rectangle<int> mainArea() const { return rowArea().withTrimmedLeft (kToggleW + 6); }
    void updateMode() { valueBox.setVisible (! isBpm()); repaint(); }

    juce::RangedAudioParameter &sync, &beat;
    juce::String label, unitText;
    bool compact;
    ScrubBox valueBox;
    juce::ParameterAttachment syncAttachment, beatAttachment;
    bool dragging = false;
    float dragAccum = 0.0f, lastY = 0.0f;
    int dragStartBase = 0;
};
} // namespace pvst
