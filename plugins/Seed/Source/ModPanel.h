#pragma once
// The modulator panel (v0.7): a column on the right side of the window.
//
//   top      the SELECTED modulator's editor: shape, presets, grid, the drawn LFO curve (LfoView.h),
//            trigger (Never / MIDI / Audio with source + threshold) and Loop / One-shot
//   below    all 8 modulators top to bottom: [ 1 ] shape picture · Hz-or-BPM rate · depth · live meter,
//            and under each the fields it drives
//
//   - drag the numbered chip onto ANY control (number boxes, switches, choices, plots, tables...) to link it
//   - one modulator can drive as many controls as you like; each link has its own amount
//     (drag a link chip up/down; wheel = fine; right-click / middle-click = remove)
//   - CLICK a link chip: the plugin jumps to that control (opens its tab) and lights it up
//   - click a modulator row (or its number) to edit its curve above
// The links live in the processor (plugin state); this file only draws and edits them.
#include "LfoView.h"
#include "PluginProcessor.h"
#include "ui/Controls.h"

/// Small live meter showing a modulator's current output.
class ModMeter : public juce::Component
{
public:
    explicit ModMeter (std::atomic<float>& v) : value (v) {}
    void tick() { const float v = value.load(); if (! juce::exactlyEqual (v, shown)) { shown = v; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (pvst::theme::boxFill()); g.fillRect (b);
        g.setColour (pvst::theme::frame()); g.drawRect (b, 1.0f);
        const float cx = b.getCentreX(), w = b.getWidth() * 0.5f * shown;
        g.setColour (pvst::theme::mix (0.6f));
        g.fillRect (juce::Rectangle<float> (juce::jmin (cx, cx + w), b.getY() + 4, std::abs (w), b.getHeight() - 8));
        g.setColour (pvst::theme::frame()); g.drawVerticalLine ((int) cx, b.getY(), b.getBottom());
    }
private:
    std::atomic<float>& value;
    float shown = 0.0f;
};

/// The draggable number: pull it onto a control to link the modulator to it. A plain click selects the modulator.
class ModChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onClick;
    explicit ModChip (int modNumber) : mod (modNumber)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip ("Drag this number onto any control to modulate it (numbers, switches, choices, plots, tables...). One modulator can drive as many as you like. Click = edit this modulator.");
    }
    void setSelected (bool s) { if (s != selected) { selected = s; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        namespace th = pvst::theme;
        auto b = getLocalBounds().toFloat();
        g.setColour (selected ? th::modified.withAlpha (0.3f) : isMouseOverOrDragging() ? th::selected() : th::boxFill());
        g.fillRect (b);
        g.setColour (th::modified);
        g.drawRect (b, 1.5f);
        g.setFont (th::mono (16.0f));
        g.drawText (juce::String (mod), getLocalBounds(), juce::Justification::centred);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (started || e.getDistanceFromDragStart() < 4) return;
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            juce::Image img (juce::Image::ARGB, 34, 28, true);
            {
                juce::Graphics g (img);
                g.setColour (pvst::theme::bg.withAlpha (0.9f));
                g.fillRect (img.getBounds());
                g.setColour (pvst::theme::modified);
                g.drawRect (img.getBounds(), 2);
                g.setFont (pvst::theme::mono (16.0f));
                g.drawText (juce::String (mod), img.getBounds(), juce::Justification::centred);
            }
            c->startDragging ("mod:" + juce::String (mod), this, juce::ScaledImage (img));
            started = true;
        }
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! started && ! e.mouseWasDraggedSinceMouseDown() && onClick) onClick();
        started = false;
    }

private:
    int mod;
    bool started = false, selected = false;
};

