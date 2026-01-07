#include "GuiRenderer.h"
#include "gui/Widgets.h"
#include "gui/DragDrop.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <raymath.h>

namespace fs = std::filesystem;

GuiRenderer::GuiRenderer(GuiState& s, AudioEngine& e) 
    : state(s), engine(e), transportBar(s, e), trackView(s, e), patternEditor(s, e) {
    font = GetFontDefault();
}
#include "tinyfiledialogs.h"

void GuiRenderer::Update() {
    // Use modular drag and drop handler
    gui::HandleDragAndDrop(state);
    
    // Sync playback state
    state.isPlaying = engine.isPlaying();
}

void GuiRenderer::Draw() {
    ClearBackground(Color{20, 20, 20, 255});
    
    // Draw Headers
    Rectangle headerRect = {0, 0, (float)GetScreenWidth(), (float)state.HEADER_HEIGHT};
    DrawRectangleRec(headerRect, Color{30, 30, 30, 255});
    DrawText("Quadracollision BlackLang", 20, 15, 30, WHITE);
    
    // Main View Area
    Rectangle viewRect = {0, (float)state.HEADER_HEIGHT, (float)GetScreenWidth(), (float)GetScreenHeight() - state.HEADER_HEIGHT - state.FOOTER_HEIGHT};
    
    // Calculate Content Width
    float startX = 20;
    float contentW = startX + state.columns.size() * (state.COLUMN_WIDTH + 10) + 60; // + Add Button + Margin
    state.mainContentWidth = contentW;
    
    // Scroll Input (Shift + Wheel)
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
        state.mainScrollX -= GetMouseWheelMove() * 30.0f;
    }
    
    // Clamp Scroll
    float maxScroll = std::max(0.0f, state.mainContentWidth - GetScreenWidth());
    if (state.mainScrollX < 0) state.mainScrollX = 0;
    if (state.mainScrollX > maxScroll) state.mainScrollX = maxScroll;
    
    // Draw Columns (Scissored)
    BeginScissorMode((int)viewRect.x, (int)viewRect.y, (int)viewRect.width, (int)viewRect.height);
        
        float currentX = startX - state.mainScrollX;
        float startY = state.HEADER_HEIGHT + 20;
        
        for (size_t i = 0; i < state.columns.size(); ++i) {
            Rectangle colRect = {
                currentX + i * (state.COLUMN_WIDTH + 10),
                startY,
                (float)state.COLUMN_WIDTH,
                (float)GetScreenHeight() - state.HEADER_HEIGHT - state.FOOTER_HEIGHT - 40
            };
            state.columns[i].bounds = colRect; // Update bounds for collision logic
            DrawColumn(i, state.columns[i]);
        }
        
        // Add Column Button
        float addColX = currentX + state.columns.size() * (state.COLUMN_WIDTH + 10);
        Rectangle addColRect = {addColX, startY, 40, (float)state.PATTERN_HEIGHT};
        DrawRectangleRec(addColRect, Color{40, 40, 40, 255});
        DrawText("+", addColX + 13, startY + 30, 30, GRAY);
        
        // Only allow click if visible/in-bounds (Scissor handles visibility, but we check collision)
        // AND if editor is not open
        if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), addColRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
             // Basic collision check passes, but we should ensure we are not clicking scrollbar area if checked there.
             // Rely on z-order: input handled before scrollbar?
             // Actually, ScissorMode clips *drawing*. It does NOT prevent *input*.
             // We should check if mouse is within viewRect too.
             if (CheckCollisionPointRec(GetMousePosition(), viewRect)) {
                 state.columns.push_back({"Track " + std::to_string(state.columns.size() + 1), {}, {0,0,0,0}});
             }
        }
        
    EndScissorMode();
    
    // Draw Horizontal Scrollbar (if needed)
    if (state.mainContentWidth > GetScreenWidth()) {
        float barH = 10;
        float barY = viewRect.y + viewRect.height - barH - 5;
        
        float viewRatio = GetScreenWidth() / state.mainContentWidth;
        float thumbW = std::max(30.0f, GetScreenWidth() * viewRatio);
        float thumbX = (state.mainScrollX / (state.mainContentWidth - GetScreenWidth())) * (GetScreenWidth() - thumbW);
        
        // Track
        DrawRectangle(0, barY, GetScreenWidth(), barH, Color{20, 20, 20, 200});
        // Thumb
        Rectangle thumbRect = {thumbX, barY, thumbW, barH};
        DrawRectangleRec(thumbRect, Color{100, 100, 100, 255});
        
        // Drag Scrollbar Logic
        static bool isDraggingScroll = false;
        static float dragOffsetX = 0;
        
        if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), thumbRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            isDraggingScroll = true;
            dragOffsetX = GetMousePosition().x - thumbRect.x;
        }
        
        if (isDraggingScroll) {
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) isDraggingScroll = false;
            else {
                float targetX = GetMousePosition().x - dragOffsetX;
                float pct = targetX / (GetScreenWidth() - thumbW);
                state.mainScrollX = pct * (state.mainContentWidth - GetScreenWidth());
                if (state.mainScrollX < 0) state.mainScrollX = 0;
                if (state.mainScrollX > maxScroll) state.mainScrollX = maxScroll;
            }
        }
    }
    
    // Use modular transport bar
    transportBar.Draw();
    
    // Draw dragged item using modular DragDrop
    gui::DrawDragGhost(state);
    
    // Use modular pattern editor
    if (state.editor.isOpen) {
        patternEditor.Draw();
    }
}

