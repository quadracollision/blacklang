#pragma once

#include <string>

namespace crash {

// Initialize crash logger - call before any other initialization
// Sets up signal handlers to catch crashes
void initCrashLogger();

// Log a message to the crash log
// Use for tracking app progress before a crash
void logMessage(const std::string& message);

// Get the path to the crash log file (in app internal storage)
std::string getLogPath();

} // namespace crash
