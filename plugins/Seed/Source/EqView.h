#pragma once
// The 5-band EQ (v0.6), at the right of the FX tab: settings of the selected band on the left, a live
// response display on the right.
//
// The EQ runs PER VOICE, before the FX rack, so each band can follow the note that is playing
// ("Key track": 0 % = fixed frequency, 100 % = the band moves one octave per octave of pitch; it sits
// exactly at its Freq on the base key). The display shows the curve for the note you are holding right
// now (or the base key when nothing is held), so you can watch key tracking move it.
//
//   Display:  drag a square = frequency (left/right) and gain (up/down; for cuts / notch / band-pass,
//             up/down changes Q) · mouse wheel = Q · right-click or double-click = band on/off.
//   Settings: band picker 1-5, On, Type (Bell, Low/High shelf, Low/High cut, Notch, Band-pass),
//             Freq, Gain, Q, Steepness (cuts: 12-48 dB/oct), Key track.
#include "PluginProcessor.h"
#include "ui/Controls.h"

class EqView : public juce::Component, public juce::DragAndDropTarget
{
public:
    explicit EqView (SeedProcessor& p) : proc (p) { rebuildSettings(); }

    /// Parameter IDs of the selected band (for the FX page's Dice / Mutate / Reset buttons).
    juce::StringArray bandIds() const
    {
        juce::StringArray a;
        for (auto* n : { "on", "type", "freq", "gain", "q", "steep", "kt" }) a.add (seed::ids::eq (selected, n));
        return a;
    }
    juce::StringArray allIds() const
    {
        juce::StringArray a;
        for (int b = 1; b <= seed::kEqBands; ++b)
            for (auto* n : { "on", "type", "freq", "gain", "q", "steep", "kt" }) a.add (seed::ids::eq (b, n));
        return a;
    }

    void tick()
    {
        // Follow the held note (smoothly) so key tracking is visible.
        float held = std::nanf ("");
        for (auto& h : proc.uiHeld) { const float c = h.load (std::memory_order_relaxed); if (! std::isnan (c)) { held = c; break; } }
        const double target = std::isnan (held) ? 0.0 : (double) held;
        refCents += (target - refCents) * 0.25;
        haveNote = ! std::isnan (held);
        const auto t = typeOf (selected);
        steepBox->setAlpha (pvst::eq::usesSteepness (t) ? 1.0f : 0.3f);
        gainBox->setAlpha (pvst::eq::usesGain (t) ? 1.0f : 0.3f);
        repaint (displayArea());
    }

    void resized() override { layoutSettings(); }

    /// Selects a band (used when a modulator link is clicked: show the control it drives).
    void selectBand (int b) { select (juce::jlimit (1, seed::kEqBands, b)); }
    int selectedBand() const { return selected; }

