#pragma once
// Seed — the plugin shell. Owns the two halves and keeps them apart:
//   CALCULATOR: the tuning params + a background worker (xen::CalculatorWorker) that turns them
//               into a scale. Runs from a message-thread timer (pumpCalculator), never on the
//               audio thread. The raw scale reaches the audio thread through a try-lock handoff.
//   SYNTH:      SeedEngine (voices, 5-band EQ), FX rack, modulators, safety stage.
// Also: presets (All / Tuning / Sound), Dice / Mutate, scale lock, modulation links, state save/load.
#include <juce_audio_processors/juce_audio_processors.h>
#include <unordered_map>

#include "SeedEngine.h"
#include "SeedParams.h"
#include "dsp/Modulator.h"
#include <mutex>
#include "dsp/ChirpReverb.h"
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

    // ---- Scala scales (message thread) -------------------------------------------------------
    /// Loads .scl text as the "Imported" scale and switches the method to Imported.
    /// Returns false (and changes nothing) if the text has no usable notes.
    bool importScl (const juce::String& sclText);
    /// .scl text of the scale the keyboard plays now (note edits applied, disabled notes left out).
    juce::String exportScl() const;
    /// .kbm keyboard map for exportScl(): key 60 = unison at the Fundamental.
    juce::String exportKbm() const;
    /// Searches the seed whose stretched partials make the current scale's notes sit in dissonance
    /// valleys ("which timbre fits this scale?") and sets it. Returns the new seed.
    double fitSeedToCurrentScale();
    /// Name + note count of the imported scale ("" if none).
    juce::String importedScaleName() const { return importedName; }
    int importedScaleSize() const { return (int) importedCents.size(); }

    // ---- Scale lock (message thread) ------------------------------------------------------------
    /// Locked = the keyboard keeps playing the scale that was current when the lock was set, however the
    /// seed / partials / method change afterwards (note edits still apply on top). The live curve keeps
    /// updating, so you can see the locked notes against the new timbre.
    bool scaleLocked() const { return P (seed::ids::scaleLock)->getValue() > 0.5f; }
    /// Locks (snapshotting the scale right now) or unlocks (the scale is recalculated from the current
    /// settings: the notes WILL change, the UI asks first).
    void setScaleLocked (bool lock);

    // ---- Partials lock (message thread) -----------------------------------------------------------------
    /// Locked = the partial series keeps the seed stretch, rolloff and count it had when locked (hand
    /// edits still apply on top). The seed then only sets the "octave" / scale period.
    bool partialsLocked() const { return P (seed::ids::partialsLock)->getValue() > 0.5f; }
    void setPartialsLocked (bool lock);

    // ---- Drawn LFO shapes (message thread) ------------------------------------------------------------------
    pvst::LfoShape lfoShape (int mod) const { return lfoUI[(size_t) mod - 1]; }
    /// Replaces modulator `mod`'s drawn curve and switches its shape to "Drawn".
    void setLfoShape (int mod, const pvst::LfoShape& shape);
    void applyLfoPreset (int mod, int preset);
    /// Where each modulator is in its cycle (0..1) and how often it was retriggered (for the UI).
    std::array<std::atomic<float>, seed::kNumMods> uiModPhase {};
    std::array<std::atomic<uint32_t>, seed::kNumMods> uiModTrig {};

    // ---- Window scale (message thread) ----------------------------------------------------------------------------------
    /// Editor scale in percent (25..200), remembered between sessions in a small settings file.
    int uiScale() const { return uiScalePercent; }
    void setUiScale (int percent);

    // ---- Modulation links (message thread, except modOffsetNorm) ------------------------------------
    // A modulator is linked to any number of float parameters (drag a modulator onto a field). Each link
    // has its own amount (-100..+100 %, multiplies the modulator's depth). Stored in the plugin state
    // (ValueTree "MODLINKS"), mirrored into a lock-free table for the audio thread.
    struct ModLink { int mod = 0; juce::String paramId; float amount = 100.0f; };
    static constexpr int kMaxLinks = 256;
    std::vector<ModLink> modLinks() const;
    /// True for every parameter a modulator can be dropped on (all but the two locks and legacy ones).
    bool isModulatable (const juce::String& paramId) const;
    /// Links `mod` (1..kNumMods) to the parameter. False if not a float parameter, already linked or table full.
    bool addModLink (int mod, const juce::String& paramId, float amountPercent = 100.0f);
    void removeModLink (int mod, const juce::String& paramId);
    void setModLinkAmount (int mod, const juce::String& paramId, float amountPercent);
    void clearModLinks();
    /// Changes whenever the links change (so the UI knows when to rebuild its link chips).
    uint32_t linksVersion() const { return linksEpoch.load(); }
    /// Bit m-1 set = modulator m drives this parameter.
    unsigned modMaskFor (const juce::String& paramId) const;
    /// Live modulation of a parameter in normalised units (audio thread's last block), for the UI.
    float modOffsetNorm (const juce::RangedAudioParameter& p) const;

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
    /// Dice / Mutate only these parameters (one synth page).
    void randomizeParams (const juce::StringArray& ids, double amount);
    /// Back to defaults: these parameters only (one synth page).
    void resetParams (const juce::StringArray& ids);
    /// Calculator back to what it calculates: every hand edit, Scale tick, key-map choice, and
    /// every modulator aimed at a calculator value is removed. The core settings stay.
    void resetCalculatorEdits();
    /// Everything back to defaults (a fresh Seed).
    void initAll();
    /// Swap two FX rack slots (all their settings); used by the ▲▼ buttons.
    void swapFxSlots (int a, int b);
    /// Host tempo (for the UI).
    double currentBpm() const { return bpm; }

    // ---- For the UI -----------------------------------------------------------------------------
    std::atomic<int> uiActiveVoices { 0 };
    std::array<std::atomic<float>, seed::kNumMods> uiModValue {};
    /// Held notes (cents above the Fundamental), NaN = empty slot. Written by the audio thread.
    std::array<std::atomic<float>, seed::kMaxVoices> uiHeld {};
    /// Every note-on since the window opened, newest last in a ring (cents above the Fundamental).
    /// Lets the UI light up notes that were shorter than one refresh. Sequence counter = total so far.
    static constexpr int kHitRing = 64;
    std::array<std::atomic<float>, kHitRing> uiHitCents {};
    std::atomic<uint32_t> uiHitSeq { 0 };
    int safetyFaults() const { return safety.faultCount(); }

