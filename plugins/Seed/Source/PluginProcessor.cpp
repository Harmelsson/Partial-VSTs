#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace seed;

// Mod target indices (must match modTargetNames()/modTargetIds() order in SeedParams.h).
enum Tgt { T_None = 0, T_Seed, T_Fund, T_Rolloff, T_MinDip, T_Pitch, T_FShift, T_Tilt, T_OddEven, T_Spread,
           T_Shimmer, T_FormFreq, T_FormGain, T_Noise, T_Cutoff, T_Reso, T_FilterEnv, T_SendA, T_DelayTime,
           T_DelayFb, T_Spiral, T_SendB, T_RvbDecay, T_Rough, T_Magnet, T_Life, T_Glide, T_Gain, T_Wander, T_Scramble, T_Count };

SeedProcessor::SeedProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SeedState", createParameterLayout())
{
    for (int n = 1; n <= xen::kSeriesPartials; ++n)
    {
        pLvl[(size_t) n - 1] = apvts.getParameter (ids::partialLevel (n));
        pPos[(size_t) n - 1] = n >= 2 ? apvts.getParameter (ids::partialPos (n)) : nullptr;
    }
    for (int n = 2; n <= xen::kSeriesPartials; ++n) pScl[(size_t) n - 1] = apvts.getParameter (ids::partialInScale (n));
    for (int k = 1; k <= xen::kCustomPartials; ++k)
    {
        cpScl[(size_t) k - 1] = apvts.getParameter (ids::customPartialInScale (k));
        cpOn[(size_t) k - 1] = apvts.getParameter (ids::customPartialOn (k));
        cpC[(size_t) k - 1] = apvts.getParameter (ids::customPartialCents (k));
        cpL[(size_t) k - 1] = apvts.getParameter (ids::customPartialLevel (k));
    }
    for (int d = 1; d <= xen::kEditableDegrees; ++d)
    {
        nOn[(size_t) d - 1] = apvts.getParameter (ids::noteOn (d));
        nOff[(size_t) d - 1] = apvts.getParameter (ids::noteOffset (d));
    }
    for (int k = 1; k <= xen::kCustomNotes; ++k)
    {
        cnOn[(size_t) k - 1] = apvts.getParameter (ids::customNoteOn (k));
        cnC[(size_t) k - 1] = apvts.getParameter (ids::customNoteCents (k));
    }
    for (int m = 1; m <= kNumMods; ++m)
    {
        mShape[(size_t) m - 1] = apvts.getParameter (ids::modShape (m));
        mRate[(size_t) m - 1] = apvts.getParameter (ids::modRate (m));
        mDepth[(size_t) m - 1] = apvts.getParameter (ids::modDepth (m));
        mTarget[(size_t) m - 1] = apvts.getParameter (ids::modTarget (m));
        mods[(size_t) m - 1].reset ((uint32_t) (m * 7919));
    }
    for (auto* id : modTargetIds())
        targetParams.push_back (juce::String (id).isEmpty() ? nullptr : apvts.getParameter (id));
    jassert ((int) targetParams.size() == T_Count);
    modOffset.assign ((size_t) T_Count, 0.0);
    for (auto& h : uiHeld) h.store (std::nanf (""));

    pumpCalculator();
    startTimerHz (30);
}

SeedProcessor::~SeedProcessor() { stopTimer(); }

bool SeedProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SeedProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateHz = sampleRate;
    buildEngineParams (0.0);
    engine.setParams (ep);
    engine.prepare (sampleRate);
    delay.prepare (sampleRate);
    reverb.prepare (sampleRate);
    safety.prepare (sampleRate);
    const auto n = (size_t) std::max (samplesPerBlock, 1);
    dryL.assign (n, 0.0f); dryR.assign (n, 0.0f); busA.assign (n, 0.0f); busB.assign (n, 0.0f);
    droneSounding = -1;
}

