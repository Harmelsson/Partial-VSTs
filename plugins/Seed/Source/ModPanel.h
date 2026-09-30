#pragma once
// The modulator panel (v0.6): a column on the right side of the window, 8 modulators top to bottom.
//
//   [ 1 ]  shape v   [Hz|BPM] rate / division        <- the numbered chip is a HANDLE: drag it onto any
//          depth %   live meter                          number box in the plugin to modulate that field
//          [Seed +100%] [FX 1 mix -40%] ...          <- one modulator can drive as many fields as you like;
//                                                       each link has its own amount (drag a chip up/down,
//                                                       wheel = fine, right-click or middle-click = remove)
//
// The links live in the processor (plugin state); this file only draws and edits them.
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

/// The draggable number: pull it onto a field to link the modulator to it.
class ModChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ModChip (int modNumber) : mod (modNumber)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip ("Drag this number onto any number box to modulate it. One modulator can drive as many fields as you like.");
    }
    void paint (juce::Graphics& g) override
    {
        namespace th = pvst::theme;
        auto b = getLocalBounds().toFloat();
        g.setColour (isMouseOverOrDragging() ? th::selected() : th::boxFill());
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
    void mouseUp (const juce::MouseEvent&) override { started = false; }

private:
    int mod;
    bool started = false;
};

/// One link: "Seed  +100%". Drag up/down (or wheel) = amount; right-click / middle-click = remove.
class LinkChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    LinkChip (SeedProcessor& p, SeedProcessor::ModLink l) : proc (p), link (std::move (l))
    {
        if (auto* prm = proc.apvts.getParameter (link.paramId)) name = prm->getName (22);
        setTooltip ("Drag up/down or scroll = how much this modulator moves this field (-100..+100 %). Right-click = remove the link.");
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
        if (e.mods.isPopupMenu() || e.mods.isMiddleButtonDown() || e.mods.isAltDown())
        {
            const int m = link.mod; const auto id = link.paramId;
            juce::MessageManager::callAsync ([&p = proc, m, id] { p.removeModLink (m, id); });
            return;
        }
        start = link.amount; startY = e.position.y;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float perPx = e.mods.isShiftDown() ? 0.1f : 1.0f;
        link.amount = juce::jlimit (-100.0f, 100.0f, start + (startY - e.position.y) * perPx);
        proc.setModLinkAmount (link.mod, link.paramId, link.amount);
        repaint();
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
};

class ModPanel : public juce::Component
{
public:
    explicit ModPanel (SeedProcessor& p) : proc (p), rows (*this)
    {
        namespace ids = seed::ids;
        for (int m = 1; m <= seed::kNumMods; ++m)
        {
            auto& r = row[(size_t) m - 1];
            r.chip = std::make_unique<ModChip> (m);
            r.shape = std::make_unique<pvst::ChoiceBox> (proc.param (ids::modShape (m)), "", seed::modShapeNames());
            r.shape->setCompact (true);
            r.shape->setTooltip ("Wave shape. S&H = a new random value each cycle, Smooth rnd = gliding random values, Drift = slow random walk.");
            r.rate = std::make_unique<pvst::RateControl> (proc.param (ids::modSync (m)), proc.param (ids::modRate (m)), proc.param (ids::modBeat (m)),
                                                          "", 2, 3, 0.1, "Hz", true);
            r.rate->setHelp ("Speed: Hz, or tempo-synced with the BPM switch (scroll the division; '-' straight, 't' triplet, '.' dotted).");
            r.depth = std::make_unique<pvst::ScrubBox> (proc.param (ids::modDepth (m)), "Depth (%)", 3, 3, 0.1);
            r.depth->setCompact (true);
            r.depth->setTooltip ("Master depth of this modulator in %. Each link then scales it again. Negative = inverted.");
            r.meter = std::make_unique<ModMeter> (proc.uiModValue[(size_t) m - 1]);
            for (juce::Component* c : std::initializer_list<juce::Component*> { r.chip.get(), r.shape.get(), r.rate.get(), r.depth.get(), r.meter.get() })
                rows.addAndMakeVisible (*c);
            ids_.add (ids::modShape (m)); ids_.add (ids::modRate (m)); ids_.add (ids::modDepth (m));
            ids_.add (ids::modSync (m)); ids_.add (ids::modBeat (m));
        }
        viewport.setViewedComponent (&rows, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        viewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, pvst::theme::mix (0.3f));
        viewport.getVerticalScrollBar().setColour (juce::ScrollBar::trackColourId, pvst::theme::mix (0.06f));
        addAndMakeVisible (viewport);
        for (auto* b : { &dice, &mutate, &clear }) addAndMakeVisible (*b);
        dice.onClick = [this] { proc.randomizeParams (ids_, 1.0); };
        mutate.onClick = [this] { proc.randomizeParams (ids_, 0.15); };
        clear.onClick = [this] { proc.clearModLinks(); };
        dice.setTooltip ("Randomise every modulator's shape, rate and depth (the links stay).");
        mutate.setTooltip ("Nudge every modulator's shape, rate and depth a little (the links stay).");
        clear.setTooltip ("Remove every link (all modulators stop moving anything).");
        rebuildLinks();
    }

