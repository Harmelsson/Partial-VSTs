#include "PluginEditor.h"
#include "xen/Stretch.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

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
      edoStepsBox (p (ids::edoSteps), "Steps", 3, 0, 1.0),
      methodBox (p (ids::method), "Scale method", seed::methodNames()),
      modPanel (pr, [this] (const juce::String& id) { locateParam (id); })
{
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &prevPreset, &presetName, &nextPreset, &savePreset, &scopeBtn, &fundamentalBox, &seedBox, &partialsBox,
             &rolloffBox, &methodBox, &minDipBox, &toleranceBox, &minSharedBox, &edoStepsBox, &lockScale, &resetEdits,
             &diceTuning, &lockPartials, &scaleDown, &scaleLabel, &scaleUp, &curvePlot, &partialsPlot, &scaleTable, &partialsTable, &dicePage, &mutatePage, &resetPage,
             &initAll, &importScl, &exportScl, &fitSeed, &modPanel })
        addAndMakeVisible (c);

    // Tooltips for the calculator row (hover ~0.6 s).
    fundamentalBox.setTooltip (seed::helpFor (ids::fundamental)); seedBox.setTooltip (seed::helpFor (ids::seed));
    partialsBox.setTooltip (seed::helpFor (ids::partials)); rolloffBox.setTooltip (seed::helpFor (ids::rolloff));
    minDipBox.setTooltip (seed::helpFor (ids::minDip)); toleranceBox.setTooltip (seed::helpFor (ids::tolerance));
    minSharedBox.setTooltip (seed::helpFor (ids::minShared)); edoStepsBox.setTooltip (seed::helpFor (ids::edoSteps));
    methodBox.setTooltip ("How the scale's notes are found: Dissonance / Consonance = calculated from the partials; Imported = a Scala file; EDO = N equal steps of 1200 cents; Seed-EDO = N equal steps of the seed's 'octave'.");
    lockScale.setTooltip ("Freeze the scale: the notes stay exactly where they are while you change the seed, partials or method. The curve keeps updating so you can see the locked notes against the new sound. Unlocking recalculates the scale (you are asked first).");
    lockScale.onClick = [this] { onLockClicked(); };
    lockPartials.setTooltip ("Freeze the partials (seed stretch, rolloff and count): the timbre stays exactly as it is while you change the seed or rolloff. The seed then only sets the 'octave' / scale period. Hand edits still apply on top. Unlocking recalculates the partials (you are asked first).");
    lockPartials.onClick = [this] { onLockPartialsClicked(); };
    // Window scale (25..200 %).
    scaleDown.setTooltip ("Smaller window"); scaleUp.setTooltip ("Bigger window"); scaleLabel.setTooltip ("Window size 25-200 % (click for a list)");
    scaleDown.onClick = [this] { static const int steps[] = { 25, 33, 40, 50, 60, 67, 75, 80, 90, 100, 110, 125, 150, 175, 200 }; int cur = proc.uiScale(), next = 25; for (int v : steps) if (v < cur) next = v; changeScale (next); };
    scaleUp.onClick = [this] { static const int steps[] = { 25, 33, 40, 50, 60, 67, 75, 80, 90, 100, 110, 125, 150, 175, 200 }; int cur = proc.uiScale(), next = 200; for (int i = (int) std::size (steps) - 1; i >= 0; --i) if (steps[i] > cur) next = steps[i]; changeScale (next); };
    scaleLabel.onClick = [this] {
        juce::PopupMenu m;
        for (int v : { 25, 33, 40, 50, 60, 67, 75, 80, 90, 100, 110, 125, 150, 175, 200 }) m.addItem (v, juce::String (v) + " %", true, v == proc.uiScale());
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&scaleLabel), [this] (int r) { if (r > 0) changeScale (r); });
    };
    scaleLabel.setText (juce::String (proc.uiScale()) + "%");
    addMouseListener (this, true); // to hand the keyboard back to the host after a click (mouseUp)

    // ---- Scala ---------------------------------------------------------------------------------
    // Import = load a .scl as the scale (method -> Imported; the partials stay yours, the curve shows
    // how rough the timbre is on those notes). Export = the scale the keyboard plays now (edits
    // included) as .scl + a .kbm (key 60 = unison at the Fundamental). Fit seed = search the seed whose
    // stretched partials put the scale's notes in dissonance valleys (Sethares: fit timbre to tuning).
    importScl.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser> ("Import a Scala scale", juce::File(), "*.scl");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) {
                                  const auto f = fc.getResult();
                                  if (! f.existsAsFile()) return;
                                  if (proc.importScl (f.loadFileAsString()))
                                      showScalaMessage ("Imported " + proc.importedScaleName() + " (" + juce::String (proc.importedScaleSize()) + " notes)");
                                  else
                                      showScalaMessage ("Could not read " + f.getFileName());
                              });
    };
    exportScl.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser> ("Export the scale as Scala", juce::File(), "*.scl");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc) {
                                  auto f = fc.getResult();
                                  if (f == juce::File()) return;
                                  f = f.withFileExtension ("scl");
                                  const bool ok = f.replaceWithText (proc.exportScl())
                                               && f.withFileExtension ("kbm").replaceWithText (proc.exportKbm());
                                  showScalaMessage (ok ? "Saved " + f.getFileName() + " + .kbm" : "Could not save " + f.getFileName());
                              });
    };
    fitSeed.onClick = [this] {
        const double s = proc.fitSeedToCurrentScale();
        showScalaMessage ("Fitted seed " + juce::String (s, 4) + " to the scale");
    };
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
    // The FX page's buttons act on the selected rack slot.
    auto idsOfPage = [this] {
        if (currentTab != 3 || fxPage == nullptr) return pageIds[(size_t) currentTab];
        auto a = fxPage->rack.pageIds();
        a.addArray (fxPage->eq.bandIds());
        return a;
    };
    dicePage.onClick = [this, idsOfPage] { proc.randomizeParams (idsOfPage(), 1.0); };
    mutatePage.onClick = [this, idsOfPage] { proc.randomizeParams (idsOfPage(), 0.15); };
    resetPage.onClick = [this, idsOfPage] { proc.resetParams (idsOfPage()); };
    initAll.onClick = [this] { proc.initAll(); refreshPresetLabel(); };
    // Every hand edit, Scale tick and calculator-aimed modulation goes; core settings stay.
    resetEdits.onClick = [this] { proc.resetCalculatorEdits(); };
    partialsTable.onToggleScale = [this] (const xen::ModelPartial& m) {
        const juce::String id = m.custom ? ids::customPartialInScale (m.index - xen::kSeriesPartials + 1) : ids::partialInScale (m.index + 1);
        setParam (id, m.inScale ? 0.0 : 1.0);
    };

    wireCalculatorEditing();
    buildPages();
    showTab (0);

    setSize (kMainW + kModW + 12, 960);
    addAndMakeVisible (glow); // last = on top
    setScaleFactor ((float) proc.uiScale() / 100.0f);
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

