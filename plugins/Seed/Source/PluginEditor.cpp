#include "PluginEditor.h"
#include "xen/Stretch.h"

namespace th = pvst::theme;
using namespace seed;

SeedEditor::SeedEditor (SeedProcessor& pr)
    : AudioProcessorEditor (pr), proc (pr),
      fundamentalBox (p (ids::fundamental), "Fundamental (Hz)", 4, 2, 1.0),
      seedBox (p (ids::seed), "Seed", 1, 6, 0.01),
      partialsBox (p (ids::partials), "Partials", 2, 0, 1.0),
      rolloffBox (p (ids::rolloff), "Rolloff", 1, 2, 0.1),
      minDipBox (p (ids::minDip), "Min dip (%)", 2, 1, 1.0),
      toleranceBox (p (ids::tolerance), "Tolerance (c)", 2, 1, 0.5),
      minSharedBox (p (ids::minShared), "Min shared", 1, 0, 1.0),
      methodSeg (p (ids::method), "Scale method", { "Dissonance", "Consonance" })
{
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &prevPreset, &presetName, &nextPreset, &savePreset, &scopeBtn, &fundamentalBox, &seedBox, &partialsBox,
             &rolloffBox, &methodSeg, &minDipBox, &toleranceBox, &minSharedBox, &resetEdits, &diceTuning, &curvePlot,
             &partialsPlot, &scaleTable, &mutateBtn, &diceSound })
        addAndMakeVisible (c);
    addChildComponent (presetNameEditor);
    presetNameEditor.setFont (th::sans (14.0f));
    presetNameEditor.setColour (juce::TextEditor::backgroundColourId, th::mix (0.08f));
    presetNameEditor.setColour (juce::TextEditor::textColourId, th::text());
    presetNameEditor.setColour (juce::TextEditor::outlineColourId, th::mix (0.5f));
    presetNameEditor.setTextToShowWhenEmpty ("preset name, Enter to save", th::dim());
    presetNameEditor.onReturnKey = [this] {
        if (proc.savePreset (presetNameEditor.getText())) refreshPresetLabel();
        presetNameEditor.setVisible (false);
    };
    presetNameEditor.onEscapeKey = [this] { presetNameEditor.setVisible (false); };

    // ---- Presets -------------------------------------------------------------------------------
    auto step = [this] (int dir) {
        const auto files = proc.listPresets();
        if (files.isEmpty()) return;
        int idx = -1;
        for (int i = 0; i < files.size(); ++i) if (files[i].getFileNameWithoutExtension() == proc.currentPresetName) idx = i;
        idx = (idx + dir + files.size()) % files.size();
        proc.loadPreset (files[idx], loadScope);
        refreshPresetLabel();
    };
    prevPreset.onClick = [step] { step (-1); };
    nextPreset.onClick = [step] { step (1); };
    presetName.onClick = [this] {
        juce::PopupMenu m;
        const auto files = proc.listPresets();
        for (int i = 0; i < files.size(); ++i) m.addItem (i + 1, files[i].getFileNameWithoutExtension());
        if (files.isEmpty()) m.addItem (-1, "(no presets yet - use Save as...)", false);
        m.addSeparator();
        m.addItem (1000, "Open preset folder");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName), [this, files] (int r) {
            if (r == 1000) { SeedProcessor::presetFolder().createDirectory(); SeedProcessor::presetFolder().startAsProcess(); }
            else if (r > 0 && r <= files.size()) { proc.loadPreset (files[r - 1], loadScope); refreshPresetLabel(); }
        });
    };
    savePreset.onClick = [this] { savePresetFlow(); };
    scopeBtn.onClick = [this] {
        loadScope = (SeedProcessor::Scope) (((int) loadScope + 1) % 3);
        scopeBtn.setText (loadScope == SeedProcessor::Scope::All ? "Load: All" : loadScope == SeedProcessor::Scope::Tuning ? "Load: Tuning" : "Load: Sound");
    };
    refreshPresetLabel();

    // ---- Chaos + reset ----------------------------------------------------------------------
    diceTuning.onClick = [this] { proc.randomize (SeedProcessor::Scope::Tuning, 1.0); };
    diceSound.onClick = [this] { proc.randomize (SeedProcessor::Scope::Sound, 1.0); };
    mutateBtn.onClick = [this] { proc.randomize (SeedProcessor::Scope::Sound, 0.15); };
    resetEdits.onClick = [this] {
        for (auto* prm : proc.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            {
                const auto id = rp->getParameterID();
                if (isTuningParam (id) && (id.startsWith ("p") || id.startsWith ("cp") || id.startsWith ("n") || id.startsWith ("cn"))
                    && id != ids::partials)
                    rp->setValueNotifyingHost (rp->getDefaultValue());
            }
    };

    wireCalculatorEditing();
    buildPages();
    showTab (0);

    setSize (1200, 820);
    lastTick = juce::Time::getMillisecondCounterHiRes();
    timerCallback();
    startTimerHz (60);
}