/// One link: "Seed  +100%". Drag up/down (or wheel) = amount; right-click / middle-click = remove;
/// a plain click jumps to the control it drives.
class LinkChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void (const juce::String&)> onLocate;
    LinkChip (SeedProcessor& p, SeedProcessor::ModLink l) : proc (p), link (std::move (l))
    {
        if (auto* prm = proc.apvts.getParameter (link.paramId)) name = prm->getName (22);
        setTooltip ("Click = show the control this drives (opens its tab and lights it up). Drag up/down or scroll = how much (-100..+100 %). Right-click = remove the link.");
    }
    void paint (juce::Graphics& g) override
    {
        namespace th = pvst::theme;
        auto b = getLocalBounds().toFloat();
        g.setColour (isMouseOverOrDragging() ? th::selected() : th::boxFill());
        g.fillRect (b);
        g.setColour (th::frame());
        g.drawRect (b, 1.0f);
        const float frac = link.amount / 100.0f;
        g.setColour (th::modified.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (b.getCentreX() + juce::jmin (0.0f, frac) * b.getWidth() * 0.5f, b.getBottom() - 3.0f,
                                            std::abs (frac) * b.getWidth() * 0.5f, 2.0f));
        g.setColour (th::text());
        g.setFont (th::sans (11.5f));
        g.drawFittedText (name, getLocalBounds().reduced (5, 0).withTrimmedRight (50), juce::Justification::centredLeft, 1, 0.75f);
        g.setFont (th::mono (11.5f));
        g.setColour (th::modified);
        g.drawText ((link.amount >= 0 ? "+" : "") + juce::String (link.amount, 1) + "%", getLocalBounds().reduced (5, 0), juce::Justification::centredRight);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        ignoreUp = false;
        if (e.mods.isPopupMenu() || e.mods.isMiddleButtonDown() || e.mods.isAltDown())
        {
            ignoreUp = true;
            const int m = link.mod; const auto id = link.paramId;
            juce::MessageManager::callAsync ([&p = proc, m, id] { p.removeModLink (m, id); });
            return;
        }
        start = link.amount; startY = e.position.y;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() < 3) return;
        const float perPx = e.mods.isShiftDown() ? 0.1f : 1.0f;
        link.amount = juce::jlimit (-100.0f, 100.0f, start + (startY - e.position.y) * perPx);
        proc.setModLinkAmount (link.mod, link.paramId, link.amount);
        repaint();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! ignoreUp && ! e.mouseWasDraggedSinceMouseDown() && onLocate) onLocate (link.paramId);
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        link.amount = juce::jlimit (-100.0f, 100.0f, link.amount + (w.deltaY > 0 ? 1.0f : -1.0f) * (e.mods.isShiftDown() ? 1.0f : 5.0f));
        proc.setModLinkAmount (link.mod, link.paramId, link.amount);
        repaint();
    }

private:
    SeedProcessor& proc;
    SeedProcessor::ModLink link;
    juce::String name;
    float start = 0.0f, startY = 0.0f;
    bool ignoreUp = false;
};

