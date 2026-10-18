#pragma once
// Sane — the plugin shell: parameters -> SaneEngine, MIDI, the shared safety stage, state save/load.
// The editor reads `engine.report` (latest hit, scope, held keys) and can play notes through `uiKeys`.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "SaneEngine.h"
#include "SaneParams.h"
#include "dsp/SafetyStage.h"

class SaneProcessor : public juce::AudioProcessor
{
public:
    SaneProcessor();
    ~SaneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Sane"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::RangedAudioParameter& param (const juce::String& id) const { return *apvts.getParameter (id); }
    float value (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }

    /// The engine's current parameter snapshot built from the APVTS (also used by the UI for drawing).
    sane::EngineParams readParams() const;

    sane::SaneEngine engine;
    /// Notes played from the editor's on-screen keyboard (merged into the MIDI stream).
    juce::MidiKeyboardState uiKeys;

    int safetyFaults() const { return safety.faultCount(); }
    /// Host tempo (BPM mode of the loop time). 120 until the host tells us.
    std::atomic<double> hostBpm { 120.0 };

private:
    pvst::SafetyStage safety;
    juce::AudioBuffer<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SaneProcessor)
};
