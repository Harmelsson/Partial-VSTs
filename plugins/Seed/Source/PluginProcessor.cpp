#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace seed;

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
        mSync[(size_t) m - 1] = apvts.getParameter (ids::modSync (m));
        mBeat[(size_t) m - 1] = apvts.getParameter (ids::modBeat (m));
        if (m <= kLegacyMods) mTarget[(size_t) m - 1] = apvts.getParameter (ids::modTarget (m));
        mods[(size_t) m - 1].reset ((uint32_t) (m * 7919));
    }
    for (auto& id : modTargetIds())
        targetParams.push_back (id.isEmpty() ? nullptr : apvts.getParameter (id));
    pSeed = apvts.getParameter (ids::seed); pFund = apvts.getParameter (ids::fundamental);
    pRolloff = apvts.getParameter (ids::rolloff); pMinDip = apvts.getParameter (ids::minDip);
    pTol = apvts.getParameter (ids::tolerance);
    for (int sl = 1; sl <= kNumFx; ++sl)
    {
        auto g = [sl, this] (const char* n) { return apvts.getParameter (ids::fx (sl, n)); };
        fxp[(size_t) sl - 1] = { g ("type"), g ("on"), g ("in"), g ("picks"), g ("mix"), g ("pan"), g ("width"), g ("dTime"), g ("dSync"),
                                 g ("dBeat"), g ("dFb"), g ("dSpiral"), g ("dPing"), g ("dTone"), g ("rDecay"), g ("rRough"), g ("rDamp"),
                                 g ("cDecay"), g ("cChirp"), g ("cSize"), g ("cDamp"), g ("kThr"), g ("kSlope"), g ("kAtt"), g ("kRel"), g ("kMake") };
    }
    for (int b = 1; b <= kEqBands; ++b)
    {
        auto g = [b, this] (const char* n) { return apvts.getParameter (ids::eq (b, n)); };
        eqp[(size_t) b - 1] = { g ("on"), g ("type"), g ("freq"), g ("gain"), g ("q"), g ("steep"), g ("kt") };
    }
    const auto numParams = (size_t) getParameters().size();
    modOffset.assign (numParams, 0.0);
    uiModOffset = std::make_unique<std::atomic<float>[]> (numParams);
    for (size_t i = 0; i < numParams; ++i) uiModOffset[i].store (0.0f);
    for (auto& h : uiHeld) h.store (std::nanf (""));
    for (auto& h : uiHitCents) h.store (0.0f);

    absorbLegacyTargets();
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
double SeedProcessor::eff (juce::RangedAudioParameter* p) const
{
    double norm = p->getValue();
    const double off = modOffset[(size_t) p->getParameterIndex()];
    if (! juce::exactlyEqual (off, 0.0)) norm = juce::jlimit (0.0, 1.0, norm + off);
    return p->convertFrom0to1 ((float) norm);
}

/// Modulators -> per-parameter offsets (normalised units). Depth 100 % swings a full half-range each
/// way; each link's amount scales that. A modulator's own rate / depth can be modulated too, using the
/// other modulators' values from the previous block (so there is no ordering problem).
void SeedProcessor::computeModOffsets (double blockSeconds)
{
    auto accumulate = [this] (const std::array<double, kNumMods>& value, const std::array<double, kNumMods>& depth) {
        std::fill (modOffset.begin(), modOffset.end(), 0.0);
        for (auto& sl : linkSlots)
        {
            const int pi = sl.param.load (std::memory_order_relaxed), m = sl.mod.load (std::memory_order_relaxed);
            if (pi < 0 || m < 1 || m > kNumMods) continue;
            modOffset[(size_t) pi] += 0.5 * depth[(size_t) m - 1] * (double) sl.amount.load (std::memory_order_relaxed) * 0.01 * value[(size_t) m - 1];
        }
    };
    std::array<double, kNumMods> depth {}, value {};
    for (int m = 0; m < kNumMods; ++m) depth[(size_t) m] = mDepth[(size_t) m]->convertFrom0to1 (mDepth[(size_t) m]->getValue()) / 100.0;
    accumulate (lastModValue, depth);
    for (int m = 0; m < kNumMods; ++m)
    {
        const auto um = (size_t) m;
        const int shape = (int) mShape[um]->convertFrom0to1 (mShape[um]->getValue());
        // Hz, or synced to the host tempo with a division (straight / triplet / dotted).
        const double rate = eff (mSync[um]) > 0.5 ? (bpm / 60.0) / beatLength ((int) std::lround (eff (mBeat[um]))) : eff (mRate[um]);
        depth[um] = eff (mDepth[um]) / 100.0;
        value[um] = mods[um].advance (shape, rate, blockSeconds);
        uiModValue[um].store ((float) value[um], std::memory_order_relaxed);
    }
    lastModValue = value;
    accumulate (value, depth);
    for (auto& sl : linkSlots) // the UI's view of the same offsets
        if (const int pi = sl.param.load (std::memory_order_relaxed); pi >= 0) uiModOffset[(size_t) pi].store ((float) modOffset[(size_t) pi], std::memory_order_relaxed);
}