// ================================================================================================
// Parameters -> engine (audio thread)
// ================================================================================================
double SeedProcessor::eff (juce::RangedAudioParameter* p, int modTarget) const
{
    double norm = p->getValue();
    if (modTarget > 0) norm = juce::jlimit (0.0, 1.0, norm + modOffset[(size_t) modTarget]);
    return p->convertFrom0to1 ((float) norm);
}

void SeedProcessor::buildEngineParams (double blockSeconds)
{
    // 1. Modulators -> offsets (normalised). Depth 100% swings a full half-range each way.
    std::fill (modOffset.begin(), modOffset.end(), 0.0);
    for (int m = 0; m < kNumMods; ++m)
    {
        const int shape = (int) mShape[(size_t) m]->convertFrom0to1 (mShape[(size_t) m]->getValue());
        const double rate = mRate[(size_t) m]->convertFrom0to1 (mRate[(size_t) m]->getValue());
        const double depth = mDepth[(size_t) m]->convertFrom0to1 (mDepth[(size_t) m]->getValue()) / 100.0;
        const int tgt = (int) mTarget[(size_t) m]->convertFrom0to1 (mTarget[(size_t) m]->getValue());
        const double v = mods[(size_t) m].advance (shape, rate, blockSeconds);
        uiModValue[(size_t) m].store ((float) v, std::memory_order_relaxed);
        if (tgt > 0 && tgt < T_Count) modOffset[(size_t) tgt] += 0.5 * depth * v;
    }
    auto E = [this] (int t) { return eff (targetParams[(size_t) t], t); };
    auto V = [this] (const char* id) { auto* p = P (id); return (double) p->convertFrom0to1 (p->getValue()); };

    // 2. Calculator inputs (effective) — also published for the calculator pump.
    ep.seed = E (T_Seed);
    ep.fundamentalHz = E (T_Fund);
    ep.rolloff = E (T_Rolloff);
    ep.partials = juce::jlimit (1, xen::kSeriesPartials, (int) std::lround (V (ids::partials)));
    effSeed.store ((float) ep.seed); effFund.store ((float) ep.fundamentalHz); effRolloff.store ((float) ep.rolloff);
    effMinDip.store ((float) E (T_MinDip));
    ep.edits = partialEditsFromParams();

    // 3. Synth.
    ep.attackMs = V (ids::attack); ep.decayMs = V (ids::decay); ep.sustain = V (ids::sustain) / 100.0;
    ep.releaseMs = V (ids::release); ep.velocitySens = V (ids::velocity) / 100.0; ep.gainDb = E (T_Gain);
    ep.keyMode = (int) V (ids::keyMode);
    ep.magnet = E (T_Magnet) / 100.0; ep.life = E (T_Life) / 100.0; ep.glideMs = E (T_Glide);
    ep.pitchCents = E (T_Pitch); ep.fshiftHz = E (T_FShift);
    ep.bendCents = bendNorm * V (ids::bendRange) * 100.0;
    ep.tiltDbOct = E (T_Tilt); ep.oddEven = E (T_OddEven) / 100.0; ep.spread = E (T_Spread) / 100.0;
    ep.shimmer = E (T_Shimmer) / 100.0; ep.shimmerRate = V (ids::shimmerRate);
    ep.formFreq = E (T_FormFreq); ep.formGainDb = E (T_FormGain); ep.formWidth = V (ids::formWidth);
    ep.noiseDb = E (T_Noise);
    ep.filterMode = (int) V (ids::filterMode); ep.cutoff = E (T_Cutoff); ep.reso = E (T_Reso) / 100.0;
    ep.keyTrack = V (ids::keyTrack) / 100.0; ep.filterEnvOct = E (T_FilterEnv); ep.filterDecayMs = V (ids::filterDecay);
    ep.sendA = E (T_SendA) / 100.0; ep.ruleA = (int) V (ids::ruleA);
    ep.sendB = E (T_SendB) / 100.0; ep.ruleB = (int) V (ids::ruleB);
    ep.wanderCents = E (T_Wander); ep.wanderRate = V (ids::wanderRate); ep.scramble = E (T_Scramble) / 100.0;
    chordSize = juce::jlimit (1, 5, (int) std::lround (V (ids::chordSize)));
    chordStep = juce::jlimit (1, 12, (int) std::lround (V (ids::chordStep)));
}

