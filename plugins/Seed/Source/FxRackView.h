#pragma once
// FX tab (v0.5): the FX RACK. v0.6: 12 slots, the list scrolls (mouse wheel), the settings pane scrolls.
//   left  : the slots top to bottom (= processing order). Click a slot to edit it, the small
//           square switches it on/off, the arrows swap it with its neighbour.
//   middle: the selected slot's settings: type, on, input (Chain = the sound so far / Partials =
//           only the partials it picks), Picks (a continuous morph: scroll between Odd and Even for
//           a 50/50 mix), mix, pan, width, then the settings of its effect type.
// Every setting is a host parameter (automatable); swapping slots swaps their parameter values.
#include "PluginProcessor.h"
#include "ui/Controls.h"

/// Picks as a scrollable morph between pick rules (user v0.4: "scrolling in between Odd and Even
/// would sound really cool"). Drag up/down (40 px per rule, Shift = slower), wheel = 0.1 steps,
/// middle-click = default. Shows e.g. "Odd 50% / Even 50%".
class PicksBox : public juce::Component, public juce::SettableTooltipClient, public pvst::ModAware, public pvst::ParamBound
{
public:
    PicksBox (juce::RangedAudioParameter& p, juce::String l)
        : pvst::ModAware (this), param (p), label (std::move (l)), attachment (p, [this] (float) { repaint(); }, nullptr)
    {
        attachment.sendInitialUpdate();
        setRepaintsOnMouseActivity (true);
    }
    bool usesParam (const juce::String& id) const override { return param.getParameterID() == id; }
    juce::RangedAudioParameter* modParamAt (juce::Point<int>) override { return &param; }
    void paint (juce::Graphics& g) override
    {
        namespace th = pvst::theme;
        auto b = getLocalBounds();
        g.setColour (th::dim()); g.setFont (th::sans (12.5f));
        g.drawText (label, b.removeFromTop (16), juce::Justification::bottomLeft);
        b.removeFromTop (3);
        b = b.withHeight (28);
        g.setColour (th::boxFill()); g.fillRect (b);
        // A bar showing where we are across all rules.
        const auto& r = param.getNormalisableRange();
        const float frac = (float) ((value() - r.start) / (r.end - r.start));
        g.setColour (th::mix (0.12f)); g.fillRect (b.withWidth ((int) ((float) b.getWidth() * frac)));
        g.setColour (isMouseOverOrDragging() ? th::mix (0.45f) : th::frame()); g.drawRect (b, 1);
        g.setColour (th::text()); g.setFont (th::sans (13.0f));
        g.drawFittedText (param.getCurrentValueAsText(), b.reduced (6, 0), juce::Justification::centredLeft, 1, 0.8f);
        paintModOverlay (g, b);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isMiddleButtonDown() || e.mods.isAltDown()) { attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue())); return; }
        attachment.beginGesture(); dragging = true; start = value(); startY = e.position.y;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        const double perPx = e.mods.isShiftDown() ? 1.0 / 160.0 : 1.0 / 40.0;
        set (start + (startY - e.position.y) * perPx, false);
    }
    void mouseUp (const juce::MouseEvent&) override { if (dragging) attachment.endGesture(); dragging = false; }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        set (value() + (w.deltaY > 0 ? 1.0 : -1.0) * (e.mods.isShiftDown() ? 0.02 : 0.1), true);
    }
private:
    double value() const { return param.convertFrom0to1 (param.getValue()); }
    void set (double v, bool complete)
    {
        const auto& r = param.getNormalisableRange();
        v = juce::jlimit ((double) r.start, (double) r.end, v);
        if (complete) attachment.setValueAsCompleteGesture ((float) v); else attachment.setValueAsPartOfGesture ((float) v);
    }
    juce::RangedAudioParameter& param;
    juce::String label;
    juce::ParameterAttachment attachment;
    bool dragging = false;
    double start = 0.0;
    float startY = 0.0f;
};

