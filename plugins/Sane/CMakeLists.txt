# Sane — feedback-synthesis drums. See plugins/Sane/README.md and docs/DRUM_RESEARCH.md.
juce_add_plugin(Sane
    COMPANY_NAME "Partial"
    PLUGIN_MANUFACTURER_CODE Prtl      # shared by every plugin in this repo
    PLUGIN_CODE Sane                   # unique per plugin, never change after release
    PRODUCT_NAME "Sane"
    VERSION 0.2.0
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum
    FORMATS VST3 Standalone)

target_sources(Sane PRIVATE
    Source/SaneParams.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(Sane PUBLIC Source)

target_compile_definitions(Sane PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Sane
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        pvst_shared
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
