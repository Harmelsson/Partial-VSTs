#include "PluginEditor.h"
#include "xen/Stretch.h"

namespace th = pvst::theme;

SeedEditor::SeedEditor (SeedProcessor& pr)
    : AudioProcessorEditor (pr), proc (pr),
      fundamentalBox (p (seed::ids::fundamental), "Fundamental (Hz)", 4, 2, 1.0),
      seedBox (p (seed::ids::seed), "Seed", 1, 6, 0.01),
      partialsBox (p (seed::ids::partials), "Partials", 2, 0, 1.0),
      rolloffBox (p (seed::ids::rolloff), "Rolloff", 1, 2, 0.1),
      minDipBox (p (seed::ids::minDip), "Min dip strength (%)", 2, 1, 1.0),
      envelope (p (seed::ids::attack), p (seed::ids::decay), p (seed::ids::sustain), p (seed::ids::release)),
      attackBox (p (seed::ids::attack), "Attack (ms)", 4, 0, 1.0),
      decayBox (p (seed::ids::decay), "Decay (ms)", 5, 0, 10.0),
      sustainBox (p (seed::ids::sustain), "Sustain (%)", 3, 1, 1.0),
      releaseBox (p (seed::ids::release), "Release (ms)", 5, 0, 10.0),
      velocityBox (p (seed::ids::velocity), "Velocity (%)", 3, 0, 5.0),
      gainBox (p (seed::ids::gain), "Gain (dB)", 2, 1, 0.5),
      droneNoteBox (p (seed::ids::droneNote), "Drone note", 3, 0, 1.0),
      keysToggle (p (seed::ids::keysFollow), "Keys", "12-EDO", "Seed"),
      droneToggle (p (seed::ids::drone), "Drone", "Off", "On")
{
    droneNoteBox.suffix = [] (double n) { return juce::MidiMessage::getMidiNoteName (juce::roundToInt (n), true, true, 4); };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &fundamentalBox, &seedBox, &partialsBox, &rolloffBox, &minDipBox, &curvePlot, &partialsPlot, &scaleTable,
             &envelope, &attackBox, &decayBox, &sustainBox, &releaseBox, &velocityBox, &gainBox, &droneNoteBox,
             &keysToggle, &droneToggle })
        addAndMakeVisible (c);

    setSize (1160, 800);
    lastTick = juce::Time::getMillisecondCounterHiRes();
    timerCallback();
    startTimerHz (60);
}

SeedEditor::~SeedEditor() { stopTimer(); }

xen::CalculatorSettings SeedEditor::currentSettings() const
{
    auto& s = proc.apvts;
    xen::CalculatorSettings c;
    c.fundamentalHz = s.getRawParameterValue (seed::ids::fundamental)->load();
    c.seed = s.getRawParameterValue (seed::ids::seed)->load();
    c.partials = (int) std::lround (s.getRawParameterValue (seed::ids::partials)->load());
    c.rolloff = s.getRawParameterValue (seed::ids::rolloff)->load();
    c.minStrength = s.getRawParameterValue (seed::ids::minDip)->load() / 100.0;
    return c;
}

void SeedEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.001, 0.1, (now - lastTick) / 1000.0);
    lastTick = now;

    const auto settings = currentSettings();
    worker.request (settings);
    if (worker.fetch (latest, seenId))
    {
        curvePlot.setResult (latest);
        scaleTable.setResult (latest);
    }
    // Only say "calculating..." if it takes long enough to notice.
    if (! worker.isBusy()) busySince = 0.0;
    else if (busySince <= 0.0) busySince = now;
    curvePlot.setBusy (busySince > 0.0 && now - busySince > 150.0);

    partialsPlot.setSettings (settings);
    curvePlot.tick (dt);
    partialsPlot.tick (dt);
    envelope.tick (dt);

    // Info line under the calculator's boxes (ArtHar wording).
    const int found = latest.curve.valid ? (int) latest.curve.notes.size() - 2 : 0;
    const auto newInfo = "Stretch factor " + juce::String (xen::stretchFactor (settings.seed), 6)
                       + th::dot() + "\"octave\" " + juce::String (xen::periodCents (settings.seed), 4) + " cents"
                       + th::dot() + juce::String (found) + " notes found by the dissonance curve"
                       + th::dot() + "voices " + juce::String (proc.uiActiveVoices.load());
    if (newInfo != info) { info = newInfo; repaint (calcArea); }
}

