cmake_minimum_required(VERSION 3.22)
project(BlackLang VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE (for audio only)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.0
)
FetchContent_MakeAvailable(JUCE)

# Fetch Raylib for GUI
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.0
)
FetchContent_MakeAvailable(raylib)

# Main executable (Raylib GUI)
add_executable(blacklang-raylib
    src/main_raylib.cpp
    src/GuiRenderer.cpp
    src/AudioEngine.cpp
    src/ProjectFile.cpp
    src/tinyfiledialogs.c
    src/gui/Widgets.cpp
    src/gui/TransportBar.cpp
    src/gui/StepGrid.cpp
    src/gui/DragDrop.cpp
    src/gui/TrackView.cpp
    src/gui/PatternEditor.cpp
    src/gui/FXControls.cpp
    src/fx/FXProcessor.cpp
)

target_include_directories(blacklang-raylib PRIVATE src)

target_link_libraries(blacklang-raylib
    PRIVATE
        raylib
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(blacklang-raylib
    PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
)
