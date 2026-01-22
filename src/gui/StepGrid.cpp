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
    
    DrawTextApp(name.c_str(), bounds.x + 5, bounds.y + 2, 20, WHITE); // Size 20!
}

void DrawStepGrid(Rectangle bounds, const Pattern& pattern, int activeStep, GuiState& state) {
    int steps = pattern.steps > 0 ? pattern.steps : 16;
    
    // Grid Layout: Even steps use even divisors, odd steps use odd divisors
    bool isEven = (steps % 2 == 0);
    
    int bestRows = 1;
    float bestDiff = 99999.0f;
    float targetRatio = 1.0f; // Aim for square cells
    
    // Find best row count that respects even/odd preference
    for (int r = 1; r <= steps; r++) {
        // Skip if parity doesn't match (even steps prefer even rows, odd prefer odd)
        if (isEven && r % 2 != 0 && r != 1) continue;
        if (!isEven && r % 2 == 0) continue;
        
        // Check if this row count divides evenly or nearly
        int c = (steps + r - 1) / r; // ceil(steps / r)
        
        // Calculate theoretical cell dimensions
        float cellW = bounds.width / c;
        float cellH = bounds.height / r;
        
        float ratio = cellW / cellH;
        float diff = std::abs(ratio - targetRatio);
        
        if (diff < bestDiff) {
            bestDiff = diff;
            bestRows = r;
        }
    }
    
    int rows = bestRows;
    int cols = (steps + rows - 1) / rows;
    
    // Maximize space with tiny gaps
    float gap = state.isPortrait ? 1.0f : 2.0f;
    
    float totalGapW = gap * (cols + 1);
    float totalGapH = gap * (rows + 1);
    
    float stepW = (bounds.width - totalGapW) / cols;
    float stepH = (bounds.height - totalGapH) / rows;
    
    for (int i = 0; i < steps; ++i) {
        int col = i % cols;
        int row = i / cols;
        
        Rectangle cellRect = {
            bounds.x + gap + col * (stepW + gap),
            bounds.y + gap + row * (stepH + gap),
            stepW,
            stepH
        };
        
        bool active = pattern.shouldTriggerAt(i + 1);
        Color c = active ? RED : DARKGRAY;
        
        // Highlight active step (activeStep is 1-indexed)
        if (i == (activeStep - 1) && activeStep > 0) {
            c = active ? ORANGE : YELLOW;
        }
        
        // Draw Step Background
        DrawRectangleRec(cellRect, c);
        
        // Check velocity (Partial Fill)
        if (pattern.stepVelocities.count(i + 1)) {
            float vel = pattern.stepVelocities.at(i + 1);
            if (vel < 1.0f && active) {
                 float fillH = cellRect.height * vel;
                 Rectangle fillRect = {cellRect.x, cellRect.y + (cellRect.height - fillH), cellRect.width, fillH};
                 DrawRectangleRec(fillRect, ColorBrightness(c, 0.3f));
            }
        }
        
        DrawRectangleLinesEx(cellRect, 1, active ? WHITE : GRAY);
        
        // Nudge Indicator
        if (pattern.stepFXParams.count(i + 1)) {
             auto& params = pattern.stepFXParams.at(i + 1);
             if (params.count(Pattern::PAR_NUDGE_OFFSET)) {
                 float offset = params.at(Pattern::PAR_NUDGE_OFFSET);
                 // Default offset 0.5 (Center). If significantly different, draw line.
                 if (offset != 0.0f && std::abs(offset - 0.5f) > 0.05f) {
                     float nudgeX = cellRect.x + (cellRect.width * offset);
                     DrawLine(nudgeX, cellRect.y, nudgeX, cellRect.y + cellRect.height, ORANGE);
                 }
             }
        }
        
        // Pitch Text
        if (pattern.stepPitches.count(i + 1)) {
             int pitch = pattern.stepPitches.at(i + 1);
             if (pitch < 0) pitch = 0;
             if (pitch > 127) pitch = 127;
             
             int noteIdx = pitch % 12;
             int octave = pitch / 12;
             
             static const char* nNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
             char pText[8];
             snprintf(pText, sizeof(pText), "%s%d", nNames[noteIdx], octave);
             
             // Scale font size based on cell height
             int fontSize = (int)(cellRect.height * 0.4f);
             if (fontSize < 10) fontSize = 10;
             if (fontSize > 20) fontSize = 20;
             
             int textW = MeasureTextApp(pText, fontSize);
             DrawTextApp(pText, cellRect.x + (cellRect.width - textW)/2, cellRect.y + (cellRect.height - fontSize)/2, fontSize, WHITE);
        }
    }
    
    // Draw beat sync dividers if syncBase is set
    int syncBase = pattern.syncBase;
    if (syncBase > 0 && syncBase < steps) {
        for (int i = syncBase; i < steps; i += syncBase) {
            int col = i % cols;
            int row = i / cols;
            
            // Draw at left edge of this step
            float cellX = bounds.x + gap + col * (stepW + gap);
            float cellY = bounds.y + gap + row * (stepH + gap);
            float lineX = cellX - gap/2.0f;
            
            // Draw vertical divider
            DrawLineEx({lineX, cellY}, {lineX, cellY + stepH}, 2.0f, Color{0, 180, 180, 255});
        }
    }
}

} // namespace gui