xen::PartialEdits SeedProcessor::partialEditsFromParams() const
{
    xen::PartialEdits e;
    auto val = [] (juce::RangedAudioParameter* p) { return (double) p->convertFrom0to1 (p->getValue()); };
    for (int i = 0; i < xen::kSeriesPartials; ++i)
    {
        e.levelDb[(size_t) i] = val (pLvl[(size_t) i]);
        e.posCents[(size_t) i] = pPos[(size_t) i] ? val (pPos[(size_t) i]) : 0.0;
        e.inScale[(size_t) i] = pScl[(size_t) i] == nullptr || pScl[(size_t) i]->getValue() > 0.5f;
    }
    for (int k = 0; k < xen::kCustomPartials; ++k)
    {
        e.customOn[(size_t) k] = cpOn[(size_t) k]->getValue() > 0.5f;
        e.customCents[(size_t) k] = val (cpC[(size_t) k]);
        e.customLevelDb[(size_t) k] = val (cpL[(size_t) k]);
        e.customInScale[(size_t) k] = cpScl[(size_t) k]->getValue() > 0.5f;
    }
    return e;
}

xen::NoteEdits SeedProcessor::noteEditsFromParams() const
{
    xen::NoteEdits e;
    for (int d = 0; d < xen::kEditableDegrees; ++d)
    {
        e.on[(size_t) d] = nOn[(size_t) d]->getValue() > 0.5f;
        e.offset[(size_t) d] = nOff[(size_t) d]->convertFrom0to1 (nOff[(size_t) d]->getValue());
    }
    for (int k = 0; k < xen::kCustomNotes; ++k)
    {
        e.customOn[(size_t) k] = cnOn[(size_t) k]->getValue() > 0.5f;
        e.customCents[(size_t) k] = cnC[(size_t) k]->convertFrom0to1 (cnC[(size_t) k]->getValue());
    }
    return e;
}

void SeedProcessor::updateScaleOnAudioThread()
{
    {
        const juce::SpinLock::ScopedTryLockType tl (scaleLock);
        if (tl.isLocked() && sharedVersion != audioVersion) { audioRaw = sharedRaw; audioVersion = sharedVersion; }
    }
    const auto fin = xen::applyNoteEdits (audioRaw, noteEditsFromParams());
    std::array<double, xen::kMaxFinalNotes> c {}, st {};
    const int n = fin.valid ? fin.playable (c.data(), st.data(), (int) c.size()) : 0;
    engine.setScale (c.data(), st.data(), n, fin.period);

    // Rough Reverb rings on the scale's notes, folded into a comfortable 110-880 Hz band.
    std::array<double, pvst::RoughReverb::kMaxNotes> hz {};
    const int count = std::min (n, pvst::RoughReverb::kMaxNotes);
    const double period = std::max (50.0, fin.period);
    for (int i = 0; i < count; ++i)
    {
        double f = ep.fundamentalHz * std::exp2 (c[(size_t) i] / 1200.0);
        const double step = std::exp2 (period / 1200.0);
        while (f < 110.0) f *= step;
        while (f > 880.0 && f / step >= 30.0) f /= step;
        hz[(size_t) i] = f;
    }
    if (count == 0) { hz[0] = ep.fundamentalHz; }
    reverb.setTuning (hz.data(), std::max (1, count), eff (targetParams[T_Rough], T_Rough) / 100.0);
}

