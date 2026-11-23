cmake_minimum_required(VERSION 3.22)
if(NOT DEFINED TOY_VERSION OR TOY_VERSION STREQUAL "")
  set(TOY_VERSION 0.1.0)
endif()
project(CeamakitosToy VERSION ${TOY_VERSION})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(CeamakitosToy
  VERSION ${TOY_VERSION}
  COMPANY_NAME "Razvan"
  PLUGIN_MANUFACTURER_CODE Razv
  PLUGIN_CODE Cmkt
  FORMATS VST3 Standalone
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
  Source/PluginEditor.cpp
  Source/Updater.cpp
  Source/Speech.cpp)

target_compile_definitions(CeamakitosToy PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0
  TOY_VERSION_STRING="${TOY_VERSION}")

# ASIO (pentru intarziere mica in aplicatie), daca SDK-ul Steinberg a fost descarcat in asiosdk/
if(EXISTS "${CMAKE_SOURCE_DIR}/asiosdk/common/iasiodrv.h")
  target_compile_definitions(CeamakitosToy PUBLIC JUCE_ASIO=1)
  target_include_directories(CeamakitosToy PRIVATE "${CMAKE_SOURCE_DIR}/asiosdk/common")
endif()

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