void SeedProcessor::buildEngineParams (double blockSeconds)
{
    computeModOffsets (blockSeconds);
    auto E = [this] (juce::RangedAudioParameter* p) { return eff (p); };
    auto V = [this] (const char* id) { return eff (P (id)); };

    // Calculator inputs (effective) — also published for the calculator pump.
    ep.seed = E (pSeed);
    ep.fundamentalHz = E (pFund);
    ep.rolloff = E (pRolloff);
    ep.partials = juce::jlimit (1, xen::kSeriesPartials, (int) std::lround (V (ids::partials)));
    effSeed.store ((float) ep.seed); effFund.store ((float) ep.fundamentalHz); effRolloff.store ((float) ep.rolloff);
    effMinDip.store ((float) E (pMinDip)); effTol.store ((float) E (pTol));
    ep.edits = partialEditsFromParams();

    // Synth.
    ep.attackMs = V (ids::attack); ep.decayMs = V (ids::decay); ep.sustain = V (ids::sustain) / 100.0;
    ep.releaseMs = V (ids::release); ep.velocitySens = V (ids::velocity) / 100.0; ep.gainDb = V (ids::gain);
    ep.keyMode = (int) std::lround (V (ids::keyMode));
    ep.baseKey = juce::jlimit (0, 127, (int) std::lround (V (ids::baseKey)));
    ep.magnet = V (ids::magnet) / 100.0; ep.life = V (ids::life) / 100.0; ep.glideMs = V (ids::glide);
    ep.pitchCents = V (ids::pitch); ep.fshiftHz = V (ids::fshift);
    ep.bendCents = bendNorm * V (ids::bendRange) * 100.0;
    ep.tiltDbOct = V (ids::tilt); ep.oddEven = V (ids::oddEven) / 100.0; ep.spread = V (ids::spread) / 100.0;
    ep.shimmer = V (ids::shimmer) / 100.0;
    ep.shimmerRate = V (ids::shimmerSync) > 0.5 ? (bpm / 60.0) / beatLength ((int) std::lround (V (ids::shimmerBeat))) : V (ids::shimmerRate);
    ep.formFreq = V (ids::formFreq); ep.formGainDb = V (ids::formGain); ep.formWidth = V (ids::formWidth);
    // v0.6: the per-voice filter and the noise layer were taken out of the UI (user request); the
    // engine still contains them, dormant, so they can come back in a rework.
    ep.noiseDb = -60.0; ep.filterMode = FilterOff;
    for (int b = 0; b < kEqBands; ++b)
    {
        const auto& q = eqp[(size_t) b];
        auto& band = ep.eq[(size_t) b];
        band.on = E (q.on) > 0.5;
        band.type = juce::jlimit (0, pvst::eq::kNumTypes - 1, (int) std::lround (E (q.type)));
        band.freq = E (q.freq); band.gainDb = E (q.gain); band.q = E (q.q);
        band.steep = juce::jlimit (1, pvst::eq::kMaxSections, (int) std::lround (E (q.steep)) + 1);
        ep.eqKeyTrack[(size_t) b] = E (q.kt) / 100.0;
    }
    for (int sl = 1; sl <= kNumFx; ++sl)
    {
        const auto& f = fxp[(size_t) sl - 1];
        const int type = (int) std::lround (E (f.type));
        ep.busOn[(size_t) sl - 1] = type != FxOff && E (f.on) > 0.5 && (int) std::lround (E (f.in)) == 1;
        ep.busPicks[(size_t) sl - 1] = E (f.picks);
    }
    ep.wanderCents = V (ids::wander);
    ep.wanderRate = V (ids::wanderSync) > 0.5 ? (bpm / 60.0) / beatLength ((int) std::lround (V (ids::wanderBeat))) : V (ids::wanderRate);
    ep.scramble = V (ids::scramble) / 100.0;
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
        if ((int) std::lround (eff (fxp[(size_t) sl - 1].type)) == FxRough)
            fxSlots[(size_t) sl - 1].rough.setTuning (hz.data(), std::max (1, count), eff (fxp[(size_t) sl - 1].rRough) / 100.0);
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
    publishHits();
}