void SeedEditor::paint (juce::Graphics& g)
{
    g.fillAll (th::bg);

    // Header: name + letter-spaced subtitle (ArtHar: "ArtHar  XENHARMONIC SOUND-DESIGN CALCULATOR").
    g.setColour (th::fg);
    g.setFont (th::sans (30.0f));
    g.drawText ("Seed", 14, 6, 90, 36, juce::Justification::centredLeft);
    g.setColour (th::dim());
    g.setFont (th::sans (11.5f).withExtraKerningFactor (0.25f));
    g.drawText ("STRETCHED-PARTIAL SYNTH", 94, 12, 400, 24, juce::Justification::centredLeft);

    // Version + build: the user checks this to confirm FL loaded the newest build.
    g.setColour (th::mix (0.4f));
    g.setFont (th::sans (11.5f));
    const juce::String ver = juce::String ("v") + seed::kVersion + th::dot() + "build " + PVST_BUILD_NUMBER + th::dot() + PVST_GIT_HASH;
    g.drawText (ver, getLocalBounds().reduced (14, 0).removeFromTop (46), juce::Justification::centredRight);

    // Section frames that hold several child controls.
    auto calcInner = th::drawSection (g, calcArea, "Calculator");
    g.setColour (th::dim());
    g.setFont (th::sans (12.5f));
    g.drawText (info, calcInner.removeFromBottom (18), juce::Justification::centredLeft);
    th::drawSection (g, envArea, "Envelope");
    th::drawSection (g, voiceArea, "Voice & output");
}

void SeedEditor::resized()
{
    constexpr int m = 12, gap = 8;
    auto b = getLocalBounds().reduced (m, 0);
    b.removeFromTop (46);

    // Calculator bar.
    calcArea = b.removeFromTop (100);
    {
        auto row = calcArea.reduced (10, 0).withTrimmedTop (26).withHeight (48);
        auto place = [&] (juce::Component& c, int w) { c.setBounds (row.removeFromLeft (w)); row.removeFromLeft (18); };
        place (fundamentalBox, 130);
        place (seedBox, 150);
        place (partialsBox, 80);
        place (rolloffBox, 90);
        place (minDipBox, 150);
    }
    b.removeFromTop (gap);

    // Plots + scale table.
    auto middle = b.removeFromTop (450);
    scaleTable.setBounds (middle.removeFromRight (340));
    middle.removeFromRight (gap);
    curvePlot.setBounds (middle.removeFromTop (260));
    middle.removeFromTop (gap);
    partialsPlot.setBounds (middle);
    b.removeFromTop (gap);

    // Envelope + voice.
    auto bottom = b.removeFromTop (176);
    voiceArea = bottom.removeFromRight (340);
    bottom.removeFromRight (gap);
    envArea = bottom;
    {
        auto inner = envArea.reduced (10, 6).withTrimmedTop (20);
        envelope.setBounds (inner.removeFromLeft (390));
        inner.removeFromLeft (16);
        auto row1 = inner.removeFromTop (60).withTrimmedTop (4);
        auto row2 = inner.removeFromTop (60).withTrimmedTop (4);
        auto place = [] (juce::Rectangle<int>& row, juce::Component& c, int w) { c.setBounds (row.removeFromLeft (w).withHeight (48)); row.removeFromLeft (14); };
        place (row1, attackBox, 100);
        place (row1, decayBox, 110);
        place (row1, sustainBox, 110);
        place (row2, releaseBox, 110);
        place (row2, velocityBox, 110);
    }
    {
        auto inner = voiceArea.reduced (10, 6).withTrimmedTop (20);
        auto row1 = inner.removeFromTop (60).withTrimmedTop (4);
        auto row2 = inner.removeFromTop (60).withTrimmedTop (4);
        auto place = [] (juce::Rectangle<int>& row, juce::Component& c, int w) { c.setBounds (row.removeFromLeft (w).withHeight (48)); row.removeFromLeft (16); };
        place (row1, gainBox, 110);
        place (row1, keysToggle, 170);
        place (row2, droneToggle, 110);
        place (row2, droneNoteBox, 150);
    }
}