void SeedEditor::onLockClicked()
{
    if (! proc.scaleLocked()) { proc.setScaleLocked (true); return; }
    // Unlocking recalculates the scale from the current settings: the notes will change. Ask first.
    juce::Component::SafePointer<SeedEditor> safe (this);
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::WarningIcon)
                                      .withTitle ("Unlock the scale?")
                                      .withMessage ("The scale will change.\n\nUnlocking recalculates the notes from the current seed, partials and method, "
                                                    "so the notes you have now will be replaced.")
                                      .withButton ("Unlock")
                                      .withButton ("Keep locked")
                                      .withAssociatedComponent (this),
                                  [safe] (int result) { if (safe != nullptr && result == 1) safe->proc.setScaleLocked (false); });
}

void SeedEditor::modDroppedOn (juce::RangedAudioParameter& prm, int mod)
{
    proc.addModLink (mod, prm.getParameterID(), 100.0f);
}

void SeedEditor::onLockPartialsClicked()
{
    if (! proc.partialsLocked()) { proc.setPartialsLocked (true); return; }
    juce::Component::SafePointer<SeedEditor> safe (this);
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withIconType (juce::MessageBoxIconType::WarningIcon)
                                      .withTitle ("Unlock the partials?")
                                      .withMessage ("The partials will change.\n\nUnlocking recalculates them from the current seed, rolloff and partial count, "
                                                    "so the timbre you have now will be replaced.")
                                      .withButton ("Unlock")
                                      .withButton ("Keep locked")
                                      .withAssociatedComponent (this),
                                  [safe] (int result) { if (safe != nullptr && result == 1) safe->proc.setPartialsLocked (false); });
}

