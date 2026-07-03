# ==============================================================================
#  The Crock-Pot — root build file
#  Derived from sudara/pamplejuce (MIT) 2026-07-02, adapted:
#    - JUCE pinned to release 8.0.13 via FetchContent (pamplejuce tracks the
#      `develop` branch as a git submodule; we want reproducible gift builds).
#      Release tag 8.0.13 == commit 7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2
#      (github.com/juce-framework/JUCE/releases, fetched 2026-07-02).
#    - Dropped for M0: CLAP, Intel IPP, sccache, Catch2 tests, signing hooks.
#      Tests return in M1. Signing never (gift path, decision D6).
#  Hard rules honored here: D3 native UI (JUCE_WEB_BROWSER=0), D9 audio-effect
#  device (IS_SYNTH FALSE, VST3 "Fx", AU kAudioUnitType_Effect).
# ==============================================================================

cmake_minimum_required(VERSION 3.25)

# ------------------------------------------------------------------ versioning
# Single source of truth for the version: the VERSION file next to this one.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" CURRENT_VERSION LIMIT_COUNT 1)

# --------------------------------------------------------------- macOS basics
# Must be set before project(). Values from pamplejuce's PamplejuceMacOS.cmake
# (fetched 2026-07-02): support back to Mojave, universal binary on CI Release.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Support macOS down to Mojave" FORCE)
if((DEFINED ENV{CI}) AND (CMAKE_BUILD_TYPE STREQUAL "Release"))
    message(STATUS "CI Release: building macOS universal (arm64 + x86_64)")
    set(CMAKE_OSX_ARCHITECTURES arm64 x86_64)
endif()
set(CMAKE_XCODE_GENERATE_SCHEME OFF)

# ---------------------------------------------------------------- the project
set(PROJECT_NAME "CrockPot")            # internal target name — no spaces
set(PRODUCT_NAME "The Crock-Pot")       # what Ableton Live displays
set(COMPANY_NAME "Poppys Kitchen")      # shows as the plugin vendor in Live
set(BUNDLE_ID    "com.poppyskitchen.crockpot")

project(${PROJECT_NAME} VERSION ${CURRENT_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# -------------------------------------------------------------- fetching JUCE
# Pinned to the 8.0.13 release tag (latest stable as of 2026-07-02).
# First configure downloads JUCE (~100 MB); later configures reuse the cache.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.13      # == commit 7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ------------------------------------------------------------------ the plugin
# See JUCE/docs/CMake API.md for every option used below.
juce_add_plugin("${PROJECT_NAME}"
    COMPANY_NAME                "${COMPANY_NAME}"
    BUNDLE_ID                   "${BUNDLE_ID}"
    PRODUCT_NAME                "${PRODUCT_NAME}"

    # Identity codes. Manufacturer: first char uppercase (AU requirement).
    # Plugin code: unique, at least one uppercase. Never change these after
    # people have saved Live sets with the plugin, or their sets break.
    PLUGIN_MANUFACTURER_CODE    Popy
    PLUGIN_CODE                 Ckpt

    # D9 — an AUDIO EFFECT device, explicitly. This is what makes Live put us
    # in the audio-effect browser and lets us nest in an Audio Effect Rack.
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    VST3_CATEGORIES             "Fx"
    AU_MAIN_TYPE                "kAudioUnitType_Effect"

    # D2 — VST3 (Win+Mac) + AU (Mac). JUCE skips AU automatically off-Apple.
    FORMATS                     VST3 AU

    # Deliberate: don't auto-copy into system plugin folders after build.
    # (Copying into C:\Program Files needs admin and can fail the build —
    # BUILDING.md tells Poppy exactly which file to copy where instead.)
    COPY_PLUGIN_AFTER_BUILD     FALSE

    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

# ------------------------------------------------- shared code (pamplejuce ism)
# Plugin sources live in an INTERFACE target so the M1 test target can link
# the same code without ODR violations.
add_library(SharedCode INTERFACE)

file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
target_sources(SharedCode INTERFACE ${SourceFiles})

target_compile_definitions(SharedCode
    INTERFACE
    # D3/D7 — native UI, local-first: no web view, no networking, compiled out.
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    # NOTE (license): JUCE splash stays ON. We may set JUCE_DISPLAY_SPLASH_SCREEN=0
    # only under a paid license or if the plugin itself is released GPLv3/AGPLv3
    # (juce.com/legal/juce-8-licence, noted 2026-07-02). Poppy's call, later.
    CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    VERSION="${CURRENT_VERSION}"
    PRODUCT_NAME_WITHOUT_VERSION="Crock-Pot"
)

# MSVC: our sources are UTF-8 (the editor draws "·" and "—"). Without this flag
# MSVC assumes Windows-1252 and mangles those into mojibake — M0 bug #1, found
# by Poppy in the first real Live 12 load (2026-07-02). Clang/AppleClang already
# assume UTF-8, so mac builds were never affected.
target_compile_options(SharedCode INTERFACE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(SharedCode
    INTERFACE
    juce_audio_utils
    juce_dsp
    juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

target_link_libraries("${PROJECT_NAME}" PRIVATE SharedCode)

# ----------------------------------------------------------------------- tests
# Catch2 console app covering the offline splitter engine (M4a). The plugin
# target stays test-free; the audio-thread gate remains pluginval.
option(CROCKPOT_BUILD_TESTS "Build the Catch2 test target" ON)
if(CROCKPOT_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(Catch2)

    juce_add_console_app(Tests PRODUCT_NAME "CrockPot Tests")

    target_sources(Tests PRIVATE tests/SplitterTests.cpp)
    target_include_directories(Tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(Tests PRIVATE cxx_std_20)
    target_compile_options(Tests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

    target_link_libraries(Tests PRIVATE
        Catch2::Catch2WithMain
        juce_audio_formats
        juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    enable_testing()
    add_test(NAME crockpot_splitter_tests COMMAND Tests)
endif()

# ------------------------------------------------------------------- CI helper
# Writes a .env the GitHub workflow reads (pamplejuce's GitHubENV.cmake, inlined).
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/../.env"
    "PROJECT_NAME=${PROJECT_NAME}\n"
    "PRODUCT_NAME=${PRODUCT_NAME}\n"
    "VERSION=${CURRENT_VERSION}\n")
