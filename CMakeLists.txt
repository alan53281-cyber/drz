cmake_minimum_required(VERSION 3.22)
project(FORGE_DRUM VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is downloaded automatically (GitHub Actions does this for you)
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ForgeDrum
    COMPANY_NAME "Forge"
    PLUGIN_MANUFACTURER_CODE Frge
    PLUGIN_CODE FrDr
    PRODUCT_NAME "FORGE-DRUM"
    FORMATS VST3
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum Generator)

target_sources(ForgeDrum PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(ForgeDrum PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ForgeDrum
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Tests (not needed for the plugin): engine checks + a headless run of the real processor
option(FORGE_BUILD_TESTS "Build tests" OFF)
if(FORGE_BUILD_TESTS)
    add_executable(EngineTest Tests/EngineTest.cpp)
    target_include_directories(EngineTest PRIVATE Source)

    juce_add_console_app(ProcessorTest PRODUCT_NAME "ProcessorTest")
    target_sources(ProcessorTest PRIVATE Tests/ProcessorTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(ProcessorTest PRIVATE Source)
    target_compile_definitions(ProcessorTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(ProcessorTest PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
