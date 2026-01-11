#pragma once

#include "Pattern.h"
#include "PatternChain.h"
#include <string>
#include <map>
#include <vector>

// Simple struct to represent column layout without GUI dependency
struct SerializedColumn {
    std::string title;
    std::string trackName;
    std::vector<std::string> patternNames;
    std::vector<bool> slotSyncEnabled;  // Per-slot sync flags
};

class ProjectFile {
public:
    // Save project to JSON file
    static bool save(const std::string& filename,
                     const std::map<std::string, Pattern>& patterns,
                     const PatternChain& currentChain,
                     const std::vector<SerializedColumn>& columns);
    
    // Load project from JSON file
    static bool load(const std::string& filename,
                     std::map<std::string, Pattern>& patterns,
                     PatternChain& currentChain,
                     std::vector<SerializedColumn>& columns);
};
