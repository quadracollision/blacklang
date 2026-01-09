#pragma once

#include <raylib.h>
#include "../fx/FXTypes.h"

// Forward declarations
class AudioEngine;
struct GuiState;
struct Pattern;

namespace gui {

class FXControls {
public:
    FXControls(GuiState& state, AudioEngine& engine);
    
    // Draw FX controls panel (when FX mode is active)
    // Returns height used by the controls
    float Draw(Rectangle area, Pattern& pattern, Rectangle parentScissor);
    
    // Handle FX button clicks on the FX panel
    void HandleInput(Rectangle area, Pattern& pattern);
    
private:
    GuiState& state;
    AudioEngine& engine;
    
    // Draw individual FX control
    void DrawFXButton(Rectangle rect, fx::FXType type, bool active);
    void DrawFXSlider(Rectangle rect, const char* label, float* value, float min, float max);
    
    // FX-specific parameter panels
    void DrawStutterParams(float x, float y, Pattern& pattern, int step);
    void DrawSlideParams(float x, float y, Pattern& pattern, int step);
    void DrawNudgeParams(float x, float y, Pattern& pattern, int step);
    
    // Apply FX to selected step
    void ApplyFXToStep(Pattern& pattern, int step, fx::FXType fxType, bool add);
};

} // namespace gui