class ModPanel : public juce::Component
{
public:
    /// `locate` is called with a parameter ID when a link chip is clicked.
    ModPanel (SeedProcessor& p, std::function<void (const juce::String&)> locateFn) : proc (p), locate (std::move (locateFn)), rows (*this), lfo (p)
    {
        namespace ids = seed::ids;
        for (int m = 1; m <= seed::kNumMods; ++m)
        {
            auto& r = row[(size_t) m - 1];
            r.chip = std::make_unique<ModChip> (m);
            r.chip->onClick = [this, m] { select (m); };
            r.thumb = std::make_unique<ShapeThumb> (proc, m);
            r.rate = std::make_unique<pvst::RateControl> (proc.param (ids::modSync (m)), proc.param (ids::modRate (m)), proc.param (ids::modBeat (m)),
                                                          "", 2, 3, 0.1, "Hz", true);
            r.rate->setHelp ("Speed: Hz, or tempo-synced with the BPM switch (scroll the division; '-' straight, 't' triplet, '.' dotted).");
            r.depth = std::make_unique<pvst::ScrubBox> (proc.param (ids::modDepth (m)), "Depth (%)", 3, 3, 0.1);
            r.depth->setCompact (true);
            r.depth->setTooltip ("Master depth of this modulator in %. Each link then scales it again. Negative = inverted.");
            r.meter = std::make_unique<ModMeter> (proc.uiModValue[(size_t) m - 1]);
            for (juce::Component* c : std::initializer_list<juce::Component*> { r.chip.get(), r.thumb.get(), r.rate.get(), r.depth.get(), r.meter.get() })
                rows.addAndMakeVisible (*c);

            // The editor block's per-modulator controls (only the selected modulator's are visible).
            auto& e = ed[(size_t) m - 1];
            e.shape = std::make_unique<pvst::ChoiceBox> (proc.param (ids::modShape (m)), "", seed::modShapeNames());
            e.shape->setCompact (true);
            e.shape->setTooltip ("Wave shape. Drawn = your own curve (edit it below). S&H = a new random value each cycle, Smooth rnd = gliding random values, Drift = slow random walk.");
            e.trig = std::make_unique<pvst::SegmentedChoice> (proc.param (ids::modTrig (m)), "", seed::trigNames());
            e.trig->setCompact (true);
            e.trig->setTooltip ("When the LFO restarts. Never = free-running. MIDI = every note-on. Audio = when the level of the chosen source rises through the threshold.");
            e.loop = std::make_unique<pvst::SegmentedToggle> (proc.param (ids::modLoop (m)), "", "One-shot", "Loop");
            e.loop->setCompact (true);
            e.loop->setTooltip ("Loop = the cycle repeats. One-shot = it plays one cycle after each trigger, then holds its last value.");
            e.src = std::make_unique<pvst::ChoiceBox> (proc.param (ids::modSrc (m)), "", seed::trigSourceNames());
            e.src->setCompact (true);
            e.src->setTooltip ("Which signal the audio trigger listens to. Input / Sidechain only carry audio if your host connects them to the plugin; Output = Seed's own sound.");
            e.thr = std::make_unique<pvst::ScrubBox> (proc.param (ids::modThr (m)), "Threshold (dB)", 3, 1, 1.0);
            e.thr->setCompact (true);
            e.thr->setTooltip ("Audio trigger level in dB. The LFO restarts each time the source rises through it (it re-arms 6 dB lower).");
            for (juce::Component* c : std::initializer_list<juce::Component*> { e.shape.get(), e.trig.get(), e.loop.get(), e.src.get(), e.thr.get() })
                addChildComponent (*c);
            ids_.add (ids::modShape (m)); ids_.add (ids::modRate (m)); ids_.add (ids::modDepth (m));
            ids_.add (ids::modSync (m)); ids_.add (ids::modBeat (m)); ids_.add (ids::modTrig (m));
            ids_.add (ids::modThr (m)); ids_.add (ids::modSrc (m)); ids_.add (ids::modLoop (m));
        }
        addAndMakeVisible (lfo);
        viewport.setViewedComponent (&rows, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        viewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, pvst::theme::mix (0.3f));
        viewport.getVerticalScrollBar().setColour (juce::ScrollBar::trackColourId, pvst::theme::mix (0.06f));
        addAndMakeVisible (viewport);
        for (auto* b : { &dice, &mutate, &clear, &presets, &gridXBtn, &gridYBtn }) addAndMakeVisible (*b);
        dice.onClick = [this] { proc.randomizeParams (ids_, 1.0); };
        mutate.onClick = [this] { proc.randomizeParams (ids_, 0.15); };
        clear.onClick = [this] { proc.clearModLinks(); };
        dice.setTooltip ("Randomise every modulator's shape, rate, depth and trigger settings (the links and drawn curves stay).");
        mutate.setTooltip ("Nudge every modulator's settings a little (the links and drawn curves stay).");
        clear.setTooltip ("Remove every link (all modulators stop moving anything).");
        presets.setTooltip ("Replace the curve with a preset shape, or invert / reverse it.");
        gridXBtn.setTooltip ("Grid: how many vertical divisions the cycle has (Shift+drag snaps to them).");
        gridYBtn.setTooltip ("Grid: how many horizontal divisions between -1 and +1 (Shift+drag snaps to them).");
        presets.onClick = [this] { showPresets(); };
        gridXBtn.onClick = [this] { pickGrid (true); };
        gridYBtn.onClick = [this] { pickGrid (false); };
        select (1);
        rebuildLinks();
    }

    void tick()
    {
        if (seenVersion != proc.linksVersion()) rebuildLinks();
        for (auto& r : row) { r.meter->tick(); r.thumb->tick(); }
        lfo.tick();
        // The audio-trigger line only matters when the trigger is Audio.
        auto& tp = proc.param (seed::ids::modTrig (selected));
        const int trig = (int) std::lround (tp.convertFrom0to1 (tp.getValue()));
        if (trig != shownTrig) { shownTrig = trig; layoutEditor(); }
    }

    void select (int m)
    {
        selected = juce::jlimit (1, seed::kNumMods, m);
        lfo.setModulator (selected);
        for (int i = 1; i <= seed::kNumMods; ++i) row[(size_t) i - 1].chip->setSelected (i == selected);
        shownTrig = -1;
        layoutEditor();
        rows.repaint();
        repaint();
    }
    int selectedModulator() const { return selected; }

