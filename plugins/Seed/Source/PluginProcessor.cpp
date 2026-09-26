#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace seed;

juce::AudioProcessorValueTreeState::ParameterLayout SeedProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const int v = 1; // parameter version hint for VST3; bump only when ADDING parameters

    // Seed: 0.5..4. Below 1 the partials fall below the fundamental (undertones), exactly 1
    // collapses them all onto one pitch. Both kept on purpose: weird is welcome here.
    // The fine interval keeps full float precision for host automation.
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::seed, v }, "Seed",
        NormalisableRange<float> (0.5f, 4.0f, 0.0f), 2.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float x, int) { return String (x, 4); })));

    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::partials, v }, "Partials", 1, kMaxPartials, 16));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::rolloff, v }, "Rolloff",
        NormalisableRange<float> (0.0f, 3.0f, 0.0f), 1.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float x, int) { return String (x, 2); })));

    // Times use a skewed range so the short end (where the ear is most sensitive) gets more knob.
    NormalisableRange<float> attackRange (1.0f, 5000.0f, 0.0f);
    attackRange.setSkewForCentre (120.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::attack, v }, "Attack", attackRange, 8.0f,
        AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction (
            [] (float x, int) { return x < 1000 ? String (roundToInt (x)) + " ms" : String (x / 1000.0f, 2) + " s"; })));

    NormalisableRange<float> releaseRange (5.0f, 10000.0f, 0.0f);
    releaseRange.setSkewForCentre (500.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::release, v }, "Release", releaseRange, 400.0f,
        AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction (
            [] (float x, int) { return x < 1000 ? String (roundToInt (x)) + " ms" : String (x / 1000.0f, 2) + " s"; })));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::gain, v }, "Gain",
        NormalisableRange<float> (-48.0f, 0.0f, 0.0f), -12.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (
            [] (float x, int) { return String (x, 1) + " dB"; })));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::keysFollow, v }, "Keys follow seed", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::drone, v }, "Drone", false));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::droneNote, v }, "Drone note", 24, 96, 48,
        AudioParameterIntAttributes().withStringFromValueFunction (
            [] (int n, int) { return MidiMessage::getMidiNoteName (n, true, true, 4); })));
    return layout;
}

SeedProcessor::SeedProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SeedState", createLayout())
{
    pSeed = apvts.getRawParameterValue (ids::seed);
    pPartials = apvts.getRawParameterValue (ids::partials);
    pRolloff = apvts.getRawParameterValue (ids::rolloff);
    pAttack = apvts.getRawParameterValue (ids::attack);
    pRelease = apvts.getRawParameterValue (ids::release);
    pGain = apvts.getRawParameterValue (ids::gain);
    pKeysFollow = apvts.getRawParameterValue (ids::keysFollow);
    pDrone = apvts.getRawParameterValue (ids::drone);
    pDroneNote = apvts.getRawParameterValue (ids::droneNote);
}

bool SeedProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void SeedProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    syncParams();
    engine.setParams (params);
    engine.prepare (sampleRate);
    safety.prepare (sampleRate);
    mono.assign ((size_t) std::max (samplesPerBlock, 1), 0.0f);
    droneSounding = -1;
}

void SeedProcessor::syncParams()
{
    params.seed = pSeed->load();
    params.partials = juce::jlimit (1, kMaxPartials, (int) std::lround (pPartials->load()));
    params.rolloff = pRolloff->load();
    params.attackMs = pAttack->load();
    params.releaseMs = pRelease->load();
    params.gainDb = pGain->load();
    params.keysFollowSeed = pKeysFollow->load() > 0.5f ? 1.0 : 0.0;
    params.pitchBendCents = pitchBendCents;
}

void SeedProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
    {
        // Never release the drone's key from the piano roll: the drone owns it.
        if (m.getNoteNumber() != droneSounding)
            engine.noteOff (m.getNoteNumber());
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        engine.allNotesOff();
        droneSounding = -1; // re-triggered by updateDrone() if still switched on
    }
    else if (m.isPitchWheel())
    {
        // +-2 semitones, the common default. Smoothed inside the engine.
        pitchBendCents = (m.getPitchWheelValue() - 8192) / 8192.0 * 200.0;
        params.pitchBendCents = pitchBendCents;
        engine.setParams (params);
    }
}

void SeedProcessor::updateDrone()
{
    const bool on = pDrone->load() > 0.5f;
    const int key = (int) std::lround (pDroneNote->load());
    if (on && droneSounding != key)
    {
        if (droneSounding >= 0) engine.noteOff (droneSounding);
        engine.noteOn (key, 0.8f);
        droneSounding = key;
    }
    else if (! on && droneSounding >= 0)
    {
        engine.noteOff (droneSounding);
        droneSounding = -1;
    }
}

void SeedProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if ((int) mono.size() < n) mono.resize ((size_t) n); // hosts may exceed the promised size

    syncParams();
    engine.setParams (params);
    updateDrone();

    std::fill (mono.begin(), mono.begin() + n, 0.0f);

    // Render between MIDI events so notes start exactly where the piano roll put them.
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > pos) { engine.render (mono.data() + pos, at - pos); pos = at; }
        handleMidi (meta.getMessage());
    }
    if (pos < n) engine.render (mono.data() + pos, n - pos);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, mono.data(), n);

    safety.process (buffer); // always last, never bypassed
    uiActiveVoices.store (engine.activeVoiceCount(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* SeedProcessor::createEditor() { return new SeedEditor (*this); }

void SeedProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SeedProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SeedProcessor(); }
