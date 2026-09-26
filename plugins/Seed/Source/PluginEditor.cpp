#include "PluginEditor.h"
#include "xen/Stretch.h"

namespace th = pvst::theme;

// ================================================================================================
// PartialView
// ================================================================================================
void PartialView::tick()
{
    const double seedVal = juce::jmax (1e-6f, state.getRawParameterValue (seed::ids::seed)->load());
    const int count = (int) std::lround (state.getRawParameterValue (seed::ids::partials)->load());
    const double rolloff = state.getRawParameterValue (seed::ids::rolloff)->load();
    const double S = xen::stretchFactor (seedVal);

    float lo = 0.0f, hi = 0.0f;
    for (int p = 0; p < seed::kMaxPartials; ++p)
    {
        const float c = (float) (1200.0 * S * std::log2 ((double) p + 1));
        // Height: loudness on a gentle (square-root) scale so quiet high partials stay visible.
        const float h = p < count ? (float) std::sqrt (xen::rolloffShape (p + 1, rolloff)) : 0.0f;
        cents[(size_t) p] = first ? c : th::ease (cents[(size_t) p], c, 0.18f);
        height[(size_t) p] = first ? h : th::ease (height[(size_t) p], h, 0.18f);
        if (p < count) { lo = juce::jmin (lo, c); hi = juce::jmax (hi, c); }
    }
    // The view range glides too, so the picture never snaps.
    const float span = juce::jmax (1200.0f, hi - lo);
    viewMin = first ? lo - span * 0.04f : th::ease (viewMin, lo - span * 0.04f, 0.12f);
    viewMax = first ? hi + span * 0.04f : th::ease (viewMax, hi + span * 0.04f, 0.12f);
    period = first ? (float) xen::periodCents (seedVal) : th::ease (period, (float) xen::periodCents (seedVal), 0.18f);
    first = false;
    repaint();
}

void PartialView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (th::panel);
    g.fillRoundedRectangle (b, 14.0f);
    auto plot = b.reduced (18.0f, 16.0f);
    const float base = plot.getBottom();
    auto xOf = [&] (float c) { return plot.getX() + (c - viewMin) / (viewMax - viewMin) * plot.getWidth(); };

    // Seed "octave" guides (skip if they'd be too dense to read).
    if (std::abs (period) > 1.0f && std::abs (period) / (viewMax - viewMin) * plot.getWidth() > 14.0f)
    {
        g.setColour (th::panelEdge.brighter (0.25f));
        const float step = std::abs (period);
        for (float c = std::floor (viewMin / step) * step; c <= viewMax; c += step)
            if (c >= viewMin)
                g.drawVerticalLine ((int) xOf (c), plot.getY(), base);
    }
    g.setColour (th::panelEdge.brighter (0.4f));
    g.drawHorizontalLine ((int) base, plot.getX(), plot.getRight());

    // Partials: a soft glow line + a bright cap. The fundamental gets the second accent.
    for (int p = seed::kMaxPartials - 1; p >= 0; --p)
    {
        const float h = height[(size_t) p];
        if (h < 0.002f) continue;
        const float x = xOf (cents[(size_t) p]);
        const float top = base - h * plot.getHeight();
        const auto col = p == 0 ? th::accent2 : th::accent;
        g.setColour (col.withAlpha (0.18f * juce::jmin (1.0f, h * 3.0f)));
        g.fillRoundedRectangle (x - 3.0f, top, 6.0f, base - top, 3.0f);
        g.setColour (col.withAlpha (0.35f + 0.65f * juce::jmin (1.0f, h * 2.0f)));
        g.drawLine (x, top, x, base, 1.6f);
        g.fillEllipse (x - 3.0f, top - 3.0f, 6.0f, 6.0f);
    }

    g.setColour (th::textDim);
    g.setFont (th::font (12.0f));
    g.drawText ("partials", b.reduced (14.0f, 8.0f).removeFromTop (16.0f), juce::Justification::topLeft);
}