SeedEditor::~SeedEditor() { stopTimer(); }

void SeedEditor::setParam (const juce::String& id, double value)
{
    auto& prm = p (id);
    const auto& r = prm.getNormalisableRange();
    prm.setValueNotifyingHost (prm.convertTo0to1 ((float) juce::jlimit ((double) r.start, (double) r.end, value)));
}

void SeedEditor::refreshPresetLabel()
{
    presetName.setText (proc.currentPresetName.isEmpty() ? juce::String ("(no preset)") : proc.currentPresetName);
}

void SeedEditor::savePresetFlow()
{
    presetNameEditor.setText (proc.currentPresetName, false);
    presetNameEditor.setBounds (presetName.getBounds().withWidth (presetName.getWidth() + nextPreset.getWidth() + 6));
    presetNameEditor.setVisible (true);
    presetNameEditor.grabKeyboardFocus();
    presetNameEditor.selectAll();
}

// ================================================================================================
// Calculator editing: every edit is an offset parameter; removing it = the calculator decides again.
// ================================================================================================
void SeedEditor::wireCalculatorEditing()
{
    auto gestureFor = [this] (const xen::FinalNote& n, bool begin) {
        const juce::String id = n.custom ? ids::customNoteCents (-n.degree) : ids::noteOffset (n.degree);
        if (n.custom || n.degree <= xen::kEditableDegrees)
        { if (begin) p (id).beginChangeGesture(); else p (id).endChangeGesture(); }
    };
    curvePlot.onDragBegin = [gestureFor] (const xen::FinalNote& n) { gestureFor (n, true); };
    curvePlot.onDragEnd = [gestureFor] (const xen::FinalNote& n) { gestureFor (n, false); };
    curvePlot.onDrag = [this] (const xen::FinalNote& n, double cents) {
        if (n.custom) setParam (ids::customNoteCents (-n.degree), cents);
        else if (n.degree >= 1 && n.degree <= xen::kEditableDegrees) setParam (ids::noteOffset (n.degree), cents - n.home);
    };
    auto resetNote = [this] (const xen::FinalNote& n) {
        if (n.custom) setParam (ids::customNoteOn (-n.degree), 0.0);
        else if (n.degree >= 1 && n.degree <= xen::kEditableDegrees) { setParam (ids::noteOffset (n.degree), 0.0); setParam (ids::noteOn (n.degree), 1.0); }
    };
    curvePlot.onReset = resetNote;
    scaleTable.onReset = resetNote;
    auto toggleNote = [this] (const xen::FinalNote& n) {
        if (n.custom) setParam (ids::customNoteOn (-n.degree), 0.0);
        else if (n.degree >= 1 && n.degree <= xen::kEditableDegrees) setParam (ids::noteOn (n.degree), n.enabled ? 0.0 : 1.0);
    };
    curvePlot.onToggle = toggleNote;
    scaleTable.onToggle = toggleNote;
    curvePlot.onAdd = [this] (double cents) {
        for (int k = 1; k <= xen::kCustomNotes; ++k)
            if (p (ids::customNoteOn (k)).getValue() < 0.5f) { setParam (ids::customNoteCents (k), cents); setParam (ids::customNoteOn (k), 1.0); return; }
    };

    // Partials.
    auto partialIds = [] (const xen::ModelPartial& m, juce::String& posId, juce::String& lvlId) {
        if (m.custom) { const int k = m.index - xen::kSeriesPartials + 1; posId = ids::customPartialCents (k); lvlId = ids::customPartialLevel (k); }
        else { posId = m.index > 0 ? ids::partialPos (m.index + 1) : juce::String(); lvlId = ids::partialLevel (m.index + 1); }
    };
    partialsPlot.onDragBegin = [this, partialIds] (const xen::ModelPartial& m) {
        juce::String a, b; partialIds (m, a, b);
        if (a.isNotEmpty()) p (a).beginChangeGesture();
        p (b).beginChangeGesture();
    };
    partialsPlot.onDragEnd = [this, partialIds] (const xen::ModelPartial& m) {
        juce::String a, b; partialIds (m, a, b);
        if (a.isNotEmpty()) p (a).endChangeGesture();
        p (b).endChangeGesture();
    };
    partialsPlot.onDrag = [this, partialIds] (const xen::ModelPartial& m, double cents, double amp) {
        juce::String posId, lvlId; partialIds (m, posId, lvlId);
        const auto st = proc.currentCalculatorSettings();
        if (std::abs (cents - m.cents) > 1e-6 && posId.isNotEmpty())
        {
            if (m.custom) setParam (posId, cents);
            else setParam (posId, cents - 1200.0 * xen::stretchFactor (st.seed) * std::log2 ((double) m.index + 1));
        }
        if (std::abs (amp - std::min (1.0, m.amp)) > 1e-6)
        {
            const double base = m.custom ? 1.0 : xen::rolloffShape (m.index + 1, st.rolloff);
            setParam (lvlId, amp <= 1e-4 ? xen::kMuteDb : 20.0 * std::log10 (amp / base));
        }
    };
    partialsPlot.onReset = [this, partialIds] (const xen::ModelPartial& m) {
        juce::String posId, lvlId; partialIds (m, posId, lvlId);
        if (m.custom) { setParam (ids::customPartialOn (m.index - xen::kSeriesPartials + 1), 0.0); return; }
        if (posId.isNotEmpty()) setParam (posId, 0.0);
        setParam (lvlId, 0.0);
    };
    partialsPlot.onRightClick = [this, partialIds] (const xen::ModelPartial& m) {
        juce::String posId, lvlId; partialIds (m, posId, lvlId);
        if (m.custom) { setParam (ids::customPartialOn (m.index - xen::kSeriesPartials + 1), 0.0); return; }
        setParam (lvlId, m.amp > 0.0 ? xen::kMuteDb : 0.0);
    };
    partialsPlot.onAdd = [this] (double cents, double amp) {
        for (int k = 1; k <= xen::kCustomPartials; ++k)
            if (p (ids::customPartialOn (k)).getValue() < 0.5f)
            {
                setParam (ids::customPartialCents (k), cents);
                setParam (ids::customPartialLevel (k), 20.0 * std::log10 (amp));
                setParam (ids::customPartialOn (k), 1.0);
                return;
            }
    };
}

