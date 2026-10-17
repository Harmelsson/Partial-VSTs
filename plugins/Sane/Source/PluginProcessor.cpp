#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace sane;

SaneProcessor::SaneProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SANE", createParameterLayout())
{
}

bool SaneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void SaneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    safety.prepare (sampleRate);
    scratch.setSize (2, std::max (1, samplesPerBlock));
}

EngineParams SaneProcessor::readParams() const
{
    EngineParams p;
    auto v = [this] (const char* id) { return value (id); };
    p.exColor = v (ids::exColor); p.exLength = v (ids::exLength); p.exGrain = v (ids::exGrain); p.exLevelDb = v (ids::exLevel);
    p.exRandom = v (ids::exRandom); p.exVelRandom = v (ids::exVelRandom); p.exVelLength = v (ids::exVelLength);
    p.exKeyColor = v (ids::exKeyColor); p.exSpread = v (ids::exSpread);
    p.stBase = v (ids::stBase); p.stVel = v (ids::stVel); p.stKey = v (ids::stKey); p.stRandom = v (ids::stRandom);
    p.core = (int) v (ids::core);
    p.feedback = v (ids::feedback); p.damp = v (ids::damp); p.tension = v (ids::tension); p.coupling = v (ids::coupling); p.width = v (ids::width);
    p.table = (int) v (ids::table); p.srcA = (int) v (ids::srcA); p.srcB = (int) v (ids::srcB);
    p.morph = v (ids::morph); p.calcSeed = v (ids::calcSeed); p.rolloff = v (ids::rolloff);
    p.pitchAmt = v (ids::pitchAmt); p.pitchTime = v (ids::pitchTime); p.lengthMs = v (ids::length);
    p.tune = v (ids::tune); p.fine = v (ids::fine); p.bendRange = v (ids::bendRange); p.gainDb = v (ids::gain);
    for (int s = 0; s < kNumSlots; ++s)
    {
        p.slotType[(size_t) s] = (int) value (ids::slot (s + 1, "type"));
        p.slotA[(size_t) s] = value (ids::slot (s + 1, "a"));
        p.slotB[(size_t) s] = value (ids::slot (s + 1, "b"));
        p.slotC[(size_t) s] = value (ids::slot (s + 1, "c"));
    }
    return p;
}

void SaneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    uiKeys.processNextMidiBuffer (midi, 0, n, true);

    const auto p = readParams();
    engine.setParams (p);

    if (scratch.getNumSamples() < n) scratch.setSize (2, n, false, false, true);
    scratch.clear();
    float* L = scratch.getWritePointer (0);
    float* R = scratch.getWritePointer (1);

    // Sample-accurate MIDI: render up to each event, then apply it.
    int pos = 0;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > pos) { engine.render (L + pos, R + pos, at - pos); pos = at; }
        if (m.isNoteOn()) engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff()) engine.noteOff (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
        else if (m.isPitchWheel()) engine.setPitchBend ((float) ((m.getPitchWheelValue() - 8192) / 8192.0) * p.bendRange);
    }
    if (pos < n) engine.render (L + pos, R + pos, n - pos);

    if (buffer.getNumChannels() >= 2)
    {
        buffer.copyFrom (0, 0, scratch, 0, 0, n);
        buffer.copyFrom (1, 0, scratch, 1, 0, n);
    }
    else if (buffer.getNumChannels() == 1)
    {
        buffer.copyFromWithRamp (0, 0, scratch.getReadPointer (0), n, 0.5f, 0.5f);
        buffer.addFrom (0, 0, scratch, 1, 0, n, 0.5f);
    }
    safety.process (buffer); // hearing safety: never bypassable
}

juce::AudioProcessorEditor* SaneProcessor::createEditor() { return new SaneEditor (*this); }

void SaneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("version", kVersion, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void SaneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SaneProcessor(); }
