#include "PluginEditor.h"

using namespace sane;
namespace L = sane::look;

namespace
{
constexpr int kHeader = 62, kLine = 2, kRow1 = 470, kRow2 = 290;
constexpr int kCol1 = 298, kCol2 = 558, kCol3 = 416;
constexpr int kPad = 13;
} // namespace

SaneEditor::SaneEditor (SaneProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      initBtn ("init", [this] { initPatch(); }),
      kitBtn (L::u8 ("kit \xc2\xb7 v0.2"), [] {}), tunedBtn ("tuned", [] {}), linkBtn (L::u8 ("link \xc2\xb7 v0.2"), [] {}),
      scaleDown (L::u8 ("\xe2\x88\x92"), [this] { setUiScale (uiScale - 0.1f); }), scaleUp ("+", [this] { setUiScale (uiScale + 0.1f); }),
      noiseBtn ("noise", [] {}), audioBtn (L::u8 ("audio in \xc2\xb7 later"), [] {}),
      exciter (p),
      coreChips (p.param (ids::core), { "membrane", "partials", "delay" }),
      timeChips (p.param (ids::loopMode), { "keys", "hz", "ms", "bpm" }),
      ring (p, [this] (int s) { slotEditor.setSlot (s); }),
      tableChips (p.param (ids::table), { "membrane", "timpani", "tabla", "bar", "plate" }, 10.0f),
      srcAChips (p.param (ids::srcA), { "physical", "calc", "file", "linked" }, 10.5f),
      srcBChips (p.param (ids::srcB), { "physical", "calc", "file", "linked" }, 10.5f),
      modes (p),
      morphBar (p.param (ids::morph), L::u8 ("morph A \xe2\x86\x92 B")),
      strike (p), slotEditor (p), hit (p), keys (p)
{
    setLookAndFeel (&lnf);
    tunedBtn.on = true;
    kitBtn.isEnabled = linkBtn.isEnabled = false;
    noiseBtn.on = true; audioBtn.isEnabled = false;
    initBtn.setTooltip ("INIT: a neutral patch. Nothing random, no velocity / key effects, no slots, plain membrane.");
    for (auto* c : std::initializer_list<juce::Component*> { &initBtn, &timeChips, &kitBtn, &tunedBtn, &linkBtn, &scaleDown, &scaleUp, &noiseBtn, &audioBtn, &exciter,
                                                             &coreChips, &ring, &tableChips, &srcAChips, &srcBChips, &modes, &morphBar,
                                                             &strike, &slotEditor, &hit, &keys })
        addAndMakeVisible (c);

    auto knob = [&] (std::vector<std::unique_ptr<ui::Knob>>& v, const char* id, const juce::String& label) {
        v.push_back (std::make_unique<ui::Knob> (p.param (id), label));
        addAndMakeVisible (*v.back());
    };
    knob (excKnobs, ids::exColor, "colour"); knob (excKnobs, ids::exLength, "length"); knob (excKnobs, ids::exGrain, "grain");
    knob (excKnobs, ids::exLevel, L::u8 ("\xe2\x86\x92 loop")); knob (excKnobs, ids::exDirect, "direct");
    knob (loopKnobs, ids::feedback, "feedback"); knob (loopKnobs, ids::damp, "damp"); knob (loopKnobs, ids::tension, "tension");
    knob (loopKnobs, ids::coupling, "coupling"); knob (loopKnobs, ids::width, "width");
    knob (modeKnobs, ids::calcSeed, "seed"); knob (modeKnobs, ids::rolloff, "rolloff"); knob (modeKnobs, ids::tune, "tune");
    knob (modeKnobs, ids::fine, "fine"); knob (modeKnobs, ids::bendRange, "bend"); knob (modeKnobs, ids::gain, "output");

    auto bar = [&] (std::vector<std::unique_ptr<ui::Bar>>& v, const char* id, const juce::String& label) {
        v.push_back (std::make_unique<ui::Bar> (p.param (id), label));
        addAndMakeVisible (*v.back());
    };
    bar (excBars, ids::exRandom, "random per note"); bar (excBars, ids::exVelRandom, L::u8 ("velocity \xe2\x86\x92 random"));
    bar (excBars, ids::exVelLength, L::u8 ("velocity \xe2\x86\x92 length")); bar (excBars, ids::exKeyColor, L::u8 ("key \xe2\x86\x92 colour"));
    bar (excBars, ids::exSpread, "stereo spread");
    bar (strikeBars, ids::stBase, "base position"); bar (strikeBars, ids::stVel, L::u8 ("velocity \xe2\x86\x92 pos"));
    bar (strikeBars, ids::stKey, L::u8 ("key \xe2\x86\x92 pos")); bar (strikeBars, ids::stRandom, "random pos");
    morphBar.labelWidth = 104;

    // Linked has no link groups yet: it uses the Calc partials meanwhile — still selectable.
    coreChips.onChange = [this] { repaint(); };
    timeKnob = std::make_unique<ui::Knob> (p.param (ids::loopHz), "time");
    keyTrackKnob = std::make_unique<ui::Knob> (p.param (ids::keyTrack), "key track");
    addAndMakeVisible (*timeKnob); addAndMakeVisible (*keyTrackKnob);
    timeChips.onChange = [this] { updateTimeKnob(); resized(); repaint(); };
    updateTimeKnob();

    // 90 % by default so the window fits a laptop screen inside FL; the user's choice is remembered in the project
    uiScale = (float) proc.apvts.state.getProperty ("uiScale", 0.9f);
    setSize (kW, kH);
    setUiScale (uiScale);
    startTimerHz (10);
}

SaneEditor::~SaneEditor() { setLookAndFeel (nullptr); }

void SaneEditor::setUiScale (float s)
{
    uiScale = juce::jlimit (0.5f, 2.0f, std::round (s * 10.0f) / 10.0f);
    proc.apvts.state.setProperty ("uiScale", uiScale, nullptr);
    setScaleFactor (uiScale);
    repaint();
}

void SaneEditor::updateTimeKnob()
{
    const int mode = (int) proc.value (ids::loopMode);
    if (mode == shownTimeMode) return;
    shownTimeMode = mode;
    const char* id = mode == LoopMs ? ids::loopMs : (mode == LoopBpm ? ids::loopBeat : ids::loopHz);
    timeKnob->attach (proc.param (id), "time");
    // Keys mode: the played key IS the loop time, so the time / key track knobs have nothing to do
    timeKnob->setVisible (mode != LoopKeys);
    keyTrackKnob->setVisible (mode != LoopKeys);
}

void SaneEditor::initPatch()
{
    // every parameter to its default first ...
    for (auto* ap : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
        {
            rp->beginChangeGesture(); rp->setValueNotifyingHost (rp->getDefaultValue()); rp->endChangeGesture();
        }
    // ... then neutral: nothing random, nothing velocity / key dependent, no slots, no drawn pitch drop
    auto set = [this] (const juce::String& id, float v) {
        auto& p = proc.param (id);
        p.beginChangeGesture(); p.setValueNotifyingHost (p.convertTo0to1 (v)); p.endChangeGesture();
    };
    for (auto* id : { ids::exRandom, ids::exVelRandom, ids::exVelLength, ids::exKeyColor, ids::exSpread, ids::stVel, ids::stKey,
                      ids::stRandom, ids::pitchAmt, ids::tension })
        set (id, 0.0f);
    set (ids::exDirect, -60.0f);
    for (int sl = 1; sl <= kNumSlots; ++sl) set (ids::slot (sl, "type"), 0.0f);
}

void SaneEditor::resized()
{
    const int x1 = kLine, x2 = x1 + kCol1 + kLine, x3 = x2 + kCol2 + kLine;
    const int y1 = kLine + kHeader + kLine, y2 = y1 + kRow1 + kLine, y3 = y2 + kRow2 + kLine;
    cExc = { x1, y1, kCol1, kRow1 };   cLoop = { x2, y1, kCol2, kRow1 };  cModes = { x3, y1, kCol3, kRow1 };
    cStrike = { x1, y2, kCol1, kRow2 }; cSlot = { x2, y2, kCol2, kRow2 }; cHit = { x3, y2, kCol3, kRow2 };
    cKeys = { x1, y3, kW - 2 * kLine, kH - kLine - y3 };

    // header (right side)
    {
        auto h = juce::Rectangle<int> (kLine, kLine, kW - 2 * kLine, kHeader).reduced (14, 15);
        initBtn.setBounds (360, h.getY(), initBtn.preferredWidth() + 10, h.getHeight());
        scaleUp.setBounds (h.removeFromRight (28)); h.removeFromRight (52);
        scaleDown.setBounds (h.removeFromRight (28)); h.removeFromRight (76);
        linkBtn.setBounds (h.removeFromRight (linkBtn.preferredWidth())); h.removeFromRight (18);
        kitBtn.setBounds (h.removeFromRight (kitBtn.preferredWidth())); h.removeFromRight (4);
        tunedBtn.setBounds (h.removeFromRight (tunedBtn.preferredWidth()));
    }
    // exciter
    {
        auto r = cExc.reduced (kPad);
        auto top = r.removeFromTop (22);
        audioBtn.setBounds (top.removeFromRight (audioBtn.preferredWidth())); top.removeFromRight (4);
        noiseBtn.setBounds (top.removeFromRight (noiseBtn.preferredWidth()));
        r.removeFromTop (6);
        exciter.setBounds (r.removeFromTop (104));
        r.removeFromTop (8);
        auto kr = r.removeFromTop (71);
        const int kw = kr.getWidth() / (int) excKnobs.size();
        for (auto& k : excKnobs) k->setBounds (kr.removeFromLeft (kw));
        r.removeFromTop (30); // separator + "new noise on every note"
        for (auto& b : excBars) { b->labelWidth = 128; b->setBounds (r.removeFromTop (24)); }
    }
    // loop
    {
        auto r = cLoop.reduced (kPad);
        auto top = r.removeFromTop (22);
        top.removeFromLeft (70);
        coreChips.setBounds (top.removeFromLeft (coreChips.preferredWidth()));
        r.removeFromTop (4);
        auto tr = r.removeFromTop (22);
        tr.removeFromLeft (70);
        timeChips.setBounds (tr.removeFromLeft (timeChips.preferredWidth()));
        r.removeFromTop (2);
        ring.setBounds (r.removeFromTop (318));
        r.removeFromTop (2);
        auto kr = r.removeFromTop (71);
        for (auto& k : loopKnobs) k->setBounds (kr.removeFromLeft (74));
        timeKnob->setBounds (kr.removeFromLeft (74));
        keyTrackKnob->setBounds (kr.removeFromLeft (74));
    }
    // modes
    {
        auto r = cModes.reduced (kPad);
        auto top = r.removeFromTop (22);
        tableChips.setBounds (top.removeFromRight (tableChips.preferredWidth()));
        r.removeFromTop (6);
        modes.setBounds (r.removeFromTop (228));
        r.removeFromTop (8);
        auto a = r.removeFromTop (22); a.removeFromLeft (34); srcAChips.setBounds (a.removeFromLeft (srcAChips.preferredWidth()));
        r.removeFromTop (5);
        auto b = r.removeFromTop (22); b.removeFromLeft (34); srcBChips.setBounds (b.removeFromLeft (srcBChips.preferredWidth()));
        r.removeFromTop (6);
        morphBar.setBounds (r.removeFromTop (24));
        r.removeFromTop (4);
        auto kr = r.removeFromTop (71);
        const int kw = kr.getWidth() / 6;
        for (auto& k : modeKnobs) k->setBounds (kr.removeFromLeft (kw));
    }
    // strike
    {
        auto r = cStrike.reduced (kPad);
        r.removeFromTop (28);
        strike.setBounds (r.removeFromTop (140));
        r.removeFromTop (6);
        for (auto& b : strikeBars) { b->labelWidth = 128; b->setBounds (r.removeFromTop (22)); }
    }
    slotEditor.setBounds (cSlot.reduced (kPad));
    {
        auto r = cHit.reduced (kPad);
        r.removeFromTop (28);
        hit.setBounds (r.removeFromTop (196));
    }
    keys.setBounds (cKeys.withTrimmedLeft (130).reduced (0, 4).withTrimmedRight (14));
}

void SaneEditor::paint (juce::Graphics& g)
{
    g.fillAll (L::ground);
    // the grid: ink background shows through 2 px gaps — every line runs through, corners meet
    g.setColour (L::ink);
    g.drawRect (getLocalBounds(), kLine);
    g.fillRect (0, kLine + kHeader, kW, kLine);
    g.fillRect (cExc.getRight(), cExc.getY(), kLine, kRow1 + kLine + kRow2);
    g.fillRect (cLoop.getRight(), cLoop.getY(), kLine, kRow1 + kLine + kRow2);
    g.fillRect (0, cExc.getBottom(), kW, kLine);
    g.fillRect (0, cStrike.getBottom(), kW, kLine);

    // header
    g.setColour (L::ink);
    g.setFont (L::sans (44.0f, true).withHorizontalScale (0.8f));
    g.drawText ("SANE", 16, 10, 110, 46, juce::Justification::centredLeft, false);
    L::drawLabel (g, "feedback drums", { 118, 18, 140, 14 }, juce::Justification::left);
    g.setColour (L::dim); g.setFont (L::mono (10.5f));
    g.drawText ("v" + juce::String (kVersion) + L::u8 (" \xc2\xb7 build ") + PVST_BUILD_NUMBER + L::u8 (" \xc2\xb7 ") + PVST_GIT_HASH, 118, 34, 220, 14, juce::Justification::left, false);
    L::drawLabel (g, "keys", { tunedBtn.getX() - 44, tunedBtn.getY(), 40, 32 }, juce::Justification::centredRight);
    g.setColour (L::ink); g.setFont (L::mono (12.0f, true));
    g.drawText (juce::String (juce::roundToInt (uiScale * 100.0f)) + " %", scaleDown.getRight(), scaleDown.getY(), 52, scaleDown.getHeight(), juce::Justification::centred, false);
    L::drawLabel (g, "window", { scaleDown.getX() - 64, scaleDown.getY(), 60, 32 }, juce::Justification::centredRight);

    // exciter
    L::drawTitle (g, "exciter", cExc.getX() + kPad, cExc.getY() + kPad + 3);
    {
        const int y = excKnobs.front()->getBottom() + 10;
        g.setColour (L::ink); g.fillRect (cExc.getX() + kPad, y, cExc.getWidth() - 2 * kPad, 1);
        L::drawLabel (g, "new noise on every note, varied by:", { cExc.getX() + kPad, y + 6, cExc.getWidth() - 2 * kPad, 14 }, juce::Justification::left, L::ink);
    }
    // loop
    L::drawTitle (g, "loop", cLoop.getX() + kPad, cLoop.getY() + kPad + 3);
    {
        const int core = (int) proc.value (ids::core);
        const char* hints[] = { "8 coupled fine delays (feedback delay network): modes emerge, pulled toward the Modes targets",
                                "one tiny loop per mode, exactly at the positions shown in Modes",
                                "one loop (Karplus-Strong family): harmonic until an insert (Disperse, Flip...) breaks it" };
        L::drawLabel (g, "time", { cLoop.getX() + kPad, timeChips.getY(), 64, 22 }, juce::Justification::centredLeft, L::ink);
        const int tm = (int) proc.value (ids::loopMode);
        const juce::String timeHint = tm == LoopKeys ? juce::String ("the played key sets the loop length (pitched drum)")
                                    : (tm == LoopBpm ? juce::String ("loop length = a tempo division (Delay core: up to 2 s, echo-like)")
                                                     : juce::String ("a fixed loop length, like a delay; KEY TRACK bends it with the keys"));
        g.setColour (L::dim); g.setFont (L::mono (11.0f));
        g.drawText (timeHint, timeChips.getRight() + 10, timeChips.getY(), cLoop.getRight() - kPad - timeChips.getRight() - 10, 22, juce::Justification::centredLeft, true);
        coreChips.setTooltip (juce::String ("LOOP CORE\n") + hints[juce::jlimit (0, 2, core)]);
        L::drawLabel (g, "loop gain", { cLoop.getRight() - kPad - 170, cLoop.getY() + kPad, 90, 22 }, juce::Justification::centredRight);
        g.setColour (L::ink); g.setFont (L::mono (12.0f, true));
        g.drawText (proc.param (ids::feedback).getCurrentValueAsText(), cLoop.getRight() - kPad - 76, cLoop.getY() + kPad, 76, 22, juce::Justification::centredRight, false);
        g.setColour (L::dim); g.setFont (L::mono (10.5f));
        g.drawText ("drag a slot onto another = swap", cLoop.getX() + kPad, ring.getBottom() - 14, 260, 13, juce::Justification::left, false);
    }
    // modes
    L::drawTitle (g, "modes", cModes.getX() + kPad, cModes.getY() + kPad + 3);
    {
        g.setColour (L::ink); g.setFont (L::caps (12.5f));
        g.drawText (L::u8 ("A \xe2\x97\x86"), cModes.getX() + kPad, srcAChips.getY(), 32, 22, juce::Justification::centredLeft, false);
        g.drawText (L::u8 ("B \xe2\x96\xb2"), cModes.getX() + kPad, srcBChips.getY(), 32, 22, juce::Justification::centredLeft, false);
        auto desc = [&] (int src) -> juce::String {
            switch (src)
            {
                case SrcPhysical: return tableNames()[(int) proc.value (ids::table)].toLowerCase() + " table";
                case SrcCalc: return "calc " + L::u8 ("\xc2\xb7 seed ") + juce::String (proc.value (ids::calcSeed), 3);
                case SrcFile: return "demo list (import: v0.2)";
                default: return L::u8 ("not linked \xe2\x86\x92 calc");
            }
        };
        g.setColour (L::dim); g.setFont (L::mono (10.0f));
        g.drawText (desc ((int) proc.value (ids::srcA)), srcAChips.getRight() + 4, srcAChips.getY(), cModes.getRight() - kPad - srcAChips.getRight() - 4, 22, juce::Justification::centredRight, true);
        g.drawText (desc ((int) proc.value (ids::srcB)), srcBChips.getRight() + 4, srcBChips.getY(), cModes.getRight() - kPad - srcBChips.getRight() - 4, 22, juce::Justification::centredRight, true);
    }
    // strike
    L::drawTitle (g, "strike", cStrike.getX() + kPad, cStrike.getY() + kPad + 3);
    {
        const int core = (int) proc.value (ids::core);
        const char* as[] = { "acts as: where energy enters", "acts as: real skin mode shapes", "acts as: comb on the hit" };
        g.setColour (L::dim); g.setFont (L::mono (10.0f));
        g.drawText (as[juce::jlimit (0, 2, core)], cStrike.getX() + 90, cStrike.getY() + kPad + 3, cStrike.getWidth() - 90 - kPad, 14, juce::Justification::right, true);
    }
    // hit
    L::drawTitle (g, "hit", cHit.getX() + kPad, cHit.getY() + kPad + 3);
    {
        auto& rp = proc.engine.report;
        const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int n = rp.note.load();
        g.setColour (L::ink); g.setFont (L::mono (10.5f));
        g.drawText ("last: vel " + juce::String (juce::roundToInt (rp.velocity.load() * 127.0f)) + L::u8 (" \xc2\xb7 ") + names[n % 12] + juce::String (n / 12 - 1)
                        + L::u8 (" \xc2\xb7 strike ") + juce::String (rp.strikeR.load(), 2),
                    cHit.getX() + 60, cHit.getY() + kPad + 3, cHit.getWidth() - 60 - kPad, 14, juce::Justification::right, false);
        g.setColour (L::dim); g.setFont (L::mono (10.0f));
        g.drawText (L::u8 ("drag \xe2\x96\xa0: pitch drop (below 0 = rising) \xc2\xb7 time \xc2\xb7 length"),
                    cHit.getX() + kPad, hit.getBottom() + 6, cHit.getWidth() - 2 * kPad, 13, juce::Justification::left, true);
    }
    // keys
    L::drawLabel (g, L::u8 ("tuned \xc2\xb7 12-edo"), { cKeys.getX() + 14, cKeys.getY() + 8, 112, 14 }, juce::Justification::left, L::ink);
    g.setColour (L::dim); g.setFont (L::mono (9.5f));
    g.drawText ("click = hit", cKeys.getX() + 14, cKeys.getY() + 26, 116, 13, juce::Justification::left, false);
}

void SaneEditor::timerCallback()
{
    // header/cell texts that follow parameters (core hint, loop gain, A/B descriptions, last hit)
    repaint (cLoop.withHeight (60));
    repaint (cModes.withTrimmedTop (260).withHeight (60));
    repaint (cStrike.withHeight (30));
    repaint (cHit.withHeight (30));
}
