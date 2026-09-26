#pragma once
// Seed — the plugin shell. Owns the two halves and keeps them apart:
//   CALCULATOR: the tuning params + a background worker (xen::CalculatorWorker) that turns them
//               into a scale. Runs from a message-thread timer (pumpCalculator), never on the
//               audio thread. The raw scale reaches the audio thread through a try-lock handoff.
//   SYNTH:      SeedEngine (voices), FX (Partial Delay, Rough Reverb), modulators, safety stage.
// Also: presets (All / Tuning / Sound), Dice / Mutate, and state save/load.
#include <juce_audio_processors/juce_audio_processors.h>

#include "SeedEngine.h"
#include "SeedParams.h"
#include "dsp/Modulator.h"
#include "dsp/PartialDelay.h"
#include "dsp/RoughReverb.h"
#include "dsp/SafetyStage.h"
#include "xen/CalculatorWorker.h"

class SeedProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    SeedProcessor();
    ~SeedProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Seed"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::RangedAudioParameter& param (const juce::String& id) const { return *apvts.getParameter (id); }

    // ---- Calculator (message thread) ------------------------------------------------------
    /// Sends the current (modulated) calculator settings to the worker and collects results.
    /// Called by a 30 Hz timer; tests call it directly.
    void pumpCalculator();
    xen::CalculatorSettings currentCalculatorSettings() const;
    const xen::CalculatorOutput& calculatorOutput() const { return calcOut; }
    bool calculatorBusy() const { return worker.isBusy(); }
    bool hasCalculatorResult() const { return calcSeen > 0; }
    /// The latest scale with the note edits applied (what the keyboard plays).
    xen::FinalScale finalScale() const;
    xen::NoteEdits noteEditsFromParams() const;
    xen::PartialEdits partialEditsFromParams() const;

    // ---- Presets (message thread) -----------------------------------------------------------
    enum class Scope { All = 0, Tuning = 1, Sound = 2 };
    static juce::File presetFolder();
    juce::Array<juce::File> listPresets() const;
    bool savePreset (const juce::String& name);
    bool loadPreset (const juce::File& file, Scope scope);
    juce::String currentPresetName;

    // ---- Chaos (message thread) ---------------------------------------------------------------
    /// amount 1 = Dice (fully random), < 1 = Mutate (nudge). Scope chooses the group.
    void randomize (Scope scope, double amount);

    // ---- For the UI -----------------------------------------------------------------------------
    std::atomic<int> uiActiveVoices { 0 };
    std::array<std::atomic<float>, seed::kNumMods> uiModValue {};
    int safetyFaults() const { return safety.faultCount(); }

private:
    void timerCallback() override { pumpCalculator(); }
    double eff (juce::RangedAudioParameter* p, int modTarget) const;
    void buildEngineParams (double blockSeconds);
    void updateScaleOnAudioThread();
    void handleMidi (const juce::MidiMessage& m);
    void updateDrone();

    // Engine + FX + safety.
    seed::Engine engine;
    seed::EngineParams ep;
    pvst::PartialDelay delay;
    pvst::RoughReverb reverb;
    pvst::SafetyStage safety;
    std::array<pvst::Modulator, seed::kNumMods> mods;
    std::vector<double> modOffset;                 // per target index, normalised units
    std::vector<float> dryL, dryR, busA, busB;
    int droneSounding = -1;
    double bendNorm = 0.0;                          // pitch wheel, -1..1
    int chordSize = 1, chordStep = 2;
    double sampleRateHz = 48000.0;

    // Calculator worker + audio handoff.
    xen::CalculatorWorker worker;
    xen::CalculatorOutput calcOut;
    uint64_t calcSeen = 0;
    juce::SpinLock scaleLock;
    xen::RawScale sharedRaw, audioRaw;
    uint64_t sharedVersion = 0, audioVersion = 0;
    // Effective (modulated) calculator inputs, published by the audio thread for pumpCalculator.
    std::atomic<float> effSeed { -1.0f }, effFund { -1.0f }, effRolloff { -1.0f }, effMinDip { -1.0f };

    // Cached parameter pointers.
    juce::RangedAudioParameter* P (const char* id) const { return apvts.getParameter (id); }
    std::array<juce::RangedAudioParameter*, xen::kSeriesPartials> pLvl {}, pPos {};
    std::array<juce::RangedAudioParameter*, xen::kCustomPartials> cpOn {}, cpC {}, cpL {};
    std::array<juce::RangedAudioParameter*, xen::kEditableDegrees> nOn {}, nOff {};
    std::array<juce::RangedAudioParameter*, xen::kCustomNotes> cnOn {}, cnC {};
    std::array<juce::RangedAudioParameter*, seed::kNumMods> mShape {}, mRate {}, mDepth {}, mTarget {};
    std::vector<juce::RangedAudioParameter*> targetParams; // index = mod target

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeedProcessor)
};
