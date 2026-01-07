#include "TrackView.h"
#include "TrackFX.h"
#include <raymath.h>
#include "Widgets.h"
#include "StepGrid.h"
#include "../GuiState.h"
#include "../AudioEngine.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath> // for abs

namespace gui {

TrackView::TrackView(GuiState& s, AudioEngine& e) : state(s), engine(e) {}

void TrackView::Draw() {
    // 1. Manage Drag State Promotion (Hold -> Drag)
    if (state.drag.isHolding) {
        float dist = Vector2Distance(GetMousePosition(), state.drag.initialClickPos);
        double holdDuration = GetTime() - state.drag.holdStartTime;
        
        // Promote if moved > 5px OR held > 0.3s
        if (dist > 5.0f || holdDuration > 0.3) {
            state.drag.isDragging = true;
            state.drag.isHolding = false;
        } else if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            // Released before dragging - just a click, already handled by HandlePatternClick
            state.drag.isHolding = false;
        }
    }

    // 2. Draw Columns
    float startX = 20.0f;
    float colX = startX - state.mainScrollX;
    
    // Scissor for main view area
    Rectangle viewRect = {0, (float)state.HEADER_HEIGHT, (float)GetScreenWidth(), (float)GetScreenHeight() - state.HEADER_HEIGHT - state.FOOTER_HEIGHT};
    BeginScissorMode((int)viewRect.x, (int)viewRect.y, (int)viewRect.width, (int)viewRect.height);

    for (size_t i = 0; i < state.columns.size(); ++i) {
        state.columns[i].bounds = {
            colX,
            (float)state.HEADER_HEIGHT + 20, // Margin
            (float)state.COLUMN_WIDTH,
            (float)(GetScreenHeight() - state.HEADER_HEIGHT - state.FOOTER_HEIGHT - 40)
        };
        DrawColumn((int)i, state.columns[i]);
        colX += state.COLUMN_WIDTH + 10;
    }
    
    // Add Column Button
    Rectangle addColBtn = {colX, (float)state.HEADER_HEIGHT + 20, 40, (float)state.PATTERN_HEIGHT};
    DrawRectangleRec(addColBtn, Color{40, 40, 40, 255});
    DrawText("+", addColBtn.x + 13, addColBtn.y + 30, 30, GRAY);
    
    if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), addColBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
         if (CheckCollisionPointRec(GetMousePosition(), viewRect)) {
             state.columns.push_back({"Track " + std::to_string(state.columns.size() + 1), std::vector<std::string>(16, ""), {0, 0, 0, 0}, 0.0f});
         }
    }
    
    EndScissorMode();

    // 3. Global Drop Logic (if Dragging)
    if (state.drag.isDragging) {
        // Draw Ghost
        Vector2 mouse = GetMousePosition();
        Rectangle ghostRect = { mouse.x + 10, mouse.y + 10, 140, 45 };
        DrawRectangleRec(ghostRect, Color{60, 60, 60, 200});
        DrawRectangleLinesEx(ghostRect, 1, WHITE);
        DrawText(state.drag.patternName.c_str(), ghostRect.x + 10, ghostRect.y + 15, 10, WHITE);

        // Handle Release
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            bool dropped = false;
            // Find Target
            for (size_t c = 0; c < state.columns.size(); ++c) {
                PatternColumn& col = state.columns[c];
                
                // Check if mouse is within column Content Area (approximate)
                if (CheckCollisionPointRec(mouse, col.bounds)) {
                    // Calculate Slot Index based on Scroll
                    float topMargin = 40.0f;
                    float relativeY = mouse.y - (col.bounds.y + topMargin) + col.scrollY;
                    int slotIdx = (int)(relativeY / (state.PATTERN_HEIGHT + 5));
                    
                    if (slotIdx >= 0 && slotIdx < (int)col.patternNames.size()) {
                        // VALID DROP TARGET
                        std::string draggedName = state.drag.patternName;
                        int srcCol = state.drag.sourceColumnIndex;
                        int srcSlot = state.drag.sourceSlotIndex;

                        if (!draggedName.empty() && srcCol != -1 && srcSlot != -1) {
                            bool isSelfDrop = ((int)c == srcCol && slotIdx == srcSlot);
                            
                            if (!isSelfDrop) {
                                // Execute Move
                                if (col.patternNames[(size_t)slotIdx].empty()) {
                                    col.patternNames[(size_t)slotIdx] = draggedName;
                                    
                                    // Safety check for source (it might have changed?)
                                    if (srcCol >= 0 && srcCol < (int)state.columns.size()) {
                                        if (srcSlot >= 0 && srcSlot < (int)state.columns[(size_t)srcCol].patternNames.size()) {
                                            state.columns[(size_t)srcCol].patternNames[(size_t)srcSlot] = "";
                                        }
                                    }
                                } else {
                                    // Swap
                                    std::string targetName = col.patternNames[(size_t)slotIdx];
                                    col.patternNames[(size_t)slotIdx] = draggedName;
                                     if (srcCol >= 0 && srcCol < (int)state.columns.size()) {
                                        if (srcSlot >= 0 && srcSlot < (int)state.columns[(size_t)srcCol].patternNames.size()) {
                                            state.columns[(size_t)srcCol].patternNames[(size_t)srcSlot] = targetName;
                                        }
                                    }
                                }
                                
                                // Fix Active Selection
                                if (state.activePatternSlots.count(srcCol) && state.activePatternSlots[srcCol] == srcSlot) {
                                    state.activePatternSlots.erase(srcCol);
                                }

                                dropped = true;
                            }
                        }
                    } 
                    break; // Found column
                }
            }
            // End Drag
            state.drag.isDragging = false;
            state.drag.isHolding = false;
        }
    }
}

