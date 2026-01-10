#pragma once

#include <string>

// Cross-platform file dialog wrapper
// On desktop: uses tinyfiledialogs
// On Android: uses Android SAF (Storage Access Framework) intent

namespace FilePicker {

// Opens a file picker dialog for audio files
// Returns the file path, or empty string if cancelled
std::string openAudioFile();

// Opens a file picker dialog for project files  
// Returns the file path, or empty string if cancelled
std::string openProjectFile();

// Saves a file dialog for project files
// Returns the file path, or empty string if cancelled  
std::string saveProjectFile();

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
