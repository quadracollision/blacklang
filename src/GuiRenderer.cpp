#include "GuiRenderer.h"
#include <iostream>
#include <algorithm>
#include <cstring>

GuiRenderer::GuiRenderer(GuiState& s, AudioEngine& e) : state(s), engine(e) {
    font = GetFontDefault();
}

void GuiRenderer::Update() {
    HandleDragAndDrop();
    
    // Sync playback state
    state.isPlaying = engine.isPlaying();
}

void GuiRenderer::Draw() {
    ClearBackground(Color{20, 20, 20, 255});
    
    // Draw Headers
    Rectangle headerRect = {0, 0, (float)GetScreenWidth(), (float)state.HEADER_HEIGHT};
    DrawRectangleRec(headerRect, Color{30, 30, 30, 255});
    DrawText("BlackLang", 20, 15, 30, WHITE);
    
    // Draw Columns
    float startY = state.HEADER_HEIGHT + 20;
    float startX = 20;
    
    for (size_t i = 0; i < state.columns.size(); ++i) {
        Rectangle colRect = {
            startX + i * (state.COLUMN_WIDTH + 10),
            startY,
            (float)state.COLUMN_WIDTH,
            (float)GetScreenHeight() - state.HEADER_HEIGHT - state.FOOTER_HEIGHT - 40
        };
        state.columns[i].bounds = colRect;
        DrawColumn(i, state.columns[i]);
    }
    
    // Add Column Button
    float addColX = startX + state.columns.size() * (state.COLUMN_WIDTH + 10);
    Rectangle addColRect = {addColX, startY, 40, (float)state.PATTERN_HEIGHT};
    DrawRectangleRounded(addColRect, 0.2f, 4, Color{40, 40, 40, 255});
    DrawText("+", addColX + 13, startY + 30, 30, GRAY);
    
    if (CheckCollisionPointRec(GetMousePosition(), addColRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.columns.push_back({"Track " + std::to_string(state.columns.size() + 1), {}, {0,0,0,0}});
    }
    
    DrawTransportBar();
    
    // Draw dragged item
    if (state.drag.isDragging) {
        Rectangle dragRect = {
            state.drag.currentPos.x - state.COLUMN_WIDTH/2,
            state.drag.currentPos.y - state.PATTERN_HEIGHT/2,
            (float)state.COLUMN_WIDTH,
            (float)state.PATTERN_HEIGHT
        };
        DrawPatternBox(state.drag.patternName, dragRect, true);
    }
    
    if (state.editor.isOpen) {
        DrawPatternEditor();
    }
}

void GuiRenderer::DrawColumn(int index, PatternColumn& col) {
    DrawRectangleRounded(col.bounds, 0.1f, 4, Color{35, 35, 35, 255});
    DrawText(col.title.c_str(), col.bounds.x + 10, col.bounds.y + 10, 20, LIGHTGRAY);
    
    float y = col.bounds.y + 40;
    
    for (size_t i = 0; i < col.patternNames.size(); ++i) {
        Rectangle patRect = {
            col.bounds.x + 5,
            y,
            col.bounds.width - 10,
            (float)state.PATTERN_HEIGHT
        };
        
        bool isSelected = std::find(state.selectedPatterns.begin(), state.selectedPatterns.end(), 
                                  col.patternNames[i]) != state.selectedPatterns.end();
                                  
        // Skip drawing if being dragged
        if (state.drag.isDragging && 
            state.drag.sourceColumnIndex == index && 
            state.drag.patternName == col.patternNames[i]) {
            // Draw placeholder
            DrawRectangleRoundedLines(patRect, 0.1f, 4, 2.0f, DARKGRAY);
        } else {
            DrawPatternBox(col.patternNames[i], patRect, isSelected);
            
        // Interaction
        if (CheckCollisionPointRec(GetMousePosition(), patRect)) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                // Start tracking hold
                state.drag.isHolding = true;
                state.drag.holdStartTime = GetTime();
                state.drag.initialClickPos = GetMousePosition();
                state.drag.sourceColumnIndex = index;
                state.drag.patternName = col.patternNames[i];
            }
            
            // Double click to edit (using simple time check)
            static double lastClickTime = 0;
            static std::string lastClickName = "";
            
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                double now = GetTime();
                if (now - lastClickTime < 0.3 && lastClickName == col.patternNames[i]) {
                    // Open Editor
                    Pattern* p = engine.getPattern(col.patternNames[i]);
                    if (p) {
                        state.editor.currentPattern = *p;
                        state.editor.isOpen = true;
                        strcpy(state.editor.nameBuffer, p->name.c_str());
                        strcpy(state.editor.originalName, p->name.c_str());
                        strcpy(state.editor.samplePathBuffer, p->samplePath.c_str());
                        sprintf(state.editor.bpmBuffer, "%d", p->bpm);
                        sprintf(state.editor.stepsBuffer, "%d", p->steps);
                        for(int s=0; s<64; ++s) state.editor.stepStates[s] = p->shouldTriggerAt(s+1);
                    }
                }
                lastClickTime = now;
                lastClickName = col.patternNames[i];
            }
        }
    }
    
    // Add Pattern Button
    Rectangle addBtnRect = {col.bounds.x + 5, y, col.bounds.width - 10, 30};
    DrawRectangleRounded(addBtnRect, 0.2f, 4, Color{50, 50, 50, 255});
    DrawText("+ Add", addBtnRect.x + 40, addBtnRect.y + 5, 10, WHITE);
    
    if (CheckCollisionPointRec(GetMousePosition(), addBtnRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Create new pattern
        Pattern p;
        p.name = "Pat" + std::to_string(rand() % 999);
        p.bpm = state.bpm;
        col.patternNames.push_back(p.name);
        
        // Register with audio engine
        engine.addPattern(p); // Empty pattern
        
        // Open Editor immediately
        state.editor.currentPattern = p;
        state.editor.isOpen = true;
        strcpy(state.editor.nameBuffer, p.name.c_str());
        // ... rest of init
    }
}