// ================================================================================================
// Synth pages
// ================================================================================================
void SeedEditor::buildPages()
{
    const char* names[kTabs] = { "Keys & pitch", "Envelope", "Timbre", "Filter & noise", "FX", "Modulators" };
    for (int t = 0; t < kTabs; ++t)
    {
        tabButtons[(size_t) t] = std::make_unique<pvst::FlatButton> (names[t]);
        tabButtons[(size_t) t]->onClick = [this, t] { showTab (t); };
        addAndMakeVisible (*tabButtons[(size_t) t]);
        addChildComponent (pages[(size_t) t]);
    }
    auto box = [this] (ControlPage& pg, const juce::String& id, const juce::String& label, int intD, int dec, double step, int w = 104) {
        auto c = std::make_unique<pvst::ScrubBox> (p (id), label, intD, dec, step);
        pg.add (*c, w);
        owned.push_back (std::move (c));
    };
    auto seg = [this] (ControlPage& pg, const juce::String& id, const juce::String& label, juce::StringArray opts, int w) {
        auto c = std::make_unique<pvst::SegmentedChoice> (p (id), label, opts);
        pg.add (*c, w);
        owned.push_back (std::move (c));
    };
    auto choice = [this] (ControlPage& pg, const juce::String& id, const juce::String& label, juce::StringArray opts, int w) {
        auto c = std::make_unique<pvst::ChoiceBox> (p (id), label, opts);
        pg.add (*c, w);
        owned.push_back (std::move (c));
    };
    auto caption = [this] (ControlPage& pg, const juce::String& text, int w) {
        auto l = std::make_unique<juce::Label> (juce::String(), text);
        l->setFont (th::sans (11.5f).withExtraKerningFactor (0.2f));
        l->setColour (juce::Label::textColourId, th::text());
        l->setJustificationType (juce::Justification::bottomLeft);
        pg.add (*l, w, 44);
        owned.push_back (std::move (l));
    };

    auto& keys = pages[0];
    seg (keys, ids::keyMode, "Key mode (how keys become pitches)", keyModeNames(), 330);
    box (keys, ids::magnet, "Magnet (%)", 3, 0, 5.0);
    box (keys, ids::life, "Life (%)", 3, 0, 5.0);
    box (keys, ids::glide, "Glide (ms)", 4, 0, 10.0);
    box (keys, ids::velocity, "Velocity (%)", 3, 0, 5.0);
    box (keys, ids::gain, "Gain (dB)", 2, 1, 0.5);
    keys.newLine();
    box (keys, ids::pitch, "Pitch shift (c)", 4, 1, 100.0, 120);
    box (keys, ids::fshift, "Freq shift (Hz)", 4, 1, 1.0, 120);
    box (keys, ids::bendRange, "Bend range (st)", 2, 0, 1.0);
    box (keys, ids::chordSize, "Chord size", 1, 0, 1.0, 90);
    box (keys, ids::chordStep, "Chord step", 2, 0, 1.0, 90);
    seg (keys, ids::drone, "Drone", { "Off", "On" }, 110);
    {
        auto c = std::make_unique<pvst::ScrubBox> (p (ids::droneNote), "Drone note", 3, 0, 1.0);
        c->suffix = [] (double n) { return juce::MidiMessage::getMidiNoteName (juce::roundToInt (n), true, true, 4); };
        keys.add (*c, 140);
        owned.push_back (std::move (c));
    }

    auto& env = pages[1];
    {
        auto ev = std::make_unique<pvst::EnvelopeView> (p (ids::attack), p (ids::decay), p (ids::sustain), p (ids::release));
        envelopeView = ev.get();
        env.add (*ev, 420, 116);
        owned.push_back (std::move (ev));
    }
    box (env, ids::attack, "Attack (ms)", 4, 0, 1.0);
    box (env, ids::decay, "Decay (ms)", 5, 0, 10.0, 112);
    box (env, ids::sustain, "Sustain (%)", 3, 1, 1.0);
    box (env, ids::release, "Release (ms)", 5, 0, 10.0, 112);

    auto& timbre = pages[2];
    caption (timbre, "ADDITIVE", 96);
    box (timbre, ids::tilt, "Tilt (dB/oct)", 2, 1, 0.5);
    box (timbre, ids::oddEven, "Odd/Even (%)", 3, 0, 5.0);
    box (timbre, ids::spread, "Stereo spread (%)", 3, 0, 5.0, 120);
    box (timbre, ids::shimmer, "Shimmer (%)", 3, 0, 5.0);
    box (timbre, ids::shimmerRate, "Shimmer (Hz)", 2, 2, 0.1);
    box (timbre, ids::formFreq, "Formant (Hz)", 5, 0, 10.0, 112);
    box (timbre, ids::formGain, "Formant (dB)", 2, 1, 1.0);
    timbre.newLine();
    caption (timbre, "", 96);
    box (timbre, ids::formWidth, "Formant (oct)", 1, 2, 0.1);
    caption (timbre, "CHAOS (synth only)", 150);
    box (timbre, ids::wander, "Wander (c)", 3, 1, 5.0);
    box (timbre, ids::wanderRate, "Wander (Hz)", 2, 2, 0.1);
    box (timbre, ids::scramble, "Scramble (%)", 3, 0, 5.0);

    auto& filt = pages[3];
    seg (filt, ids::filterMode, "Filter (per voice, after the partials)", filterModeNames(), 420);
    box (filt, ids::cutoff, "Cutoff (Hz)", 5, 0, 10.0, 112);
    box (filt, ids::reso, "Resonance (%)", 3, 0, 5.0);
    filt.newLine();
    box (filt, ids::keyTrack, "Key track (%)", 3, 0, 5.0);
    box (filt, ids::filterEnv, "Filter env (oct)", 1, 2, 0.5, 116);
    box (filt, ids::filterDecay, "Filter decay (ms)", 4, 0, 10.0, 120);
    box (filt, ids::noise, "Noise (dB)", 2, 1, 1.0);

    auto& fx = pages[4];
    caption (fx, "PARTIAL DELAY  (only the partials it picks)", 150);
    box (fx, ids::sendA, "Send (%)", 3, 0, 5.0, 90);
    choice (fx, ids::ruleA, "Picks", sendRuleNames(), 120);
    box (fx, ids::delayTime, "Time (ms)", 4, 0, 10.0, 96);
    box (fx, ids::delayFb, "Feedback (%)", 2, 0, 5.0, 96);
    box (fx, ids::spiral, "Spiral (c/repeat)", 4, 1, 50.0, 124);
    box (fx, ids::delayLevel, "Level (dB)", 2, 1, 1.0, 96);
    fx.newLine();
    caption (fx, "ROUGH REVERB  (rings on the scale, twins beat)", 150);
    box (fx, ids::sendB, "Send (%)", 3, 0, 5.0, 90);
    choice (fx, ids::ruleB, "Picks", sendRuleNames(), 120);
    box (fx, ids::rvbDecay, "Decay (s)", 2, 2, 0.5, 96);
    box (fx, ids::rough, "Roughness (%)", 3, 0, 5.0, 110);
    box (fx, ids::rvbLevel, "Level (dB)", 2, 1, 1.0, 96);

    auto& mod = pages[5];
    for (int m = 1; m <= kNumMods; ++m)
    {
        choice (mod, ids::modShape (m), "Mod " + juce::String (m) + " shape", modShapeNames(), 104);
        box (mod, ids::modRate (m), "Rate (Hz)", 2, 3, 0.1, 92);
        box (mod, ids::modDepth (m), "Depth (%)", 3, 0, 5.0, 84);
        choice (mod, ids::modTarget (m), "Target", modTargetNames(), 118);
        auto meter = std::make_unique<ModMeter> (proc.uiModValue[(size_t) m - 1]);
        meters.push_back (meter.get());
        mod.add (*meter, 44);
        owned.push_back (std::move (meter));
        if (m % 2 == 0) mod.newLine();
    }
}