void TrackView::DrawColumn(int index, PatternColumn& col) {
    DrawRectangleRec(col.bounds, Color{35, 35, 35, 255});
    
    // 0. Mixer Mode Check
    if (col.mixerMode) {
        state.columns[index].bounds = col.bounds; // Ensure bounds synced
        DrawTrackMixer(col.bounds, col, state, engine);
        return;
    }
    
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
    
    // Initial Calc
    float totalContentHeight = (float)col.patternNames.size() * (float)(state.PATTERN_HEIGHT + 5);
    
    // Scroll Logic
    if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), col.bounds)) {
        float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.001f) {
            col.scrollY -= wheel * 20.0f; // Smoother scrolling (reduced from 30)
        }
    }
    
    // Header (Title Bar)
    Rectangle headerRect = {col.bounds.x, col.bounds.y, col.bounds.width, 30};
    
    // Draw Window Title Bar
    DrawRectangleRec(headerRect, Color{20, 60, 100, 255}); // Darker Blue Title Bar

    if (state.renamingColumnIndex == index) {
        DrawTextInput(headerRect, state.columnRenameBuffer, 63, 1000 + index, state.focusedFieldId);
        if (IsKeyPressed(KEY_ENTER)) {
            col.title = state.columnRenameBuffer;
            state.renamingColumnIndex = -1;
            state.focusedFieldId = -1;
        }
    } else {
        DrawText(col.title.c_str(), headerRect.x + 5, headerRect.y + 4, 18, WHITE);
        
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
    
    // Add button at bottom (Small, Left)
    float btnY = col.bounds.y + col.bounds.height - 35;
    Rectangle addBtnRect = {col.bounds.x + 5, btnY, 40, 30};
    
    BeginScissorMode((int)contentArea.x, (int)contentArea.y, (int)contentArea.width, (int)contentArea.height);
    
    float y = contentArea.y - col.scrollY;
    
    // Auto-fill grid to visible height to prevent "popping into existence" or empty space
    int visibleCells = (int)(contentArea.height / (state.PATTERN_HEIGHT + 5)) + 2; 
    int minCells = std::max(16, visibleCells);
    if (col.patternNames.size() < (size_t)minCells) col.patternNames.resize((size_t)minCells, "");

    // Clamp Scroll (Recalculate after resize)
    totalContentHeight = (float)col.patternNames.size() * (float)(state.PATTERN_HEIGHT + 5);
    float maxScroll = std::max(0.0f, totalContentHeight - contentArea.height);
    if (col.scrollY < 0) col.scrollY = 0;
    if (col.scrollY > maxScroll) col.scrollY = maxScroll;
    
    for (size_t i = 0; i < col.patternNames.size(); ++i) {
        Rectangle cellRect = {
            col.bounds.x + 5,
            y,
            col.bounds.width - 20, // Reduced width to clear scrollbar
            (float)state.PATTERN_HEIGHT
        };
        y += state.PATTERN_HEIGHT + 5;
        
        bool isSelected = (state.activePatternSlots.count(index) && state.activePatternSlots[index] == (int)i);
        bool overlapsButton = CheckCollisionRecs(cellRect, addBtnRect);
        
        if (state.drag.isDragging && 
            state.drag.sourceColumnIndex == index && 
            state.drag.sourceSlotIndex == (int)i) {
            // Beind dragged source
            DrawRectangleLinesEx(cellRect, 2.0f, DARKGRAY);
        } else if (!overlapsButton) {
            if (col.patternNames[i].empty()) {
                // Empty Cell
                DrawRectangleRec(cellRect, Color{30, 30, 30, 255});
                DrawRectangleLinesEx(cellRect, 1, Color{50, 50, 50, 255});
                DrawText("-", cellRect.x + cellRect.width/2 - 5, cellRect.y + cellRect.height/2 - 10, 20, DARKGRAY);
                
                // Allow interaction
                HandlePatternClick(index, (int)i, "", cellRect);
            } else {
                // Occupied Cell
                bool isHoldingThis = state.drag.isHolding && 
                                     state.drag.sourceColumnIndex == index && 
                                     state.drag.sourceSlotIndex == (int)i;
                
                int currentStep = engine.getPatternProgress(col.patternNames[i]);
                
                DrawPatternBox(col.patternNames[i], cellRect, isSelected, currentStep);
                
                // Draw mini step grid inside the pattern box
                Pattern* pat = engine.getPattern(col.patternNames[i]);
                if (pat) {
                    Rectangle gridRect = {cellRect.x + 5, cellRect.y + 20, cellRect.width - 10, cellRect.height - 25};
                    DrawStepGrid(gridRect, *pat, currentStep, state);
                }

                if (isHoldingThis) {
                    float progress = (float)(GetTime() - state.drag.holdStartTime) / 0.3f;
                    if (progress > 1.0f) progress = 1.0f;
                    DrawRectangleLinesEx(cellRect, 1, YELLOW);
                    DrawRectangle(cellRect.x, cellRect.y + cellRect.height - 5, cellRect.width * progress, 5, YELLOW);
                }
                
                if (state.isShiftMode && !state.shiftEditingPatternName.empty() && 
                    col.patternNames[i] == state.shiftEditingPatternName && !isSelected) {
                    DrawRectangleLinesEx(cellRect, 1, YELLOW);
                }
                
                HandlePatternClick(index, (int)i, col.patternNames[i], cellRect);
            }
        }
        
        // Draw selection for EMPTY cells too if selected
        if (state.activePatternSlots.count(index) && state.activePatternSlots[index] == (int)i) {
             DrawRectangleLinesEx(cellRect, 1.0f, YELLOW); 
        }

        // Drag Destination Logic - VISUAL OVERLAY (Draw LAST to be visible over content)
        if (state.drag.isDragging && CheckCollisionPointRec(GetMousePosition(), cellRect) && !overlapsButton) {
            // Visible Indicator
            DrawRectangle(cellRect.x, cellRect.y, cellRect.width, cellRect.height, Color{0, 255, 0, 60}); // Brighter Green Highlight
            DrawRectangleLinesEx(cellRect, 3.0f, GREEN); // Thicker Border
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

    // Add Button (Small, Left) - Drawn using pre-calculated rect
    DrawRectangleRec(addBtnRect, Color{50, 50, 50, 255});
    DrawText("+", addBtnRect.x + 13, addBtnRect.y + 2, 24, WHITE);
    
    // Delete Button (Next to Add)
    Rectangle delBtnRect = {addBtnRect.x + addBtnRect.width + 5, btnY, 40, 30};
    DrawRectangleRec(delBtnRect, Color{80, 20, 20, 255});  // Dark red
    DrawText("-", delBtnRect.x + 15, delBtnRect.y + 2, 24, WHITE);
    
    // Delete button action
    if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), delBtnRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Delete the selected pattern in this column
        if (state.activePatternSlots.count(index)) {
            int selectedSlot = state.activePatternSlots[index];
            if (selectedSlot >= 0 && selectedSlot < (int)col.patternNames.size()) {
                col.patternNames[(size_t)selectedSlot] = "";  // Clear the slot
                state.activePatternSlots.erase(index);  // Clear selection
            }
        }
    }
    
    // Mixer Button (Rest)
    Rectangle mixBtnRect = {col.bounds.x + 95, btnY, col.bounds.width - 100, 30};
    DrawRectangleRec(mixBtnRect, Color{30, 30, 40, 255});
    DrawText("MIXER", mixBtnRect.x + mixBtnRect.width/2 - 25, mixBtnRect.y + 8, 14, GRAY);
    
    if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), mixBtnRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        col.mixerMode = true;
    }
    
    HandleAddPattern(index, col);
}

