#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "AudioEngine.h"
#include "CLIParser.h"
#include "ProjectFile.h"
#include "Pattern.h"
#include "PatternChain.h"

#include <iostream>
#include <string>

#ifdef HAS_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

// Read a line with optional readline support
std::string readLine(const char* prompt, bool& eof) {
#ifdef HAS_READLINE
    char* line = readline(prompt);
    if (line == nullptr) {
        eof = true;
        return "";
    }
    std::string result(line);
    if (!result.empty()) {
        add_history(line);
    }
    free(line);
    eof = false;
    return result;
#else
    std::cout << prompt;
    std::cout.flush();
    std::string result;
    if (!std::getline(std::cin, result)) {
        eof = true;
        return "";
    }
    eof = false;
    return result;
#endif
}

void printHelp() {
    std::cout << "\n=== BlackLang ===\n\n";
    std::cout << "PATTERN DEFINITION:\n";
    std::cout << "  name:[sample.wav][bpm][steps][positions]\n";
    std::cout << "  Example: kick:[kick.wav][120][16][1 5 9 13]\n\n";
    std::cout << "  Step notation formats:\n";
    std::cout << "    [1 5 9 13]           - Explicit step numbers (1-indexed)\n";
    std::cout << "    [x...x...x...x...]   - Visual grid (x=hit, .=rest)\n";
    std::cout << "    [4n]                  - Every N steps\n\n";
    std::cout << "PATTERN CHAIN:\n";
    std::cout << "  Type comma-separated pattern names:\n";
    std::cout << "  kick, snare, kick, hats\n";
    std::cout << "  [kick, snare]x3         - Repeat group 3 times\n\n";
    std::cout << "COMMANDS:\n";
    std::cout << "  play [name]   - Play pattern or current chain\n";
    std::cout << "  stop          - Stop playback\n";
    std::cout << "  list          - Show all patterns\n";
    std::cout << "  edit          - Edit current chain\n";
    std::cout << "  save <file>   - Save project\n";
    std::cout << "  load <file>   - Load project\n";
    std::cout << "  help          - Show this help\n";
    std::cout << "  quit          - Exit\n\n";
}

