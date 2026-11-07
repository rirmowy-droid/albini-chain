cmake_minimum_required(VERSION 3.22)

project(AlbiniChain VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static MSVC runtime -> the VST3 runs on machines without the VC++ redistributable.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---------------------------------------------------------------------------
# JUCE: use ./JUCE if present (CI clones it there), otherwise fetch it.
# ---------------------------------------------------------------------------
set(ALBINI_JUCE_TAG "8.0.4" CACHE STRING "JUCE git tag used when fetching")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    message(STATUS "JUCE not found in ./JUCE - fetching ${ALBINI_JUCE_TAG}")
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${ALBINI_JUCE_TAG}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
juce_add_plugin(AlbiniChain
    COMPANY_NAME                "Custom"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    PLUGIN_MANUFACTURER_CODE    Cust
    PLUGIN_CODE                 Albi
    FORMATS                     VST3
    VST3_CATEGORIES             Fx Distortion
    PRODUCT_NAME                "Albini Chain"
    BUNDLE_ID                   "com.custom.albinichain")

target_sources(AlbiniChain
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DSP/AmpStage.cpp
        Source/DSP/CabinetStage.cpp
        Source/DSP/SimpleRoom.cpp
        Source/DSP/ConsoleSaturation.cpp
        Source/DSP/TapeEmulation.cpp)

target_include_directories(AlbiniChain PRIVATE Source)

target_compile_definitions(AlbiniChain
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

if(MSVC)
    # Source files are UTF-8; keep MSVC from guessing a code page.
    target_compile_options(AlbiniChain PRIVATE /utf-8)
endif()

target_link_libraries(AlbiniChain
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
