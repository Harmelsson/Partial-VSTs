#pragma once
// Seed — the plugin shell around SeedEngine: parameters, MIDI, safety stage, state save/load.
#include <juce_audio_processors/juce_audio_processors.h>

#include "SeedEngine.h"
#include "dsp/SafetyStage.h"

namespace seed
{
/// Stable parameter IDs. NEVER rename one: FL Studio projects and automation store these.
namespace ids
{
    inline constexpr const char* seed = "seed";
    inline constexpr const char* partials = "partials";
    inline constexpr const char* rolloff = "rolloff";
    inline constexpr const char* attack = "attack";
    inline constexpr const char* release = "release";
    inline constexpr const char* gain = "gain";
    inline constexpr const char* keysFollow = "keysFollow";
    inline constexpr const char* drone = "drone";
    inline constexpr const char* droneNote = "droneNote";
} // namespace ids

/// Version shown in the UI. Bump on EVERY change handed to the user.
inline constexpr const char* kVersion = "0.1.0";
} // namespace seed

class SeedProcessor : public juce::AudioProcessor
{
public:
    SeedProcessor();

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
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    /// For the UI: how many voices are sounding, and how many safety faults happened.
    std::atomic<int> uiActiveVoices { 0 };
    int safetyFaults() const { return safety.faultCount(); }

private:
    void syncParams();
    void handleMidi (const juce::MidiMessage& m);
    void updateDrone();

    seed::Engine engine;
    pvst::SafetyStage safety;
    seed::EngineParams params;
    std::vector<float> mono;

    // Drone: a virtual held note, owned by the plugin rather than the piano roll.
    int droneSounding = -1; // key the drone is currently holding, -1 = none
    double pitchBendCents = 0.0;

    // Cached raw parameter pointers (fast, lock-free reads on the audio thread).
    std::atomic<float>* pSeed = nullptr; std::atomic<float>* pPartials = nullptr;
    std::atomic<float>* pRolloff = nullptr; std::atomic<float>* pAttack = nullptr;
    std::atomic<float>* pRelease = nullptr; std::atomic<float>* pGain = nullptr;
    std::atomic<float>* pKeysFollow = nullptr; std::atomic<float>* pDrone = nullptr;
    std::atomic<float>* pDroneNote = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeedProcessor)
};