void SeedEditor::changeScale (int percent)
{
    percent = juce::jlimit (25, 200, percent);
    proc.setUiScale (percent);
    scaleLabel.setText (juce::String (percent) + "%");
    setScaleFactor ((float) percent / 100.0f);
}

/// Windows / FL Studio: a click inside a plugin window leaves the keyboard focus on the plugin's own window,
/// so FL's typing-keyboard-as-piano stops working until you click FL's window border. After a click we hand
/// the focus back to the window that contains the plugin (what clicking its border does) unless we are
/// typing into one of our own text boxes or a menu / dialog is open.
void SeedEditor::mouseUp (const juce::MouseEvent&) { releaseFocusToHost(); }

void SeedEditor::releaseFocusToHost()
{
#if JUCE_WINDOWS
    juce::Component::SafePointer<SeedEditor> safe (this);
    juce::Timer::callAfterDelay (120, [safe] {
        if (safe == nullptr) return;
        if (dynamic_cast<juce::TextEditor*> (juce::Component::getCurrentlyFocusedComponent()) != nullptr) return;
        if (juce::ModalComponentManager::getInstance()->getNumModalComponents() > 0) return;
        auto& desktop = juce::Desktop::getInstance();
        for (int i = 0; i < desktop.getNumComponents(); ++i)
            if (auto* c = desktop.getComponent (i); c != nullptr && c->getName() == "menu" && c->isVisible()) return; // a popup menu is open
        if (auto* peer = safe->getPeer())
            if (auto* hwnd = (HWND) peer->getNativeHandle())
                if (auto* root = ::GetAncestor (hwnd, GA_ROOT); root != nullptr && root != hwnd) ::SetFocus (root);
    });
#endif
}

// ================================================================================================
// "Where is this control?" (click a modulator link): open its tab, scroll to it, light it up.
// ================================================================================================
static juce::Component* findBoundControl (juce::Component& parent, const juce::String& id)
{
    for (auto* c : parent.getChildren())
    {
        if (auto* pb = dynamic_cast<pvst::ParamBound*> (c); pb != nullptr && pb->usesParam (id)) return c;
        if (auto* deeper = findBoundControl (*c, id)) return deeper;
    }
    return nullptr;
}