    // A modulator chip dropped on a node: ask which of its settings.
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("mod:") && displayArea().contains (d.localPosition) && nodeAt (d.localPosition.toFloat()) > 0; }
    void itemDragEnter (const SourceDetails& d) override { dropBand = nodeAt (d.localPosition.toFloat()); }
    void itemDragMove (const SourceDetails& d) override { dropBand = nodeAt (d.localPosition.toFloat()); }
    void itemDragExit (const SourceDetails&) override { dropBand = -1; }
    void itemDropped (const SourceDetails& d) override
    {
        const int b = nodeAt (d.localPosition.toFloat());
        dropBand = -1;
        if (b < 1) return;
        const auto name = "Band " + juce::String (b) + " ";
        pvst::chooseAndLink (*this, { { name + "frequency", &param (b, "freq") }, { name + "gain", &param (b, "gain") }, { name + "Q", &param (b, "q") },
                                      { name + "key track", &param (b, "kt") }, { name + "on / off", &param (b, "on") }, { name + "type", &param (b, "type") } }, pvst::modOfDrag (d));
    }

    void paint (juce::Graphics& g) override
    {
        namespace th = pvst::theme;
        // Band picker.
        for (int b = 1; b <= seed::kEqBands; ++b)
        {
            const auto r = pickerRect (b);
            const bool on = param (b, "on").getValue() > 0.5f;
            g.setColour (b == selected ? th::selected() : th::boxFill());
            g.fillRect (r);
            g.setColour (b == selected ? th::mix (0.6f) : th::frame());
            g.drawRect (r, 1);
            g.setColour (on ? th::fg : th::dim());
            g.setFont (th::mono (14.0f));
            g.drawText (juce::String (b), r, juce::Justification::centred);
            if (on) g.fillRect (r.getX() + 5, r.getBottom() - 5, r.getWidth() - 10, 2);
        }

        // Display.
        const auto area = displayArea();
        g.setColour (th::frame());
        g.drawRect (area, 1);
        g.setColour (th::dim());
        g.setFont (th::sans (11.0f));
        g.drawText (haveNote ? "showing the held note" : "showing the base key", area.reduced (6, 3).removeFromTop (14), juce::Justification::centredRight);
        const auto plot = plotArea();
        g.setFont (th::sans (10.5f));
        for (double db = -24.0; db <= 24.0; db += 12.0)
        {
            const float y = yOf (db);
            g.setColour (juce::exactlyEqual (db, 0.0) ? th::mix (0.25f) : th::grid());
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            g.setColour (th::dim());
            g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (plot.getX() - 30.0f, y - 6.0f, 26.0f, 12.0f), juce::Justification::centredRight);
        }
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        {
            const float x = xOf (f);
            g.setColour (th::grid());
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (th::dim());
            g.drawText (f >= 1000.0 ? juce::String (f / 1000.0, 0) + "k" : juce::String ((int) f), juce::Rectangle<float> (x - 18.0f, plot.getBottom() + 2.0f, 36.0f, 12.0f), juce::Justification::centred);
        }

        // Combined response + the selected band's own curve.
        const auto bands = readBands();
        juce::Path curve, fill, own;
        const int w = (int) plot.getWidth();
        for (int px = 0; px <= w; px += 2)
        {
            const double f = freqAtX (plot.getX() + (float) px);
            double db = 0.0, dbOwn = 0.0;
            for (int b = 0; b < seed::kEqBands; ++b)
            {
                const double d = 20.0 * std::log10 (std::max (1e-6, pvst::eq::magnitude (bands[(size_t) b].coefs, f, kSr)));
                db += d;
                if (b + 1 == selected) dbOwn = d;
            }
            const float x = plot.getX() + (float) px, y = yOf (std::clamp (db, -24.0, 24.0)), yo = yOf (std::clamp (dbOwn, -24.0, 24.0));
            if (px == 0) { curve.startNewSubPath (x, y); own.startNewSubPath (x, yo); fill.startNewSubPath (x, yOf (0.0)); fill.lineTo (x, y); }
            else { curve.lineTo (x, y); own.lineTo (x, yo); fill.lineTo (x, y); }
        }
        fill.lineTo (plot.getRight(), yOf (0.0));
        fill.closeSubPath();
        g.setColour (th::mix (0.14f));
        g.fillPath (fill);
        if (bands[(size_t) selected - 1].on)
        {
            g.setColour (th::mix (0.45f));
            g.strokePath (own, juce::PathStrokeType (1.0f));
        }
        g.setColour (th::fg);
        g.strokePath (curve, juce::PathStrokeType (1.8f));

        // Nodes.
        for (int b = 1; b <= seed::kEqBands; ++b)
        {
            const auto& bd = bands[(size_t) b - 1];
            const auto c = nodePos (bd);
            const float side = b == selected ? 14.0f : 11.0f;
            const auto r = juce::Rectangle<float> (side, side).withCentre (c);
            g.setColour (th::bg);
            g.fillRect (r);
            g.setColour (b == dropBand ? th::modified : bd.on ? (b == selected ? th::modified : th::fg) : th::mix (0.35f));
            g.drawRect (r, b == dropBand ? 3.0f : b == selected ? 2.0f : 1.5f);
            g.setFont (th::mono (10.5f));
            g.drawText (juce::String (b), r.translated (0.0f, -side - 1.0f), juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int b = 1; b <= seed::kEqBands; ++b)
            if (pickerRect (b).contains (e.getPosition())) { select (b); return; }
        if (! displayArea().contains (e.getPosition())) return;
        const int hit = nodeAt (e.position);
        if (hit < 0) return;
        select (hit);
        if (e.mods.isPopupMenu()) { toggleOn (hit); return; }
        dragBand = hit;
        const auto bands = readBands();
        startFreq = val (hit, "freq"); startGain = val (hit, "gain"); startQ = val (hit, "q");
        startPos = e.position;
        startTypeUsesGain = pvst::eq::usesGain (bands[(size_t) hit - 1].type);
        param (hit, "freq").beginChangeGesture(); param (hit, "gain").beginChangeGesture(); param (hit, "q").beginChangeGesture();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragBand < 0) return;
        const auto plot = plotArea();
        const auto kt = val (dragBand, "kt") / 100.0;
        // The node's x is where the band sits for the shown note: undo key tracking to get its Freq.
        const double shown = freqAtX (juce::jlimit (plot.getX(), plot.getRight(), e.position.x));
        set (dragBand, "freq", shown / std::exp2 (kt * refCents / 1200.0));
        if (startTypeUsesGain)
            set (dragBand, "gain", std::clamp (dbAtY (juce::jlimit (plot.getY(), plot.getBottom(), e.position.y)), -24.0, 24.0));
        else
            set (dragBand, "q", startQ * std::exp ((startPos.y - e.position.y) / 70.0));
        repaint (displayArea());
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragBand < 0) return;
        param (dragBand, "freq").endChangeGesture(); param (dragBand, "gain").endChangeGesture(); param (dragBand, "q").endChangeGesture();
        dragBand = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (const int hit = nodeAt (e.position); hit > 0 && displayArea().contains (e.getPosition())) toggleOn (hit);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (! displayArea().contains (e.getPosition())) return;
        int hit = nodeAt (e.position);
        if (hit < 0) hit = selected;
        set (hit, "q", val (hit, "q") * std::exp ((w.deltaY > 0 ? 1.0 : -1.0) * (e.mods.isShiftDown() ? 0.03 : 0.12)));
        repaint (displayArea());
    }