// ... helper for text input ...
void DrawTextInput(Rectangle rect, char* buffer, size_t maxLen, int fieldId, int& focusedId) {
    bool isFocused = (focusedId == fieldId);
    
    if (CheckCollisionPointRec(GetMousePosition(), rect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        focusedId = fieldId;
    }
    
    DrawRectangleRec(rect, isFocused ? WHITE : LIGHTGRAY);
    DrawRectangleLinesEx(rect, 1, isFocused ? BLUE : DARKGRAY);
    DrawText(buffer, rect.x + 5, rect.y + 5, 20, BLACK);
    
    if (isFocused) {
        int key = GetCharPressed();
        while (key > 0) {
            if ((key >= 32) && (key <= 125) && (strlen(buffer) < maxLen)) {
                size_t len = strlen(buffer);
                buffer[len] = (char)key;
                buffer[len+1] = '\0';
            }
            key = GetCharPressed();
        }
        
        if (IsKeyPressed(KEY_BACKSPACE)) {
            size_t len = strlen(buffer);
            if (len > 0) buffer[len-1] = '\0';
        }
    }
}

void GuiRenderer::HandleDragAndDrop() {
    // 1. Handle Hold Logic
    if (state.drag.isHolding) {
        // Cancel if mouse released
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            state.drag.isHolding = false;
            return;
        }
        
        // Cancel if moved too much
        Vector2 mouse = GetMousePosition();
        float dist = Vector2Distance(mouse, state.drag.initialClickPos);
        if (dist > 5) {
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
    
    state.drag.currentPos = GetMousePosition();
    
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        // ... (Drop logic same as before)
        // Find target column
        int targetCol = -1;
        for (size_t i = 0; i < state.columns.size(); ++i) {
            if (CheckCollisionPointRec(GetMousePosition(), state.columns[i].bounds)) {
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

void GuiRenderer::DrawPatternEditor() {
    // Overlay
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0,0,0, 200});
    
    // Window
    Rectangle winRect = {
        (float)GetScreenWidth()/2 - 250, 
        (float)GetScreenHeight()/2 - 200, 
        500, 400
    };
    DrawRectangleRec(winRect, Color{40, 40, 40, 255});
    DrawRectangleLinesEx(winRect, 2, LIGHTGRAY);
    
    DrawText("Edit Pattern", winRect.x + 10, winRect.y + 10, 20, WHITE);
    
    // Close button
    Rectangle closeRect = {winRect.x + winRect.width - 30, winRect.y + 5, 25, 25};
    DrawRectangleRec(closeRect, RED);
    DrawText("X", closeRect.x + 8, closeRect.y + 5, 20, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), closeRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.isOpen = false;
        state.editor.focusedFieldId = -1;
    }
    
    float startY = winRect.y + 50;
    
    // Name
    DrawText("Name:", winRect.x + 20, startY, 20, WHITE);
    DrawTextInput({winRect.x + 100, startY, 200, 30}, state.editor.nameBuffer, 63, 0, state.editor.focusedFieldId);
    
    // Sample
    startY += 40;
    DrawText("Sample:", winRect.x + 20, startY, 20, WHITE);
    DrawTextInput({winRect.x + 100, startY, 300, 30}, state.editor.samplePathBuffer, 255, 1, state.editor.focusedFieldId);
    
    // BPM
    startY += 40;
    DrawText("BPM:", winRect.x + 20, startY, 20, WHITE);
    DrawTextInput({winRect.x + 100, startY, 60, 30}, state.editor.bpmBuffer, 7, 2, state.editor.focusedFieldId);
    
    // Steps
    DrawText("Steps:", winRect.x + 200, startY, 20, WHITE);
    DrawTextInput({winRect.x + 280, startY, 50, 30}, state.editor.stepsBuffer, 3, 3, state.editor.focusedFieldId);
    
    // Editor Grid
    Pattern& p = state.editor.currentPattern;
    Rectangle gridRect = {winRect.x + 20, startY + 50, winRect.width - 40, 150};
    
    // Update steps from buffer for visualization
    int steps = atoi(state.editor.stepsBuffer);
    if (steps <= 0) steps = 16;
    if (steps > 64) steps = 64;
    
    int cols = std::min(16, steps);
    int rows = (steps + cols - 1) / cols;
    float cellW = gridRect.width / cols;
    float cellH = gridRect.height / rows;
    float size = std::min(cellW, cellH) * 0.9f;
    
    for (int i = 0; i < steps; ++i) {
        int col = i % cols;
        int row = i / cols;
        float x = gridRect.x + col * cellW + (cellW - size)/2;
        float y = gridRect.y + row * cellH + (cellH - size)/2;
        
        bool active = state.editor.stepStates[i];
        Color c = active ? RED : DARKGRAY;
        Rectangle stepRect = {x, y, size, size};
        DrawRectangleRec(stepRect, c);
        
        if (CheckCollisionPointRec(GetMousePosition(), stepRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.editor.stepStates[i] = !state.editor.stepStates[i];
        }
    }
    
    // Save Button
    Rectangle saveRect = {winRect.x + winRect.width - 100, winRect.y + winRect.height - 40, 80, 30};
    DrawRectangleRec(saveRect, BLUE);
    DrawText("Save", saveRect.x + 20, saveRect.y + 5, 20, WHITE);
    
    if (CheckCollisionPointRec(GetMousePosition(), saveRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Save back to pattern
        p.name = state.editor.nameBuffer;
        p.samplePath = state.editor.samplePathBuffer;
        p.bpm = atoi(state.editor.bpmBuffer);
        p.steps = steps;
        
        p.activeSteps.clear();
        for (int i=0; i<64; ++i) {
            if (state.editor.stepStates[i]) p.activeSteps.push_back(i+1);
        }
        
        // Register new pattern data
        engine.addPattern(p);
        if (p.samplePath != "") engine.loadSample(p);
        
        // Rename in columns if name changed
        std::string oldName = state.editor.originalName;
        if (p.name != oldName) {
            // Update all columns
            for (auto& col : state.columns) {
                std::replace(col.patternNames.begin(), col.patternNames.end(), oldName, p.name);
            }
            // Update selection if needed
            std::replace(state.selectedPatterns.begin(), state.selectedPatterns.end(), oldName, p.name);
        }
        
        // Refresh renderer
        state.editor.isOpen = false;
        state.editor.focusedFieldId = -1;
    }
}