/// Notes struck during this block go into a ring for the UI, so even a note shorter than one screen
/// refresh lights up (user: "the visual feedback is not consistently showing").
void SeedProcessor::publishHits()
{
    std::array<float, 32> hits {};
    const int n = engine.takeHits (hits.data(), (int) hits.size());
    uint32_t seq = uiHitSeq.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i) uiHitCents[seq++ % (uint32_t) kHitRing].store (hits[(size_t) i], std::memory_order_relaxed);
    if (n > 0) uiHitSeq.store (seq, std::memory_order_release);
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
    c.method = m == 1 ? xen::Method::Consonance : m == 2 ? xen::Method::Imported : m == 3 ? xen::Method::Edo
             : m == 4 ? xen::Method::SeedEdo : xen::Method::Dissonance;
    c.imported = importedCents;
    c.edoSteps = juce::jlimit (1, 120, (int) std::lround (V (ids::edoSteps)));
    c.toleranceCents = effTol.load() >= 0.0f ? effTol.load() : V (ids::tolerance);
    c.minShared = (int) std::lround (V (ids::minShared));
    c.edits = partialEditsFromParams();
    return c;
}

// ---- scale lock -----------------------------------------------------------------------------------
static juce::String rawScaleToText (const xen::RawScale& r)
{
    juce::String t (r.period, 9);
    for (int i = 0; i < r.count; ++i)
        t << ";" << juce::String (r.notes[(size_t) i].cents, 9) << "," << juce::String (r.notes[(size_t) i].strength, 6) << "," << (int) r.notes[(size_t) i].kind;
    return t;
}

void SeedProcessor::setLockedFromText (const juce::String& text)
{
    lockedRaw = {};
    const auto parts = juce::StringArray::fromTokens (text, ";", "");
    if (parts.size() >= 3)
    {
        lockedRaw.period = parts[0].getDoubleValue();
        for (int i = 1; i < parts.size() && lockedRaw.count < xen::kMaxRawNotes; ++i)
        {
            const auto f = juce::StringArray::fromTokens (parts[i], ",", "");
            if (f.size() != 3) continue;
            lockedRaw.notes[(size_t) lockedRaw.count++] = { f[0].getDoubleValue(), f[1].getDoubleValue(), (xen::ScaleNote::Kind) juce::jlimit (0, 2, f[2].getIntValue()) };
        }
        lockedRaw.valid = lockedRaw.count >= 2 && lockedRaw.period > 1.0;
        if (! lockedRaw.valid) lockedRaw = {};
    }
    apvts.state.setProperty ("lockedScale", lockedRaw.valid ? text : juce::String(), nullptr);
}

xen::RawScale SeedProcessor::rawScaleNow() const
{
    if (lockedRaw.valid && scaleLocked()) return lockedRaw;
    xen::RawScale raw;
    raw.assign (calcOut.curve);
    return raw;
}

void SeedProcessor::publishScale()
{
    const auto raw = rawScaleNow();
    const juce::SpinLock::ScopedLockType sl (scaleLock);
    sharedRaw = raw;
    ++sharedVersion;
}

void SeedProcessor::setScaleLocked (bool lock)
{
    if (lock && calcOut.curve.valid)
    {
        xen::RawScale snap;
        snap.assign (calcOut.curve);
        setLockedFromText (rawScaleToText (snap));
    }
    else if (! lock) setLockedFromText ({});
    auto* p = P (ids::scaleLock);
    p->setValueNotifyingHost (lock ? 1.0f : 0.0f);
    publishedLocked = scaleLocked();
    publishScale();
}