    void paint (juce::Graphics& g) override
    {
        namespace th = pvst::theme;
        th::drawSection (g, getLocalBounds(), "Modulators");
        g.setColour (th::dim());
        g.setFont (th::sans (11.5f));
        g.drawText ("drag a number onto any control", getLocalBounds().reduced (10, 6).removeFromTop (18), juce::Justification::centredRight);
        // Editor block.
        g.setColour (th::text());
        g.setFont (th::sans (11.5f).withExtraKerningFactor (0.2f));
        g.drawText ("MODULATOR " + juce::String (selected), editorTitle, juce::Justification::centredLeft);
        if (shownTrig == 2)
        {
            g.setColour (th::dim()); g.setFont (th::sans (11.0f));
            g.drawText ("source", srcLabel, juce::Justification::centredLeft);
        }
        g.setColour (th::frame());
        g.drawHorizontalLine (editorBottom, 9.0f, (float) getWidth() - 9.0f);
    }

    void resized() override
    {
        auto inner = getLocalBounds().reduced (9, 6).withTrimmedTop (22);
        auto buttons = inner.removeFromTop (26);
        dice.setBounds (buttons.removeFromLeft (70)); buttons.removeFromLeft (6);
        mutate.setBounds (buttons.removeFromLeft (70)); buttons.removeFromLeft (6);
        clear.setBounds (buttons.removeFromLeft (100));
        inner.removeFromTop (8);
        editorArea = inner.removeFromTop (kEditorH);
        editorBottom = editorArea.getBottom() + 3;
        inner.removeFromTop (8);
        viewport.setBounds (inner);
        layoutEditor();
        layoutRows();
    }

private:
    static constexpr int kEditorH = 262;

    struct Row
    {
        std::unique_ptr<ModChip> chip;
        std::unique_ptr<ShapeThumb> thumb;
        std::unique_ptr<pvst::RateControl> rate;
        std::unique_ptr<pvst::ScrubBox> depth;
        std::unique_ptr<ModMeter> meter;
        int top = 0, height = 0;
    };
    struct Ed
    {
        std::unique_ptr<pvst::ChoiceBox> shape, src;
        std::unique_ptr<pvst::SegmentedChoice> trig;
        std::unique_ptr<pvst::SegmentedToggle> loop;
        std::unique_ptr<pvst::ScrubBox> thr;
    };

    /// The scrolled content (draws the row frames; a click on a row selects that modulator).
    struct Rows : juce::Component
    {
        explicit Rows (ModPanel& o) : owner (o) {}
        void paint (juce::Graphics& g) override
        {
            namespace th = pvst::theme;
            for (size_t m = 0; m < owner.row.size(); ++m)
            {
                const auto& r = owner.row[m];
                g.setColour ((int) m + 1 == owner.selected ? th::modified : th::frame());
                g.drawRect (0, r.top, getWidth(), r.height, (int) m + 1 == owner.selected ? 2 : 1);
                g.setColour (th::dim());
                g.setFont (th::sans (12.0f));
                g.drawText ("depth", 8, r.top + 38, 44, 28, juce::Justification::centredLeft);
            }
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            for (size_t m = 0; m < owner.row.size(); ++m)
                if (e.y >= owner.row[m].top && e.y < owner.row[m].top + owner.row[m].height) { owner.select ((int) m + 1); return; }
        }
        ModPanel& owner;
    };

    void layoutEditor()
    {
        for (int m = 1; m <= seed::kNumMods; ++m)
        {
            auto& e = ed[(size_t) m - 1];
            const bool sel = m == selected;
            e.shape->setVisible (sel); e.trig->setVisible (sel); e.loop->setVisible (sel);
            e.src->setVisible (sel && shownTrig == 2); e.thr->setVisible (sel && shownTrig == 2);
        }
        auto a = editorArea;
        editorTitle = a.removeFromTop (16);
        a.removeFromTop (4);
        auto top = a.removeFromTop (28);
        auto& e = ed[(size_t) selected - 1];
        e.shape->setBounds (top.removeFromLeft (100)); top.removeFromLeft (6);
        presets.setBounds (top.removeFromLeft (84)); top.removeFromLeft (6);
        gridXBtn.setBounds (top.removeFromLeft (62)); top.removeFromLeft (6);
        gridYBtn.setBounds (top.removeFromLeft (62));
        gridXBtn.setText ("X  " + juce::String (lfo.gridXValue()));
        gridYBtn.setText ("Y  " + juce::String (lfo.gridYValue()));
        a.removeFromTop (6);
        lfo.setBounds (a.removeFromTop (140));
        a.removeFromTop (6);
        auto t1 = a.removeFromTop (28);
        e.trig->setBounds (t1.removeFromLeft (190)); t1.removeFromLeft (8);
        e.loop->setBounds (t1.removeFromLeft (120));
        a.removeFromTop (6);
        auto t2 = a.removeFromTop (28);
        srcLabel = t2.removeFromLeft (46);
        e.src->setBounds (t2.removeFromLeft (100)); t2.removeFromLeft (8);
        e.thr->setBounds (t2.removeFromLeft (110));
        repaint();
    }

