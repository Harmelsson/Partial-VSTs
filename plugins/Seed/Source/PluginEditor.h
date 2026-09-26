#pragma once
#include "PluginProcessor.h"
#include "ui/SmoothKnob.h"

/// Animated picture of the current partial series: one soft line per partial on a pitch
/// (cents) axis, height = loudness. Faint vertical guides mark the seed's "octaves", so with
/// seed 2 you can see partials 1, 2, 4, 8, 16 sit exactly on them.
class PartialView : public juce::Component
{
public:
    explicit PartialView (juce::AudioProcessorValueTreeState& s) : state (s) {}
    void tick();
    void paint (juce::Graphics&) override;

private:
    juce::AudioProcessorValueTreeState& state;
    std::array<float, seed::kMaxPartials> cents {}, height {};
    float viewMin = 0.0f, viewMax = 7200.0f, period = 1200.0f;
    bool first = true;
};

class SeedEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SeedEditor (SeedProcessor&);
    ~SeedEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::RangedAudioParameter& p (const char* id) { return *proc.apvts.getParameter (id); }

    SeedProcessor& proc;
    PartialView partialView;
    pvst::SmoothKnob seedKnob, partialsKnob, rolloffKnob, attackKnob, releaseKnob, gainKnob, droneNoteKnob;
    pvst::SmoothToggle keysToggle, droneToggle;
    juce::String seedInfo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeedEditor)
};
