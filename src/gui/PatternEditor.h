#pragma once

#include <raylib.h>
#include <string>
#include <vector>
#include "FXControls.h"

// Forward declarations
class AudioEngine;
struct GuiState;
struct Pattern;

namespace gui {

class PatternEditor {
public:
    PatternEditor(GuiState& state, AudioEngine& engine);
    
    // Draw the pattern editor overlay
    void Draw();
    
    // Check if editor is open
    bool IsOpen() const;
    
private:
    GuiState& state;
    AudioEngine& engine;
    FXControls fxControls;  // FX controls component
    
    void DrawHeader(Rectangle winRect);
    void DrawNameSampleFields(Rectangle& area);
    void DrawBpmStepsFields(Rectangle& area);
    void DrawStepButtons(Rectangle& area);
    void DrawMelodicControls(Rectangle& area);
    void DrawSlicerControls(Rectangle& area);
    void DrawModeButtons(Rectangle& area);
    void DrawSaveButton(Rectangle winRect);

    
    void SavePattern();
    void LoadPatternIntoEditor(Pattern* p);
};

} // namespace gui

