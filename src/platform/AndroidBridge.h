#pragma once

#include <string>

namespace platform {

// Show a native Android Toast message
void ShowToast(const std::string& message);

// Launch the Android System File Picker (SAF) to let the user choose where to save
void LaunchFileSaver(const std::string& sourcePath, const std::string& filename);

} // namespace platform
