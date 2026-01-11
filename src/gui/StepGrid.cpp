#include "StepGrid.h"
#include "../Pattern.h"
#include "../GuiState.h"
#include <algorithm>

namespace gui {

void DrawPatternBox(const std::string& name, Rectangle bounds, bool selected, int progress) {
    Color bgColor = selected ? Color{58, 123, 213, 255} : Color{60, 60, 60, 255};
    DrawRectangleRec(bounds, bgColor);
    DrawRectangleLinesEx(bounds, 1.0f, selected ? WHITE : GRAY);
    
    // Title Bar for Pattern
    Rectangle titleBar = {bounds.x, bounds.y, bounds.width, 22};
    DrawRectangleRec(titleBar, Color{20, 20, 20, 255}); // Solid Black Header
    // DrawRectangleLinesEx(titleBar, 1, Color{200, 200, 200, 255}); // No Border requested
    
    DrawText(name.c_str(), bounds.x + 5, bounds.y + 2, 20, WHITE); // Size 20!
}

void DrawStepGrid(Rectangle bounds, const Pattern& pattern, int activeStep, GuiState& state) {
    int steps = pattern.steps > 0 ? pattern.steps : 16;
    int cols = std::min(16, steps);
    int rows = (steps + cols - 1) / cols;
    
    // Calculate uniform step size based on available space
    float gapRatio = 0.1f; // 10% gap between steps
    float totalGapW = bounds.width * gapRatio;
    float totalGapH = bounds.height * gapRatio;
    float gapW = totalGapW / (cols + 1); // gaps on both sides
    float gapH = rows > 1 ? totalGapH / (rows + 1) : 0;
    
    float stepW = (bounds.width - totalGapW) / cols;
    float stepH = (bounds.height - totalGapH) / rows;
    float size = std::min(stepW, stepH); // Keep steps square
    
    // Recalculate gaps to center content if size was constrained
    float actualWidth = cols * size;
    float actualGapW = (bounds.width - actualWidth) / (cols + 1);
    
    float actualHeight = rows * size;
    float actualGapH = (bounds.height - actualHeight) / (rows + 1);
    
    for (int i = 0; i < steps; ++i) {
        int col = i % cols;
        int row = i / cols;
        float x = bounds.x + actualGapW + col * (size + actualGapW);
        float y = bounds.y + actualGapH + row * (size + actualGapH);
        
        bool active = pattern.shouldTriggerAt(i + 1);
        Color c = active ? RED : DARKGRAY;
        
        // Highlight active step (activeStep is 1-indexed, i is 0-indexed)
        if (i == (activeStep - 1) && activeStep > 0) {
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
    
    // Draw beat sync dividers if syncBase is set
    int syncBase = pattern.syncBase;
    if (syncBase > 0 && syncBase < steps) {
        for (int i = syncBase; i < steps; i += syncBase) {
            int col = i % cols;
            int row = i / cols;
            
            // Draw at left edge of this step (in the gap before it)
            float lineX = bounds.x + actualGapW + col * (size + actualGapW) - actualGapW / 2;
            float lineY = bounds.y + actualGapH + row * (size + actualGapH);
            
            // Use teal color, matching step height
            DrawLineEx({lineX, lineY}, {lineX, lineY + size}, 2.0f, Color{0, 180, 180, 255});
        }
    }
}

} // namespace gui