// ================================================================================================
// MIDI + audio
// ================================================================================================
void SeedProcessor::handleMidi (const juce::MidiMessage& m)
{
    // Chord: every key also plays (chordSize - 1) more keys, chordStep keys apart. In Scale mode
    // keys are scale degrees, so this stacks the calculator's own notes.
    if (m.isNoteOn())
    {
        for (int k = 0; k < chordSize; ++k)
            if (const int key = m.getNoteNumber() + k * chordStep; key <= 127)
                engine.noteOn (key, m.getFloatVelocity() * (k == 0 ? 1.0f : 0.85f));
    }
    else if (m.isNoteOff())
    {
        for (int k = 0; k < chordSize; ++k)
            if (const int key = m.getNoteNumber() + k * chordStep; key <= 127 && key != droneSounding) engine.noteOff (key);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) { engine.allNotesOff(); droneSounding = -1; }
    else if (m.isPitchWheel()) bendNorm = (m.getPitchWheelValue() - 8192) / 8192.0;
}

void SeedProcessor::updateDrone()
{
    const bool on = P (ids::drone)->getValue() > 0.5f;
    const int key = (int) std::lround (P (ids::droneNote)->convertFrom0to1 (P (ids::droneNote)->getValue()));
    if (on && droneSounding != key)
    {
        if (droneSounding >= 0) engine.noteOff (droneSounding);
        engine.noteOn (key, 0.8f);
        droneSounding = key;
    }
    else if (! on && droneSounding >= 0) { engine.noteOff (droneSounding); droneSounding = -1; }
}

void SeedProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if ((int) dryL.size() < n) { dryL.resize ((size_t) n); dryR.resize ((size_t) n); busA.resize ((size_t) n); busB.resize ((size_t) n); }

    buildEngineParams (n / sampleRateHz);
    engine.setParams (ep);
    updateScaleOnAudioThread();
    updateDrone();

    std::fill_n (dryL.begin(), n, 0.0f); std::fill_n (dryR.begin(), n, 0.0f);
    std::fill_n (busA.begin(), n, 0.0f); std::fill_n (busB.begin(), n, 0.0f);

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > pos)
        {
            engine.render (dryL.data() + pos, dryR.data() + pos, busA.data() + pos, busB.data() + pos, at - pos);
            pos = at;
        }
        handleMidi (meta.getMessage());
    }
    if (pos < n) engine.render (dryL.data() + pos, dryR.data() + pos, busA.data() + pos, busB.data() + pos, n - pos);

    // FX on the send buses (they only contain the partials each send picked).
    pvst::PartialDelay::Params dp;
    dp.timeMs = eff (targetParams[T_DelayTime], T_DelayTime);
    dp.feedback = eff (targetParams[T_DelayFb], T_DelayFb) / 100.0;
    dp.spiralCents = eff (targetParams[T_Spiral], T_Spiral);
    dp.level = juce::Decibels::decibelsToGain ((double) P (ids::delayLevel)->convertFrom0to1 (P (ids::delayLevel)->getValue()));
    delay.process (busA.data(), dryL.data(), dryR.data(), n, dp);
    reverb.process (busB.data(), dryL.data(), dryR.data(), n, eff (targetParams[T_RvbDecay], T_RvbDecay),
                    juce::Decibels::decibelsToGain ((double) P (ids::rvbLevel)->convertFrom0to1 (P (ids::rvbLevel)->getValue())));

    buffer.copyFrom (0, 0, dryL.data(), n);
    if (buffer.getNumChannels() > 1) buffer.copyFrom (1, 0, dryR.data(), n);
    safety.process (buffer); // always last, never bypassed
    uiActiveVoices.store (engine.activeVoiceCount(), std::memory_order_relaxed);
    std::array<float, kMaxVoices> held {};
    const int nh = engine.heldNoteCents (held.data(), kMaxVoices);
    for (int i = 0; i < kMaxVoices; ++i) uiHeld[(size_t) i].store (i < nh ? held[(size_t) i] : std::nanf (""), std::memory_order_relaxed);
}

