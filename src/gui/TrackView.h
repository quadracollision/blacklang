#pragma once

#include <raylib.h>
#include <string>
#include <vector>

// Forward declarations
class AudioEngine;
struct GuiState;
struct PatternColumn;

namespace gui {

class TrackView {
public:
    TrackView(GuiState& state, AudioEngine& engine);
    
    // Draw all columns and their patterns
    void Draw();
    
    // Draw a single column
    void DrawColumn(int index, PatternColumn& col);
    
private:
    GuiState& state;
    AudioEngine& engine;
    
    void HandlePatternClick(int colIndex, int slotIndex, const std::string& patternName);
    void HandleAddPattern(int colIndex, PatternColumn& col);
};

} // namespace gui