void SeedProcessor::pumpCalculator()
{
    worker.request (currentCalculatorSettings());
    bool changed = worker.fetch (calcOut, calcSeen);
    // The lock parameter can also change from the host (automation, preset load): keep the snapshot in step.
    const bool locked = scaleLocked();
    if (locked && ! lockedRaw.valid && calcOut.curve.valid)
    {
        xen::RawScale snap;
        snap.assign (calcOut.curve);
        setLockedFromText (rawScaleToText (snap));
        changed = true;
    }
    else if (! locked && lockedRaw.valid)
    {
        setLockedFromText ({});
        changed = true;
    }
    if (changed || locked != publishedLocked)
    {
        publishedLocked = locked;
        publishScale();
    }
}

xen::FinalScale SeedProcessor::finalScale() const
{
    return xen::applyNoteEdits (rawScaleNow(), noteEditsFromParams());
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
    const auto methodName = methodNames()[(int) cs.method];
    juce::String desc = "Seed " + juce::String (cs.seed, 4) + " (" + methodName + ")";
    if (cs.method == xen::Method::Edo) desc = juce::String (cs.edoSteps) + "-EDO (Seed)";
    if (cs.method == xen::Method::SeedEdo) desc = juce::String (cs.edoSteps) + " equal steps of seed " + juce::String (cs.seed, 4);
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
    if (lockedRaw.valid && scaleLocked()) root.setAttribute ("lockedScale", rawScaleToText (lockedRaw)); // part of the Tuning
    for (auto& l : links)
    {
        auto* e = root.createNewChildElement ("LINK");
        e->setAttribute ("mod", l.mod);
        e->setAttribute ("id", l.paramId);
        e->setAttribute ("amount", (double) l.amount);
    }
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
    if (scope != Scope::Sound)
    {
        setImportedFromText (xml->getStringAttribute ("importedScl"));
        setLockedFromText (xml->getStringAttribute ("lockedScale"));
    }
    // Modulation links follow the same split: links aimed at tuning values belong to the Tuning part.
    std::vector<ModLink> kept;
    for (auto& l : links)
    {
        const bool tuning = isTuningParam (l.paramId);
        if ((scope == Scope::Tuning && ! tuning) || (scope == Scope::Sound && tuning)) kept.push_back (l);
    }
    links = kept;
    for (auto* e : xml->getChildWithTagNameIterator ("LINK"))
    {
        const auto id = e->getStringAttribute ("id");
        const bool tuning = isTuningParam (id);
        if ((scope == Scope::Tuning && ! tuning) || (scope == Scope::Sound && tuning)) continue;
        links.push_back ({ e->getIntAttribute ("mod"), id, (float) e->getDoubleAttribute ("amount", 100.0) });
    }
    absorbLegacyTargets(); // presets from v0.5 carry single mod targets instead of links
    commitLinks();
    publishScale();
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
    // EQ: keep dicing musical-ish (no +24 dB / Q 18 needles); Dice still moves everything else freely.
    if (id.startsWith ("eq") && id.endsWith ("gain")) next = juce::jlimit (rp.convertTo0to1 (-12.0f), rp.convertTo0to1 (12.0f), next);
    if (id.startsWith ("eq") && id.endsWith ("q")) next = std::min (next, rp.convertTo0to1 (6.0f));
    // Dice may land on "Imported" only when a scale was imported (else there'd be no notes).
    if (id == ids::method && importedCents.empty() && std::lround (rp.convertFrom0to1 (next)) == 2) next = rp.convertTo0to1 (0.0f);
    rp.setValueNotifyingHost (next);
}

/// Never randomised: output gain (hearing!), drone, legacy, bend range, velocity, fundamental.
static bool neverRandomised (const juce::String& id)
{
    static const juce::StringArray never { ids::gain, ids::drone, ids::droneNote, ids::keysFollow, ids::bendRange,
                                           ids::velocity, ids::fundamental, ids::scaleLock, ids::baseKey };
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
    links.erase (std::remove_if (links.begin(), links.end(), [] (const ModLink& l) { return isTuningParam (l.paramId); }), links.end());
    commitLinks();
}

