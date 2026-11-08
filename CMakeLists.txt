cmake_minimum_required(VERSION 3.25)

project(LuckySeven VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES arm64)
set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(LuckySeven
    COMPANY_NAME "Lucky Seven"
    BUNDLE_ID com.luckyseven.reverb
    PLUGIN_MANUFACTURER_CODE Lcky
    PLUGIN_CODE Lsv7
    FORMATS VST3 Standalone
    PRODUCT_NAME "Lucky Seven"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Fx Reverb
    COPY_PLUGIN_AFTER_BUILD TRUE
)

set(LUCKY_SEVEN_CORE_SOURCES)

target_sources(LuckySeven PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ${LUCKY_SEVEN_CORE_SOURCES}
)

target_include_directories(LuckySeven PRIVATE Source)

target_compile_definitions(LuckySeven PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LuckySeven
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
