#include "DragDrop.h"
#include "../GuiState.h"
#include <raymath.h>
#include <algorithm>

namespace gui {

void HandleDragAndDrop(GuiState& state) {
    // 1. Handle Hold Logic
    if (state.drag.isHolding) {
        // Cancel if mouse released
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            state.drag.isHolding = false;
            return;
        }
        
        // Cancel if moved too much
        Vector2 mouse = state.getMousePosition();
        float dist = Vector2Distance(mouse, state.drag.initialClickPos);
        if (dist > 10) {
             state.drag.isHolding = false;
             return;
        }
        
        // Check time
        if (GetTime() - state.drag.holdStartTime > 1.0) {
            state.drag.isHolding = false;
            state.drag.isDragging = true;
            state.drag.currentPos = mouse;
            state.drag.startPos = mouse;
        }
    }

    if (!state.drag.isDragging) return;
    
    state.drag.currentPos = state.getMousePosition();
    
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        // Find target column
        int targetCol = -1;
        for (size_t i = 0; i < state.columns.size(); ++i) {
            if (CheckCollisionPointRec(state.getMousePosition(), state.columns[i].bounds)) {
                targetCol = i;
                break;
            }
        }
        
        if (targetCol != -1) {
            // Move pattern
            auto& sourceVec = state.columns[state.drag.sourceColumnIndex].patternNames;
            auto it = std::find(sourceVec.begin(), sourceVec.end(), state.drag.patternName);
            if (it != sourceVec.end()) {
                sourceVec.erase(it);
                state.columns[targetCol].patternNames.push_back(state.drag.patternName);
            }
        }
        
        state.drag.isDragging = false;
    }
}

void DrawDragGhost(GuiState& state) {
    if (!state.drag.isDragging) return;
    
    // Draw ghost at cursor position
    Rectangle ghostRect = {
        state.drag.currentPos.x - 50,
        state.drag.currentPos.y - 20,
        100, 40
    };
    DrawRectangleRec(ghostRect, Color{100, 100, 255, 180});
    DrawRectangleLinesEx(ghostRect, 2, WHITE);
    DrawText(state.drag.patternName.c_str(), ghostRect.x + 5, ghostRect.y + 12, 14, WHITE);
}

} // namespace gui
