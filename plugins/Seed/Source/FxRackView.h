#pragma once
// FX tab (v0.5): the FX RACK.
//   left  : the 5 slots top to bottom (= processing order). Click a slot to edit it, the small
//           square switches it on/off, the arrows swap it with its neighbour.
//   right : the selected slot's settings: type, on, input (Chain = the sound so far / Partials =
//           only the partials it picks), Picks (a continuous morph: scroll between Odd and Even for
//           a 50/50 mix), mix, pan, width, then the settings of its effect type.
// Every setting is a host parameter (automatable); swapping slots swaps their parameter values.
#include "PluginProcessor.h"
#include "ui/Controls.h"

/// Picks as a scrollable morph between pick rules (user v0.4: "scrolling in between Odd and Even
/// would sound really cool"). Drag up/down (40 px per rule, Shift = slower), wheel = 0.1 steps,
/// middle-click = default. Shows e.g. "Odd 50% / Even 50%".
class PicksBox : public juce::Component
{
public:
    PicksBox (juce::RangedAudioParameter& p, juce::String l)
        : param (p), label (std::move (l)), attachment (p, [this] (float) { repaint(); }, nullptr)
    {
        attachment.sendInitialUpdate();
        setRepaintsOnMouseActivity (true);
    }
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

class FxRackView : public juce::Component
{
public:
    explicit FxRackView (SeedProcessor& p) : proc (p) { rebuildDetail(); }

