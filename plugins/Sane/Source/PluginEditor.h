#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
class SaneProcessor;

// TEMPORARY placeholder (engine bring-up): the real desert-themed editor replaces this.
class SaneEditor : public juce::GenericAudioProcessorEditor
{
public:
    explicit SaneEditor (SaneProcessor& p);
};
