cmake_minimum_required(VERSION 3.22)
project(BlackLang VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.9
)
FetchContent_MakeAvailable(JUCE)

# Main executable
add_executable(blacklang
    src/main.cpp
    src/AudioEngine.cpp
    src/CLIParser.cpp
    src/ProjectFile.cpp
)

target_include_directories(blacklang PRIVATE src)

# Find readline for CLI line editing
find_library(READLINE_LIB readline)
if(READLINE_LIB)
    target_compile_definitions(blacklang PRIVATE HAS_READLINE=1)
    target_link_libraries(blacklang PRIVATE ${READLINE_LIB})
endif()

target_link_libraries(blacklang
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(blacklang
    PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
)

# GUI executable
add_executable(blacklang-gui
    src/main_gui.cpp
    src/AudioEngine.cpp
    src/CLIParser.cpp
    src/ProjectFile.cpp
)

target_include_directories(blacklang-gui PRIVATE src)

target_link_libraries(blacklang-gui
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Raylib GUI executable
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.0
)
FetchContent_MakeAvailable(raylib)

add_executable(blacklang-raylib
    src/main_raylib.cpp
    src/GuiRenderer.cpp
    src/AudioEngine.cpp
    src/CLIParser.cpp
    src/ProjectFile.cpp
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

