cmake_minimum_required(VERSION 3.22)
project(CONSTELLATION VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
else()
  add_subdirectory(JUCE)
endif()

juce_add_plugin(Constellation
  COMPANY_NAME "Polaris Sound Lab"
  COMPANY_WEBSITE "https://github.com/drophlex/CONSTELLATION"
  BUNDLE_ID "com.polarissoundlab.constellation"
  PLUGIN_MANUFACTURER_CODE Plrs
  PLUGIN_CODE Cnst
  FORMATS VST3 AU
  PRODUCT_NAME "Constellation"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Spatial Surround
  AU_MAIN_TYPE kAudioUnitType_Effect
  DESCRIPTION "3D visual mixer with spectral unmasking, binaural and surround")

target_sources(Constellation PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_include_directories(Constellation PRIVATE Source)

target_compile_definitions(Constellation PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Constellation
  PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

option(CST_BUILD_TESTS "Build offline DSP test" OFF)
if(CST_BUILD_TESTS)
  juce_add_console_app(CstTest PRODUCT_NAME "CstTest")
  target_sources(CstTest PRIVATE Tests/DspTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
  target_include_directories(CstTest PRIVATE Source)
  target_compile_definitions(CstTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 CST_TEST=1 JUCE_MODAL_LOOPS_PERMITTED=1)
  target_link_libraries(CstTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_cryptography juce::juce_recommended_config_flags)
endif()