void GuiRenderer::DrawColumn(int index, PatternColumn& col) {
    DrawRectangleRec(col.bounds, Color{35, 35, 35, 255});
    
    // Highlight column when in paste mode and hovering
    if (state.trackClipboard.isPasteMode && state.trackClipboard.hasData) {
        if (CheckCollisionPointRec(GetMousePosition(), col.bounds)) {
            DrawRectangleRec(col.bounds, Color{255, 0, 255, 40}); // Magenta tint
            DrawRectangleLinesEx(col.bounds, 3, MAGENTA);
            
            // Click anywhere in the column to paste to bottom - CREATE A COPY
            if (!state.editor.isOpen && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                // Get the original pattern
                Pattern* origPat = engine.getPattern(state.trackClipboard.patternName);
                if (origPat) {
                    // Create a copy with a unique name
                    Pattern copy = *origPat;
                    copy.name = origPat->name + "_" + std::to_string(state.patternIdCounter++);
                    
                    // Add to engine and column
                    if (copy.samplePath != "") engine.loadSample(copy);
                    engine.addPattern(copy);
                    col.patternNames.push_back(copy.name);
                }
                // Keep paste mode active for multiple pastes
            }
        }
    }
    
    // Define Content Area
    // Header is ~40px high (10 offset + 25 height + 5 margin)
    // Add Button is 30px high at bottom + 5 margin
    float topMargin = 40.0f;
    float bottomMargin = 40.0f;
    
    Rectangle contentArea = {
        col.bounds.x,
        col.bounds.y + topMargin,
        col.bounds.width,
        col.bounds.height - topMargin - bottomMargin
    };
    
    // Calculate Total Content Height
    float totalContentHeight = col.patternNames.size() * (state.PATTERN_HEIGHT + 5);
    
    // Scroll Logic
    if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), col.bounds)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            col.scrollY -= wheel * 30.0f;
        }
    }
    
    // Clamp Scroll
    float maxScroll = std::max(0.0f, totalContentHeight - contentArea.height);
    if (col.scrollY < 0) col.scrollY = 0;
    if (col.scrollY > maxScroll) col.scrollY = maxScroll;
    
    // Header Renaming
    Rectangle headerRect = {col.bounds.x + 10, col.bounds.y + 10, col.bounds.width - 20, 25};
    
    if (state.renamingColumnIndex == index) {
        DrawTextInput(headerRect, state.columnRenameBuffer, 63, 1000 + index, state.focusedFieldId);
        // Commit on Enter or Click Outside (handled by global focus somewhat, but let's be explicit)
        if (IsKeyPressed(KEY_ENTER)) {
             col.title = state.columnRenameBuffer;
             state.renamingColumnIndex = -1;
             state.focusedFieldId = -1;
        }
    } else {
        DrawText(col.title.c_str(), headerRect.x, headerRect.y, 20, LIGHTGRAY);
        
        // Double Click to Rename Header
        if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), headerRect)) {
            static double lastHeaderClickTime = 0;
            static int lastHeaderClickIndex = -1;
            
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                double now = GetTime();
                if (now - lastHeaderClickTime < 0.3 && lastHeaderClickIndex == index) {
                    // Start Renaming
                    state.renamingColumnIndex = index;
                    strcpy(state.columnRenameBuffer, col.title.c_str());
                    state.focusedFieldId = 1000 + index;
                }
                lastHeaderClickTime = now;
                lastHeaderClickIndex = index;
            }
        }
    }
    
    // Add Pattern Button (Fixed at Bottom)
    Rectangle addBtnRect = {col.bounds.x + 5, col.bounds.y + col.bounds.height - 35, col.bounds.width - 10, 30};

    // Calculate visible area for patterns (above the button)
    // We only process patterns that are not covered by the button area
    
    BeginScissorMode((int)contentArea.x, (int)contentArea.y, (int)contentArea.width, (int)contentArea.height);
    
    float y = contentArea.y - col.scrollY;
    
    for (size_t i = 0; i < col.patternNames.size(); ++i) {
        Rectangle patRect = {
            col.bounds.x + 5,
            y,
            col.bounds.width - 10,
            (float)state.PATTERN_HEIGHT
        };
        y += state.PATTERN_HEIGHT + 5; // Advance Y
        
        // Selection Logic
        bool isSelected = (state.activePatternSlots.count(index) && state.activePatternSlots[index] == (int)i);
        
        // Skip drawing/interaction if completely covered by add button
        bool overlapsButton = CheckCollisionRecs(patRect, addBtnRect);
                                  
        // Skip drawing if being dragged
        if (state.drag.isDragging && 
            state.drag.sourceColumnIndex == index && 
            state.drag.sourceSlotIndex == (int)i) {
            // Draw placeholder
            DrawRectangleLinesEx(patRect, 2.0f, DARKGRAY);
        } else {
            // Check if holding this specific pattern
            bool isHoldingThis = state.drag.isHolding && 
                                 state.drag.sourceColumnIndex == index && 
                                 state.drag.sourceSlotIndex == (int)i;
            
            // Draw only if not covered
            if (!overlapsButton) {
                // Get Playback Progress
                int currentStep = engine.getPatternProgress(col.patternNames[i]);
                bool isPlaying = (currentStep != -1);
                
                // Draw Box
                DrawPatternBox(col.patternNames[i], patRect, isSelected);

                if (isHoldingThis) {
                    float progress = (float)(GetTime() - state.drag.holdStartTime) / 1.0f;
                    if (progress > 1.0f) progress = 1.0f;
                    DrawRectangleLinesEx(patRect, 2, YELLOW);
                    DrawRectangle(patRect.x, patRect.y + patRect.height - 5, patRect.width * progress, 5, YELLOW);
                }
                
                // Highlight Shift-editing pattern with yellow border (but not the same as playing selection)
                if (state.isShiftMode && !state.shiftEditingPatternName.empty() && 
                    col.patternNames[i] == state.shiftEditingPatternName && !isSelected) {
                    DrawRectangleLinesEx(patRect, 3, YELLOW);
                }
                
                // Interaction - only if not overlapping button
                // Interaction - only if not overlapping button AND within visible content area
                // AND Editor is NOT open
                if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), contentArea) && CheckCollisionPointRec(GetMousePosition(), patRect)) {
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        
                        // --- TRACK COPY MODE ---
                        if (state.trackClipboard.isCopyMode) {
                            state.trackClipboard.patternName = col.patternNames[i];
                            state.trackClipboard.hasData = true;
                            state.trackClipboard.isCopyMode = false;
                            state.trackClipboard.isPasteMode = true; // Auto-switch to Paste mode
                            continue; // Don't process further
                        }
                        
                        
                        // --- TRACK PASTE MODE ---
                        // (Paste to bottom is handled in DrawColumn - skip normal click processing here)
                        if (state.trackClipboard.isPasteMode && state.trackClipboard.hasData) {
                            continue; // Don't process further - paste is handled at column level
                        }
                        
                        // --- SHIFT MODE: Open for editing without changing selection ---
                        if (state.isShiftMode && state.isLiveEditMode) {
                            Pattern* p = engine.getPattern(col.patternNames[i]);
                            if (p) {
                                state.shiftEditingPatternName = p->name; // Track which pattern is being edited
                                state.editor.currentPattern = *p;
                                state.editor.isOpen = true;
                                state.editor.showFileBrowser = false;
                                strcpy(state.editor.nameBuffer, p->name.c_str());
                                strcpy(state.editor.originalName, p->name.c_str());
                                strcpy(state.editor.samplePathBuffer, p->samplePath.c_str());
                                sprintf(state.editor.bpmBuffer, "%d", p->bpm);
                                sprintf(state.editor.stepsBuffer, "%d", p->steps);
                                
                                // Load step states
                                for (int s = 0; s < 64; ++s) state.editor.stepStates[s] = false;
                                for (int step : p->activeSteps) {
                                    if (step >= 1 && step <= 64) state.editor.stepStates[step-1] = true;
                                }
                            }
                            continue; // Don't change selection
                        }
                        
                        // --- EDIT MODE (without Shift): Open for editing AND change playback to this pattern ---
                        if (state.isLiveEditMode && !state.isShiftMode) {
                            Pattern* p = engine.getPattern(col.patternNames[i]);
                            if (p) {
                                state.editor.currentPattern = *p;
                                state.editor.isOpen = true;
                                state.editor.showFileBrowser = false;
                                strcpy(state.editor.nameBuffer, p->name.c_str());
                                strcpy(state.editor.originalName, p->name.c_str());
                                strcpy(state.editor.samplePathBuffer, p->samplePath.c_str());
                                sprintf(state.editor.bpmBuffer, "%d", p->bpm);
                                sprintf(state.editor.stepsBuffer, "%d", p->steps);
                                
                                // Load step states
                                for (int s = 0; s < 64; ++s) state.editor.stepStates[s] = false;
                                for (int step : p->activeSteps) {
                                    if (step >= 1 && step <= 64) state.editor.stepStates[step-1] = true;
                                }
                            }
                            // Continue to also change selection (fall through)
                        }
                        
                        // Logic:
                        
                        // We need to differentiate single click vs hold/drag.
                        // Standard: MouseDown starts potential hold. MouseUp confirms click if not held long.
                        // But DoubleClick needs fast successive clicks.
                        
                        // Let's use MouseDown for potential hold + selection
                        // Immediate Selection feeling:
                        // Toggle Selection Logic
                        bool wasSelected = (state.activePatternSlots.count(index) && state.activePatternSlots[index] == (int)i);
                        
                        if (wasSelected) {
                            state.activePatternSlots.erase(index);
                        } else {
                            state.activePatternSlots[index] = (int)i;
                        }
                        
                        // Sync with Audio Engine
                        if (engine.isPlaying()) {
                            std::vector<std::string> allActive;
                            for (auto& pair : state.activePatternSlots) {
                                int colIdx = pair.first;
                                int slotIdx = pair.second;
                                if (colIdx >= 0 && colIdx < (int)state.columns.size() && slotIdx >= 0 && slotIdx < (int)state.columns[colIdx].patternNames.size()) {
                                    allActive.push_back(state.columns[colIdx].patternNames[slotIdx]);
                                }
                            }
                            engine.updateActivePatterns(allActive);
                        }
                        
                        // Reset Drag state just in case, or start drag logic if needed
                        // But since we just clicked, let's treat it as selection toggle primarily.
                        // Drag logic relies on Hold, which is separate? 
                        // The current code has mixed Click/Hold logic which is tricky.
                        // For now, let's keep the Hold check but ensure Toggle happens on Click.
                        
                        state.drag.isHolding = true;
                        state.drag.holdStartTime = GetTime();
                        state.drag.initialClickPos = GetMousePosition();
                        state.drag.sourceColumnIndex = index;
                        state.drag.sourceSlotIndex = (int)i;
                        state.drag.patternName = col.patternNames[i];
                    }
                    
                    static double lastClickTime = 0;
                    static std::string lastClickName = "";
                    
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        double now = GetTime();
                        if (now - lastClickTime < 0.3 && lastClickName == col.patternNames[i]) {
                            Pattern* p = engine.getPattern(col.patternNames[i]);
                            // ... (Open Editor Logic Same as Before)
                            if (p) {
                                state.editor.currentPattern = *p;
                                state.editor.isOpen = true;
                                state.editor.showFileBrowser = false;
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
        }
    }
    
    EndScissorMode();
    
    // Draw Vertical Scrollbar if needed
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
    
    if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), addBtnRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Create new unique pattern
        Pattern p;
        p.name = "Pat" + std::to_string(state.patternIdCounter++);
        p.bpm = state.bpm;
        col.patternNames.push_back(p.name);
        
        // Register with audio engine
        engine.addPattern(p); // Empty pattern
        
        // Open Editor immediately
        state.editor.currentPattern = p;
        state.editor.isOpen = true;
        strcpy(state.editor.nameBuffer, p.name.c_str());
        strcpy(state.editor.originalName, p.name.c_str());
        sprintf(state.editor.bpmBuffer, "%d", p.bpm);
        sprintf(state.editor.stepsBuffer, "%d", p.steps);
        // Load step states
        for(int s=0; s<64; ++s) state.editor.stepStates[s] = false;
    }
}