void SeedEditor::showTab (int index)
{
    currentTab = juce::jlimit (0, kTabs - 1, index);
    for (int t = 0; t < kTabs; ++t)
    {
        pages[(size_t) t].setVisible (t == currentTab);
        tabButtons[(size_t) t]->setText ((t == currentTab ? "> " : "") + juce::String (std::initializer_list<const char*> {
            "Keys & pitch", "Envelope", "Timbre", "Filter & noise", "FX", "Modulators" }.begin()[t]));
    }
}

// ================================================================================================
void SeedEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.001, 0.1, (now - lastTick) / 1000.0);
    lastTick = now;

    proc.pumpCalculator(); // snappier than the processor's own 30 Hz timer while the window is open
    const auto settings = proc.currentCalculatorSettings();
    const auto& out = proc.calculatorOutput();
    const auto fin = proc.finalScale();
    curvePlot.setResult (out);
    curvePlot.setScale (fin);
    if (! proc.calculatorBusy()) busySince = 0.0;
    else if (busySince <= 0.0) busySince = now;
    curvePlot.setBusy (busySince > 0.0 && now - busySince > 150.0);

    int playable = 0, found = 0;
    for (int i = 0; i < fin.count; ++i)
    {
        playable += (fin.notes[(size_t) i].enabled && fin.notes[(size_t) i].kind != xen::ScaleNote::Period) ? 1 : 0;
        found += fin.notes[(size_t) i].kind == xen::ScaleNote::Found ? 1 : 0;
    }
    juce::String footer = juce::String (playable) + " playable" + th::dot() + juce::String (out.millis, 1) + " ms";
    if (out.settings.method == xen::Method::Consonance && ! out.searchComplete) footer << th::dot() << "search stopped early";
    scaleTable.setScale (fin, settings.fundamentalHz, footer);
    partialsPlot.setPartials (settings.modelPartials(), settings.seed, settings.fundamentalHz);

    curvePlot.tick (dt);
    partialsPlot.tick (dt);
    if (envelopeView != nullptr && currentTab == 1) envelopeView->tick (dt);
    for (auto* m : meters) m->tick();

    const auto newInfo = "Stretch factor " + juce::String (xen::stretchFactor (settings.seed), 6)
                       + th::dot() + "\"octave\" " + juce::String (xen::periodCents (settings.seed), 4) + " cents"
                       + th::dot() + juce::String (found) + " notes found" + th::dot() + "voices " + juce::String (proc.uiActiveVoices.load());
    if (newInfo != info) { info = newInfo; repaint (calcArea); }
}

