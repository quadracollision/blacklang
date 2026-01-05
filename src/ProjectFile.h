#pragma once

#include "Pattern.h"
#include "PatternChain.h"
#include <string>
#include <map>

class ProjectFile {
public:
    // Save project to JSON file
    static bool save(const std::string& filename,
                     const std::map<std::string, Pattern>& patterns,
                     const PatternChain& currentChain);
    
    // Load project from JSON file
    static bool load(const std::string& filename,
                     std::map<std::string, Pattern>& patterns,
                     PatternChain& currentChain);
};