// HandleDragAndDrop moved to gui/DragDrop.cpp

// DrawPatternEditor moved to gui/PatternEditor.cpp (1290 lines)
// The patternEditor.Draw() call replaces this 
void GuiRenderer::DrawPatternBox(const std::string& name, Rectangle bounds, bool selected) {
    Color bgColor = selected ? Color{58, 123, 213, 255} : Color{60, 60, 60, 255};
    DrawRectangleRec(bounds, bgColor);
    DrawRectangleLinesEx(bounds, 1.0f, selected ? WHITE : GRAY);
    
    DrawText(name.c_str(), bounds.x + 5, bounds.y + 5, 10, WHITE);
    
    // Draw mini grid
    Pattern* p = engine.getPattern(name);
    if (p) {
        Rectangle gridRect = {bounds.x + 5, bounds.y + 20, bounds.width - 10, bounds.height - 25};
        // Get active step for highlighting
        int activeStep = engine.getPatternProgress(name);
        DrawStepGrid(gridRect, *p, activeStep);
    }
}

void GuiRenderer::DrawStepGrid(Rectangle bounds, const Pattern& pattern, int activeStep) {
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
        if ((i + 1) == activeStep) {
            c = WHITE; // Cursor
            if (active) c = ORANGE; // Cursor on active step
        }
        
        DrawRectangleRec({x, y, size, size}, DARKGRAY); // Background slot always gray
        
        // Draw cursor for non-active steps (playback position indicator)
        if (!active && (i + 1) == activeStep) {
            DrawRectangleRec({x, y, size, size}, WHITE);
        }
        
        if (active) {
            // Check for Nudge FX to modify visual
            float offset = 0.0f;
            if (pattern.stepFX.count(i+1)) {
                 for (int fx : pattern.stepFX.at(i+1)) {
                     if (fx == Pattern::FX_NUDGE) {
                         if (pattern.stepFXParams.count(i+1) && pattern.stepFXParams.at(i+1).count(Pattern::PAR_NUDGE_OFFSET)) {
                             offset = pattern.stepFXParams.at(i+1).at(Pattern::PAR_NUDGE_OFFSET);
                         }
                     }
                 }
            }
            
            // Calculate effective shape (Bipolar)
            // 0.5 = Full (drawX=x, drawW=size)
            // > 0.5 = Trim Start (drawX shift right, width -)
            // < 0.5 = Trim End (drawX=x, width -)
            
            float drawX = x;
            float drawW = size;
            float DEFAULT_OFFSET = 0.5f;
            if (offset == 0.0f) offset = DEFAULT_OFFSET; // Handle unset/default 0 to be center
            
            if (offset > 0.5f) {
                // Right Nudge: Trim Start
                // 0.5 -> 1.0 (Full -> 0)
                float norm = (offset - 0.5f) * 2.0f; // 0..1
                drawX = x + (size * norm);
                drawW = size * (1.0f - norm);
            } else if (offset < 0.5f) {
                 // Left Nudge: Trim End
                 // 0.5 -> 0.0 (Full -> 0)
                 float norm = offset * 2.0f; // 1..0 -> This norm is Length.
                 drawW = size * norm;
            }
            
            if (drawW > 0) {
                DrawRectangleRec({drawX, y, drawW, size}, c);
            }
        }
        
        // Draw Note Pitch if active
        if (active && pattern.stepPitches.count(i+1)) {
            int shift = pattern.stepPitches.at(i+1);
            int octave = 4 + (shift / 12);
            int noteIdx = (shift % 12 + 12) % 12;
            const char* nNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            char pText[8];
            snprintf(pText, 8, "%s%d", nNames[noteIdx], octave);
            
            // Adjust text size based on cell
            int fontSize = (int)(size * 0.4f);
            if (fontSize < 10) fontSize = 10;
            DrawText(pText, x + 2, y + size/2 - fontSize/2, fontSize, BLACK);
        }
    }
}

// DrawTransportBar moved to gui/TransportBar.cpp
// The transportBar.Draw() call replaces this 188-line function