void TrackView::HandlePatternClick(int colIndex, int slotIndex, const std::string& patternName, Rectangle cellRect) {
    // patternName can be empty now
    
    // PatternColumn& col = state.columns[colIndex]; // Unused variable warning? Used in logic
    PatternColumn& col = state.columns[(size_t)colIndex];
    Rectangle contentArea = {
        col.bounds.x,
        col.bounds.y + 40.0f,
        col.bounds.width,
        col.bounds.height - 80.0f
    };
    
    // Check Collision using PASSED cellRect (Matches Drawing)
    if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), contentArea) && CheckCollisionPointRec(GetMousePosition(), cellRect)) {
        static double lastPatternClickTime = 0;
        static std::string lastPatternClickName = "";

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            // Check Double Click FIRST
            double now = GetTime();
            bool isDoubleClick = (now - lastPatternClickTime < 0.3 && lastPatternClickName == patternName);
            lastPatternClickTime = now;
            lastPatternClickName = patternName;
            
            if (isDoubleClick) {
                 Pattern* p = engine.getPattern(patternName);
                 if (p) {
                     state.editor.currentPattern = *p;
                     state.editor.isOpen = true;
                     state.editor.showFileBrowser = false;
                     strcpy(state.editor.nameBuffer, p->name.c_str());
                     strcpy(state.editor.originalName, p->name.c_str());
                     strcpy(state.editor.samplePathBuffer, p->samplePath.c_str());
                     sprintf(state.editor.bpmBuffer, "%d", p->bpm);
                     sprintf(state.editor.stepsBuffer, "%d", p->steps);
                     for (int s = 0; s < 64; ++s) state.editor.stepStates[s] = p->shouldTriggerAt(s+1);
                 }
                 return; // Stop further processing (don't toggle selection/drag)
            }

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
            
            // Selection Logic - Allow one pattern per column by default
            bool isMulti = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) || IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
            
            // Check if this pattern is already selected in this column
            bool wasSelected = (state.activePatternSlots.count(colIndex) && state.activePatternSlots[colIndex] == slotIndex);
            
            if (!isMulti) {
                // Normal click: toggle selection in this column
                if (wasSelected) {
                    // Deselect if clicking the same pattern again
                    state.activePatternSlots.erase(colIndex);
                } else {
                    // Select this pattern (keeps selections in other columns)
                    state.activePatternSlots[colIndex] = slotIndex;
                }
            } else {
                // Shift/Ctrl: Toggle behavior for multi-select within same column
                if (wasSelected) {
                    state.activePatternSlots.erase(colIndex);
                } else {
                    state.activePatternSlots[colIndex] = slotIndex;
                }
            }
            
            // Sync with audio engine
            if (engine.isPlaying()) {
                std::vector<std::string> allActive;
                for (auto& pair : state.activePatternSlots) {
                    int cIdx = pair.first;
                    int sIdx = pair.second;
                    if (cIdx >= 0 && cIdx < (int)state.columns.size()) {
                        PatternColumn& pc = state.columns[(size_t)cIdx];
                        if (sIdx >= 0 && sIdx < (int)pc.patternNames.size()) {
                            std::string pname = pc.patternNames[(size_t)sIdx];
                            if (!pname.empty()) {
                                allActive.push_back(pname);
                            }
                        }
                    }
                }
                engine.updateActivePatterns(allActive);
            }
            
            // Start hold for drag
            state.drag.isDragging = false; // Reset drag?
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
    // colIndex is used for selection lookup
    
    Rectangle addBtnRect = {col.bounds.x + 5, col.bounds.y + col.bounds.height - 35, 40, 30};
    
    if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), addBtnRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Only create pattern if we have a selected EMPTY slot
        int targetSlot = -1;
        
        if (state.activePatternSlots.count(colIndex)) {
            int selectedSlot = state.activePatternSlots[colIndex];
            if (selectedSlot >= 0 && selectedSlot < (int)col.patternNames.size()) {
                if (col.patternNames[(size_t)selectedSlot].empty()) {
                    targetSlot = selectedSlot;
                }
            }
        }
        
        // Only create pattern if we found an empty slot
        if (targetSlot != -1) {
            Pattern p;
            p.name = "Pat" + std::to_string(state.patternIdCounter++);
            p.bpm = state.bpm;
            engine.addPattern(p);
            col.patternNames[(size_t)targetSlot] = p.name;
        }
        // Otherwise, do nothing (user tried to add on occupied slot)
    }
}
} // namespace gui
