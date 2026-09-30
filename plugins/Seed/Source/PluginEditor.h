#pragma once
// Seed's window, ArtHar's dark style (v0.6 layout):
//   header      name, PRESETS (< name > Save, load scope All/Tuning/Sound), version
//   CALCULATOR  fundamental, seed, partials, rolloff, method + its settings, Lock scale, Reset edits,
//               Dice tuning; curve / partials plots (editable) and the scale table (on/off)
//   SYNTH       tabs: Keys & pitch | Envelope | Timbre | FX (rack + 5-band EQ), plus Dice / Mutate / Reset page
//   MODULATORS  a column on the right: 8 modulators, dragged onto fields to link them
// The editor only draws and edits parameters; the calculator itself lives in the processor.
#include "EqView.h"
#include "FxRackView.h"
#include "Help.h"
#include "ModPanel.h"
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

/// The FX tab: the rack (list + slot settings) on the left, the 5-band EQ on the right.
class FxPage : public juce::Component
{
public:
    explicit FxPage (SeedProcessor& p) : rack (p), eq (p) { addAndMakeVisible (rack); addAndMakeVisible (eq); }
    void resized() override
    {
        auto b = getLocalBounds().reduced (4, 4);
        eq.setBounds (b.removeFromRight (548));
        b.removeFromRight (12);
        rack.setBounds (b);
    }
    FxRackView rack;
    EqView eq;
};

class SeedEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, public pvst::ModDropHandler, private juce::Timer
{
public:
    explicit SeedEditor (SeedProcessor&);
    ~SeedEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool hasCalculatorResult() const { return proc.hasCalculatorResult(); }
    void showTab (int index);

    // pvst::ModDropHandler: a modulator chip dropped on a number box links it.
    void modDroppedOn (juce::RangedAudioParameter& prm, int mod) override;
    unsigned modMaskFor (const juce::RangedAudioParameter& prm) override { return proc.modMaskFor (prm.getParameterID()); }
    float modOffsetNorm (const juce::RangedAudioParameter& prm) override { return proc.modOffsetNorm (prm); }
    void modRemoveFrom (juce::RangedAudioParameter& prm, int mod) override { proc.removeModLink (mod, prm.getParameterID()); }

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
    pvst::ScrubBox fundamentalBox, seedBox, partialsBox, rolloffBox, minDipBox, toleranceBox, minSharedBox, edoStepsBox;
    pvst::ChoiceBox methodBox;
    pvst::FlatButton resetEdits { "Reset edits" }, diceTuning { "Dice tuning" }, lockScale { "Lock scale" };
    void onLockClicked();
    // Scala (v0.5): import a scale / export what the keyboard plays / fit the seed to the scale.
    pvst::FlatButton importScl { "Import .scl" }, exportScl { "Export .scl" }, fitSeed { "Fit seed" };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String scalaMessage; // shown in the info line for a few seconds after an action
    double scalaMessageUntil = 0.0;
    void showScalaMessage (const juce::String& m);
    pvst::CurvePlot curvePlot;
    pvst::PartialsPlot partialsPlot;
    pvst::ScaleTable scaleTable;
    pvst::PartialsTable partialsTable;
    std::vector<float> emphasis; // per FinalScale note: lit while played (ArtHar style)
    uint32_t hitsSeen = 0;                 // how many note-on events from the processor's ring were handled
    std::vector<double> litUntil;          // per FinalScale note: a struck note stays lit at least this long (ms)

    // Synth.
    static constexpr int kTabs = 4;
    static constexpr int kMainW = 1240, kModW = 340;
    std::array<std::unique_ptr<pvst::FlatButton>, kTabs> tabButtons;
    std::array<ControlPage, kTabs> pages;
    int currentTab = 0;
    // Per page (user v0.3.1: "Dice settings on each page that only affect the page they are on").
    pvst::FlatButton dicePage { "Dice page" }, mutatePage { "Mutate page" }, resetPage { "Reset page" }, initAll { "Init" };
    std::array<juce::StringArray, kTabs> pageIds; // which parameters live on each page
    int shownMethod = -1;
    std::vector<std::unique_ptr<juce::Component>> owned; // controls living on the pages
    pvst::EnvelopeView* envelopeView = nullptr;
    FxPage* fxPage = nullptr;
    ModPanel modPanel;
    juce::TooltipWindow tooltipWindow { this, 600 };

    juce::Rectangle<int> calcArea, synthArea;
    juce::String info;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeedEditor)
};