void SeedProcessor::initAll()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p)) rp->setValueNotifyingHost (rp->getDefaultValue());
    links.clear();
    setLockedFromText ({});
    commitLinks();
    publishScale();
    currentPresetName = {};
}

// ================================================================================================
// FX rack (v0.5, 12 slots since v0.6)
// ================================================================================================
/// Top to bottom. A slot reads either the CHAIN (the sound so far) or its own PARTIAL BUS (only
/// the partials it picks, straight from the synth). Chain slots blend wet into the chain by Mix;
/// partial slots ADD their wet (the picked partials are already in the dry sound). Pan / width
/// shape the wet signal.
void SeedProcessor::processRack (int n)
{
    for (int sl = 1; sl <= kNumFx; ++sl)
    {
        auto& fx = fxSlots[(size_t) sl - 1];
        const auto& f = fxp[(size_t) sl - 1];
        auto E = [this] (juce::RangedAudioParameter* p) { return eff (p); };
        const int type = (int) std::lround (E (f.type));
        if (type != fx.lastType)
        {
            fx.delay.reset(); fx.rough.reset(); fx.chirp.reset(); fx.squash.reset(); // no stale tails
            fx.lastType = type;
        }
        if (type == FxOff || E (f.on) < 0.5) continue;
        const bool partials = (int) std::lround (E (f.in)) == 1;
        const float* inL = partials ? partialBus[(size_t) sl - 1].data() : dryL.data();
        const float* inR = partials ? partialBus[(size_t) sl - 1].data() : dryR.data();
        switch (type)
        {
            case FxDelay:
            {
                pvst::PartialDelay::Params dp;
                dp.timeMs = E (f.dSync) > 0.5 ? beatLength ((int) std::lround (E (f.dBeat))) * 60000.0 / bpm : E (f.dTime);
                dp.feedback = E (f.dFb) / 100.0;
                dp.spiralCents = E (f.dSpiral);
                dp.pingPong = E (f.dPing) / 100.0;
                dp.tone = E (f.dTone) / 100.0;
                fx.delay.process (inL, inR, wetL.data(), wetR.data(), n, dp);
                break;
            }
            case FxRough:
                fx.rough.process (inL, inR, wetL.data(), wetR.data(), n, E (f.rDecay), E (f.rDamp) / 100.0);
                break;
            case FxChirp:
            {
                pvst::ChirpReverb::Params cp;
                cp.decaySec = E (f.cDecay); cp.chirp = E (f.cChirp) / 100.0;
                cp.sizeMs = E (f.cSize); cp.damping = E (f.cDamp) / 100.0;
                fx.chirp.process (inL, inR, wetL.data(), wetR.data(), n, cp);
                break;
            }
            case FxSquash:
            {
                pvst::SquashCompressor::Params kp;
                kp.thresholdDb = E (f.kThr); kp.slope = E (f.kSlope);
                kp.attackMs = E (f.kAtt); kp.releaseMs = E (f.kRel); kp.makeupDb = E (f.kMake);
                fx.squash.process (inL, inR, wetL.data(), wetR.data(), n, kp);
                break;
            }
            default: continue;
        }
        const float mix = (float) juce::jlimit (0.0, 1.0, E (f.mix) / 100.0);
        const float pan = (float) juce::jlimit (-1.0, 1.0, E (f.pan) / 100.0);
        const float width = (float) juce::jlimit (0.0, 2.0, E (f.width) / 100.0);
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
    // Modulation links travel with the slot's settings.
    auto swapPrefix = [&] (juce::String& id) {
        for (auto& [from, to] : { std::pair { pa, pb }, std::pair { pb, pa } })
            if (id.startsWith (from) && ! id.substring (from.length(), from.length() + 1).containsOnly ("0123456789"))
            {
                id = to + id.substring (from.length());
                return;
            }
    };
    for (auto& l : links) swapPrefix (l.paramId);
    commitLinks();
}

// ================================================================================================
// Modulation links (v0.6)
// ================================================================================================
juce::String SeedProcessor::linksToText (const std::vector<ModLink>& l)
{
    juce::StringArray parts;
    for (auto& k : l) parts.add (juce::String (k.mod) + "|" + k.paramId + "|" + juce::String (k.amount, 3));
    return parts.joinIntoString (";");
}

void SeedProcessor::loadLinksFromText (const juce::String& text)
{
    links.clear();
    for (auto& part : juce::StringArray::fromTokens (text, ";", ""))
    {
        const auto f = juce::StringArray::fromTokens (part, "|", "");
        if (f.size() != 3) continue;
        const int m = f[0].getIntValue();
        if (m < 1 || m > kNumMods || dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (f[1])) == nullptr) continue;
        links.push_back ({ m, f[1], f[2].getFloatValue() });
    }
    commitLinks();
}

