cmake_minimum_required(VERSION 3.22)

# ============================================================================
#  Redline 120 — high-gain 6L6 amp simulator (VST3 / AU / Standalone)
#
#  JUCE is pulled in via FetchContent, so a clean checkout builds with just:
#     cmake -B build -DCMAKE_BUILD_TYPE=Release
#     cmake --build build --parallel
#
#  DSP unit tests (no JUCE required) build alongside and register with CTest:
#     ctest --test-dir build --output-on-failure
# ============================================================================

project(Redline120 VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(REDLINE_BUILD_PLUGIN "Build the JUCE plugin (needs network to fetch JUCE)" ON)
option(REDLINE_BUILD_TESTS  "Build the standalone DSP unit tests" ON)
# LTO gives a marginally faster plugin but makes the MSVC link brutally slow on
# CI runners (tens of minutes, sometimes memory-bound). On by default for local
# release builds; CI configures with -DREDLINE_ENABLE_LTO=OFF.
option(REDLINE_ENABLE_LTO   "Link the plugin with link-time optimisation" ON)

# ----------------------------------------------------------------------------
#  DSP unit tests — pure C++, compile without JUCE. Always cheap to build.
# ----------------------------------------------------------------------------
if(REDLINE_BUILD_TESTS)
    enable_testing()
    add_executable(redline_dsp_tests tests/dsp_tests.cpp)
    target_include_directories(redline_dsp_tests PRIVATE Source)
    target_compile_features(redline_dsp_tests PRIVATE cxx_std_17)
    if(NOT MSVC)
        target_compile_options(redline_dsp_tests PRIVATE -O2 -Wall -Wextra)
    endif()
    add_test(NAME dsp_tests COMMAND redline_dsp_tests)

    add_executable(redline_ja_tests tests/transformer_ja_tests.cpp)
    target_include_directories(redline_ja_tests PRIVATE Source)
    target_compile_features(redline_ja_tests PRIVATE cxx_std_17)
    if(NOT MSVC)
        target_compile_options(redline_ja_tests PRIVATE -O2 -Wall -Wextra)
    endif()
    add_test(NAME transformer_ja_tests COMMAND redline_ja_tests)
endif()

# ----------------------------------------------------------------------------
#  The plugin.
# ----------------------------------------------------------------------------
if(REDLINE_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(Redline120
        PRODUCT_NAME              "Redline 120"
        COMPANY_NAME              "TEKK Audio Labs"
        BUNDLE_ID                 com.tekkaudio.redline120
        PLUGIN_MANUFACTURER_CODE  Tekk
        PLUGIN_CODE               Rl12
        FORMATS                   VST3 AU Standalone
        IS_SYNTH                  FALSE
        NEEDS_MIDI_INPUT          FALSE
        NEEDS_MIDI_OUTPUT         FALSE
        IS_MIDI_EFFECT            FALSE
        COPY_PLUGIN_AFTER_BUILD   FALSE
        VST3_CATEGORIES           "Fx" "Distortion"
        AU_MAIN_TYPE              "kAudioUnitType_Effect")

    juce_generate_juce_header(Redline120)

    target_sources(Redline120 PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_compile_definitions(Redline120 PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0)

    set(redline_juce_flags
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    if(REDLINE_ENABLE_LTO)
        list(APPEND redline_juce_flags juce::juce_recommended_lto_flags)
    endif()

    target_link_libraries(Redline120
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            ${redline_juce_flags})

    # End-to-end smoke test: instantiates the processor and runs audio through
    # the whole chain. Console app so it links the same JUCE modules.
    if(REDLINE_BUILD_TESTS)
        juce_add_console_app(redline_smoke PRODUCT_NAME "redline_smoke")
        juce_generate_juce_header(redline_smoke)
        target_include_directories(redline_smoke PRIVATE Source)
        target_sources(redline_smoke PRIVATE
            tests/processor_smoke.cpp
            Source/PluginProcessor.cpp
            Source/PluginEditor.cpp)
        target_compile_definitions(redline_smoke PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_STANDALONE_APPLICATION=1)
        target_link_libraries(redline_smoke
            PRIVATE
                juce::juce_audio_utils
                juce::juce_dsp
            PUBLIC
                juce::juce_recommended_config_flags
                juce::juce_recommended_warning_flags)
        add_test(NAME processor_smoke COMMAND redline_smoke)
    endif()
endif()
