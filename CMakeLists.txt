cmake_minimum_required(VERSION 3.22)
project(RetroSurroundVST3 VERSION 0.1.0 LANGUAGES C CXX)

set(JUCE_SOURCE_DIR "" CACHE PATH "Path to the JUCE source checkout")
if(NOT JUCE_SOURCE_DIR OR NOT EXISTS "${JUCE_SOURCE_DIR}/modules/CMakeLists.txt")
    message(FATAL_ERROR "Set JUCE_SOURCE_DIR to a JUCE source checkout containing modules/CMakeLists.txt")
endif()

add_subdirectory("${JUCE_SOURCE_DIR}" JUCE)

juce_add_plugin(RetroSurround
    VERSION 0.1.0
    COMPANY_NAME "Daniel Ricardo"
    BUNDLE_ID com.retrosurround.plugin
    PLUGIN_MANUFACTURER_CODE Hbrw
    PLUGIN_CODE Rtsr
    FORMATS VST3
    PRODUCT_NAME "RetroSurround"
    DESCRIPTION "Stereo-to-multichannel surround decoder"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Spatial
)

juce_generate_juce_header(RetroSurround)

target_sources(RetroSurround PRIVATE
    Source/PluginProcessor.cpp
    Source/freesurround_decoder.cpp
    Source/channelmaps.cpp
    Source/kiss_fft.c
    Source/kiss_fftr.c
)

target_compile_features(RetroSurround PRIVATE cxx_std_17)
target_compile_definitions(RetroSurround PRIVATE
    kiss_fft_scalar=double
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(RetroSurround PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_basics
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags
)
