#include "TrackView.h"
#include "Widgets.h"
#include "StepGrid.h"
#include "../GuiState.h"
#include "../AudioEngine.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace gui {

TrackView::TrackView(GuiState& s, AudioEngine& e) : state(s), engine(e) {}

void TrackView::Draw() {
    // Calculate column layout
    float colX = 0;
    for (size_t i = 0; i < state.columns.size(); ++i) {
        state.columns[i].bounds = {
            colX,
            (float)state.HEADER_HEIGHT,
            (float)state.COLUMN_WIDTH,
            (float)(GetScreenHeight() - state.HEADER_HEIGHT - state.FOOTER_HEIGHT)
        };
        DrawColumn(i, state.columns[i]);
        colX += state.COLUMN_WIDTH;
    }
    
    // Add Column Button
    Rectangle addColBtn = {colX + 10, (float)state.HEADER_HEIGHT + 10, 40, 40};
    DrawRectangleRec(addColBtn, DARKGRAY);
    DrawText("+", addColBtn.x + 12, addColBtn.y + 8, 24, WHITE);
    
    if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), addColBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.columns.push_back({"Track " + std::to_string(state.columns.size() + 1), {}, {0, 0, 0, 0}});
    }
}

void TrackView::DrawColumn(int index, PatternColumn& col) {
    DrawRectangleRec(col.bounds, Color{35, 35, 35, 255});
    
    // Highlight column when in paste mode and hovering
    if (state.trackClipboard.isPasteMode && state.trackClipboard.hasData) {
        if (CheckCollisionPointRec(GetMousePosition(), col.bounds)) {
            DrawRectangleRec(col.bounds, Color{255, 0, 255, 40});
            DrawRectangleLinesEx(col.bounds, 3, MAGENTA);
            
            if (!state.editor.isOpen && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Pattern* origPat = engine.getPattern(state.trackClipboard.patternName);
                if (origPat) {
                    Pattern copy = *origPat;
                    copy.name = origPat->name + "_" + std::to_string(state.patternIdCounter++);
                    if (copy.samplePath != "") engine.loadSample(copy);
                    engine.addPattern(copy);
                    col.patternNames.push_back(copy.name);
                }
            }
        }
    }
    
    // Content area
    float topMargin = 40.0f;
    float bottomMargin = 40.0f;
    Rectangle contentArea = {
        col.bounds.x,
        col.bounds.y + topMargin,
        col.bounds.width,
        col.bounds.height - topMargin - bottomMargin
    };
    
    float totalContentHeight = col.patternNames.size() * (state.PATTERN_HEIGHT + 5);
    
    // Scroll logic
    if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), col.bounds)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            col.scrollY -= wheel * 30.0f;
        }
    }
    
    float maxScroll = std::max(0.0f, totalContentHeight - contentArea.height);
    if (col.scrollY < 0) col.scrollY = 0;
    if (col.scrollY > maxScroll) col.scrollY = maxScroll;
    
    // Header
    Rectangle headerRect = {col.bounds.x + 10, col.bounds.y + 10, col.bounds.width - 20, 25};
    
    if (state.renamingColumnIndex == index) {
        DrawTextInput(headerRect, state.columnRenameBuffer, 63, 1000 + index, state.focusedFieldId);
        if (IsKeyPressed(KEY_ENTER)) {
            col.title = state.columnRenameBuffer;
            state.renamingColumnIndex = -1;
            state.focusedFieldId = -1;
        }
    } else {
        DrawText(col.title.c_str(), headerRect.x, headerRect.y, 20, LIGHTGRAY);
        
        // Double-click to rename
        if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), headerRect)) {
            static double lastHeaderClickTime = 0;
            static int lastHeaderClickIndex = -1;
            
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                double now = GetTime();
                if (now - lastHeaderClickTime < 0.3 && lastHeaderClickIndex == index) {
                    state.renamingColumnIndex = index;
                    strcpy(state.columnRenameBuffer, col.title.c_str());
                    state.focusedFieldId = 1000 + index;
                }
                lastHeaderClickTime = now;
                lastHeaderClickIndex = index;
            }
        }
    }
    
    // Add button at bottom
    Rectangle addBtnRect = {col.bounds.x + 5, col.bounds.y + col.bounds.height - 35, col.bounds.width - 10, 30};
    
    BeginScissorMode((int)contentArea.x, (int)contentArea.y, (int)contentArea.width, (int)contentArea.height);
    
    float y = contentArea.y - col.scrollY;
    
    for (size_t i = 0; i < col.patternNames.size(); ++i) {
        Rectangle patRect = {
            col.bounds.x + 5,
            y,
            col.bounds.width - 10,
            (float)state.PATTERN_HEIGHT
        };
        y += state.PATTERN_HEIGHT + 5;
        
        bool isSelected = (state.activePatternSlots.count(index) && state.activePatternSlots[index] == (int)i);
        bool overlapsButton = CheckCollisionRecs(patRect, addBtnRect);
        
        if (state.drag.isDragging && 
            state.drag.sourceColumnIndex == index && 
            state.drag.sourceSlotIndex == (int)i) {
            DrawRectangleLinesEx(patRect, 2.0f, DARKGRAY);
        } else {
            bool isHoldingThis = state.drag.isHolding && 
                                 state.drag.sourceColumnIndex == index && 
                                 state.drag.sourceSlotIndex == (int)i;
            
            if (!overlapsButton) {
                int currentStep = engine.getPatternProgress(col.patternNames[i]);
                
                DrawPatternBox(col.patternNames[i], patRect, isSelected, currentStep);
                
                // Draw mini step grid inside the pattern box
                Pattern* pat = engine.getPattern(col.patternNames[i]);
                if (pat) {
                    Rectangle gridRect = {patRect.x + 5, patRect.y + 20, patRect.width - 10, patRect.height - 25};
                    DrawStepGrid(gridRect, *pat, currentStep, state);
                }

                if (isHoldingThis) {
                    float progress = (float)(GetTime() - state.drag.holdStartTime) / 1.0f;
                    if (progress > 1.0f) progress = 1.0f;
                    DrawRectangleLinesEx(patRect, 2, YELLOW);
                    DrawRectangle(patRect.x, patRect.y + patRect.height - 5, patRect.width * progress, 5, YELLOW);
                }
                
                if (state.isShiftMode && !state.shiftEditingPatternName.empty() && 
                    col.patternNames[i] == state.shiftEditingPatternName && !isSelected) {
                    DrawRectangleLinesEx(patRect, 3, YELLOW);
                }
                
                HandlePatternClick(index, i, col.patternNames[i]);
            }
        }
    }
    
    EndScissorMode();
    
    // Scrollbar
    if (totalContentHeight > contentArea.height) {
        float barW = 6;
        float barX = col.bounds.x + col.bounds.width - barW - 2;
        float barY = contentArea.y;
        float barH = contentArea.height;
        
        float viewRatio = contentArea.height / totalContentHeight;
        float thumbH = std::max(20.0f, contentArea.height * viewRatio);
        float thumbY = barY + (col.scrollY / (totalContentHeight - contentArea.height)) * (contentArea.height - thumbH);
        
        DrawRectangle(barX, barY, barW, barH, Color{0, 0, 0, 100});
        DrawRectangle(barX, thumbY, barW, thumbH, Color{80, 80, 80, 200});
    }

    DrawRectangleRec(addBtnRect, Color{50, 50, 50, 255});
    DrawText("+ Add", addBtnRect.x + 40, addBtnRect.y + 5, 10, WHITE);
    
    HandleAddPattern(index, col);
}

