cmake_minimum_required(VERSION 3.22)

# ---------------------------------------------------------------------------
#  SUCCO - Vocal Chain Plugin (VST3 / AU / Standalone)
#  Original modular vocal chain inspired by the "syrup" concept.
#  Built with JUCE 8. Pulls VST3 SDK automatically via FetchContent
#  (no submodule init / VST3 SDK download needed by the user).
# ---------------------------------------------------------------------------

project(SuccoDue2 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Global build options
# ---------------------------------------------------------------------------
option(SUCCO_BUILD_VST3          "Build VST3 format"                 ON)
option(SUCCO_BUILD_AU            "Build Audio Unit (macOS only)"    ON)
option(SUCCO_BUILD_STANDALONE    "Build Standalone app"             ON)
option(SUCCO_BUILD_LV2           "Build LV2 (Linux only)"           OFF)
option(SUCCO_COPY_PLUGIN_AFTER_BUILD "Copy plugin to user folder on build" OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# macOS deployment target — set to Catalina (10.15) for backwards compat.
# Can be overridden by passing -DCMAKE_OSX_DEPLOYMENT_TARGET=<version> on the
# command line.
# ---------------------------------------------------------------------------
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "macOS min deployment target" FORCE)
endif()

# ---------------------------------------------------------------------------
# Pull JUCE via FetchContent (no submodule needed)
# ---------------------------------------------------------------------------
include(FetchContent)

# Pin JUCE to a stable tag. Update here to upgrade JUCE.
set(JUCE_GIT_TAG "8.0.4" CACHE STRING "JUCE git tag")

message(STATUS "Fetching JUCE ${JUCE_GIT_TAG} from GitHub (one-time download)...")
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${JUCE_GIT_TAG}
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Resolve the FORMATS list (must be set BEFORE juce_add_plugin)
# ---------------------------------------------------------------------------
set(SUCCO_BUILD_FORMATS)

if(SUCCO_BUILD_VST3)
    list(APPEND SUCCO_BUILD_FORMATS VST3)
endif()
if(APPLE AND SUCCO_BUILD_AU)
    list(APPEND SUCCO_BUILD_FORMATS AU)
endif()
if(SUCCO_BUILD_STANDALONE)
    list(APPEND SUCCO_BUILD_FORMATS Standalone)
endif()
if(UNIX AND NOT APPLE AND SUCCO_BUILD_LV2)
    list(APPEND SUCCO_BUILD_FORMATS LV2)
endif()

if(NOT SUCCO_BUILD_FORMATS)
    message(WARNING "No plugin formats selected. Forcing VST3.")
    set(SUCCO_BUILD_FORMATS VST3)
endif()

message(STATUS "SUCCO formats: ${SUCCO_BUILD_FORMATS}")

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
set(SUCCO_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/dsp/ChainProcessor.h
    Source/dsp/ChainProcessor.cpp
    Source/dsp/Gate.h
    Source/dsp/EQ.h
    Source/dsp/Compressor.h
    Source/dsp/DeEsser.h
    Source/dsp/Drive.h
    Source/dsp/Doubler.h
    Source/dsp/Chorus.h
    Source/dsp/Delay.h
    Source/dsp/Reverb.h
    Source/dsp/Limiter.h
    Source/dsp/PitchShift.h
    Source/dsp/Oversampler.h
    Source/gui/SuccoLookAndFeel.h
    Source/gui/SuccoLookAndFeel.cpp
    Source/gui/KnobComponent.h
    Source/gui/KnobComponent.cpp
    Source/gui/ModuleTabBar.h
    Source/gui/ModuleTabBar.cpp
    Source/gui/DisplayComponent.h
    Source/gui/DisplayComponent.cpp
    Source/gui/MeterComponent.h
    Source/gui/CurveDisplayComponent.h
    Source/params/ParameterManager.h
    Source/params/ParameterManager.cpp
    Source/presets/PresetManager.h
    Source/presets/PresetManager.cpp
)

# ---------------------------------------------------------------------------
# JUCE plugin target
# ---------------------------------------------------------------------------
juce_add_plugin(SuccoDue2
    COMPANY_NAME              "CantinaAudio"
    PLUGIN_MANUFACTURER_CODE  Cant
    PLUGIN_CODE               Sdu2
    FORMATS                   ${SUCCO_BUILD_FORMATS}
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   ${SUCCO_COPY_PLUGIN_AFTER_BUILD}
    PRODUCT_NAME              "SUCCO Due 2"
    PLUGIN_VST3_CATEGORY      Fx Vocal
    AU_MAIN_TYPE              kAudioUnitType_Effect
    DESCRIPTION               "SUCCO - modular vocal chain (gate / EQ / comp / de-ess / drive / double / chorus / delay / reverb / limit)"
)

target_sources(SuccoDue2 PRIVATE ${SUCCO_SOURCES})

target_include_directories(SuccoDue2 PRIVATE
    Source
    Source/dsp
    Source/gui
    Source/params
    Source/presets
)

target_compile_definitions(SuccoDue2 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(SuccoDue2 PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

set_target_properties(SuccoDue2 PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