    void tick()
    {
        if (seenVersion != proc.linksVersion()) rebuildLinks();
        for (auto& r : row) r.meter->tick();
    }

    void paint (juce::Graphics& g) override
    {
        namespace th = pvst::theme;
        auto inner = th::drawSection (g, getLocalBounds(), "Modulators");
        g.setColour (th::dim());
        g.setFont (th::sans (11.5f));
        g.drawText ("drag a number onto any field", getLocalBounds().reduced (10, 6).removeFromTop (18), juce::Justification::centredRight);
    }

    void resized() override
    {
        auto inner = getLocalBounds().reduced (9, 6).withTrimmedTop (22);
        auto buttons = inner.removeFromTop (26);
        dice.setBounds (buttons.removeFromLeft (70)); buttons.removeFromLeft (6);
        mutate.setBounds (buttons.removeFromLeft (70)); buttons.removeFromLeft (6);
        clear.setBounds (buttons.removeFromLeft (100));
        inner.removeFromTop (6);
        viewport.setBounds (inner);
        layoutRows();
    }

private:
    struct Row
    {
        std::unique_ptr<ModChip> chip;
        std::unique_ptr<pvst::ChoiceBox> shape;
        std::unique_ptr<pvst::RateControl> rate;
        std::unique_ptr<pvst::ScrubBox> depth;
        std::unique_ptr<ModMeter> meter;
        int top = 0, height = 0;
    };

    /// The scrolled content (draws the row frames).
    struct Rows : juce::Component
    {
        explicit Rows (ModPanel& o) : owner (o) {}
        void paint (juce::Graphics& g) override
        {
            namespace th = pvst::theme;
            for (size_t m = 0; m < owner.row.size(); ++m)
            {
                const auto& r = owner.row[m];
                g.setColour (th::frame());
                g.drawRect (0, r.top, getWidth(), r.height, 1);
                g.setColour (th::dim());
                g.setFont (th::sans (12.0f));
                g.drawText ("depth", 8, r.top + 38, 44, 28, juce::Justification::centredLeft);
            }
        }
        ModPanel& owner;
    };

    void rebuildLinks()
    {
        seenVersion = proc.linksVersion();
        linkChips.clear();
        chipMod.clear();
        const auto links = proc.modLinks();
        for (auto& l : links)
        {
            auto c = std::make_unique<LinkChip> (proc, l);
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
            r.shape->setBounds (48, y + 8, 100, 28);
            r.rate->setBounds (154, y + 8, w - 154 - 8, 28);
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
    Rows rows;
    juce::Viewport viewport;
    std::array<Row, seed::kNumMods> row;
    std::vector<std::unique_ptr<LinkChip>> linkChips;
    std::vector<int> chipMod;
    juce::StringArray ids_;
    pvst::FlatButton dice { "Dice" }, mutate { "Mutate" }, clear { "Clear links" };
    uint32_t seenVersion = 0xffffffffu;
};
