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
if (ANDROID)
    set(PLATFORM "Android" CACHE STRING "Raylib Platform" FORCE)
endif()
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.0
)
FetchContent_MakeAvailable(raylib)

# Copy JUCE Java files for Android
if (ANDROID)
    message(STATUS "Copying JUCE Java files...")
    file(COPY "${juce_SOURCE_DIR}/modules/juce_core/native/javacore/init/com" DESTINATION "${CMAKE_SOURCE_DIR}/android/app/src/main/java")
    file(COPY "${juce_SOURCE_DIR}/modules/juce_core/native/javacore/app/com" DESTINATION "${CMAKE_SOURCE_DIR}/android/app/src/main/java")
    file(COPY "${juce_SOURCE_DIR}/modules/juce_audio_devices/native/java/app/com" DESTINATION "${CMAKE_SOURCE_DIR}/android/app/src/main/java")
endif()

# Main Target
if (ANDROID)
    # Android requires a shared library
    add_library(blacklang SHARED
        src/main_raylib.cpp
        src/GuiRenderer.cpp
        src/gui/FontManager.cpp
        src/FilePicker.cpp
        src/gui/FileBrowser.cpp


        src/AudioEngine.cpp
        src/AudioRecorder.cpp
        src/BusManager.cpp
        src/ProjectFile.cpp
        src/tinyfiledialogs.c
        src/gui/Widgets.cpp
        src/gui/TransportBar.cpp
        src/gui/StepGrid.cpp
        src/gui/DragDrop.cpp
        src/gui/TrackView.cpp
        src/gui/PatternEditor.cpp
        src/gui/FXControls.cpp
        src/gui/SampleSlicer.cpp
        src/gui/TrackFX.cpp
        src/gui/RecordingUI.cpp
        src/fx/FXProcessor.cpp
        src/platform/AndroidBridge.cpp
    )
    
    target_include_directories(blacklang PRIVATE src)
    
    # Android App Glue
    add_library(app-glue STATIC
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    )
    target_include_directories(app-glue PUBLIC ${ANDROID_NDK}/sources/android/native_app_glue)

    target_link_libraries(blacklang
        PRIVATE
            -Wl,--whole-archive app-glue -Wl,--no-whole-archive
            raylib
            juce::juce_audio_basics
            juce::juce_audio_devices
            juce::juce_audio_formats
            juce::juce_audio_utils
            juce::juce_core
            log
            android
            EGL
            GLESv2
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )
    
    target_compile_definitions(blacklang
        PRIVATE
            JUCE_STANDALONE_APPLICATION=0
            JUCE_ANDROID=1
            JUCE_USE_CURL=0
            JUCE_WEB_BROWSER=0
    )
else()
    # Desktop Executable
    add_executable(blacklang-raylib
        src/main_raylib.cpp
        src/GuiRenderer.cpp
        src/gui/FontManager.cpp
        src/FilePicker.cpp
        src/gui/FileBrowser.cpp


        src/AudioEngine.cpp
        src/AudioRecorder.cpp
        src/BusManager.cpp
        src/ProjectFile.cpp
        src/tinyfiledialogs.c
        src/gui/Widgets.cpp
        src/gui/TransportBar.cpp
        src/gui/StepGrid.cpp
        src/gui/DragDrop.cpp
        src/gui/TrackView.cpp
        src/gui/PatternEditor.cpp
        src/gui/FXControls.cpp
        src/gui/SampleSlicer.cpp
        src/gui/TrackFX.cpp
        src/gui/RecordingUI.cpp
        src/fx/FXProcessor.cpp
        src/platform/AndroidBridge.cpp
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
endif()
