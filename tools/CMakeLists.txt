# Headless tests. Run by the agent in-session (Linux) and by CI (Windows) via ctest.
#
# SeedTests compiles the plugin's processor/editor sources directly (not the plugin target) so
# it can drive the real SeedProcessor: parameters, MIDI, safety stage, state save/load.
#   ./SeedTests                   run all checks (non-zero exit on failure)
#   ./SeedTests --snapshot x.png  also render the editor to a PNG (needs a display: xvfb-run)
juce_add_console_app(SeedTests PRODUCT_NAME "SeedTests")
target_sources(SeedTests PRIVATE
    SeedTests.cpp
    ${CMAKE_SOURCE_DIR}/plugins/Seed/Source/SeedParams.cpp
    ${CMAKE_SOURCE_DIR}/plugins/Seed/Source/PluginProcessor.cpp
    ${CMAKE_SOURCE_DIR}/plugins/Seed/Source/PluginEditor.cpp)
target_include_directories(SeedTests PRIVATE ${CMAKE_SOURCE_DIR}/plugins/Seed/Source)
target_compile_definitions(SeedTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(SeedTests PRIVATE
    juce::juce_audio_utils
    pvst_shared
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

add_test(NAME SeedTests COMMAND SeedTests)
