#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace seed;

// Mod target indices (must match modTargetNames()/modTargetIds() order in SeedParams.h).
enum Tgt { T_None = 0, T_Seed, T_Fund, T_Rolloff, T_MinDip, T_Pitch, T_FShift, T_Tilt, T_OddEven, T_Spread,
           T_Shimmer, T_FormFreq, T_FormGain, T_Noise, T_Cutoff, T_Reso, T_FilterEnv, T_Fx1Mix, T_Fx1Time,
           T_Fx1Fb, T_Fx1Spiral, T_Fx2Mix, T_Fx2Decay, T_Fx2Rough, T_Magnet, T_Life, T_Glide, T_Gain, T_Wander, T_Scramble,
           T_Fx1Picks, T_Fx2Picks, T_Fx3Mix, T_Fx3Picks, T_Fx4Mix, T_Fx5Mix, T_Count };

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
    for (auto& id : modTargetIds())
        targetParams.push_back (id.isEmpty() ? nullptr : apvts.getParameter (id));
    for (int t = 1; t < (int) targetParams.size(); ++t)
        if (targetParams[(size_t) t] != nullptr) targetOf[targetParams[(size_t) t]] = t;
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
    for (auto& sl : fxSlots)
    {
        sl.delay.prepare (sampleRate); sl.rough.prepare (sampleRate); sl.chirp.prepare (sampleRate); sl.squash.prepare (sampleRate);
        sl.lastType = -1;
    }
    safety.prepare (sampleRate);
    const auto n = (size_t) std::max (samplesPerBlock, 1);
    dryL.assign (n, 0.0f); dryR.assign (n, 0.0f); wetL.assign (n, 0.0f); wetR.assign (n, 0.0f);
    for (auto& b : partialBus) b.assign (n, 0.0f);
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
        // Hz, or synced to the host tempo (v0.5).
        auto* syncP = P (ids::modSync (m + 1).toRawUTF8());
        auto* divP = P (ids::modDiv (m + 1).toRawUTF8());
        const double rate = syncP->getValue() > 0.5f ? (bpm / 60.0) / syncDivBeats ((int) std::lround (divP->convertFrom0to1 (divP->getValue())))
                                                     : mRate[(size_t) m]->convertFrom0to1 (mRate[(size_t) m]->getValue());
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
    ep.shimmer = E (T_Shimmer) / 100.0; ep.shimmerRate = V (ids::shimmerSync) > 0.5 ? (bpm / 60.0) / syncDivBeats ((int) V (ids::shimmerDiv)) : V (ids::shimmerRate);
    ep.formFreq = E (T_FormFreq); ep.formGainDb = E (T_FormGain); ep.formWidth = V (ids::formWidth);
    ep.noiseDb = E (T_Noise);
    ep.filterMode = (int) V (ids::filterMode); ep.cutoff = E (T_Cutoff); ep.reso = E (T_Reso) / 100.0;
    ep.keyTrack = V (ids::keyTrack) / 100.0; ep.filterEnvOct = E (T_FilterEnv); ep.filterDecayMs = V (ids::filterDecay);
    for (int sl = 1; sl <= kNumFx; ++sl)
    {
        const int type = (int) std::lround (fxValue (sl, "type"));
        ep.busOn[(size_t) sl - 1] = type != FxOff && fxValue (sl, "on") > 0.5 && (int) std::lround (fxValue (sl, "in")) == 1;
        ep.busPicks[(size_t) sl - 1] = fxValue (sl, "picks");
    }
    ep.wanderCents = E (T_Wander);
    ep.wanderRate = V (ids::wanderSync) > 0.5 ? (bpm / 60.0) / syncDivBeats ((int) V (ids::wanderDiv)) : V (ids::wanderRate); ep.scramble = E (T_Scramble) / 100.0;
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
    for (int sl = 1; sl <= kNumFx; ++sl)
        if ((int) std::lround (fxValue (sl, "type")) == FxRough)
            fxSlots[(size_t) sl - 1].rough.setTuning (hz.data(), std::max (1, count), fxValue (sl, "rRough") / 100.0);
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
    if ((int) dryL.size() < n)
    {
        dryL.resize ((size_t) n); dryR.resize ((size_t) n); wetL.resize ((size_t) n); wetR.resize ((size_t) n);
        for (auto& b : partialBus) b.resize ((size_t) n);
    }
    // Host tempo for BPM-synced settings (120 if the host doesn't say).
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto hostBpm = pos->getBpm()) bpm = juce::jlimit (20.0, 999.0, *hostBpm);

    buildEngineParams (n / sampleRateHz);
    engine.setParams (ep);
    updateScaleOnAudioThread();
    updateDrone();

    std::fill_n (dryL.begin(), n, 0.0f); std::fill_n (dryR.begin(), n, 0.0f);
    for (auto& b : partialBus) std::fill_n (b.begin(), n, 0.0f);
    std::array<float*, kNumFx> busPtr {};

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > pos)
        {
            for (int b = 0; b < kNumFx; ++b) busPtr[(size_t) b] = partialBus[(size_t) b].data() + pos;
            engine.render (dryL.data() + pos, dryR.data() + pos, busPtr.data(), at - pos);
            pos = at;
        }
        handleMidi (meta.getMessage());
    }
    if (pos < n)
    {
        for (int b = 0; b < kNumFx; ++b) busPtr[(size_t) b] = partialBus[(size_t) b].data() + pos;
        engine.render (dryL.data() + pos, dryR.data() + pos, busPtr.data(), n - pos);
    }

    processRack (n);

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
    const int m = (int) std::lround (V (ids::method));
    c.method = m == 1 ? xen::Method::Consonance : m == 2 ? xen::Method::Imported : xen::Method::Dissonance;
    c.imported = importedCents;
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
// Scala scales (message thread)
// ================================================================================================
/// The enabled notes between the unison and the period, rising (what a .scl file lists).
static std::vector<double> scaleNotesAboveUnison (const xen::FinalScale& f)
{
    std::vector<double> v;
    for (int i = 0; i < f.count; ++i)
    {
        const auto& n = f.notes[(size_t) i];
        if (n.enabled && n.kind != xen::ScaleNote::Unison && n.kind != xen::ScaleNote::Period) v.push_back (n.cents);
    }
    std::sort (v.begin(), v.end());
    return v;
}

