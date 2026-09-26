# Seed — stretched-partial additive synth. See plugins/Seed/README.md.
juce_add_plugin(Seed
    COMPANY_NAME "Partial"
    PLUGIN_MANUFACTURER_CODE Prtl      # shared by every plugin in this repo
    PLUGIN_CODE Seed                   # unique per plugin, never change after release
    PRODUCT_NAME "Seed"
    VERSION 0.2.0
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    FORMATS VST3 Standalone)

target_sources(Seed PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(Seed PUBLIC Source)

target_compile_definitions(Seed PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Seed
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        pvst_shared
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