class FxRackView : public juce::Component, public juce::DragAndDropTarget
{
public:
    explicit FxRackView (SeedProcessor& p) : proc (p)
    {
        viewport.setViewedComponent (&holder, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        viewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, pvst::theme::mix (0.3f));
        viewport.getVerticalScrollBar().setColour (juce::ScrollBar::trackColourId, pvst::theme::mix (0.06f));
        addAndMakeVisible (viewport);
        rebuildDetail();
    }

    /// IDs of the selected slot's parameters (for the page's Dice / Mutate / Reset buttons).
    juce::StringArray pageIds() const
    {
        juce::StringArray a;
        for (auto* n : { "type", "on", "in", "picks", "mix", "pan", "width", "dTime", "dSync", "dBeat", "dFb", "dSpiral", "dPing",
                         "dTone", "rDecay", "rRough", "rDamp", "cDecay", "cChirp", "cSize", "cDamp", "kThr", "kSlope", "kAtt", "kRel", "kMake" })
            a.add (seed::ids::fx (selected, n));
        return a;
    }

    void tick()
    {
        const int type = typeOf (selected);
        if (type != shownType) rebuildDetail();
        repaint (listArea());
    }

    void paint (juce::Graphics& g) override
    {
        namespace th = pvst::theme;
        auto list = listArea();
        g.setColour (th::dim()); g.setFont (th::sans (12.0f));
        g.drawText ("RACK  (top to bottom, scroll for more)", list.removeFromTop (16), juce::Justification::bottomLeft);
        list.removeFromTop (3);
        rows.clear();
        listViewport = list;
        maxScroll = juce::jmax (0, seed::kNumFx * kRowH - list.getHeight());
        scroll = juce::jlimit (0, maxScroll, scroll);
        g.saveState();
        g.reduceClipRegion (list);
        int y = list.getY() - scroll;
        for (int sl = 1; sl <= seed::kNumFx; ++sl, y += kRowH)
        {
            auto row = juce::Rectangle<int> (list.getX(), y, list.getWidth() - 10, kRowH - 2);
            rows.push_back (row);
            if (row.getBottom() < list.getY() || row.getY() > list.getBottom()) continue;
            const int type = typeOf (sl);
            const bool on = param (sl, "on").getValue() > 0.5f;
            const bool sel = sl == selected;
            g.setColour (sel ? th::selected() : th::boxFill()); g.fillRect (row);
            g.setColour (sl == dropRow ? th::modified : sel ? th::mix (0.5f) : th::frame()); g.drawRect (row, sl == dropRow ? 2 : 1);
            auto r = row.reduced (6, 0);
            // Arrows (swap with neighbour).
            auto up = r.removeFromLeft (14).withSizeKeepingCentre (10, 8).toFloat().translated (0, -5);
            auto dn = up.translated (0, 10);
            juce::Path pu, pd;
            pu.addTriangle (up.getCentreX(), up.getY(), up.getRight(), up.getBottom(), up.getX(), up.getBottom());
            pd.addTriangle (dn.getX(), dn.getY(), dn.getRight(), dn.getY(), dn.getCentreX(), dn.getBottom());
            g.setColour (th::mix (sl > 1 ? 0.6f : 0.2f)); g.fillPath (pu);
            g.setColour (th::mix (sl < seed::kNumFx ? 0.6f : 0.2f)); g.fillPath (pd);
            r.removeFromLeft (6);
            g.setColour (th::dim()); g.setFont (th::mono (12.0f));
            g.drawText (juce::String (sl), r.removeFromLeft (16), juce::Justification::centredLeft);
            // On square.
            auto box = r.removeFromLeft (16).withSizeKeepingCentre (10, 10).toFloat();
            g.setColour (on ? th::mix (0.85f) : th::mix (0.3f));
            if (on) g.fillRect (box.reduced (2.0f));
            g.drawRect (box, 1.0f);
            r.removeFromLeft (6);
            g.setColour (type == seed::FxOff || ! on ? th::dim() : th::text()); g.setFont (th::sans (13.0f));
            g.drawText (seed::fxTypeNames()[type], r.removeFromTop (r.getHeight() / 2 + 2), juce::Justification::bottomLeft);
            if (type != seed::FxOff)
            {
                const bool partials = param (sl, "in").getValue() > 0.5f;
                g.setColour (th::dim()); g.setFont (th::sans (10.5f));
                g.drawFittedText ((partials ? "partials: " + param (sl, "picks").getCurrentValueAsText() : juce::String ("chain"))
                                      + "  mix " + param (sl, "mix").getCurrentValueAsText(),
                                  r, juce::Justification::topLeft, 1, 0.8f);
            }
        }
        g.restoreState();
        // Scrollbar.
        if (maxScroll > 0)
        {
            const float frac = (float) list.getHeight() / (float) (seed::kNumFx * kRowH);
            const float h = juce::jmax (20.0f, frac * (float) list.getHeight());
            const float top = (float) list.getY() + (float) scroll / (float) maxScroll * ((float) list.getHeight() - h);
            g.setColour (th::mix (0.3f));
            g.fillRect (juce::Rectangle<float> ((float) list.getRight() - 6.0f, top, 5.0f, h));
        }
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (! listArea().contains (e.getPosition())) return;
        scroll = juce::jlimit (0, maxScroll, scroll - juce::roundToInt (w.deltaY * 120.0f) * (w.isReversed ? -1 : 1));
        repaint (listArea());
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! listViewport.contains (e.getPosition())) return;
        for (int i = 0; i < (int) rows.size(); ++i)
        {
            const auto& row = rows[(size_t) i];
            if (! row.contains (e.getPosition())) continue;
            const int sl = i + 1;
            const int x = e.getPosition().x - row.getX();
            if (x < 20)
            {
                const bool upHalf = e.getPosition().y < row.getCentreY();
                const int other = upHalf ? sl - 1 : sl + 1;
                if (other >= 1 && other <= seed::kNumFx) { proc.swapFxSlots (sl, other); selected = other; rebuildDetail(); }
            }
            else if (x >= 40 && x < 62)
            {
                auto& p = param (sl, "on");
                p.setValueNotifyingHost (p.getValue() > 0.5f ? 0.0f : 1.0f);
            }
            else if (sl != selected) { selected = sl; rebuildDetail(); }
            repaint();
            return;
        }
    }

    void resized() override
    {
        viewport.setBounds (getLocalBounds().withTrimmedLeft (kListW + 10));
        layoutDetail();
    }

    /// Selects a slot (used when a modulator link is clicked: show the control it drives).
    void selectSlot (int sl)
    {
        sl = juce::jlimit (1, seed::kNumFx, sl);
        if (sl != selected) { selected = sl; rebuildDetail(); }
        const int rowTop = (sl - 1) * kRowH;
        if (rowTop < scroll || rowTop + kRowH > scroll + listViewport.getHeight()) scroll = juce::jlimit (0, maxScroll, rowTop - kRowH);
        repaint();
    }
    int selectedSlot() const { return selected; }

    // A modulator chip dropped on a rack row: the on/off square -> On, elsewhere -> asks (Mix / Pan / Width / On / Type).
    int rowAtPoint (juce::Point<int> p) const
    {
        if (! listViewport.contains (p)) return -1;
        for (int i = 0; i < (int) rows.size(); ++i) if (rows[(size_t) i].contains (p)) return i + 1;
        return -1;
    }
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("mod:") && rowAtPoint (d.localPosition) > 0; }
    void itemDragEnter (const SourceDetails& d) override { dropRow = rowAtPoint (d.localPosition); repaint(); }
    void itemDragMove (const SourceDetails& d) override { const int r = rowAtPoint (d.localPosition); if (r != dropRow) { dropRow = r; repaint(); } }
    void itemDragExit (const SourceDetails&) override { dropRow = -1; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        const int sl = rowAtPoint (d.localPosition);
        dropRow = -1; repaint();
        if (sl < 1) return;
        const int mod = pvst::modOfDrag (d);
        if (d.localPosition.x - rows[(size_t) sl - 1].getX() >= 40 && d.localPosition.x - rows[(size_t) sl - 1].getX() < 62)
        { if (auto* h = pvst::findModHandler (this)) h->modDroppedOn (param (sl, "on"), mod); return; }
        pvst::chooseAndLink (*this, { { "Slot " + juce::String (sl) + ": Mix", &param (sl, "mix") }, { "Slot " + juce::String (sl) + ": Pan", &param (sl, "pan") },
                                      { "Slot " + juce::String (sl) + ": Width", &param (sl, "width") }, { "Slot " + juce::String (sl) + ": On / off", &param (sl, "on") },
                                      { "Slot " + juce::String (sl) + ": Type", &param (sl, "type") } }, mod);
    }