void SeedProcessor::setImportedFromText (const juce::String& sclText)
{
    const auto s = xen::parseScl (sclText.toStdString());
    importedSclText = s.valid() ? sclText : juce::String();
    importedName = s.valid() ? juce::String (s.description).trim() : juce::String();
    if (s.valid() && importedName.isEmpty()) importedName = "Imported";
    importedCents = s.valid() ? s.cents : std::vector<double> {};
    // Scala allows unsorted pitches; our scale needs them rising with the period last.
    if (! importedCents.empty())
    {
        const double period = importedCents.back();
        importedCents.pop_back();
        importedCents.erase (std::remove_if (importedCents.begin(), importedCents.end(),
                                             [period] (double c) { return c <= 0.5 || c >= period - 0.5; }),
                             importedCents.end());
        std::sort (importedCents.begin(), importedCents.end());
        importedCents.push_back (period);
    }
    apvts.state.setProperty ("importedScl", importedSclText, nullptr);
}

bool SeedProcessor::importScl (const juce::String& sclText)
{
    if (! xen::parseScl (sclText.toStdString()).valid()) return false;
    setImportedFromText (sclText);
    auto* m = P (ids::method);
    m->setValueNotifyingHost (m->convertTo0to1 (2.0f));
    return true;
}

juce::String SeedProcessor::exportScl() const
{
    const auto f = finalScale();
    std::vector<double> notes;
    for (auto& n : scaleNotesAboveUnison (f)) notes.push_back (n);
    std::sort (notes.begin(), notes.end());
    auto V = [this] (const char* id) { auto* p = P (id); return (double) p->convertFrom0to1 (p->getValue()); };
    const auto cs = currentCalculatorSettings();
    const auto methodName = juce::StringArray { "Dissonance", "Consonance", "Imported" }[(int) cs.method];
    juce::String desc = "Seed " + juce::String (cs.seed, 4) + " (" + methodName + ")";
    if (cs.method == xen::Method::Imported && importedName.isNotEmpty()) desc = importedName + " (edited in Seed)";
    std::vector<std::string> comments {
        ("Seed " + juce::String (kVersion) + ": seed " + juce::String (cs.seed, 6) + ", partials " + juce::String (cs.partials)
         + ", fundamental " + juce::String (V (ids::fundamental), 3) + " Hz").toStdString()
    };
    return xen::writeScl (desc.toStdString(), comments, notes, f.valid ? f.period : 1200.0);
}