    /// IDs of the selected slot's parameters (for the page's Dice / Mutate / Reset buttons).
    juce::StringArray pageIds() const
    {
        juce::StringArray a;
        for (auto* n : { "type", "on", "in", "picks", "mix", "pan", "width", "dTime", "dSync", "dDiv", "dFb", "dSpiral", "dPing",
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
        g.drawText ("RACK  (top to bottom)", list.removeFromTop (16), juce::Justification::bottomLeft);
        list.removeFromTop (3);
        rows.clear();
        for (int sl = 1; sl <= seed::kNumFx; ++sl)
        {
            auto row = list.removeFromTop (kRowH - 2);
            list.removeFromTop (2);
            rows.push_back (row);
            const int type = typeOf (sl);
            const bool on = param (sl, "on").getValue() > 0.5f;
            const bool sel = sl == selected;
            g.setColour (sel ? th::selected() : th::boxFill()); g.fillRect (row);
            g.setColour (sel ? th::mix (0.5f) : th::frame()); g.drawRect (row, 1);
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
            g.drawText (juce::String (sl), r.removeFromLeft (14), juce::Justification::centredLeft);
            // On square.
            auto box = r.removeFromLeft (16).withSizeKeepingCentre (10, 10).toFloat();
            g.setColour (on ? th::mix (0.85f) : th::mix (0.3f));
            if (on) g.fillRect (box.reduced (2.0f));
            g.drawRect (box, 1.0f);
            r.removeFromLeft (6);
            g.setColour (type == seed::FxOff || ! on ? th::dim() : th::text()); g.setFont (th::sans (13.0f));
            g.drawText (seed::fxTypeNames()[type], r.removeFromLeft (110), juce::Justification::centredLeft);
            if (type != seed::FxOff)
            {
                const bool partials = param (sl, "in").getValue() > 0.5f;
                g.setColour (th::dim()); g.setFont (th::sans (11.5f));
                g.drawText ((partials ? "partials: " + param (sl, "picks").getCurrentValueAsText() : juce::String ("chain"))
                                + "   mix " + param (sl, "mix").getCurrentValueAsText(),
                            r, juce::Justification::centredLeft);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
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
            else if (x >= 40 && x < 58)
            {
                auto& p = param (sl, "on");
                p.setValueNotifyingHost (p.getValue() > 0.5f ? 0.0f : 1.0f);
            }
            else if (sl != selected) { selected = sl; rebuildDetail(); }
            repaint();
            return;
        }
    }

    void resized() override { layoutDetail(); }

private:
    static constexpr int kRowH = 38;
    juce::Rectangle<int> listArea() const { return getLocalBounds().removeFromLeft (360).reduced (4, 4); }
    juce::RangedAudioParameter& param (int sl, const char* n) const { return proc.param (seed::ids::fx (sl, n)); }
    int typeOf (int sl) const { auto& p = param (sl, "type"); return juce::jlimit (0, seed::fxTypeNames().size() - 1, (int) std::lround (p.convertFrom0to1 (p.getValue()))); }

    void rebuildDetail()
    {
        detail.clear();
        shownType = typeOf (selected);
        const int sl = selected;
        auto box = [&] (const char* n, const juce::String& l, int intD, int dec, double step, int w = 104) {
            auto c = std::make_unique<pvst::ScrubBox> (param (sl, n), l, intD, dec, step);
            items.push_back ({ c.get(), w }); detail.push_back (std::move (c));
        };
        auto seg = [&] (const char* n, const juce::String& l, juce::StringArray o, int w) {
            auto c = std::make_unique<pvst::SegmentedChoice> (param (sl, n), l, o);
            items.push_back ({ c.get(), w }); detail.push_back (std::move (c));
        };
        auto choice = [&] (const char* n, const juce::String& l, juce::StringArray o, int w) {
            auto c = std::make_unique<pvst::ChoiceBox> (param (sl, n), l, o);
            items.push_back ({ c.get(), w }); detail.push_back (std::move (c));
        };
        auto br = [&] { items.push_back ({ nullptr, 0 }); };
        items.clear();
        choice ("type", "Slot " + juce::String (sl) + " type", seed::fxTypeNames(), 140);
        seg ("on", "On", { "Off", "On" }, 96);
        seg ("in", "Input", seed::fxInputNames(), 150);
        {
            auto c = std::make_unique<PicksBox> (param (sl, "picks"), "Picks (scroll to morph)");
            items.push_back ({ c.get(), 210 }); detail.push_back (std::move (c));
        }
        box ("mix", "Mix (%)", 3, 1, 5.0, 96);
        box ("pan", "Pan (%)", 3, 1, 5.0, 104);
        box ("width", "Width (%)", 3, 0, 10.0, 96);
        br();
        switch (shownType)
        {
            case seed::FxDelay:
                seg ("dSync", "Time", { "ms", "BPM" }, 96);
                box ("dTime", "Time (ms)", 4, 2, 10.0, 124);
                choice ("dDiv", "Division", seed::syncDivNames(), 96);
                box ("dFb", "Feedback (%)", 2, 1, 5.0, 104);
                box ("dSpiral", "Spiral (c)", 4, 2, 50.0, 124);
                box ("dPing", "Ping-pong (%)", 3, 0, 10.0, 104);
                box ("dTone", "Tone (%)", 3, 0, 10.0, 90);
                break;
            case seed::FxRough:
                box ("rDecay", "Decay (s)", 2, 2, 0.5, 100);
                box ("rRough", "Roughness (%)", 3, 1, 5.0, 110);
                box ("rDamp", "Damping (%)", 3, 0, 10.0, 100);
                break;
            case seed::FxChirp:
                box ("cDecay", "Decay (s)", 2, 2, 0.5, 100);
                box ("cChirp", "Chirp (%)", 3, 0, 5.0, 100);
                box ("cSize", "Size (ms)", 3, 1, 5.0, 104);
                box ("cDamp", "Damping (%)", 3, 0, 10.0, 100);
                break;
            case seed::FxSquash:
                box ("kThr", "Threshold (dB)", 2, 1, 1.0, 112);
                box ("kSlope", "Slope (1 off, 0 limit, <0 over)", 1, 2, 0.05, 200);
                box ("kAtt", "Attack (ms)", 3, 2, 1.0, 110);
                box ("kRel", "Release (ms)", 4, 1, 10.0, 116);
                box ("kMake", "Makeup (dB)", 2, 1, 1.0, 104);
                break;
            default: break;
        }
        for (auto& c : detail) addAndMakeVisible (*c);
        layoutDetail();
        repaint();
    }

    void layoutDetail()
    {
        auto area = getLocalBounds().withTrimmedLeft (376);
        int x = area.getX(), y = area.getY() + 4, rowH = 48;
        for (auto& it : items)
        {
            if (it.c == nullptr || x + it.w > area.getRight()) { x = area.getX(); y += rowH + 10; if (it.c == nullptr) continue; }
            it.c->setBounds (x, y, it.w, 48);
            x += it.w + 14;
        }
    }

    struct Item { juce::Component* c; int w; };
    SeedProcessor& proc;
    int selected = 1, shownType = -1;
    std::vector<std::unique_ptr<juce::Component>> detail;
    std::vector<Item> items;
    std::vector<juce::Rectangle<int>> rows;
};
