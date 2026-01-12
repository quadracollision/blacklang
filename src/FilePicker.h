#pragma once

#include <string>

// Cross-platform file dialog wrapper
// On desktop: uses tinyfiledialogs
// On Android: uses Android SAF (Storage Access Framework) intent

namespace FilePicker {

    // Audio import
    std::string openAudioFile();
    
    // Project management
    std::string openProjectFile();
    std::string saveProjectFile();
    
    // Recording / Export
    std::string getWritablePath(); // Returns a safe directory for temporary recordings
    void exportFile(const std::string& sourcePath, const std::string& targetName); // Launches share/save intent dialog for project files

// Permission helpers
void requestPermissions();
bool hasPermissions();

// Keyboard helpers (Android)
void showKeyboard();
void hideKeyboard();

// Input queue (Android)
// Returns true if an event was popped.
bool AndroidGetInput(int& key, int& charCode);

// Android-specific: Set the result from Java callback
#if defined(__ANDROID__)
void setPickedFilePath(const char* path);
bool isFilePickerPending();
std::string getPickedFilePath();
void clearPickedFilePath();
#endif

} // namespace FilePicker
