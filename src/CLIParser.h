#pragma once

#include "Pattern.h"
#include "PatternChain.h"
#include <string>
#include <optional>
#include <variant>

enum class CommandType {
    None,
    PatternDef,
    PatternEdit,
    Chain,
    ChainEdit,
    Play,
    Stop,
    Save,
    Load,
    List,
    Help,
    Quit
};

struct Command {
    CommandType type = CommandType::None;
    std::string argument;
    Pattern pattern;
    PatternChain chain;
};

class CLIParser {
public:
    Command parse(const std::string& input);
    
private:
    // Pattern definition parsing
    bool isPatternDef(const std::string& input);
    std::optional<Pattern> parsePatternDef(const std::string& input);
    
    // Step notation parsing (supports all 3 formats)
    std::vector<int> parseSteps(const std::string& input, int totalSteps);
    std::vector<int> parseExplicitSteps(const std::string& input);
    std::vector<int> parseVisualGrid(const std::string& input);
    std::vector<int> parseShorthand(const std::string& input, int totalSteps);
    
    // Chain parsing
    bool isChainInput(const std::string& input);
    PatternChain parseChain(const std::string& input);
    
    // Utility
    std::string trim(const std::string& s);
    std::string extractBracket(const std::string& input, size_t& pos);
};