// ================================================================================================
// Calculator (message thread)
// ================================================================================================
xen::CalculatorSettings SeedProcessor::currentCalculatorSettings() const
{
    auto V = [this] (const char* id) { auto* p = P (id); return (double) p->convertFrom0to1 (p->getValue()); };
    xen::CalculatorSettings c;
    // Use the audio thread's modulated values when audio runs; the plain values otherwise.
    c.seed = effSeed.load() > 0.0f ? effSeed.load() : V (ids::seed);
    c.fundamentalHz = effFund.load() > 0.0f ? effFund.load() : V (ids::fundamental);
    c.rolloff = effRolloff.load() >= 0.0f ? effRolloff.load() : V (ids::rolloff);
    c.minStrength = (effMinDip.load() >= 0.0f ? effMinDip.load() : V (ids::minDip)) / 100.0;
    c.partials = (int) std::lround (V (ids::partials));
    c.method = (int) V (ids::method) == 1 ? xen::Method::Consonance : xen::Method::Dissonance;
    c.toleranceCents = V (ids::tolerance);
    c.minShared = (int) std::lround (V (ids::minShared));
    c.edits = partialEditsFromParams();
    return c;
}

void SeedProcessor::pumpCalculator()
{
    worker.request (currentCalculatorSettings());
    if (worker.fetch (calcOut, calcSeen))
    {
        xen::RawScale raw;
        raw.assign (calcOut.curve);
        const juce::SpinLock::ScopedLockType sl (scaleLock);
        sharedRaw = raw;
        ++sharedVersion;
    }
}

xen::FinalScale SeedProcessor::finalScale() const
{
    xen::RawScale raw;
    raw.assign (calcOut.curve);
    return xen::applyNoteEdits (raw, noteEditsFromParams());
}

// ================================================================================================
// Presets + chaos (message thread)
// ================================================================================================
juce::File SeedProcessor::presetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Partial-VSTs").getChildFile ("Seed Presets");
}

juce::Array<juce::File> SeedProcessor::listPresets() const
{
    auto files = presetFolder().findChildFiles (juce::File::findFiles, false, "*.seedpreset");
    files.sort();
    return files;
}

bool SeedProcessor::savePreset (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    presetFolder().createDirectory();
    juce::XmlElement root ("SeedPreset");
    root.setAttribute ("version", kVersion);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            auto* e = root.createNewChildElement ("PARAM");
            e->setAttribute ("id", rp->getParameterID());
            e->setAttribute ("value", (double) rp->convertFrom0to1 (rp->getValue()));
        }
    const bool ok = root.writeTo (presetFolder().getChildFile (clean + ".seedpreset"));
    if (ok) currentPresetName = clean;
    return ok;
}

bool SeedProcessor::loadPreset (const juce::File& file, Scope scope)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("SeedPreset")) return false;
    std::map<juce::String, double> values;
    for (auto* e : xml->getChildWithTagNameIterator ("PARAM")) values[e->getStringAttribute ("id")] = e->getDoubleAttribute ("value");
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = rp->getParameterID();
            const bool tuning = isTuningParam (id);
            if (scope == Scope::Tuning && ! tuning) continue;
            if (scope == Scope::Sound && tuning) continue;
            const auto it = values.find (id);
            // Params missing from an older preset go back to default (= "not modified").
            const float norm = it != values.end() ? rp->convertTo0to1 ((float) it->second) : rp->getDefaultValue();
            rp->setValueNotifyingHost (norm);
        }
    currentPresetName = file.getFileNameWithoutExtension();
    return true;
}