int main(int /*argc*/, char** /*argv*/) {
    // Initialize JUCE
    juce::ScopedJuceInitialiser_GUI juceInit;
    
    std::cout << "BlackLang v1.0\n";
    std::cout << "Type 'help' for commands.\n\n";
    
    AudioEngine engine;
    if (!engine.initialize()) {
        std::cerr << "Failed to initialize audio engine\n";
        return 1;
    }
    
    CLIParser parser;
    PatternChain currentChain;
    bool running = true;
    
    while (running) {
        std::string prompt = engine.isPlaying() ? "[playing] > " : "> ";
        bool eof = false;
        std::string input = readLine(prompt.c_str(), eof);
        
        if (eof) break;
        
        Command cmd = parser.parse(input);
        
        switch (cmd.type) {
            case CommandType::Help:
                printHelp();
                break;
                
            case CommandType::Quit:
                running = false;
                std::cout << "Goodbye!\n";
                break;
                
            case CommandType::Stop:
                engine.stop();
                std::cout << "Stopped.\n";
                break;
                
            case CommandType::List: {
                const auto& patterns = engine.getPatterns();
                if (patterns.empty()) {
                    std::cout << "No patterns defined.\n";
                } else {
                    std::cout << "Patterns:\n";
                    for (const auto& [name, pattern] : patterns) {
                        std::cout << "  " << name << " - " << pattern.samplePath 
                                  << " [" << pattern.bpm << " BPM, " 
                                  << pattern.steps << " steps]\n";
                    }
                }
                if (!currentChain.isEmpty()) {
                    std::cout << "Current chain: " << currentChain.toString() << "\n";
                }
                break;
            }
                
            case CommandType::PatternDef: {
                Pattern& pattern = cmd.pattern;
                std::cout << "Loading: " << pattern.name << "\n";
                
                if (engine.loadSample(pattern)) {
                    engine.addPattern(pattern);
                    std::cout << "Pattern '" << pattern.name << "' created.\n";
                    std::cout << "  Steps: ";
                    for (size_t i = 0; i < pattern.activeSteps.size(); ++i) {
                        if (i > 0) std::cout << ", ";
                        std::cout << pattern.activeSteps[i];
                    }
                    std::cout << "\n";
                } else {
                    std::cout << "Failed to load sample.\n";
                }
                break;
            }
                
            case CommandType::Chain:
                currentChain = cmd.chain;
                std::cout << "Chain set: " << currentChain.toString() << "\n";
                break;
                
            case CommandType::PatternEdit: {
                Pattern* pattern = engine.getPattern(cmd.argument);
                if (!pattern) {
                    std::cout << "Pattern not found: " << cmd.argument << "\n";
                } else {
                    // Build the pattern definition string
                    std::string stepStr;
                    for (size_t i = 0; i < pattern->activeSteps.size(); ++i) {
                        if (i > 0) stepStr += " ";
                        stepStr += std::to_string(pattern->activeSteps[i]);
                    }
                    std::string patternDef = pattern->name + ":[" + pattern->samplePath + "][" +
                        std::to_string(pattern->bpm) + "][" + std::to_string(pattern->steps) + 
                        "][" + stepStr + "]";
                    
                    std::cout << "Editing pattern (modify and press Enter):\n";
                    
                    // Pre-fill readline with current definition
#ifdef HAS_READLINE
                    rl_replace_line(patternDef.c_str(), 0);
                    rl_redisplay();
                    bool editEof = false;
                    std::string editInput = readLine("> ", editEof);
#else
                    std::cout << patternDef << "\n> ";
                    std::cout.flush();
                    std::string editInput;
                    bool editEof = !std::getline(std::cin, editInput);
#endif
                    if (!editEof && !editInput.empty()) {
                        Command editCmd = parser.parse(editInput);
                        if (editCmd.type == CommandType::PatternDef) {
                            if (engine.loadSample(editCmd.pattern)) {
                                engine.addPattern(editCmd.pattern);
                                std::cout << "Pattern updated.\n";
                            }
                        }
                    }
                }
                break;
            }
                
            case CommandType::ChainEdit:
                if (currentChain.isEmpty()) {
                    std::cout << "No chain to edit. Enter pattern names:\n";
                } else {
                    std::cout << "Edit chain (current: " << currentChain.toString() << "):\n";
                }
                {
                    bool editEof = false;
                    std::string editInput = readLine("> ", editEof);
                    if (!editEof) {
                        Command editCmd = parser.parse(editInput);
                        if (editCmd.type == CommandType::Chain) {
                            currentChain = editCmd.chain;
                            std::cout << "Chain updated: " << currentChain.toString() << "\n";
                        }
                    }
                }
                break;
                
            case CommandType::Play:
                if (!cmd.argument.empty()) {
                    // Play specific pattern
                    engine.playPattern(cmd.argument);
                    std::cout << "Playing pattern: " << cmd.argument << "\n";
                } else if (!currentChain.isEmpty()) {
                    // Play current chain
                    engine.playChain(currentChain);
                    std::cout << "Playing chain: " << currentChain.toString() << "\n";
                } else {
                    std::cout << "No pattern or chain to play.\n";
                }
                break;
                
            case CommandType::Save:
                if (cmd.argument.empty()) {
                    std::cout << "Usage: save <filename>\n";
                } else {
                    std::map<std::string, Pattern> patterns;
                    for (const auto& [name, pattern] : engine.getPatterns()) {
                        patterns[name] = pattern;
                    }
                    if (ProjectFile::save(cmd.argument, patterns, currentChain)) {
                        std::cout << "Saved to: " << cmd.argument << "\n";
                    }
                }
                break;
                
            case CommandType::Load: {
                if (cmd.argument.empty()) {
                    std::cout << "Usage: load <filename>\n";
                } else {
                    std::map<std::string, Pattern> patterns;
                    if (ProjectFile::load(cmd.argument, patterns, currentChain)) {
                        for (auto& [name, pattern] : patterns) {
                            if (engine.loadSample(pattern)) {
                                engine.addPattern(pattern);
                            }
                        }
                        std::cout << "Loaded from: " << cmd.argument << "\n";
                        std::cout << "Patterns: " << patterns.size() << "\n";
                        if (!currentChain.isEmpty()) {
                            std::cout << "Chain: " << currentChain.toString() << "\n";
                        }
                    }
                }
                break;
            }
                
            case CommandType::None:
            default:
                if (!input.empty()) {
                    std::cout << "Unknown command. Type 'help' for usage.\n";
                }
                break;
        }
    }
    
    engine.shutdown();
    return 0;
}