    void showPresets()
    {
        juce::PopupMenu m;
        const char* names[] = { "Sine", "Triangle", "Saw up", "Saw down", "Square", "Pulse 25 %", "Ramp (eased)", "Steps", "Bounce" };
        for (int i = 0; i < 9; ++i) m.addItem (i + 1, names[i]);
        m.addSeparator();
        m.addItem (20, "Invert (flip up/down)");
        m.addItem (21, "Reverse (flip left/right)");
        juce::Component::SafePointer<ModPanel> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presets), [safe] (int r) {
            if (safe == nullptr || r <= 0) return;
            if (r == 20) safe->lfo.invert();
            else if (r == 21) safe->lfo.reverse();
            else safe->proc.applyLfoPreset (safe->selected, r - 1);
        });
    }

    void pickGrid (bool xAxis)
    {
        juce::PopupMenu m;
        for (int v : { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32 }) m.addItem (v, juce::String (v) + (xAxis ? " divisions across" : " divisions up/down"), true, v == (xAxis ? lfo.gridXValue() : lfo.gridYValue()));
        juce::Component::SafePointer<ModPanel> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (xAxis ? &gridXBtn : &gridYBtn), [safe, xAxis] (int r) {
            if (safe == nullptr || r <= 0) return;
            if (xAxis) safe->lfo.setGrid (r, safe->lfo.gridYValue()); else safe->lfo.setGrid (safe->lfo.gridXValue(), r);
            safe->layoutEditor();
        });
    }

    void rebuildLinks()
    {
        seenVersion = proc.linksVersion();
        linkChips.clear();
        chipMod.clear();
        const auto links = proc.modLinks();
        for (auto& l : links)
        {
            auto c = std::make_unique<LinkChip> (proc, l);
            c->onLocate = [this] (const juce::String& id) { if (locate) locate (id); };
            rows.addAndMakeVisible (*c);
            chipMod.push_back (l.mod);
            linkChips.push_back (std::move (c));
        }
        layoutRows();
    }

    void layoutRows()
    {
        const int w = juce::jmax (100, viewport.getWidth() - viewport.getScrollBarThickness() - 2);
        int y = 0;
        for (int m = 1; m <= seed::kNumMods; ++m)
        {
            auto& r = row[(size_t) m - 1];
            r.top = y;
            r.chip->setBounds (8, y + 8, 34, 28);
            r.thumb->setBounds (48, y + 8, 76, 28);
            r.rate->setBounds (130, y + 8, w - 130 - 8, 28);
            r.depth->setBounds (54, y + 38, 110, 28);
            r.meter->setBounds (172, y + 38, w - 172 - 8, 28);
            int cy = y + 72, col = 0;
            const int chipW = (w - 8 * 2 - 6) / 2;
            for (size_t i = 0; i < linkChips.size(); ++i)
                if (chipMod[i] == m)
                {
                    linkChips[i]->setBounds (8 + col * (chipW + 6), cy, chipW, 22);
                    if (++col == 2) { col = 0; cy += 26; }
                }
            if (col != 0) cy += 26;
            r.height = cy - y + 4;
            y += r.height + 6;
        }
        rows.setSize (w, y);
        rows.repaint();
    }

    SeedProcessor& proc;
    std::function<void (const juce::String&)> locate;
    Rows rows;
    juce::Viewport viewport;
    LfoEditor lfo;
    std::array<Row, seed::kNumMods> row;
    std::array<Ed, seed::kNumMods> ed;
    std::vector<std::unique_ptr<LinkChip>> linkChips;
    std::vector<int> chipMod;
    juce::StringArray ids_;
    pvst::FlatButton dice { "Dice" }, mutate { "Mutate" }, clear { "Clear links" }, presets { "Presets" }, gridXBtn { "X 8" }, gridYBtn { "Y 4" };
    juce::Rectangle<int> editorArea, editorTitle, srcLabel;
    int editorBottom = 0, selected = 1, shownTrig = -1;
    uint32_t seenVersion = 0xffffffffu;
};
