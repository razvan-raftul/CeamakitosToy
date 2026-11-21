cmake_minimum_required(VERSION 3.22)
project(CeamakitosToy VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(CeamakitosToy
  COMPANY_NAME "Razvan"
  PLUGIN_MANUFACTURER_CODE Razv
  PLUGIN_CODE Cmkt
  FORMATS VST3
  PRODUCT_NAME "Ceamakitos toy"
  PLUGIN_NAME "Ceamakito's toy"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  VST3_CATEGORIES Instrument Sampler
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(CeamakitosToy PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(CeamakitosToy PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
  target_compile_options(CeamakitosToy PRIVATE /utf-8)
endif()

target_link_libraries(CeamakitosToy
  PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_formats
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
