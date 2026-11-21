# =============================================================================
#  Obsidian Amp Suite: amp sim plugin (VST3 + Standalone)
#
#  Build (Windows, from this folder):
#     cmake -B build -A x64      (uses the newest Visual Studio installed)
#     cmake --build build --config Release
#  Or just run build_windows.bat. See README.md for everything else.
# =============================================================================
cmake_minimum_required(VERSION 3.24)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" OBSIDIAN_VERSION)
string(STRIP "${OBSIDIAN_VERSION}" OBSIDIAN_VERSION)

project(ObsidianSuite VERSION ${OBSIDIAN_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    # Static runtime: the VST3 and the app run on any Windows 10/11 PC without extra installs.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/MP /utf-8 /bigobj)
endif()

option(OBSIDIAN_ENABLE_ASIO "Build the standalone app with ASIO support (Windows)" ON)

include(cmake/Dependencies.cmake)

# -----------------------------------------------------------------------------
#  NAM core (Neural Amp Modeler, MIT) as a static library
# -----------------------------------------------------------------------------
file(GLOB NAM_SOURCES_TOP "${nam_core_SOURCE_DIR}/NAM/*.cpp")
file(GLOB NAM_SOURCES_SUB "${nam_core_SOURCE_DIR}/NAM/*/*.cpp")
# Note: NAM registers its model types from static initialisers; NamEngine.cpp names them all
# (ensureNamArchitectures) so the linker can't drop SlimmableContainer & co. from the static archive.
add_library(obsidian_nam STATIC ${NAM_SOURCES_TOP} ${NAM_SOURCES_SUB})
target_include_directories(obsidian_nam SYSTEM PUBLIC
    "${nam_core_SOURCE_DIR}"
    "${nam_core_SOURCE_DIR}/Dependencies/nlohmann"
    "${eigen_SOURCE_DIR}"
    "${audiodsptools_SOURCE_DIR}")
target_compile_definitions(obsidian_nam PUBLIC NAM_SAMPLE_FLOAT NAM_ENABLE_A2_FAST)
if(MSVC)
    target_compile_definitions(obsidian_nam PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
    target_compile_options(obsidian_nam PRIVATE /W0 "$<$<CONFIG:Release>:/O2>")
else()
    target_compile_options(obsidian_nam PRIVATE -w "$<$<CONFIG:Release>:-O3>")
endif()
set_target_properties(obsidian_nam PROPERTIES FOLDER "Dependencies")

# -----------------------------------------------------------------------------
#  The plugin
# -----------------------------------------------------------------------------
set(OBSIDIAN_FORMATS VST3 Standalone)

juce_add_plugin(ObsidianSuite
    VERSION                     ${OBSIDIAN_VERSION}
    COMPANY_NAME                "Emil"
    COMPANY_WEBSITE             "https://github.com/Extricl4/ObsidianSuite"
    PRODUCT_NAME                "Obsidian Amp Suite"
    BUNDLE_ID                   "com.emil.obsidiansuite"
    PLUGIN_MANUFACTURER_CODE    Emil
    PLUGIN_CODE                 Obsd
    FORMATS                     ${OBSIDIAN_FORMATS}
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Distortion Amplifier
    MICROPHONE_PERMISSION_ENABLED TRUE
    NEEDS_WEBVIEW2              TRUE
    NEEDS_WEB_BROWSER           TRUE
    ICON_BIG                    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png")

file(GLOB_RECURSE OBSIDIAN_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h")
target_sources(ObsidianSuite PRIVATE ${OBSIDIAN_SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" FILES ${OBSIDIAN_SOURCES})
target_include_directories(ObsidianSuite PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

# The web UI and factory presets are compiled into the plugin.
# UI files live in ../ui (shared with the browser demo); the bridge script lives here.
file(GLOB OBSIDIAN_FACTORY_PRESETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Presets/*.json")
file(GLOB OBSIDIAN_UI_FONTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../ui/fonts/*.woff2")   # theme fonts (OFL)
juce_add_binary_data(ObsidianAssets
    NAMESPACE ObsidianAssets
    HEADER_NAME ObsidianAssets.h
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/../ui/obsidian-demo.html"
        "${CMAKE_CURRENT_SOURCE_DIR}/../ui/grain.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/WebUI/obsidian-bridge.js"
        "${CMAKE_CURRENT_SOURCE_DIR}/WebUI/obsidian-native.css"
        ${OBSIDIAN_UI_FONTS}
        ${OBSIDIAN_FACTORY_PRESETS})
set_target_properties(ObsidianAssets PROPERTIES FOLDER "Dependencies")

target_compile_definitions(ObsidianSuite
    PUBLIC
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
        JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE=0
        JUCE_MODAL_LOOPS_PERMITTED=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        OBSIDIAN_PRODUCT_NAME="Obsidian Amp Suite"
        OBSIDIAN_VERSION_STRING="${OBSIDIAN_VERSION}")

if(WIN32)
    target_compile_definitions(ObsidianSuite PUBLIC
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_WASAPI=1
        JUCE_DIRECTSOUND=1)
    if(OBSIDIAN_ENABLE_ASIO AND OBSIDIAN_ASIO_SDK_DIR)
        message(STATUS "Obsidian: ASIO enabled (${OBSIDIAN_ASIO_SDK_DIR})")
        target_include_directories(ObsidianSuite PRIVATE "${OBSIDIAN_ASIO_SDK_DIR}/common")
        target_compile_definitions(ObsidianSuite PUBLIC JUCE_ASIO=1 OBSIDIAN_HAS_ASIO=1)
    else()
        message(WARNING "Obsidian: building WITHOUT ASIO (WASAPI/DirectSound still work). Set OBSIDIAN_ASIO_SDK_DIR to enable it.")
    endif()
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(ObsidianSuite PUBLIC JUCE_ALSA=1 JUCE_JACK=1)
endif()

target_link_libraries(ObsidianSuite
    PRIVATE
        ObsidianAssets
        obsidian_nam
        signalsmith-stretch
        juce::juce_audio_utils
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_options(ObsidianSuite PRIVATE /wd4244 /wd4267 /wd4305 /wd4100)
endif()

# The offline self-test lives inside the standalone app:  "Obsidian Amp Suite" --selftest <report.txt>
# It renders audio through the full engine with every module and random settings and
# fails on NaN/Inf, runaway levels or CPU spikes. CI runs it on every build.