private:
    void timerCallback() override { pumpCalculator(); }
    /// Plain value of a parameter with the modulators' offsets applied (audio thread).
    double eff (juce::RangedAudioParameter* p) const;
    /// Same, usable from any thread (reads the offsets the audio thread published). For calculator values.
    double effUI (juce::RangedAudioParameter* p) const;
    void buildEngineParams (double blockSeconds);
    void updateScaleOnAudioThread();
    void handleMidi (const juce::MidiMessage& m);
    void publishHits();
    void updateDrone();

    // Engine + FX + safety.
    seed::Engine engine;
    seed::EngineParams ep;
    // FX rack: every slot owns one of each effect, so switching a slot's type is instant.
    struct FxSlot
    {
        pvst::PartialDelay delay;
        pvst::RoughReverb rough;
        pvst::ChirpReverb chirp;
        pvst::SquashCompressor squash;
        int lastType = -1;
    };
    std::array<FxSlot, seed::kNumFx> fxSlots;
    std::array<std::vector<float>, seed::kNumFx> partialBus;
    std::vector<float> wetL, wetR;
    double bpm = 120.0;
    void processRack (int n);
    struct FxParams { juce::RangedAudioParameter *type, *on, *in, *picks, *mix, *pan, *width, *dTime, *dSync, *dBeat, *dFb,
                      *dSpiral, *dPing, *dTone, *rDecay, *rRough, *rDamp, *cDecay, *cChirp, *cSize, *cDamp, *kThr, *kSlope,
                      *kAtt, *kRel, *kMake; };
    std::array<FxParams, seed::kNumFx> fxp {};
    struct EqParams { juce::RangedAudioParameter *on, *type, *freq, *gain, *q, *steep, *kt; };
    std::array<EqParams, seed::kEqBands> eqp {};
    juce::RangedAudioParameter *pSeed = nullptr, *pFund = nullptr, *pRolloff = nullptr, *pMinDip = nullptr, *pTol = nullptr;
    pvst::SafetyStage safety;
    std::array<pvst::Modulator, seed::kNumMods> mods;
    // Modulation: the link table (audio thread reads, message thread writes), per-parameter offsets.
    struct LinkSlot { std::atomic<int> param { -1 }, mod { 0 }; std::atomic<float> amount { 0.0f }; };
    std::array<LinkSlot, kMaxLinks> linkSlots;
    std::vector<double> modOffset;                 // per parameter index, normalised units (audio thread)
    std::unique_ptr<std::atomic<float>[]> uiModOffset; // same, for the UI (only linked parameters are kept fresh)
    std::array<double, seed::kNumMods> lastModValue {};
    std::atomic<uint32_t> linksEpoch { 1 };
    std::vector<double> swing;                     // per parameter: 0.5 (continuous) or 1.0 (discrete) x depth
    std::vector<uint32_t> touchStamp;              // per parameter: last block that wrote its UI offset
    uint32_t touchBlock = 0;
    std::vector<int> touchedPrev;
    // LFO: drawn shapes (message thread copy, shared copy behind a try-lock, audio copy) + trigger state.
    std::array<pvst::LfoShape, seed::kNumMods> lfoUI, lfoShared, lfoAudio;
    juce::SpinLock lfoLock;
    uint64_t lfoVersion = 0, lfoAudioVersion = 0;
    void publishLfo();
    void writeLfoState();
    void loadLfoState (const juce::String& text);
    std::array<bool, seed::kNumMods> trigArmed {};
    bool midiNoteOnInBlock = false;
    float levelIn = 0.0f, levelSc = 0.0f;          // peak of this block's input / sidechain buses
    std::atomic<float> levelOut { 0.0f };          // peak of the last block's output (before the safety stage)
    std::array<juce::RangedAudioParameter*, seed::kNumMods> mTrig {}, mThr {}, mSrc {}, mLoop {};
    // Partials lock snapshot.
    std::atomic<bool> lockedPValid { false };
    std::atomic<float> lockedPSeed { 2.0f }, lockedPRolloff { 1.0f };
    std::atomic<int> lockedPCount { 16 };
    bool publishedPLocked = false;
    void setLockedPartialsFromText (const juce::String& text);
    juce::String lockedPartialsText() const;
    int uiScalePercent = 100;
    std::vector<ModLink> links;                    // the truth (message thread); mirrored into linkSlots + state
    void commitLinks();                            // linkSlots + the "modLinks" state property
    void loadLinksFromText (const juce::String& text);
    static juce::String linksToText (const std::vector<ModLink>& l);
    void absorbLegacyTargets();                    // v0.5 single mod targets -> links
    void computeModOffsets (double blockSeconds);
    std::vector<float> dryL, dryR;
    int droneSounding = -1;
    double bendNorm = 0.0;                          // pitch wheel, -1..1
    int chordSize = 1, chordStep = 2;
    double sampleRateHz = 48000.0;

    // Calculator worker + audio handoff.
    xen::CalculatorWorker worker;
    xen::CalculatorOutput calcOut;
    // Imported Scala scale (message thread). The .scl text itself is kept in apvts.state
    // ("importedScl") and in presets, so projects remember it.
    juce::String importedSclText, importedName;
    std::vector<double> importedCents;
    void setImportedFromText (const juce::String& sclText);
    uint64_t calcSeen = 0;
    juce::SpinLock scaleLock;
    xen::RawScale sharedRaw, audioRaw;
    uint64_t sharedVersion = 0, audioVersion = 0;
    // Effective (modulated) calculator inputs, published by the audio thread for pumpCalculator.
    std::atomic<float> effSeed { -1.0f }, effFund { -1.0f }, effRolloff { -1.0f }, effMinDip { -1.0f }, effTol { -1.0f };

    // Cached parameter pointers.
    juce::RangedAudioParameter* P (const char* id) const { return apvts.getParameter (id); }
    std::array<juce::RangedAudioParameter*, xen::kSeriesPartials> pLvl {}, pPos {};
    std::array<juce::RangedAudioParameter*, xen::kCustomPartials> cpOn {}, cpC {}, cpL {};
    std::array<juce::RangedAudioParameter*, xen::kEditableDegrees> nOn {}, nOff {};
    std::array<juce::RangedAudioParameter*, xen::kCustomNotes> cnOn {}, cnC {};
    std::array<juce::RangedAudioParameter*, xen::kSeriesPartials> pScl {};
    std::array<juce::RangedAudioParameter*, xen::kCustomPartials> cpScl {};
    void randomizeOne (juce::RangedAudioParameter& p, double amount, juce::Random& r);
    std::array<juce::RangedAudioParameter*, seed::kNumMods> mShape {}, mRate {}, mDepth {}, mSync {}, mBeat {};
    std::array<juce::RangedAudioParameter*, seed::kLegacyMods> mTarget {};
    std::vector<juce::RangedAudioParameter*> targetParams; // legacy target index -> parameter (migration only)
    // The scale lock's snapshot (message thread). Text copy lives in apvts.state ("lockedScale").
    xen::RawScale lockedRaw;
    bool publishedLocked = false;
    void setLockedFromText (const juce::String& text);
    void publishScale();
    xen::RawScale rawScaleNow() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeedProcessor)
};
