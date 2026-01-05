#include "CLIParser.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>

Command CLIParser::parse(const std::string& input) {
    Command cmd;
    std::string trimmed = trim(input);
    
    if (trimmed.empty()) {
        return cmd;
    }
    
    // Check for commands first
    std::string lower = trimmed;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "help" || lower == "?") {
        cmd.type = CommandType::Help;
        return cmd;
    }
    if (lower == "quit" || lower == "exit" || lower == "q") {
        cmd.type = CommandType::Quit;
        return cmd;
    }
    if (lower == "stop") {
        cmd.type = CommandType::Stop;
        return cmd;
    }
    if (lower == "list" || lower == "ls") {
        cmd.type = CommandType::List;
        return cmd;
    }
    if (lower == "play") {
        cmd.type = CommandType::Play;
        return cmd;
    }
    if (lower.substr(0, 5) == "play ") {
        cmd.type = CommandType::Play;
        cmd.argument = trim(trimmed.substr(5));
        return cmd;
    }
    if (lower.substr(0, 5) == "save ") {
        cmd.type = CommandType::Save;
        cmd.argument = trim(trimmed.substr(5));
        return cmd;
    }
    if (lower.substr(0, 5) == "load ") {
        cmd.type = CommandType::Load;
        cmd.argument = trim(trimmed.substr(5));
        return cmd;
    }
    if (lower == "chain" || lower == "edit") {
        cmd.type = CommandType::ChainEdit;
        return cmd;
    }
    if (lower.substr(0, 5) == "edit ") {
        cmd.type = CommandType::PatternEdit;
        cmd.argument = trim(trimmed.substr(5));
        return cmd;
    }
    
    // Check for pattern definition: name:[sample][bpm][steps][positions]
    if (isPatternDef(trimmed)) {
        auto pattern = parsePatternDef(trimmed);
        if (pattern) {
            cmd.type = CommandType::PatternDef;
            cmd.pattern = *pattern;
            return cmd;
        }
    }
    
    // Check for chain input (comma-separated pattern names)
    if (isChainInput(trimmed)) {
        cmd.type = CommandType::Chain;
        cmd.chain = parseChain(trimmed);
        return cmd;
    }
    
    return cmd;
}

bool CLIParser::isPatternDef(const std::string& input) {
    // Pattern def has format: name:[sample][bpm][steps][positions]
    return input.find(':') != std::string::npos && input.find('[') != std::string::npos;
}

std::optional<Pattern> CLIParser::parsePatternDef(const std::string& input) {
    Pattern pattern;
    
    // Find the name (before the colon)
    size_t colonPos = input.find(':');
    if (colonPos == std::string::npos) return std::nullopt;
    
    pattern.name = trim(input.substr(0, colonPos));
    if (pattern.name.empty()) return std::nullopt;
    
    // Parse bracket sections
    std::string rest = input.substr(colonPos + 1);
    size_t pos = 0;
    
    // [sample.wav]
    std::string sample = extractBracket(rest, pos);
    if (sample.empty()) return std::nullopt;
    pattern.samplePath = sample;
    
    // [bpm]
    std::string bpmStr = extractBracket(rest, pos);
    if (bpmStr.empty()) return std::nullopt;
    try {
        pattern.bpm = std::stoi(bpmStr);
    } catch (...) {
        return std::nullopt;
    }
    
    // [steps]
    std::string stepsStr = extractBracket(rest, pos);
    if (stepsStr.empty()) return std::nullopt;
    try {
        pattern.steps = std::stoi(stepsStr);
    } catch (...) {
        return std::nullopt;
    }
    
    // [step positions]
    std::string positions = extractBracket(rest, pos);
    if (positions.empty()) return std::nullopt;
    
    pattern.activeSteps = parseSteps(positions, pattern.steps);
    if (pattern.activeSteps.empty()) return std::nullopt;
    
    return pattern;
}

std::string CLIParser::extractBracket(const std::string& input, size_t& pos) {
    size_t start = input.find('[', pos);
    if (start == std::string::npos) return "";
    
    size_t end = input.find(']', start);
    if (end == std::string::npos) return "";
    
    pos = end + 1;
    return input.substr(start + 1, end - start - 1);
}

std::vector<int> CLIParser::parseSteps(const std::string& input, int totalSteps) {
    std::string trimmed = trim(input);
    
    if (trimmed.empty()) return {};
    
    // Detect format
    // Visual grid: contains x and .
    if (trimmed.find('x') != std::string::npos || 
        (trimmed.find('.') != std::string::npos && trimmed.find(' ') == std::string::npos)) {
        return parseVisualGrid(trimmed);
    }
    
    // Shorthand: ends with 'n' (like "4n")
    if (trimmed.back() == 'n' && trimmed.length() > 1) {
        return parseShorthand(trimmed, totalSteps);
    }
    
    // Explicit numbers
    return parseExplicitSteps(trimmed);
}

std::vector<int> CLIParser::parseExplicitSteps(const std::string& input) {
    std::vector<int> steps;
    std::stringstream ss(input);
    int step;
    
    while (ss >> step) {
        steps.push_back(step);
    }
    
    return steps;
}

std::vector<int> CLIParser::parseVisualGrid(const std::string& input) {
    std::vector<int> steps;
    
    for (size_t i = 0; i < input.size(); ++i) {
        char c = std::tolower(input[i]);
        if (c == 'x') {
            steps.push_back(static_cast<int>(i + 1));  // 1-indexed
        }
        // '.' means rest, skip
    }
    
    return steps;
}

std::vector<int> CLIParser::parseShorthand(const std::string& input, int totalSteps) {
    std::vector<int> steps;
    
    // Parse "4n" -> every 4 steps
    std::string numStr = input.substr(0, input.length() - 1);
    try {
        int interval = std::stoi(numStr);
        if (interval <= 0) return {};
        
        for (int i = 1; i <= totalSteps; i += interval) {
            steps.push_back(i);
        }
    } catch (...) {
        return {};
    }
    
    return steps;
}

bool CLIParser::isChainInput(const std::string& input) {
    // Chain input is comma-separated pattern names
    // Must have at least one comma or be a single word without brackets/colons
    if (input.find(',') != std::string::npos) return true;
    if (input.find('[') == std::string::npos && 
        input.find(':') == std::string::npos &&
        input.find(' ') == std::string::npos) {
        // Single pattern name for chain
        return true;
    }
    return false;
}

PatternChain CLIParser::parseChain(const std::string& input) {
    PatternChain chain;
    chain.parse(input);
    return chain;
}

std::string CLIParser::trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    return s.substr(start, end - start + 1);
}
