
cmake_minimum_required(VERSION 3.22)

project(SoundModule VERSION 1.0 LANGUAGES C CXX)

add_subdirectory(External/JUCE)

find_package(FluidSynth CONFIG REQUIRED)

juce_add_gui_app(SoundModule
    PRODUCT_NAME "SoundModule"
)

juce_generate_juce_header(SoundModule)

target_sources(SoundModule PRIVATE
    Source/Main.cpp
    Source/FluidSynthEngine.cpp
    Source/FluidSynthEngine.h
)

target_compile_features(SoundModule PRIVATE cxx_std_17)

target_compile_definitions(SoundModule PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(SoundModule PRIVATE
    juce::juce_gui_extra
    juce::juce_audio_utils
    FluidSynth::libfluidsynth
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)