void SeedEditor::paint (juce::Graphics& g)
{
    g.fillAll (th::bg);
    g.setColour (th::fg);
    g.setFont (th::sans (30.0f));
    g.drawText ("Seed", 14, 6, 90, 36, juce::Justification::centredLeft);
    g.setColour (th::dim());
    g.setFont (th::sans (11.5f).withExtraKerningFactor (0.25f));
    g.drawText ("PLAYGROUND", 94, 12, 140, 24, juce::Justification::centredLeft);
    g.setColour (th::mix (0.4f));
    g.setFont (th::sans (11.5f));
    const juce::String ver = juce::String ("v") + kVersion + th::dot() + "build " + PVST_BUILD_NUMBER + th::dot() + PVST_GIT_HASH;
    g.drawText (ver, getLocalBounds().reduced (14, 0).removeFromTop (46), juce::Justification::centredRight);

    auto calcInner = th::drawSection (g, calcArea, "Calculator");
    g.setColour (th::dim());
    g.setFont (th::sans (12.5f));
    g.drawText (info, calcInner.removeFromBottom (18), juce::Justification::centredLeft);
    th::drawSection (g, synthArea, "Synth");
    g.setColour (th::dim());
    g.setFont (th::sans (11.5f));
    g.drawText ("plays the calculator's partials and notes", synthArea.reduced (10, 6).removeFromTop (18).withTrimmedLeft (60),
                juce::Justification::centredLeft);
}