void SeedProcessor::commitLinks()
{
    if ((int) links.size() > kMaxLinks) links.resize ((size_t) kMaxLinks);
    for (size_t i = 0; i < linkSlots.size(); ++i)
    {
        auto& sl = linkSlots[i];
        sl.param.store (-1, std::memory_order_relaxed); // never let the audio thread see a half-written slot
        if (i >= links.size()) continue;
        auto* p = apvts.getParameter (links[i].paramId);
        if (p == nullptr) continue;
        sl.mod.store (links[i].mod, std::memory_order_relaxed);
        sl.amount.store (links[i].amount, std::memory_order_relaxed);
        sl.param.store (p->getParameterIndex(), std::memory_order_release);
    }
    apvts.state.setProperty ("modLinks", linksToText (links), nullptr);
    ++linksEpoch;
}

std::vector<SeedProcessor::ModLink> SeedProcessor::modLinks() const { return links; }

bool SeedProcessor::addModLink (int mod, const juce::String& paramId, float amountPercent)
{
    if (mod < 1 || mod > kNumMods || (int) links.size() >= kMaxLinks) return false;
    if (dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (paramId)) == nullptr) return false;
    for (auto& l : links) if (l.mod == mod && l.paramId == paramId) return false;
    links.push_back ({ mod, paramId, juce::jlimit (-100.0f, 100.0f, amountPercent) });
    commitLinks();
    return true;
}

void SeedProcessor::removeModLink (int mod, const juce::String& paramId)
{
    links.erase (std::remove_if (links.begin(), links.end(), [&] (const ModLink& l) { return l.mod == mod && l.paramId == paramId; }), links.end());
    commitLinks();
}

void SeedProcessor::setModLinkAmount (int mod, const juce::String& paramId, float amountPercent)
{
    for (auto& l : links)
        if (l.mod == mod && l.paramId == paramId) l.amount = juce::jlimit (-100.0f, 100.0f, amountPercent);
    commitLinks();
}

void SeedProcessor::clearModLinks() { links.clear(); commitLinks(); }

unsigned SeedProcessor::modMaskFor (const juce::String& paramId) const
{
    unsigned mask = 0;
    for (auto& l : links) if (l.paramId == paramId) mask |= 1u << (l.mod - 1);
    return mask;
}

float SeedProcessor::modOffsetNorm (const juce::RangedAudioParameter& p) const
{
    return uiModOffset[(size_t) p.getParameterIndex()].load (std::memory_order_relaxed);
}

/// v0.5 projects and presets had ONE target per modulator (a choice parameter). Turn those into
/// links (amount 100 %, same maths) and clear the old parameter.
void SeedProcessor::absorbLegacyTargets()
{
    bool any = false;
    for (int m = 1; m <= kLegacyMods; ++m)
    {
        auto* tp = mTarget[(size_t) m - 1];
        const int idx = (int) std::lround (tp->convertFrom0to1 (tp->getValue()));
        if (idx <= 0 || idx >= (int) targetParams.size() || targetParams[(size_t) idx] == nullptr) continue;
        const auto id = targetParams[(size_t) idx]->getParameterID();
        bool have = false;
        for (auto& l : links) have |= (l.mod == m && l.paramId == id);
        if (! have) links.push_back ({ m, id, 100.0f });
        tp->setValueNotifyingHost (tp->getDefaultValue());
        any = true;
    }
    if (any) commitLinks();
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
            setLockedFromText (apvts.state.getProperty ("lockedScale").toString());
            loadLinksFromText (apvts.state.getProperty ("modLinks").toString());
            absorbLegacyTargets();
            publishedLocked = false;
            publishScale();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SeedProcessor(); }
