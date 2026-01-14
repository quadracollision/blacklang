#pragma once

#include "Pattern.h"
#include "PatternChain.h"
#include <string>
#include <map>
#include <vector>

struct SerializedFX {
    int type;
    bool enabled;
    std::vector<float> params; // Ordered by param index
};

// Simple struct to represent column layout without GUI dependency
struct SerializedColumn {
    std::string title;
    std::string trackName;
    float volume = 1.0f;
    float pan = 0.5f;
    std::vector<std::string> patternNames;
    std::vector<bool> slotSyncEnabled;  // Per-slot sync flags
    std::vector<SerializedFX> fxChain;
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
