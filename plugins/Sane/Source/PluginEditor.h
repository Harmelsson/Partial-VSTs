#pragma once
// Sane's window (mockup 2, approved 2026-10-17). One aligned grid, every border runs through:
//
//   header:  SANE · version/build · keys (Tuned | Kit v0.2) · link (v0.2) · window scale
//   row 1:   EXCITER          | LOOP (ring + 7 slots + core)  | MODES
//   row 2:   STRIKE           | SLOT EDITOR                    | HIT
//   bottom:  keyboard strip (lights on press, fades on release)
#include "PluginProcessor.h"
#include "SaneViews.h"

class SaneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SaneEditor (SaneProcessor& p);
    ~SaneEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    static constexpr int kW = 1280, kH = 880;

private:
    void timerCallback() override;
    void setUiScale (float s);
    void initPatch();
    void updateTimeKnob();

    SaneProcessor& proc;
    sane::look::LookAndFeel lnf;

    // header
    sane::ui::Button initBtn, kitBtn, tunedBtn, linkBtn, scaleDown, scaleUp;
    juce::TooltipWindow tooltips { this, 450 };
    // exciter
    sane::ui::Button noiseBtn, audioBtn;
    sane::ui::ExciterView exciter;
    std::vector<std::unique_ptr<sane::ui::Knob>> excKnobs, loopKnobs, modeKnobs;
    std::vector<std::unique_ptr<sane::ui::Bar>> excBars, strikeBars;
    // loop
    sane::ui::Chips coreChips, timeChips;
    std::unique_ptr<sane::ui::Knob> timeKnob, keyTrackKnob;
    int shownTimeMode = -1;
    sane::ui::LoopRing ring;
    // modes
    sane::ui::Chips tableChips, srcAChips, srcBChips;
    sane::ui::ModesView modes;
    sane::ui::Bar morphBar;
    // row 2
    sane::ui::StrikePad strike;
    sane::ui::SlotEditor slotEditor;
    sane::ui::HitView hit;
    sane::ui::KeyStrip keys;

    // grid geometry (filled in resized)
    juce::Rectangle<int> cExc, cLoop, cModes, cStrike, cSlot, cHit, cKeys;
    float uiScale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaneEditor)
};