juce::String SeedProcessor::exportKbm() const
{
    const auto f = finalScale();
    int size = 1; // the unison
    size += (int) scaleNotesAboveUnison (f).size();
    auto* fp = P (ids::fundamental);
    return xen::writeKbm (size, fp->convertFrom0to1 (fp->getValue()));
}

double SeedProcessor::fitSeedToCurrentScale()
{
    const auto f = finalScale();
    if (! f.valid) return currentCalculatorSettings().seed;
    std::vector<double> notes;
    for (auto& n : scaleNotesAboveUnison (f)) notes.push_back (n);
    notes.push_back (f.period);
    const auto cs = currentCalculatorSettings();
    auto* sp = P (ids::seed);
    const auto range = sp->getNormalisableRange();
    // Search the seed range the knob allows, but keep away from 1 (no period) and silly highs.
    const double lo = std::max ((double) range.start, 1.2), hi = std::min ((double) range.end, 4.5);
    const double best = xen::fitSeedToScale (notes, f.period, cs.fundamentalHz, std::min (cs.partials, 24), cs.rolloff, lo, hi);
    sp->setValueNotifyingHost (sp->convertTo0to1 ((float) best));
    return best;
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
    if (importedSclText.isNotEmpty()) root.setAttribute ("importedScl", importedSclText); // part of the Tuning
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
    if (scope != Scope::Sound) setImportedFromText (xml->getStringAttribute ("importedScl"));
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
    if (id.endsWith ("dFb")) next = std::min (next, rp.convertTo0to1 (85.0f));
    if (id.endsWith ("mix") && id.startsWith ("fx")) next = std::min (next, rp.convertTo0to1 (70.0f)); // keep dry audible
    if (id == ids::partials) next = std::max (next, rp.convertTo0to1 (4.0f));
    // Dice may land on "Imported" only when a scale was imported (else there'd be no notes).
    if (id == ids::method && importedCents.empty()) next = std::min (next, rp.convertTo0to1 (1.0f));
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
// FX rack (v0.5)
// ================================================================================================
double SeedProcessor::fxValue (int slot, const char* name) const
{
    auto* p = apvts.getParameter (ids::fx (slot, name));
    const auto it = targetOf.find (p);
    return it != targetOf.end() ? eff (p, it->second) : (double) p->convertFrom0to1 (p->getValue());
}

/// Top to bottom. A slot reads either the CHAIN (the sound so far) or its own PARTIAL BUS (only
/// the partials it picks, straight from the synth). Chain slots blend wet into the chain by Mix;
/// partial slots ADD their wet (the picked partials are already in the dry sound). Pan / width
/// shape the wet signal.
void SeedProcessor::processRack (int n)
{
    for (int sl = 1; sl <= kNumFx; ++sl)
    {
        auto& fx = fxSlots[(size_t) sl - 1];
        const int type = (int) std::lround (fxValue (sl, "type"));
        if (type != fx.lastType)
        {
            fx.delay.reset(); fx.rough.reset(); fx.chirp.reset(); fx.squash.reset(); // no stale tails
            fx.lastType = type;
        }
        if (type == FxOff || fxValue (sl, "on") < 0.5) continue;
        const bool partials = (int) std::lround (fxValue (sl, "in")) == 1;
        const float* inL = partials ? partialBus[(size_t) sl - 1].data() : dryL.data();
        const float* inR = partials ? partialBus[(size_t) sl - 1].data() : dryR.data();
        switch (type)
        {
            case FxDelay:
            {
                pvst::PartialDelay::Params dp;
                dp.timeMs = fxValue (sl, "dSync") > 0.5 ? syncDivBeats ((int) std::lround (fxValue (sl, "dDiv"))) * 60000.0 / bpm : fxValue (sl, "dTime");
                dp.feedback = fxValue (sl, "dFb") / 100.0;
                dp.spiralCents = fxValue (sl, "dSpiral");
                dp.pingPong = fxValue (sl, "dPing") / 100.0;
                dp.tone = fxValue (sl, "dTone") / 100.0;
                fx.delay.process (inL, inR, wetL.data(), wetR.data(), n, dp);
                break;
            }
            case FxRough:
                fx.rough.process (inL, inR, wetL.data(), wetR.data(), n, fxValue (sl, "rDecay"), fxValue (sl, "rDamp") / 100.0);
                break;
            case FxChirp:
            {
                pvst::ChirpReverb::Params cp;
                cp.decaySec = fxValue (sl, "cDecay"); cp.chirp = fxValue (sl, "cChirp") / 100.0;
                cp.sizeMs = fxValue (sl, "cSize"); cp.damping = fxValue (sl, "cDamp") / 100.0;
                fx.chirp.process (inL, inR, wetL.data(), wetR.data(), n, cp);
                break;
            }
            case FxSquash:
            {
                pvst::SquashCompressor::Params kp;
                kp.thresholdDb = fxValue (sl, "kThr"); kp.slope = fxValue (sl, "kSlope");
                kp.attackMs = fxValue (sl, "kAtt"); kp.releaseMs = fxValue (sl, "kRel"); kp.makeupDb = fxValue (sl, "kMake");
                fx.squash.process (inL, inR, wetL.data(), wetR.data(), n, kp);
                break;
            }
            default: continue;
        }
        const float mix = (float) juce::jlimit (0.0, 1.0, fxValue (sl, "mix") / 100.0);
        const float pan = (float) juce::jlimit (-1.0, 1.0, fxValue (sl, "pan") / 100.0);
        const float width = (float) juce::jlimit (0.0, 2.0, fxValue (sl, "width") / 100.0);
        const float gL = pan > 0.0f ? 1.0f - pan : 1.0f, gR = pan < 0.0f ? 1.0f + pan : 1.0f;
        for (int i = 0; i < n; ++i)
        {
            // Width on the wet signal (mid/side), then balance.
            const float m = 0.5f * (wetL[(size_t) i] + wetR[(size_t) i]);
            const float sd = 0.5f * (wetL[(size_t) i] - wetR[(size_t) i]) * width;
            const float l = (m + sd) * gL, r = (m - sd) * gR;
            if (partials) { dryL[(size_t) i] += mix * l; dryR[(size_t) i] += mix * r; }
            else
            {
                dryL[(size_t) i] = dryL[(size_t) i] * (1.0f - mix) + mix * l;
                dryR[(size_t) i] = dryR[(size_t) i] * (1.0f - mix) + mix * r;
            }
        }
    }
}

void SeedProcessor::swapFxSlots (int a, int b)
{
    if (a == b || a < 1 || b < 1 || a > kNumFx || b > kNumFx) return;
    const juce::String pa = "fx" + juce::String (a), pb = "fx" + juce::String (b);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = rp->getParameterID();
            if (! id.startsWith (pa) || id.substring (pa.length(), pa.length() + 1).containsOnly ("0123456789")) continue;
            auto* other = apvts.getParameter (pb + id.substring (pa.length()));
            if (other == nullptr) continue;
            const float va = rp->getValue(), vb = other->getValue();
            rp->setValueNotifyingHost (vb);
            other->setValueNotifyingHost (va);
        }
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
            setImportedFromText (apvts.state.getProperty ("importedScl").toString());
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SeedProcessor(); }