private:
    static constexpr int kRowH = 40, kListW = 226;
    juce::Rectangle<int> listArea() const { return getLocalBounds().removeFromLeft (kListW).reduced (2, 2); }
    juce::RangedAudioParameter& param (int sl, const char* n) const { return proc.param (seed::ids::fx (sl, n)); }
    int typeOf (int sl) const { auto& p = param (sl, "type"); return juce::jlimit (0, seed::fxTypeNames().size() - 1, (int) std::lround (p.convertFrom0to1 (p.getValue()))); }

    void rebuildDetail()
    {
        detail.clear();
        shownType = typeOf (selected);
        const int sl = selected;
        auto push = [&] (std::unique_ptr<juce::Component> c, int w) { items.push_back ({ c.get(), w }); detail.push_back (std::move (c)); };
        auto box = [&] (const char* n, const juce::String& l, int intD, int dec, double step, int w, const juce::String& help) {
            auto c = std::make_unique<pvst::ScrubBox> (param (sl, n), l, intD, dec, step);
            c->setTooltip (help);
            push (std::move (c), w);
        };
        auto seg = [&] (const char* n, const juce::String& l, juce::StringArray o, int w, const juce::String& help) {
            auto c = std::make_unique<pvst::SegmentedChoice> (param (sl, n), l, o);
            c->setTooltip (help);
            push (std::move (c), w);
        };
        auto choice = [&] (const char* n, const juce::String& l, juce::StringArray o, int w, const juce::String& help) {
            auto c = std::make_unique<pvst::ChoiceBox> (param (sl, n), l, o);
            c->setTooltip (help);
            push (std::move (c), w);
        };
        auto br = [&] { items.push_back ({ nullptr, 0 }); };
        items.clear();
        choice ("type", "Slot " + juce::String (sl) + " type", seed::fxTypeNames(), 150, "Which effect lives in this slot.");
        seg ("on", "On", { "Off", "On" }, 96, "Bypass this slot.");
        br();
        seg ("in", "Input", seed::fxInputNames(), 150, "Chain = the sound so far (classic effect). Partials = only the partials you pick below, straight from the synth.");
        {
            auto c = std::make_unique<PicksBox> (param (sl, "picks"), "Picks (scroll to morph)");
            c->setTooltip ("Which partials this slot hears when Input = Partials. Scroll between two rules for a blend (Odd 50% / Even 50%).");
            push (std::move (c), 190);
        }
        br();
        box ("mix", "Mix (%)", 3, 1, 5.0, 96, "Wet amount.");
        box ("pan", "Pan (%)", 3, 1, 5.0, 100, "Pan of the wet signal.");
        box ("width", "Width (%)", 3, 0, 10.0, 96, "Stereo width of the wet signal (0 = mono, 200 = extra wide).");
        br();
        switch (shownType)
        {
            case seed::FxDelay:
            {
                auto c = std::make_unique<pvst::RateControl> (param (sl, "dSync"), param (sl, "dTime"), param (sl, "dBeat"), "Time", 4, 2, 10.0, "ms");
                c->setHelp ("Delay time in ms, or tempo-synced with the BPM switch (scroll the division; '-' straight, 't' triplet, '.' dotted).");
                push (std::move (c), 210);
                box ("dTone", "Tone (%)", 3, 0, 10.0, 90, "Darkens repeated echoes.");
                br();
                box ("dFb", "Feedback (%)", 2, 1, 5.0, 104, "How much of each echo feeds the next one.");
                box ("dSpiral", "Spiral (c)", 4, 2, 50.0, 124, "Each repeat is transposed by this many cents: a spiral up or down.");
                box ("dPing", "Ping-pong (%)", 3, 0, 10.0, 104, "How far the echoes bounce between left and right.");
                break;
            }
            case seed::FxRough:
                box ("rDecay", "Decay (s)", 2, 2, 0.5, 100, "Reverb length.");
                box ("rRough", "Roughness (%)", 3, 1, 5.0, 110, "How far the detuned twin resonators beat against each other (100 % = maximally uncomfortable).");
                box ("rDamp", "Damping (%)", 3, 0, 10.0, 100, "Darkens the tail.");
                break;
            case seed::FxChirp:
                box ("cDecay", "Decay (s)", 2, 2, 0.5, 100, "Reverb length.");
                box ("cChirp", "Chirp (%)", 3, 0, 5.0, 100, "0 = plain spring tank, 100 = every click becomes a falling 'pew'.");
                box ("cSize", "Size (ms)", 3, 1, 5.0, 104, "Tank size.");
                box ("cDamp", "Damping (%)", 3, 0, 10.0, 100, "Darkens the tail.");
                break;
            case seed::FxSquash:
                box ("kThr", "Threshold (dB)", 2, 1, 1.0, 112, "Level where compression starts.");
                box ("kSlope", "Slope (1 off, 0 limit, <0 over)", 1, 2, 0.05, 210, "1 = no compression, 0.25 = 4:1, 0 = limiter, below 0 = over-compression (louder in = quieter out).");
                box ("kAtt", "Attack (ms)", 3, 2, 1.0, 110, "How fast it clamps down.");
                box ("kRel", "Release (ms)", 4, 1, 10.0, 116, "How fast it lets go.");
                box ("kMake", "Makeup (dB)", 2, 1, 1.0, 104, "Gain after compression.");
                break;
            default: break;
        }
        holder.removeAllChildren();
        for (auto& c : detail) holder.addAndMakeVisible (*c);
        layoutDetail();
        repaint();
    }

    void layoutDetail()
    {
        const int w = juce::jmax (100, viewport.getWidth() - viewport.getScrollBarThickness() - 4);
        int x = 4, y = 4;
        bool any = false; // something already placed on this row?
        for (auto& it : items)
        {
            if (it.c == nullptr) { if (any) { x = 4; y += 52; any = false; } continue; }
            if (x + it.w > w && any) { x = 4; y += 52; }
            it.c->setBounds (x, y, it.w, 48);
            x += it.w + 12;
            any = true;
        }
        if (! any) y -= 52;
        holder.setSize (w, y + 52);
    }

    struct Item { juce::Component* c; int w; };
    SeedProcessor& proc;
    int selected = 1, shownType = -1, scroll = 0, maxScroll = 0, dropRow = -1;
    juce::Viewport viewport;
    juce::Component holder;
    std::vector<std::unique_ptr<juce::Component>> detail;
    std::vector<Item> items;
    std::vector<juce::Rectangle<int>> rows;
    juce::Rectangle<int> listViewport;
};