// ================================================================================================
// SeedEditor
// ================================================================================================
SeedEditor::SeedEditor (SeedProcessor& pr)
    : AudioProcessorEditor (pr), proc (pr), partialView (pr.apvts),
      seedKnob (p (seed::ids::seed), "Seed"),
      partialsKnob (p (seed::ids::partials), "Partials"),
      rolloffKnob (p (seed::ids::rolloff), "Rolloff"),
      attackKnob (p (seed::ids::attack), "Attack", th::accent2),
      releaseKnob (p (seed::ids::release), "Release", th::accent2),
      gainKnob (p (seed::ids::gain), "Gain", th::accent2),
      droneNoteKnob (p (seed::ids::droneNote), "Drone note", th::accent2),
      keysToggle (p (seed::ids::keysFollow), "Keys follow seed"),
      droneToggle (p (seed::ids::drone), "Drone")
{
    seedKnob.setValueFontSize (26.0f);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &partialView, &seedKnob, &partialsKnob, &rolloffKnob,
                                                                      &attackKnob, &releaseKnob, &gainKnob, &droneNoteKnob,
                                                                      &keysToggle, &droneToggle })
        addAndMakeVisible (c);
    setSize (780, 480);
    startTimerHz (60);
}

SeedEditor::~SeedEditor() { stopTimer(); }

void SeedEditor::timerCallback()
{
    partialView.tick();
    for (auto* k : { &seedKnob, &partialsKnob, &rolloffKnob, &attackKnob, &releaseKnob, &gainKnob, &droneNoteKnob })
        k->tick();
    keysToggle.tick();
    droneToggle.tick();

    // Seed read-out under the big knob: what the seed means in musical terms.
    const double s = proc.apvts.getRawParameterValue (seed::ids::seed)->load();
    const auto info = "octave " + juce::String (xen::periodCents (s), 1) + " c   \xc2\xb7   stretch " + juce::String (xen::stretchFactor (s), 4);
    if (info != seedInfo) { seedInfo = info; repaint(); }
}

void SeedEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (th::bgTop, 0, 0, th::bgBottom, 0, (float) getHeight(), false));
    g.fillAll();

    g.setColour (th::text);
    g.setFont (th::font (22.0f, true));
    g.drawText ("Seed", 24, 14, 200, 28, juce::Justification::centredLeft);
    g.setColour (th::textDim);
    g.setFont (th::font (13.0f));
    g.drawText ("stretched-partial synth", 86, 18, 260, 22, juce::Justification::centredLeft);

    // The seed's meaning, under the big knob.
    g.setColour (th::textDim);
    g.setFont (th::font (13.0f));
    g.drawText (seedInfo, juce::Rectangle<int> (seedKnob.getX() - 60, seedKnob.getBottom() + 2, seedKnob.getWidth() + 120, 20),
                juce::Justification::centred);

    // Version + build: the user checks this to confirm FL loaded the newest build.
    g.setColour (th::textDim.withAlpha (0.7f));
    g.setFont (th::font (11.0f));
    const juce::String ver = juce::String ("v") + seed::kVersion + "  \xc2\xb7  build " + PVST_BUILD_NUMBER + "  \xc2\xb7  " + PVST_GIT_HASH
                           + "  \xc2\xb7  voices " + juce::String (proc.uiActiveVoices.load());
    g.drawText (ver, getLocalBounds().reduced (16, 8).removeFromBottom (14), juce::Justification::centredRight);
}

void SeedEditor::resized()
{
    auto b = getLocalBounds().reduced (20);
    b.removeFromTop (34); // title
    partialView.setBounds (b.removeFromTop (170));
    b.removeFromTop (14);

    auto bottom = b.removeFromBottom (40);
    keysToggle.setBounds (bottom.removeFromLeft (190).withSizeKeepingCentre (190, 26));
    droneToggle.setBounds (bottom.removeFromLeft (110).withSizeKeepingCentre (110, 26));

    auto seedArea = b.removeFromLeft (230);
    seedKnob.setBounds (seedArea.withTrimmedBottom (26).withSizeKeepingCentre (190, 190).withY (seedArea.getY()));

    b.removeFromLeft (10);
    const int w = b.getWidth() / 4;
    auto row1 = b.removeFromTop (b.getHeight() / 2);
    partialsKnob.setBounds (row1.removeFromLeft (w).reduced (6));
    rolloffKnob.setBounds (row1.removeFromLeft (w).reduced (6));
    gainKnob.setBounds (row1.removeFromLeft (w).reduced (6));
    auto row2 = b;
    attackKnob.setBounds (row2.removeFromLeft (w).reduced (6));
    releaseKnob.setBounds (row2.removeFromLeft (w).reduced (6));
    droneNoteKnob.setBounds (row2.removeFromLeft (w).reduced (6));
}
