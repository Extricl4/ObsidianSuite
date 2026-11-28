# =============================================================================
#  ORPHEUS: SIREN (the synth) + its EFFECTS + ORPHEUS (draw chord lines, it plays them)
#  as a VST3 instrument and a standalone app.
#
#  Build (Windows, from this folder):  build_windows.bat
#     or: cmake -B build -A x64  &&  cmake --build build --config Release
#  GitHub builds it on every push to the "orpheus" branch (.github/workflows/orpheus.yml).
# =============================================================================
cmake_minimum_required(VERSION 3.24)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" ORPHEUS_VERSION)
string(STRIP "${ORPHEUS_VERSION}" ORPHEUS_VERSION)
project(Orpheus VERSION ${ORPHEUS_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")   # runs on any Windows 10/11 PC
    add_compile_options(/MP /utf-8 /bigobj)
endif()

# JUCE 8.0.10 (pinned). Point at a local copy with -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/src/JUCE
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.10 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(juce)

juce_add_plugin(Orpheus
    VERSION                     ${ORPHEUS_VERSION}
    COMPANY_NAME                "Emil"
    COMPANY_WEBSITE             "https://github.com/Extricl4/ObsidianSuite"
    PRODUCT_NAME                "ORPHEUS"
    BUNDLE_ID                   "com.emil.orpheus"
    PLUGIN_MANUFACTURER_CODE    Emil
    PLUGIN_CODE                 Orph
    FORMATS                     VST3 Standalone
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    NEEDS_WEBVIEW2              TRUE
    NEEDS_WEB_BROWSER           TRUE)

set(ORPHEUS_DSP
    Source/Params.cpp Source/Params.h
    Source/DSP/Prims.h Source/DSP/Wavetables.cpp Source/DSP/Wavetables.h
    Source/DSP/SirenSynth.cpp Source/DSP/SirenSynth.h
    Source/DSP/Effects.cpp Source/DSP/Effects.h Source/DSP/Shimmer.h Source/DSP/ReverbIR.h Source/DSP/Dynamics.h
    Source/Seq/Song.h
    Source/PluginProcessor.cpp Source/PluginProcessor.h)
target_sources(Orpheus PRIVATE ${ORPHEUS_DSP} Source/PluginEditor.cpp Source/PluginEditor.h)
target_include_directories(Orpheus PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

# the page (WebUI/, built from ui/ by tools/make_ui.py) is compiled in
file(GLOB ORPHEUS_FONTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/WebUI/fonts/*.woff2")
juce_add_binary_data(OrpheusAssets
    NAMESPACE OrpheusAssets
    HEADER_NAME OrpheusAssets.h
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/WebUI/index.html"
        "${CMAKE_CURRENT_SOURCE_DIR}/WebUI/grain.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/WebUI/orpheus-bridge.js"
        ${ORPHEUS_FONTS})

set(ORPHEUS_DEFS
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)
target_compile_definitions(Orpheus PUBLIC ${ORPHEUS_DEFS})
if(WIN32)
    target_compile_definitions(Orpheus PUBLIC JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1 JUCE_WASAPI=1 JUCE_DIRECTSOUND=1)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(Orpheus PUBLIC JUCE_ALSA=1 JUCE_JACK=0)   # (Linux: development only)
endif()
target_link_libraries(Orpheus
    PRIVATE OrpheusAssets juce::juce_audio_utils juce::juce_audio_devices juce::juce_dsp juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
if(MSVC)
    target_compile_options(Orpheus PRIVATE /wd4244 /wd4267 /wd4305 /wd4100)
endif()

# -----------------------------------------------------------------------------
#  The offline test: plays the default song and a sweep of settings through the
#  engine (no window) and fails on silence, NaN, clipping or runaway levels.
#      OrpheusTest [out.wav]
# -----------------------------------------------------------------------------
juce_add_console_app(OrpheusTest PRODUCT_NAME "OrpheusTest")
target_sources(OrpheusTest PRIVATE ${ORPHEUS_DSP} Source/Tests/OrpheusTest.cpp)
target_include_directories(OrpheusTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_definitions(OrpheusTest PRIVATE ORPHEUS_NO_EDITOR=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=0)
target_link_libraries(OrpheusTest
    PRIVATE juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(MSVC)
    target_compile_options(OrpheusTest PRIVATE /wd4244 /wd4267 /wd4305 /wd4100)
endif()