void SeedEditor::resized()
{
    constexpr int m = 12, gap = 8;
    auto b = getLocalBounds().reduced (m, 0);
    auto header = b.removeFromTop (46).withTrimmedLeft (240).withSizeKeepingCentre (b.getWidth() - 480, 28);
    header = header.withWidth (520);
    prevPreset.setBounds (header.removeFromLeft (28)); header.removeFromLeft (4);
    presetName.setBounds (header.removeFromLeft (220)); header.removeFromLeft (4);
    nextPreset.setBounds (header.removeFromLeft (28)); header.removeFromLeft (10);
    savePreset.setBounds (header.removeFromLeft (100)); header.removeFromLeft (6);
    scopeBtn.setBounds (header.removeFromLeft (110));

    calcArea = b.removeFromTop (100);
    {
        auto row = calcArea.reduced (10, 0).withTrimmedTop (26).withHeight (48);
        auto place = [&] (juce::Component& c, int w) { c.setBounds (row.removeFromLeft (w)); row.removeFromLeft (14); };
        place (fundamentalBox, 116);
        place (seedBox, 134);
        place (partialsBox, 66);
        place (rolloffBox, 78);
        place (methodSeg, 200);
        place (minDipBox, 90);
        place (toleranceBox, 98);
        place (minSharedBox, 80);
        auto buttons = row.withHeight (28).withY (row.getY() + 19);
        resetEdits.setBounds (buttons.removeFromLeft (96));
        buttons.removeFromLeft (8);
        diceTuning.setBounds (buttons.removeFromLeft (96));
    }
    b.removeFromTop (gap);
    auto middle = b.removeFromTop (430);
    scaleTable.setBounds (middle.removeFromRight (360));
    middle.removeFromRight (gap);
    curvePlot.setBounds (middle.removeFromTop (230));
    middle.removeFromTop (gap);
    partialsPlot.setBounds (middle);
    b.removeFromTop (gap);

    synthArea = b.removeFromTop (b.getHeight() - 10);
    auto inner = synthArea.reduced (10, 6).withTrimmedTop (22);
    auto tabs = inner.removeFromTop (26);
    for (int t = 0; t < kTabs; ++t) { tabButtons[(size_t) t]->setBounds (tabs.removeFromLeft (124)); tabs.removeFromLeft (4); }
    diceSound.setBounds (tabs.removeFromRight (100));
    tabs.removeFromRight (6);
    mutateBtn.setBounds (tabs.removeFromRight (110));
    for (auto& pg : pages) pg.setBounds (inner);
}
