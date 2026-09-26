#pragma once
// Seed's window, in ArtHar's dark style (user direction, 2026-09-26):
//   top     CALCULATOR bar: Fundamental, Seed, Partials, Rolloff, Min dip strength + info line
//   middle  DISSONANCE CURVE and PARTIALS plots (left), SCALE table (right)
//   bottom  ENVELOPE (draggable ADSR + boxes, velocity) and VOICE & OUTPUT
// The calculator runs on a background thread (xen::CalculatorWorker) whenever its inputs change.
#include "PluginProcessor.h"
#include "ui/CalculatorViews.h"
#include "ui/Controls.h"
#include "ui/EnvelopeView.h"

class SeedEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SeedEditor (SeedProcessor&);
    ~SeedEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    /// For tests/snapshots: wait until the calculator has produced a result.
    bool hasCalculatorResult() const { return seenId > 0; }

private:
    void timerCallback() override;
    juce::RangedAudioParameter& p (const char* id) { return *proc.apvts.getParameter (id); }
    xen::CalculatorSettings currentSettings() const;

    SeedProcessor& proc;
    xen::CalculatorWorker worker;
    xen::CalculatorOutput latest;
    uint64_t seenId = 0;
    double lastTick = 0.0, busySince = 0.0;

    pvst::ScrubBox fundamentalBox, seedBox, partialsBox, rolloffBox, minDipBox;
    pvst::CurvePlot curvePlot;
    pvst::PartialsPlot partialsPlot;
    pvst::ScaleTable scaleTable;
    pvst::EnvelopeView envelope;
    pvst::ScrubBox attackBox, decayBox, sustainBox, releaseBox, velocityBox;
    pvst::ScrubBox gainBox, droneNoteBox;
    pvst::SegmentedToggle keysToggle, droneToggle;

    juce::Rectangle<int> calcArea, envArea, voiceArea;
    juce::String info;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeedEditor)
};