void SeedEditor::locateParam (const juce::String& id)
{
    auto isNum = [] (const juce::String& t) { return t.isNotEmpty() && t.containsOnly ("0123456789"); };
    // FX slot / EQ band: select it first so its controls exist.
    if (id.startsWith ("fx") && isNum (id.substring (2, 3)) && fxPage != nullptr) { showTab (3); fxPage->rack.selectSlot (id.substring (2).getIntValue()); }
    else if (id.startsWith ("eq") && isNum (id.substring (2, 3)) && fxPage != nullptr) { showTab (3); fxPage->eq.selectBand (id.substring (2).getIntValue()); }

    juce::Component* target = findBoundControl (*this, id);
    // Things without a control of their own live in the plots / tables.
    if (target == nullptr)
    {
        const auto tail3 = id.length() > 3 ? id.substring (id.length() - 3) : juce::String();
        if (id.startsWith ("cp")) target = id.endsWith ("scl") ? static_cast<juce::Component*> (&partialsTable) : &partialsPlot;
        else if (id.startsWith ("cn")) target = &curvePlot;
        else if (id.startsWith ("p") && isNum (id.substring (1, 2)) && (id.endsWith ("lvl") || id.endsWith ("pos"))) target = &partialsPlot;
        else if (id.startsWith ("p") && isNum (id.substring (1, 2)) && tail3 == "scl") target = &partialsTable;
        else if (id.startsWith ("n") && isNum (id.substring (1, 2))) target = id.endsWith ("on") ? static_cast<juce::Component*> (&scaleTable) : &curvePlot;
    }
    if (target == nullptr) return;
    for (int t = 0; t < kTabs; ++t) if (pages[(size_t) t].isParentOf (target)) showTab (t);
    // Scroll any viewport the control sits in so it is on screen.
    for (auto* vp = target->findParentComponentOfClass<juce::Viewport>(); vp != nullptr; vp = vp->findParentComponentOfClass<juce::Viewport>())
    {
        if (auto* viewed = vp->getViewedComponent())
        {
            const auto pos = viewed->getLocalPoint (target, juce::Point<int> (0, 0));
            if (pos.y < vp->getViewPositionY() || pos.y + target->getHeight() > vp->getViewPositionY() + vp->getViewHeight())
                vp->setViewPosition (0, juce::jmax (0, pos.y - 12));
        }
    }
    glow.flash (getLocalArea (target, target->getLocalBounds()));
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
            else setParam (posId, cents - 1200.0 * xen::stretchFactor (st.partialSeed()) * std::log2 ((double) m.index + 1));
        }
        if (std::abs (amp - std::min (1.0, m.amp)) > 1e-6)
        {
            const double base = m.custom ? 1.0 : xen::rolloffShape (m.index + 1, st.partialRolloff());
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
    // A modulator chip dropped on a note / partial / tick (v0.7: everything you can edit with the mouse can be modulated).
    curvePlot.onModDrop = [this] (const xen::FinalNote& n, int mod) {
        if (n.custom) pvst::chooseAndLink (curvePlot, { { "Custom note: position", &p (ids::customNoteCents (-n.degree)) }, { "Custom note: on / off", &p (ids::customNoteOn (-n.degree)) } }, mod);
        else if (n.degree >= 1 && n.degree <= xen::kEditableDegrees)
            pvst::chooseAndLink (curvePlot, { { "Note " + juce::String (n.degree) + ": position", &p (ids::noteOffset (n.degree)) }, { "Note " + juce::String (n.degree) + ": on / off", &p (ids::noteOn (n.degree)) } }, mod);
    };
    scaleTable.onModDrop = [this] (const xen::FinalNote& n, int mod) {
        if (n.custom) proc.addModLink (mod, ids::customNoteOn (-n.degree));
        else if (n.degree >= 1 && n.degree <= xen::kEditableDegrees) proc.addModLink (mod, ids::noteOn (n.degree));
    };
    partialsPlot.onModDrop = [this] (const xen::ModelPartial& m, int mod) {
        if (m.custom)
        {
            const int k = m.index - xen::kSeriesPartials + 1;
            pvst::chooseAndLink (partialsPlot, { { "Custom partial " + juce::String (k) + ": position", &p (ids::customPartialCents (k)) }, { "Custom partial " + juce::String (k) + ": loudness", &p (ids::customPartialLevel (k)) } }, mod);
        }
        else if (m.index == 0) proc.addModLink (mod, ids::partialLevel (1));
        else pvst::chooseAndLink (partialsPlot, { { "Partial " + juce::String (m.index + 1) + ": loudness", &p (ids::partialLevel (m.index + 1)) }, { "Partial " + juce::String (m.index + 1) + ": position", &p (ids::partialPos (m.index + 1)) } }, mod);
    };
    partialsTable.onModDrop = [this] (const xen::ModelPartial& m, int mod) {
        proc.addModLink (mod, m.custom ? ids::customPartialInScale (m.index - xen::kSeriesPartials + 1) : ids::partialInScale (m.index + 1));
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
    const char* names[kTabs] = { "Keys & pitch", "Envelope", "Timbre", "FX" };
    for (int t = 0; t < kTabs; ++t)
    {
        tabButtons[(size_t) t] = std::make_unique<pvst::FlatButton> (names[t]);
        tabButtons[(size_t) t]->onClick = [this, t] { showTab (t); };
        addAndMakeVisible (*tabButtons[(size_t) t]);
        addChildComponent (pages[(size_t) t]);
    }
    auto pageOf = [this] (ControlPage& pg) -> juce::StringArray& { return pageIds[(size_t) (&pg - pages.data())]; };
    auto box = [this, pageOf] (ControlPage& pg, const juce::String& id, const juce::String& label, int intD, int dec, double step, int w = 104) {
        pageOf (pg).add (id);
        auto c = std::make_unique<pvst::ScrubBox> (p (id), label, intD, dec, step);
        c->setTooltip (seed::helpFor (id));
        pg.add (*c, w);
        owned.push_back (std::move (c));
    };
    auto seg = [this, pageOf] (ControlPage& pg, const juce::String& id, const juce::String& label, juce::StringArray opts, int w) {
        pageOf (pg).add (id);
        auto c = std::make_unique<pvst::SegmentedChoice> (p (id), label, opts);
        c->setTooltip (seed::helpFor (id));
        pg.add (*c, w);
        owned.push_back (std::move (c));
    };
    /// Hz-or-BPM rate in ONE control (v0.6): Hz box, or a scrollable division with straight / t / dotted versions.
    auto rate = [this, pageOf] (ControlPage& pg, const char* syncId, const char* hzId, const char* beatId, const juce::String& label,
                                int intD, int dec, double step, int w, const juce::String& help) {
        for (auto* id : { syncId, hzId, beatId }) pageOf (pg).add (id);
        auto c = std::make_unique<pvst::RateControl> (p (syncId), p (hzId), p (beatId), label, intD, dec, step);
        c->setHelp (help);
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
    {
        auto c = std::make_unique<pvst::ScrubBox> (p (ids::baseKey), "Base key (plays the unison)", 3, 0, 1.0);
        c->suffix = [] (double n) { return juce::MidiMessage::getMidiNoteName (juce::roundToInt (n), true, true, 4); };
        c->setTooltip (seed::helpFor (ids::baseKey));
        pageIds[0].add (ids::baseKey);
        keys.add (*c, 190);
        owned.push_back (std::move (c));
    }
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
        c->setTooltip (seed::helpFor (ids::droneNote));
        pageIds[0].add (ids::droneNote);
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
    // 2 decimals of a millisecond, down to 0 ms (user v0.3.1).
    box (env, ids::attack, "Attack (ms)", 4, 2, 1.0, 124);
    box (env, ids::decay, "Decay (ms)", 5, 2, 10.0, 134);
    box (env, ids::sustain, "Sustain (%)", 3, 1, 1.0);
    box (env, ids::release, "Release (ms)", 5, 2, 10.0, 134);

    auto& timbre = pages[2];
    caption (timbre, "ADDITIVE", 96);
    box (timbre, ids::tilt, "Tilt (dB/oct)", 2, 1, 0.5);
    box (timbre, ids::oddEven, "Odd/Even (%)", 3, 0, 5.0);
    box (timbre, ids::spread, "Stereo spread (%)", 3, 0, 5.0, 120);
    box (timbre, ids::shimmer, "Shimmer (%)", 3, 0, 5.0);
    rate (timbre, ids::shimmerSync, ids::shimmerRate, ids::shimmerBeat, "Shimmer rate", 2, 2, 0.1, 210, seed::helpFor (ids::shimmerRate));
    box (timbre, ids::formFreq, "Formant (Hz)", 5, 0, 10.0, 112);
    box (timbre, ids::formGain, "Formant (dB)", 2, 1, 1.0);
    timbre.newLine();
    caption (timbre, "", 96);
    box (timbre, ids::formWidth, "Formant (oct)", 1, 2, 0.1);
    caption (timbre, "CHAOS (synth only)", 150);
    box (timbre, ids::wander, "Wander (c)", 3, 1, 5.0);
    rate (timbre, ids::wanderSync, ids::wanderRate, ids::wanderBeat, "Wander rate", 2, 2, 0.1, 210, seed::helpFor (ids::wanderRate));
    box (timbre, ids::scramble, "Scramble (%)", 3, 0, 5.0);

    auto& fx = pages[3];
    {
        auto page = std::make_unique<FxPage> (proc);
        fxPage = page.get();
        fx.fill (*page);
        owned.push_back (std::move (page));
    }
}

void SeedEditor::showTab (int index)
{
    currentTab = juce::jlimit (0, kTabs - 1, index);
    static const char* names[kTabs] = { "Keys & pitch", "Envelope", "Timbre", "FX" };
    for (int t = 0; t < kTabs; ++t)
    {
        pages[(size_t) t].setVisible (t == currentTab);
        tabButtons[(size_t) t]->setText ((t == currentTab ? "> " : "") + juce::String (names[t]));
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
    const auto model = settings.modelPartials();
    partialsPlot.setPartials (model, settings.seed, settings.fundamentalHz);
    partialsTable.setPartials (model, settings.fundamentalHz);

    // Played notes light up (ArtHar). A held key's note = its pitch folded into one period, matched to the
    // nearest scale note (always the nearest: in Scale mode it is exact, but while the seed is being
    // scrubbed the engine's scale and the drawn one can differ by many cents; the other key modes have
    // keys that fall between scale notes). Every note-on is also latched by the
    // processor, so a note shorter than one refresh still lights for at least 140 ms (v0.6: the old
    // code only looked at the keys held at the instant of each refresh, so short notes flickered).
    // Released notes fade (0.18 s half-life).
    {
        emphasis.resize ((size_t) fin.count, 0.0f);
        litUntil.resize ((size_t) fin.count, 0.0);
        std::vector<bool> lit ((size_t) fin.count, false);
        const double period = std::max (1.0, fin.period);
        auto light = [&] (double cents, bool latch) {
            double pc = std::fmod (cents, period);
            if (pc < 0) pc += period;
            int best = -1;
            double bestD = 1e18;
            for (int i = 0; i < fin.count; ++i)
            {
                const auto& n = fin.notes[(size_t) i];
                if (! n.enabled || n.kind == xen::ScaleNote::Period) continue;
                double d = std::abs (n.cents - pc);
                if (n.kind == xen::ScaleNote::Unison) d = std::min (d, period - pc); // the unison is also the period
                if (d < bestD) { bestD = d; best = i; }
            }
            if (best < 0) return;
            lit[(size_t) best] = true;
            if (latch) litUntil[(size_t) best] = now + 140.0;
        };
        for (auto& h : proc.uiHeld)
        {
            const float c = h.load (std::memory_order_relaxed);
            if (! std::isnan (c)) light ((double) c, false);
        }
        const uint32_t seq = proc.uiHitSeq.load (std::memory_order_acquire);
        if (seq - hitsSeen > (uint32_t) SeedProcessor::kHitRing) hitsSeen = seq - (uint32_t) SeedProcessor::kHitRing;
        for (; hitsSeen != seq; ++hitsSeen)
            light ((double) proc.uiHitCents[hitsSeen % (uint32_t) SeedProcessor::kHitRing].load (std::memory_order_relaxed), true);
        for (size_t i = 0; i < lit.size(); ++i) if (now < litUntil[i]) lit[i] = true;
        const float fade = std::pow (0.5f, dt / 0.18f);
        for (size_t i = 0; i < emphasis.size(); ++i) emphasis[i] = lit[i] ? 1.0f : (emphasis[i] * fade < 0.01f ? 0.0f : emphasis[i] * fade);
        curvePlot.setEmphasis (emphasis);
    }

    // Where the keys land (v0.5, simplified in v0.7: the keyboard always plays the scale). The base key
    // (default C4 = 60) = the unison; each key above is the next note of the scale, wrapping every period.
    {
        const double per = fin.valid ? fin.period : std::abs (xen::periodCents (settings.seed));
        std::array<double, xen::kMaxFinalNotes> pc {}, pst {};
        const int np = fin.valid ? fin.playable (pc.data(), pst.data(), (int) pc.size()) : 0;
        const int baseKey = (int) std::lround (p (ids::baseKey).convertFrom0to1 (p (ids::baseKey).getValue()));
        std::vector<pvst::CurvePlot::KeyMarker> km;
        for (int k = 0; np > 0 && k < 40; ++k)
            km.push_back ({ (k / np) * per + pc[(size_t) (k % np)], juce::MidiMessage::getMidiNoteName (baseKey + k, true, false, 4) });
        curvePlot.setKeyMarkers (std::move (km), false, 0.0);
    }
    scaleTable.setEmphasis (emphasis);

    // Only the chosen method's settings are shown (user v0.3.1).
    const int method = (int) std::lround (p (ids::method).convertFrom0to1 (p (ids::method).getValue()));
    if (method != shownMethod) { shownMethod = method; layoutCalculatorRow(); }


    curvePlot.tick (dt);
    partialsPlot.tick (dt);
    if (envelopeView != nullptr && currentTab == 1) envelopeView->tick (dt);
    modPanel.tick();
    if (fxPage != nullptr && currentTab == 3) { fxPage->rack.tick(); fxPage->eq.tick(); }
    const bool locked = proc.scaleLocked();
    curvePlot.setLocked (locked);
    lockScale.setActive (locked);
    lockScale.setText (locked ? "SCALE LOCKED" : "Lock scale");
    const bool pLocked = proc.partialsLocked();
    partialsPlot.setLocked (pLocked);
    lockPartials.setActive (pLocked);
    lockPartials.setText (pLocked ? "PARTIALS LOCKED" : "Lock partials");

    const auto newInfo = "Stretch factor " + juce::String (xen::stretchFactor (settings.seed), 6)
                       + th::dot() + "\"octave\" " + juce::String (xen::periodCents (settings.seed), 4) + " cents"
                       + th::dot() + juce::String (found) + " notes found" + th::dot() + "voices " + juce::String (proc.uiActiveVoices.load());
    auto newInfoText = newInfo;
    if (settings.method == xen::Method::Imported)
        newInfoText = (proc.importedScaleName().isEmpty() ? juce::String ("Imported: nothing loaded, use Import .scl")
                                                          : "Imported: " + proc.importedScaleName())
                    + th::dot() + "period " + juce::String (fin.period, 2) + " c" + th::dot() + "Stretch factor "
                    + juce::String (xen::stretchFactor (settings.seed), 4);
    if (settings.method == xen::Method::Edo || settings.method == xen::Method::SeedEdo)
    {
        const double per = settings.method == xen::Method::Edo ? 1200.0 : std::abs (xen::periodCents (settings.seed));
        newInfoText = juce::String (settings.edoSteps) + (settings.method == xen::Method::Edo ? " equal steps of 1200 c" : " equal steps of the seed's \"octave\"")
                    + th::dot() + "step " + juce::String (per / (double) settings.edoSteps, 3) + " c" + th::dot() + "period " + juce::String (per, 2) + " c";
    }
    if (proc.scaleLocked()) newInfoText = "SCALE LOCKED" + th::dot() + newInfoText;
    if (now < scalaMessageUntil) newInfoText = scalaMessage;
    if (newInfoText != info) { info = newInfoText; repaint (calcArea); }
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
    g.drawText (ver, juce::Rectangle<int> (14, 2, kMainW - 28, 20), juce::Justification::centredRight);

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
    // The modulator column lives at the far right; everything else is the old 1200-wide layout.
    auto full = getLocalBounds();
    modPanel.setBounds (full.removeFromRight (kModW + 12).withTrimmedRight (12).withTrimmedTop (46).withTrimmedBottom (10));
    auto b = full.reduced (m, 0);
    auto header = b.removeFromTop (46).withTrimmedLeft (240).withSizeKeepingCentre (b.getWidth() - 480, 28);
    header = header.withWidth (590);
    prevPreset.setBounds (header.removeFromLeft (28)); header.removeFromLeft (4);
    presetName.setBounds (header.removeFromLeft (220)); header.removeFromLeft (4);
    nextPreset.setBounds (header.removeFromLeft (28)); header.removeFromLeft (10);
    savePreset.setBounds (header.removeFromLeft (100)); header.removeFromLeft (6);
    scopeBtn.setBounds (header.removeFromLeft (110));
    header.removeFromLeft (6);
    initAll.setBounds (header.removeFromLeft (60));
    {   // window scale, left of the version text
        auto sc = juce::Rectangle<int> (kMainW - 14 - 108, 22, 108, 22);
        scaleDown.setBounds (sc.removeFromLeft (24)); sc.removeFromLeft (2);
        scaleLabel.setBounds (sc.removeFromLeft (56)); sc.removeFromLeft (2);
        scaleUp.setBounds (sc.removeFromLeft (24));
    }
    glow.setBounds (getLocalBounds());

    calcArea = b.removeFromTop (100);
    layoutCalculatorRow();
    b.removeFromTop (gap);
    auto middle = b.removeFromTop (420);
    auto right = middle.removeFromRight (360);
    middle.removeFromRight (gap);
    curvePlot.setBounds (middle.removeFromTop (225));
    middle.removeFromTop (gap);
    partialsPlot.setBounds (middle);
    scaleTable.setBounds (right.removeFromTop (225));
    right.removeFromTop (gap);
    partialsTable.setBounds (right);
    b.removeFromTop (gap);

    synthArea = b.removeFromTop (b.getHeight() - 10);
    auto inner = synthArea.reduced (10, 6).withTrimmedTop (22);
    auto tabs = inner.removeFromTop (26);
    for (int t = 0; t < kTabs; ++t) { tabButtons[(size_t) t]->setBounds (tabs.removeFromLeft (118)); tabs.removeFromLeft (4); }
    resetPage.setBounds (tabs.removeFromRight (90));
    tabs.removeFromRight (4);
    mutatePage.setBounds (tabs.removeFromRight (96));
    tabs.removeFromRight (4);
    dicePage.setBounds (tabs.removeFromRight (86));
    for (auto& pg : pages) pg.setBounds (inner);
}

void SeedEditor::layoutCalculatorRow()
{
    const bool consonance = shownMethod == 1, imported = shownMethod == 2, edo = shownMethod >= 3;
    minDipBox.setVisible (! consonance && ! imported && ! edo); // Imported / EDO: notes don't depend on it
    toleranceBox.setVisible (consonance);
    minSharedBox.setVisible (consonance);
    edoStepsBox.setVisible (edo);
    auto row = calcArea.reduced (10, 0).withTrimmedTop (26).withHeight (48);
    auto place = [&] (juce::Component& c, int w) { c.setBounds (row.removeFromLeft (w)); row.removeFromLeft (14); };
    place (fundamentalBox, 116);
    place (seedBox, 134);
    place (partialsBox, 66);
    place (rolloffBox, 78);
    place (methodBox, 130);
    if (consonance) { place (toleranceBox, 98); place (minSharedBox, 80); }
    else if (edo) place (edoStepsBox, 80);
    else if (! imported) place (minDipBox, 90);
    auto buttons = calcArea.reduced (10, 0).withTrimmedTop (26).withHeight (48);
    buttons = buttons.removeFromRight (470).withHeight (28).withY (buttons.getY() + 19);
    diceTuning.setBounds (buttons.removeFromRight (90));
    buttons.removeFromRight (6);
    resetEdits.setBounds (buttons.removeFromRight (90));
    buttons.removeFromRight (6);
    lockScale.setBounds (buttons.removeFromRight (112));
    buttons.removeFromRight (6);
    lockPartials.setBounds (buttons.removeFromRight (130));
    // Scala buttons: small, bottom-right, on the info line.
    auto sc = calcArea.reduced (10, 5).removeFromBottom (20).removeFromRight (300);
    fitSeed.setBounds (sc.removeFromRight (96));
    sc.removeFromRight (6);
    exportScl.setBounds (sc.removeFromRight (96));
    sc.removeFromRight (6);
    importScl.setBounds (sc.removeFromRight (96));
}

void SeedEditor::showScalaMessage (const juce::String& m)
{
    scalaMessage = m;
    scalaMessageUntil = juce::Time::getMillisecondCounterHiRes() + 4000.0;
}
