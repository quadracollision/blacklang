#include "StepGrid.h"
#include "../Pattern.h"
#include "../GuiState.h"
#include <algorithm>

namespace gui {

void DrawPatternBox(const std::string& name, Rectangle bounds, bool selected, int progress) {
    Color bgColor = selected ? Color{58, 123, 213, 255} : Color{60, 60, 60, 255};
    DrawRectangleRec(bounds, bgColor);
    DrawRectangleLinesEx(bounds, 1.0f, selected ? WHITE : GRAY);
    
    DrawText(name.c_str(), bounds.x + 5, bounds.y + 5, 10, WHITE);
}

void DrawStepGrid(Rectangle bounds, const Pattern& pattern, int activeStep, GuiState& state) {
    int steps = pattern.steps > 0 ? pattern.steps : 16;
    int cols = std::min(16, steps);
    int rows = (steps + cols - 1) / cols;
    
    float cellW = bounds.width / cols;
    float cellH = bounds.height / rows;
    float size = std::min(cellW, cellH) * 0.8f;
    
    for (int i = 0; i < steps; ++i) {
        int col = i % cols;
        int row = i / cols;
        float x = bounds.x + col * cellW + (cellW - size)/2;
        float y = bounds.y + row * cellH + (cellH - size)/2;
        
        bool active = pattern.shouldTriggerAt(i + 1);
        Color c = active ? RED : DARKGRAY;
        
        // Highlight active step
        if (i == activeStep && activeStep >= 0) {
            c = active ? ORANGE : YELLOW;
        }
        
        // Check if step has pitch data for melodic mode
        if (pattern.stepPitches.count(i + 1)) {
            int pitch = pattern.stepPitches.at(i + 1);
            int noteIdx = pitch % 12;
            int octave = pitch / 12;
            
            static const char* nNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
            char pText[8];
            snprintf(pText, 8, "%s%d", nNames[noteIdx], octave);
            
            // Draw cell with pitch
            DrawRectangle(x, y, size, size, c);
            DrawRectangleLinesEx({x, y, size, size}, 1, WHITE);
            DrawText(pText, x + 2, y + size/2 - 5, 8, WHITE);
        } else {
            // Check velocity (for display only)
            float vel = 1.0f;
            if (pattern.stepVelocities.count(i + 1)) {
                vel = pattern.stepVelocities.at(i + 1);
            }
            
            // Normal step drawing
            DrawRectangle(x, y, size, size, c);
            if (vel < 1.0f && active) {
                // Draw velocity indicator as partial fill
                float fillH = size * vel;
                DrawRectangle(x, y + (size - fillH), size, fillH, ColorBrightness(c, 0.3f));
            }
            DrawRectangleLinesEx({x, y, size, size}, 1, active ? WHITE : GRAY);
            
            // Show nudge indicator if applicable
            const float DEFAULT_OFFSET = 0.5f;
            float offset = DEFAULT_OFFSET;
            if (pattern.stepFXParams.count(i + 1)) {
                auto& params = pattern.stepFXParams.at(i + 1);
                if (params.count(Pattern::PAR_NUDGE_OFFSET)) {
                    offset = params.at(Pattern::PAR_NUDGE_OFFSET);
                    if (offset == 0.0f) offset = DEFAULT_OFFSET;
                }
            }
            
            if (std::abs(offset - DEFAULT_OFFSET) > 0.05f) {
                float nudgeX = x + (size / 2) + (offset - 0.5f) * (size / 2);
                DrawLine(nudgeX, y, nudgeX, y + size, ORANGE);
            }
        }
    }
}

} // namespace gui