void TrackView::HandlePatternClick(int colIndex, int slotIndex, const std::string& patternName) {
    PatternColumn& col = state.columns[colIndex];
    Rectangle contentArea = {
        col.bounds.x,
        col.bounds.y + 40.0f,
        col.bounds.width,
        col.bounds.height - 80.0f
    };
    
    Rectangle patRect = {
        col.bounds.x + 5,
        contentArea.y - col.scrollY + slotIndex * (state.PATTERN_HEIGHT + 5),
        col.bounds.width - 10,
        (float)state.PATTERN_HEIGHT
    };
    
    if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), contentArea) && CheckCollisionPointRec(GetMousePosition(), patRect)) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            // Copy mode
            if (state.trackClipboard.isCopyMode) {
                state.trackClipboard.patternName = patternName;
                state.trackClipboard.hasData = true;
                state.trackClipboard.isCopyMode = false;
                state.trackClipboard.isPasteMode = true;
                return;
            }
            
            if (state.trackClipboard.isPasteMode && state.trackClipboard.hasData) {
                return;
            }
            
            // Shift mode - edit without changing selection
            if (state.isShiftMode && state.isLiveEditMode) {
                Pattern* p = engine.getPattern(patternName);
                if (p) {
                    state.shiftEditingPatternName = p->name;
                    state.editor.currentPattern = *p;
                    state.editor.isOpen = true;
                    strcpy(state.editor.nameBuffer, p->name.c_str());
                    strcpy(state.editor.originalName, p->name.c_str());
                    strcpy(state.editor.samplePathBuffer, p->samplePath.c_str());
                    sprintf(state.editor.bpmBuffer, "%d", p->bpm);
                    sprintf(state.editor.stepsBuffer, "%d", p->steps);
                    for (int s = 0; s < 64; ++s) state.editor.stepStates[s] = p->shouldTriggerAt(s+1);
                }
                return;
            }
            
            // Live edit mode - open and change selection
            if (state.isLiveEditMode && !state.isShiftMode) {
                Pattern* p = engine.getPattern(patternName);
                if (p) {
                    state.editor.currentPattern = *p;
                    state.editor.isOpen = true;
                    strcpy(state.editor.nameBuffer, p->name.c_str());
                    strcpy(state.editor.originalName, p->name.c_str());
                    strcpy(state.editor.samplePathBuffer, p->samplePath.c_str());
                    sprintf(state.editor.bpmBuffer, "%d", p->bpm);
                    sprintf(state.editor.stepsBuffer, "%d", p->steps);
                    for (int s = 0; s < 64; ++s) state.editor.stepStates[s] = p->shouldTriggerAt(s+1);
                }
            }
            
            // Toggle selection
            bool wasSelected = (state.activePatternSlots.count(colIndex) && state.activePatternSlots[colIndex] == slotIndex);
            if (wasSelected) {
                state.activePatternSlots.erase(colIndex);
            } else {
                state.activePatternSlots[colIndex] = slotIndex;
            }
            
            // Sync with audio engine
            if (engine.isPlaying()) {
                std::vector<std::string> allActive;
                for (auto& pair : state.activePatternSlots) {
                    int cIdx = pair.first;
                    int sIdx = pair.second;
                    if (cIdx >= 0 && cIdx < (int)state.columns.size() && sIdx >= 0 && sIdx < (int)state.columns[cIdx].patternNames.size()) {
                        allActive.push_back(state.columns[cIdx].patternNames[sIdx]);
                    }
                }
                engine.updateActivePatterns(allActive);
            }
            
            // Start hold for drag
            state.drag.isHolding = true;
            state.drag.holdStartTime = GetTime();
            state.drag.initialClickPos = GetMousePosition();
            state.drag.sourceColumnIndex = colIndex;
            state.drag.sourceSlotIndex = slotIndex;
            state.drag.patternName = patternName;
        }
    }
}

void TrackView::HandleAddPattern(int colIndex, PatternColumn& col) {
    Rectangle addBtnRect = {col.bounds.x + 5, col.bounds.y + col.bounds.height - 35, col.bounds.width - 10, 30};
    
    if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), addBtnRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Pattern p;
        p.name = "Pat" + std::to_string(state.patternIdCounter++);
        p.bpm = state.bpm;
        col.patternNames.push_back(p.name);
        engine.addPattern(p);
        
        state.editor.currentPattern = p;
        state.editor.isOpen = true;
        strcpy(state.editor.nameBuffer, p.name.c_str());
        strcpy(state.editor.originalName, p.name.c_str());
        sprintf(state.editor.bpmBuffer, "%d", p.bpm);
        sprintf(state.editor.stepsBuffer, "%d", p.steps);
        for (int s = 0; s < 64; ++s) state.editor.stepStates[s] = false;
    }
}

} // namespace gui
