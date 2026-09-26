#pragma once
// Seed's window (v0.3 playground), ArtHar's dark style:
//   header      name, PRESETS (< name > Save, load scope All/Tuning/Sound), version
//   CALCULATOR  fundamental, seed, partials, rolloff, method + its settings, Reset edits,
//               Dice tuning; curve / partials plots (editable) and the scale table (on/off)
//   SYNTH       tabs: Keys & pitch | Envelope | Timbre | Filter & noise | FX | Modulators,
//               plus Mutate / Dice sound
// The editor only draws and edits parameters; the calculator itself lives in the processor.
#include "FxRackView.h"
#include "PluginProcessor.h"
#include "ui/CalculatorViews.h"
#include "ui/Controls.h"
#include "ui/EnvelopeView.h"

/// A tab page: controls laid out left-to-right in rows (flow layout).
class ControlPage : public juce::Component
{
public:
    struct Item { juce::Component* c; int width; int height; bool newLine; };
    void add (juce::Component& c, int width, int height = 48) { addAndMakeVisible (c); items.push_back ({ &c, width, height, false }); }
    /// One component filling the whole page (e.g. the FX rack).
    void fill (juce::Component& c) { addAndMakeVisible (c); filler = &c; }
    void newLine() { items.push_back ({ nullptr, 0, 0, true }); }
    void resized() override
    {
        if (filler != nullptr) { filler->setBounds (getLocalBounds()); return; }
        int x = 10, y = 8, rowH = 0;
        for (auto& it : items)
        {
            if (it.newLine || (it.c && x + it.width > getWidth() - 10)) { x = 10; y += rowH + 10; rowH = 0; if (it.newLine) continue; }
            it.c->setBounds (x, y, it.width, it.height);
            x += it.width + 16;
            rowH = juce::jmax (rowH, it.height);
        }
    }
private:
    std::vector<Item> items;
    juce::Component* filler = nullptr;
};

/// Small live meter showing a modulator's current output.
class ModMeter : public juce::Component
{
public:
    explicit ModMeter (std::atomic<float>& v) : value (v) {}
    void tick() { const float v = value.load(); if (! juce::exactlyEqual (v, shown)) { shown = v; repaint(); } }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().withTrimmedTop (19).withHeight (28).toFloat();
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

class SeedEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SeedEditor (SeedProcessor&);
    ~SeedEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool hasCalculatorResult() const { return proc.hasCalculatorResult(); }
    void showTab (int index);

private:
    void timerCallback() override;
    juce::RangedAudioParameter& p (const juce::String& id) { return proc.param (id); }
    void setParam (const juce::String& id, double value);
    void wireCalculatorEditing();
    void buildPages();
    void refreshPresetLabel();
    void layoutCalculatorRow();
    void savePresetFlow();

    SeedProcessor& proc;
    double lastTick = 0.0, busySince = 0.0;

    // Header.
    pvst::FlatButton prevPreset { "<" }, presetName { "(no preset)" }, nextPreset { ">" }, savePreset { "Save as..." }, scopeBtn { "Load: All" };
    juce::TextEditor presetNameEditor;
    SeedProcessor::Scope loadScope = SeedProcessor::Scope::All;

    // Calculator.
    pvst::ScrubBox fundamentalBox, seedBox, partialsBox, rolloffBox, minDipBox, toleranceBox, minSharedBox;
    pvst::SegmentedChoice methodSeg;
    pvst::FlatButton resetEdits { "Reset edits" }, diceTuning { "Dice tuning" };
    pvst::CurvePlot curvePlot;
    pvst::PartialsPlot partialsPlot;
    pvst::ScaleTable scaleTable;
    pvst::PartialsTable partialsTable;
    std::vector<float> emphasis; // per FinalScale note: lit while played (ArtHar style)

    // Synth.
    static constexpr int kTabs = 6;
    std::array<std::unique_ptr<pvst::FlatButton>, kTabs> tabButtons;
    std::array<ControlPage, kTabs> pages;
    int currentTab = 0;
    // Per page (user v0.3.1: "Dice settings on each page that only affect the page they are on").
    pvst::FlatButton dicePage { "Dice page" }, mutatePage { "Mutate page" }, resetPage { "Reset page" }, initAll { "Init" };
    std::array<juce::StringArray, kTabs> pageIds; // which parameters live on each page
    int shownMethod = -1;
    std::vector<std::unique_ptr<juce::Component>> owned; // controls living on the pages
    std::vector<ModMeter*> meters;
    pvst::EnvelopeView* envelopeView = nullptr;
    FxRackView* fxRack = nullptr;

    juce::Rectangle<int> calcArea, synthArea;
    juce::String info;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeedEditor)
};