private:
    static constexpr double kSr = 48000.0;
    static constexpr int kSettingsW = 204;

    struct BandView { bool on; int type; double freq, gain, q; pvst::eq::Coefs coefs; };

    juce::RangedAudioParameter& param (int b, const char* n) const { return proc.param (seed::ids::eq (b, n)); }
    double val (int b, const char* n) const { auto& p = param (b, n); return (double) p.convertFrom0to1 (p.getValue()); }
    void set (int b, const char* n, double v)
    {
        auto& p = param (b, n);
        const auto& r = p.getNormalisableRange();
        p.setValueNotifyingHost (p.convertTo0to1 ((float) juce::jlimit ((double) r.start, (double) r.end, v)));
    }
    int typeOf (int b) const { return juce::jlimit (0, pvst::eq::kNumTypes - 1, (int) std::lround (val (b, "type"))); }
    void toggleOn (int b) { auto& p = param (b, "on"); p.setValueNotifyingHost (p.getValue() > 0.5f ? 0.0f : 1.0f); repaint(); }
    void select (int b) { if (b != selected) { selected = b; rebuildSettings(); } repaint(); }

    std::array<BandView, seed::kEqBands> readBands() const
    {
        std::array<BandView, seed::kEqBands> out {};
        for (int b = 1; b <= seed::kEqBands; ++b)
        {
            pvst::eq::Band band;
            band.on = param (b, "on").getValue() > 0.5f;
            band.type = typeOf (b);
            band.freq = val (b, "freq"); band.gainDb = val (b, "gain"); band.q = val (b, "q");
            band.steep = (int) std::lround (val (b, "steep")) + 1;
            const double kt = val (b, "kt") / 100.0;
            out[(size_t) b - 1] = { band.on, band.type, pvst::eq::trackedFreq (band.freq, kt, refCents), band.gainDb, band.q,
                                    pvst::eq::makeCoefs (band, pvst::eq::trackedFreq (band.freq, kt, refCents), kSr) };
        }
        return out;
    }

    // ---- geometry ------------------------------------------------------------------------------
    juce::Rectangle<int> displayArea() const { return getLocalBounds().withTrimmedLeft (kSettingsW + 10); }
    juce::Rectangle<float> plotArea() const { return displayArea().toFloat().withTrimmedLeft (34.0f).withTrimmedRight (8.0f).withTrimmedTop (20.0f).withTrimmedBottom (16.0f); }
    float xOf (double f) const { auto p = plotArea(); return p.getX() + (float) (std::log (std::max (f, 20.0) / 20.0) / std::log (1000.0)) * p.getWidth(); }
    double freqAtX (float x) const { auto p = plotArea(); return 20.0 * std::pow (1000.0, (double) ((x - p.getX()) / p.getWidth())); }
    float yOf (double db) const { auto p = plotArea(); return p.getCentreY() - (float) (db / 24.0) * p.getHeight() * 0.5f; }
    double dbAtY (float y) const { auto p = plotArea(); return (double) ((p.getCentreY() - y) / (p.getHeight() * 0.5f)) * 24.0; }
    juce::Point<float> nodePos (const BandView& bd) const
    {
        const double gain = pvst::eq::usesGain (bd.type) ? bd.gain : 0.0;
        return { xOf (bd.freq), yOf (std::clamp (gain, -24.0, 24.0)) };
    }
    int nodeAt (juce::Point<float> p) const
    {
        const auto bands = readBands();
        int best = -1; float bestD = 14.0f;
        for (int b = 1; b <= seed::kEqBands; ++b)
            if (const float d = nodePos (bands[(size_t) b - 1]).getDistanceFrom (p); d < bestD) { bestD = d; best = b; }
        return best;
    }
    juce::Rectangle<int> pickerRect (int b) const { return { (b - 1) * 38, 0, 34, 26 }; }

    // ---- settings --------------------------------------------------------------------------------
    void rebuildSettings()
    {
        settings.clear();
        const int b = selected;
        auto add = [&] (std::unique_ptr<juce::Component> c) -> juce::Component* { addAndMakeVisible (*c); settings.push_back (std::move (c)); return settings.back().get(); };
        juce::StringArray types;
        for (auto* n : pvst::eq::kTypeNames) types.add (n);
        auto type = std::make_unique<pvst::ChoiceBox> (param (b, "type"), "Band " + juce::String (b) + " type", types);
        type->setTooltip ("What this band does: Bell = boost/cut around a frequency, shelves = boost/cut everything above/below, cuts = filters out, Notch = narrow cut, Band-pass = keeps only a band.");
        typeBox = type.get(); add (std::move (type));
        auto on = std::make_unique<pvst::SegmentedToggle> (param (b, "on"), "Band", "Off", "On");
        on->setTooltip ("Switch this band on/off. All bands start off.");
        onBox = on.get(); add (std::move (on));
        auto kt = std::make_unique<pvst::ScrubBox> (param (b, "kt"), "Key track (%)", 3, 0, 5.0);
        kt->setTooltip ("0 % = the band stays at its frequency. 100 % = it follows the note you play (one octave up per octave), like a filter that tracks the keyboard. The band sits at its Freq on the base key.");
        ktBox = kt.get(); add (std::move (kt));
        auto fr = std::make_unique<pvst::ScrubBox> (param (b, "freq"), "Freq (Hz)", 5, 0, 10.0);
        fr->setTooltip ("Centre / corner frequency (at the base key when Key track is on).");
        freqBox = fr.get(); add (std::move (fr));
        auto gn = std::make_unique<pvst::ScrubBox> (param (b, "gain"), "Gain (dB)", 2, 1, 0.5);
        gn->setTooltip ("Boost or cut (bell and shelves only).");
        gainBox = gn.get(); add (std::move (gn));
        auto q = std::make_unique<pvst::ScrubBox> (param (b, "q"), "Q", 2, 2, 0.1);
        q->setTooltip ("Width: high Q = narrow and sharp (or a resonant peak on cuts), low Q = wide and gentle.");
        qBox = q.get(); add (std::move (q));
        auto st = std::make_unique<pvst::SegmentedChoice> (param (b, "steep"), "Steepness (dB/oct, cuts)", juce::StringArray { "12", "24", "36", "48" });
        st->setTooltip ("How steep a Low cut / High cut is: 12 dB per octave (gentle) up to 48 (brick-wall-ish). Other band types ignore it.");
        steepBox = st.get(); add (std::move (st));
        layoutSettings();
    }

    void layoutSettings()
    {
        if (typeBox == nullptr) return;
        int y = 32;
        typeBox->setBounds (0, y, kSettingsW, 48); y += 52;
        onBox->setBounds (0, y, 92, 48); ktBox->setBounds (104, y, 100, 48); y += 52;
        freqBox->setBounds (0, y, 104, 48); gainBox->setBounds (116, y, 88, 48); y += 52;
        qBox->setBounds (0, y, 90, 48); y += 52;
        steepBox->setBounds (0, y, kSettingsW, 48);
    }

    SeedProcessor& proc;
    int selected = 1, dragBand = -1, dropBand = -1;
    double refCents = 0.0, startFreq = 0, startGain = 0, startQ = 1;
    bool haveNote = false, startTypeUsesGain = true;
    juce::Point<float> startPos;
    std::vector<std::unique_ptr<juce::Component>> settings;
    pvst::ChoiceBox* typeBox = nullptr;
    pvst::SegmentedToggle* onBox = nullptr;
    pvst::ScrubBox *ktBox = nullptr, *freqBox = nullptr, *gainBox = nullptr, *qBox = nullptr;
    pvst::SegmentedChoice* steepBox = nullptr;
};
