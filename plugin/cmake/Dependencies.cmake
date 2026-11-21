# =============================================================================
#  Third-party code, pinned to exact versions so a build never changes by itself.
#
#  Every dependency can be pointed at a local folder instead of downloading,
#  e.g.  -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/src/JUCE
#  (names: JUCE, NAM_CORE, EIGEN, AUDIODSPTOOLS, SIGNALSMITH-LINEAR, SIGNALSMITH-STRETCH)
# =============================================================================
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# JUCE 8 (AGPLv3 / JUCE licence). Plugin framework, VST3 + standalone wrappers, WebView UI host.
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.10
    GIT_SHALLOW    TRUE)

# NeuralAmpModelerCore (MIT): loads and runs .nam captures.
FetchContent_Declare(nam_core
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG        v0.6.0
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES "")

# Eigen 3.4.0 (MPL2): linear algebra used by NAM.
set(OBSIDIAN_EIGEN_URL "https://gitlab.com/libeigen/eigen.git" CACHE STRING "Eigen git URL (a GitHub mirror also works)")
FetchContent_Declare(eigen
    GIT_REPOSITORY ${OBSIDIAN_EIGEN_URL}
    GIT_TAG        3.4.0
    GIT_SHALLOW    TRUE)

# AudioDSPTools (MIT): sample-rate converter used when a capture was trained at another rate.
FetchContent_Declare(audiodsptools
    GIT_REPOSITORY https://github.com/sdatkinson/AudioDSPTools.git
    GIT_TAG        95cd88d5fbb31e5470298659b10e6948c46ff001)

# Signalsmith Stretch (MIT): polyphonic, studio-quality pitch shifting for HQ transpose.
FetchContent_Declare(signalsmith-linear
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/linear.git
    GIT_TAG        7bb1c8f36794d6980080ce817c91f8da8fe56a66)   # 0.6.6
FetchContent_Declare(signalsmith-stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG        88061fd4eda56a2353c5d65c74aa3f4ccf76f9da)

# JUCE and Signalsmith are CMake subprojects; the others are folders we compile ourselves.
FetchContent_MakeAvailable(juce signalsmith-linear signalsmith-stretch)

foreach(dep nam_core eigen audiodsptools)
    FetchContent_GetProperties(${dep})
    if(NOT ${dep}_POPULATED)
        if(POLICY CMP0169)
            cmake_policy(PUSH)
            cmake_policy(SET CMP0169 OLD)
        endif()
        FetchContent_Populate(${dep})
        if(POLICY CMP0169)
            cmake_policy(POP)
        endif()
    endif()
endforeach()

# -----------------------------------------------------------------------------
#  ASIO SDK (Steinberg, dual GPLv3 / proprietary since Oct 2025). Windows standalone only.
#  1. If OBSIDIAN_ASIO_SDK_DIR is set, use it.
#  2. Otherwise download it. If that fails the app still builds, without ASIO.
# -----------------------------------------------------------------------------
if(WIN32 AND OBSIDIAN_ENABLE_ASIO AND NOT OBSIDIAN_ASIO_SDK_DIR)
    FetchContent_Declare(asiosdk
        GIT_REPOSITORY https://github.com/jcelerier/asio-2.git
        GIT_TAG        1bc070d1c7b8f0bf92f9d3b7c72e5880a7dcffdb)
    FetchContent_GetProperties(asiosdk)
    if(NOT asiosdk_POPULATED)
        if(POLICY CMP0169)
            cmake_policy(PUSH)
            cmake_policy(SET CMP0169 OLD)
        endif()
        FetchContent_Populate(asiosdk)
        if(POLICY CMP0169)
            cmake_policy(POP)
        endif()
    endif()
    if(EXISTS "${asiosdk_SOURCE_DIR}/common/iasiodrv.h")
        set(OBSIDIAN_ASIO_SDK_DIR "${asiosdk_SOURCE_DIR}" CACHE PATH "ASIO SDK folder (contains common/iasiodrv.h)" FORCE)
    endif()
endif()
