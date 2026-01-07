#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <regex>

class PatternChain {
public:
    // Parse chain with optional repeat syntax: [p1, p2]x3, p3, [p4]x2
    void parse(const std::string& input) {
        patternNames.clear();
        std::string remaining = input;
        
        // Regex for [patterns]xN groups
        std::regex groupRegex(R"(\[([^\]]+)\]x(\d+))");
        std::smatch match;
        
        size_t pos = 0;
        while (pos < remaining.size()) {
            // Check for group at current position
            std::string sub = remaining.substr(pos);
            if (std::regex_search(sub, match, groupRegex) && match.position() == 0) {
                // Found a group: extract patterns and repeat count
                std::string groupPatterns = match[1].str();
                int repeatCount = std::stoi(match[2].str());
                
                // Parse patterns within the group
                std::vector<std::string> groupItems;
                parseSimple(groupPatterns, groupItems);
                
                // Add repeated patterns
                for (int r = 0; r < repeatCount; ++r) {
                    for (const auto& p : groupItems) {
                        patternNames.push_back(p);
                    }
                }
                
                pos += match.length();
                // Skip comma/whitespace after group
                while (pos < remaining.size() && 
                       (remaining[pos] == ',' || remaining[pos] == ' ' || remaining[pos] == '\t')) {
                    pos++;
                }
            } else {
                // Find next group or end
                size_t nextGroup = remaining.find('[', pos);
                std::string segment;
                if (nextGroup != std::string::npos && nextGroup > pos) {
                    segment = remaining.substr(pos, nextGroup - pos);
                    pos = nextGroup;
                } else if (nextGroup == std::string::npos) {
                    segment = remaining.substr(pos);
                    pos = remaining.size();
                } else {
                    pos++;
                    continue;
                }
                
                // Parse simple comma-separated patterns
                std::vector<std::string> items;
                parseSimple(segment, items);
                for (const auto& p : items) {
                    patternNames.push_back(p);
                }
            }
        }
    }
    
    std::string toString() const {
        std::string result;
        for (size_t i = 0; i < patternNames.size(); ++i) {
            if (i > 0) result += ", ";
            result += patternNames[i];
        }
        return result;
    }
    
    const std::vector<std::string>& getPatterns() const {
        return patternNames;
    }
    
    void clear() {
        patternNames.clear();
    }
    
    bool isEmpty() const {
        return patternNames.empty();
    }
    
    size_t size() const {
        return patternNames.size();
    }

private:
    std::vector<std::string> patternNames;
    
    void parseSimple(const std::string& input, std::vector<std::string>& out) {
        std::stringstream ss(input);
        std::string token;
        while (std::getline(ss, token, ',')) {
            size_t start = token.find_first_not_of(" \t");
            size_t end = token.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos) {
                out.push_back(token.substr(start, end - start + 1));
            }
        }
    }
};
