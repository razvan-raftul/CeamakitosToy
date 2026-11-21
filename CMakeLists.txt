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

# filele pluginului apar in Windows (UIA) ca file adevarate (TabItem), nu ca butoane radio
set(_uia_file "${juce_SOURCE_DIR}/modules/juce_gui_basics/native/accessibility/juce_AccessibilityElement_windows.cpp")
file(READ "${_uia_file}" _uia_src)
string(FIND "${_uia_src}" "case AccessibilityRole::radioButton:   return UIA_RadioButtonControlTypeId;" _uia_pos)
if(_uia_pos EQUAL -1)
  string(FIND "${_uia_src}" "return UIA_TabItemControlTypeId; // Ceamakito" _uia_done)
  if(_uia_done EQUAL -1)
    message(FATAL_ERROR "Nu am gasit maparea radioButton in ${_uia_file}")
  endif()
else()
  string(REPLACE "case AccessibilityRole::radioButton:   return UIA_RadioButtonControlTypeId;"
                 "case AccessibilityRole::radioButton:   return UIA_TabItemControlTypeId; // Ceamakito"
                 _uia_src "${_uia_src}")
  file(WRITE "${_uia_file}" "${_uia_src}")
endif()

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