void SeedProcessor::randomizeOne (juce::RangedAudioParameter& rp, double amount, juce::Random& r)
{
    const auto id = rp.getParameterID();
    const float cur = rp.getValue();
    float next = cur + (float) amount * (r.nextFloat() - cur);
    if (rp.getNumSteps() < 30 && amount < 1.0 && r.nextFloat() > amount * 0.5) next = cur; // choices change rarely
    // Keep a few things sane: noise mostly off, feedback not pinned at max, some partials.
    if (id == ids::noise) next = std::min (next, rp.convertTo0to1 (-18.0f));
    if (id == ids::delayFb) next = std::min (next, rp.convertTo0to1 (85.0f));
    if (id == ids::partials) next = std::max (next, rp.convertTo0to1 (4.0f));
    rp.setValueNotifyingHost (next);
}

/// Never randomised: output gain (hearing!), drone, legacy, bend range, velocity, fundamental.
static bool neverRandomised (const juce::String& id)
{
    static const juce::StringArray never { ids::gain, ids::drone, ids::droneNote, ids::keysFollow, ids::bendRange,
                                           ids::velocity, ids::fundamental };
    return never.contains (id);
}

/// Per-item calculator edits (partials, notes, ticks, key map) are the user's hand work.
static bool isPerItem (const juce::String& id)
{
    return id.startsWith ("cp") || id.startsWith ("cn") || (id.startsWith ("n") && id != ids::noise)
        || (id.startsWith ("p") && (id.endsWith ("lvl") || id.endsWith ("pos") || id.endsWith ("scl")));
}

void SeedProcessor::randomize (Scope scope, double amount)
{
    juce::Random r;
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr) continue;
        const auto id = rp->getParameterID();
        if (neverRandomised (id)) continue;
        const bool tuning = isTuningParam (id);
        if (scope == Scope::Tuning && ! tuning) continue;
        if (scope == Scope::Sound && tuning) continue;
        if (isPerItem (id))
        {
            // A light sprinkle of partial level edits when dicing the tuning fully.
            if (amount >= 1.0 && id.startsWith ("p") && id.endsWith ("lvl") && r.nextFloat() < 0.15f)
                rp->setValueNotifyingHost (rp->convertTo0to1 ((float) (r.nextFloat() * -30.0)));
            continue;
        }
        randomizeOne (*rp, amount, r);
    }
}

void SeedProcessor::randomizeParams (const juce::StringArray& idList, double amount)
{
    juce::Random r;
    for (auto& id : idList)
        if (auto* rp = apvts.getParameter (id); rp != nullptr && ! neverRandomised (id))
            randomizeOne (*rp, amount, r);
}

void SeedProcessor::resetParams (const juce::StringArray& idList)
{
    for (auto& id : idList)
        if (auto* rp = apvts.getParameter (id)) rp->setValueNotifyingHost (rp->getDefaultValue());
}

void SeedProcessor::resetCalculatorEdits()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (isTuningParam (rp->getParameterID()) && isPerItem (rp->getParameterID()))
                rp->setValueNotifyingHost (rp->getDefaultValue());
    // Modulators aimed at calculator values also "modify the calculator": remove those links.
    for (int m = 0; m < kNumMods; ++m)
    {
        const int tgt = (int) mTarget[(size_t) m]->convertFrom0to1 (mTarget[(size_t) m]->getValue());
        if (tgt == T_Seed || tgt == T_Fund || tgt == T_Rolloff || tgt == T_MinDip)
        {
            mTarget[(size_t) m]->setValueNotifyingHost (mTarget[(size_t) m]->getDefaultValue());
            mDepth[(size_t) m]->setValueNotifyingHost (mDepth[(size_t) m]->getDefaultValue());
        }
    }
}

void SeedProcessor::initAll()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p)) rp->setValueNotifyingHost (rp->getDefaultValue());
    currentPresetName = {};
}

// ================================================================================================
// State
// ================================================================================================
juce::AudioProcessorEditor* SeedProcessor::createEditor() { return new SeedEditor (*this); }

void SeedProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", currentPresetName, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void SeedProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentPresetName = apvts.state.getProperty ("presetName").toString();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SeedProcessor(); }
