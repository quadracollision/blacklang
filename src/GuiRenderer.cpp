#include "GuiRenderer.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <raymath.h>

namespace fs = std::filesystem;

void DrawTextInput(Rectangle rect, char* buffer, size_t maxLen, int fieldId, int& focusedId);

GuiRenderer::GuiRenderer(GuiState& s, AudioEngine& e) : state(s), engine(e) {
    font = GetFontDefault();
}
#include "tinyfiledialogs.h"

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
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{0, 0, 0, 200});
    
    // Window
    Rectangle winRect = {
        (float)GetScreenWidth()/2 - 250, 
        (float)GetScreenHeight()/2 - 275, 
        500, 550
    };
    DrawRectangleRec(winRect, Color{30, 30, 30, 255});
    DrawRectangleRec(winRect, Color{30, 30, 30, 255});
    DrawText("Edit Pattern", winRect.x + 20, winRect.y + 15, 20, WHITE);
    
    // Close Button
    Rectangle closeRect = {winRect.x + winRect.width - 30, winRect.y + 10, 20, 20};
    DrawRectangleRec(closeRect, RED);
    DrawText("X", closeRect.x + 5, closeRect.y + 2, 20, WHITE);
    
    if (CheckCollisionPointRec(GetMousePosition(), closeRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.isOpen = false;
        state.editor.showFileBrowser = false;
        state.editor.focusedFieldId = -1;
    }

    // Scroll Logic
    BeginScissorMode((int)winRect.x, (int)winRect.y + 50, (int)winRect.width, (int)winRect.height - 100); 
    
    // Handle Scroll Input (Simple Wheel)
    state.editor.scrollOffsetY -= GetMouseWheelMove() * 30.0f;
    float maxScroll = std::max(0.0f, state.editor.contentHeight - (winRect.height - 100)); // Content - Viewport
    if (state.editor.scrollOffsetY < 0) state.editor.scrollOffsetY = 0;
    if (state.editor.scrollOffsetY > maxScroll) state.editor.scrollOffsetY = maxScroll;
    
    float startY = winRect.y + 60 - state.editor.scrollOffsetY; // Base Y with Scroll
    
    // Name
    DrawText("Name:", winRect.x + 20, startY, 20, WHITE);
    DrawTextInput({winRect.x + 100, startY, 200, 30}, state.editor.nameBuffer, 63, 0, state.editor.focusedFieldId);
    
    // Sample
    startY += 40;
    DrawText("Sample:", winRect.x + 20, startY, 20, WHITE);
    // DrawTextInput({winRect.x + 100, startY, 240, 30}, state.editor.samplePathBuffer, 255, 1, state.editor.focusedFieldId);
    
    // User requested only filename display
    Rectangle sampleBox = {winRect.x + 100, startY, 240, 30};
    DrawRectangleRec(sampleBox, DARKGRAY);
    DrawRectangleLinesEx(sampleBox, 1, GRAY);
    
    std::string dispName = "";
    if (strlen(state.editor.samplePathBuffer) > 0) {
        dispName = fs::path(state.editor.samplePathBuffer).filename().string();
    }
    
    DrawText(dispName.c_str(), sampleBox.x + 5, sampleBox.y + 8, 10, WHITE);
    
    Rectangle loadBtnRect = {winRect.x + 350, startY, 50, 30};
    DrawRectangleRec(loadBtnRect, DARKGRAY);
    DrawText("Load", loadBtnRect.x + 8, loadBtnRect.y + 5, 10, WHITE);
    
    if (CheckCollisionPointRec(GetMousePosition(), loadBtnRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
         const char* filterPatterns[3] = {"*.wav", "*.mp3", "*.ogg"};
         const char* filePath = tinyfd_openFileDialog(
             "Open Sample",
             "./", // Default to current directory
             3,
             filterPatterns,
             "Audio Files",
             0 // single select
         );

         if (filePath) {
             strcpy(state.editor.samplePathBuffer, filePath);
             // Load sample immediately into editor's pattern for waveform display
             state.editor.currentPattern.samplePath = filePath;
             state.editor.currentPattern.sliceMarkers.clear(); // Clear old slices on new load
             engine.loadSample(state.editor.currentPattern);
         }
    }
    

    
    startY += 40;
    startY += 40;
    // Steps (BPM Removed)
    DrawText("Steps:", winRect.x + 20, startY, 20, WHITE);
    DrawTextInput({winRect.x + 100, startY, 60, 30}, state.editor.stepsBuffer, 4, 3, state.editor.focusedFieldId);
    
    // Velocity Knob
    float knobX = winRect.x + 280;
    float knobY = startY + 15;
    float radius = 15;
    DrawText("Vel:", knobX - 50, startY + 5, 20, WHITE);
    DrawCircle(knobX, knobY, radius, DARKGRAY);
    DrawCircleLines(knobX, knobY, radius, WHITE);
    
    // Draw Value Indicator (Line angle)
    float angle = -135.0f + (state.editor.currentVelocity * 270.0f);
    Vector2 center = {knobX, knobY};
    Vector2 end = {center.x + cosf(angle*DEG2RAD)*radius, center.y + sinf(angle*DEG2RAD)*radius};
    DrawLineEx(center, end, 2.0f, RED);
    
    // Interaction
    Rectangle knobRect = {knobX - radius, knobY - radius, radius*2, radius*2};
    
    // Start drag
    if (CheckCollisionPointRec(GetMousePosition(), knobRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.isDraggingVelocity = true;
    }
    
    // Stop drag
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        state.editor.isDraggingVelocity = false;
    }
    
    // Process Drag
    if (state.editor.isDraggingVelocity) {
         float delta = GetMouseDelta().y;
         state.editor.currentVelocity -= delta * 0.02f; // Sensitivity
         
         if (state.editor.currentVelocity < 0.0f) state.editor.currentVelocity = 0.0f;
         if (state.editor.currentVelocity > 1.0f) state.editor.currentVelocity = 1.0f;
         if (state.editor.selectedStep != -1) {
             if (state.editor.stepStates[state.editor.selectedStep]) {
                 Pattern& pat = state.editor.currentPattern;
                 pat.stepVelocities[state.editor.selectedStep + 1] = state.editor.currentVelocity;
                 engine.addPattern(pat); // SYNC
             }
         }
    }
    DrawText(TextFormat("%d%%", (int)(state.editor.currentVelocity*100)), knobX + 25, startY + 5, 10, LIGHTGRAY);
    
    // Melodic Toggle
    Rectangle melodyToggleRect = {winRect.x + 380, startY, 80, 30};
    DrawRectangleRec(melodyToggleRect, state.editor.showMelodicControls ? BLUE : DARKGRAY);
    DrawText("Melodic", melodyToggleRect.x + 8, melodyToggleRect.y + 5, 10, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), melodyToggleRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.showMelodicControls = !state.editor.showMelodicControls;
        if (state.editor.showMelodicControls) state.editor.showFxControls = false; // Exclusive
    }

    // FX Toggle (Below Melodic)
    Rectangle fxToggleRect = {winRect.x + 380, startY + 35, 80, 30}; 
    DrawRectangleRec(fxToggleRect, state.editor.showFxControls ? VIOLET : DARKGRAY);
    DrawText("FX", fxToggleRect.x + 15, fxToggleRect.y + 5, 10, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), fxToggleRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.showFxControls = !state.editor.showFxControls;
        if (state.editor.showFxControls) {
            state.editor.showMelodicControls = false;
            state.editor.showSlicerControls = false;
        }
    }
    
    // Slicer Toggle (Below FX)
    Rectangle slicerToggleRect = {winRect.x + 380, startY + 70, 80, 30};
    DrawRectangleRec(slicerToggleRect, state.editor.showSlicerControls ? GREEN : DARKGRAY);
    DrawText("Slicer", slicerToggleRect.x + 15, slicerToggleRect.y + 5, 10, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), slicerToggleRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.showSlicerControls = !state.editor.showSlicerControls;
        if (state.editor.showSlicerControls) {
            state.editor.showMelodicControls = false;
            state.editor.showFxControls = false;
        }
    }
    
    startY += 110; // Increased spacing to accommodate toggle buttons and prevent overlap 
    
    // Editor Grid (Moved ABOVE Keyboard)
    Pattern& p = state.editor.currentPattern;
    Rectangle gridRect = {winRect.x + 20, startY, winRect.width - 40, 150};
    
    int stepCount = atoi(state.editor.stepsBuffer);
    if (stepCount <= 0) stepCount = 16;
    if (stepCount > 64) stepCount = 64;
    int cols = std::min(16, stepCount);
    int rows = (stepCount + cols - 1) / cols;
    float cellW = gridRect.width / cols;
    
    // Make steps tighter and stacked vertically with slight padding
    float stepSize = cellW * 0.95f; // 5% horizontal gap
    float verticalGap = 4.0f;
    float gridHeight = rows * stepSize + (rows > 0 ? (rows - 1) * verticalGap : 0); 
    
    // Update gridRect height to match content
    gridRect.height = gridHeight;
    float cellH = stepSize; 
    
    for (int i = 0; i < stepCount; ++i) {
        int col = i % cols;
        int row = i / cols;
        float x = gridRect.x + col * cellW + (cellW - stepSize)/2;
        float y = gridRect.y + row * (stepSize + verticalGap); // Stacked with gap
        
        bool active = state.editor.stepStates[i];
        Color c = active ? RED : DARKGRAY;
        if (active && state.editor.showMelodicControls && p.stepPitches.count(i+1)) c = ORANGE;
        if (active && state.editor.showFxControls && p.stepFX.count(i+1)) {
            const auto& fxList = p.stepFX.at(i+1);
            if (!fxList.empty()) {
                // If multiple, maybe blend or show generic color?
                // Priority: CutOff (Blue) > Stutter (Gold) > Slide (SkyBlue)
                bool hasCutTime = std::find(fxList.begin(), fxList.end(), Pattern::FX_CUTOFF) != fxList.end();
                bool hasSlide = std::find(fxList.begin(), fxList.end(), Pattern::FX_SLIDE) != fxList.end();
                bool hasStutter = std::find(fxList.begin(), fxList.end(), Pattern::FX_STUTTER) != fxList.end();
                
                if (hasCutTime) c = BLUE;
                else if (hasStutter) c = GOLD;
                else if (hasSlide) c = SKYBLUE;
                else c = PURPLE; // Future FX?
            }

        }
        
        // Slicer Color Override
        if (active && state.editor.showSlicerControls) {
            if (p.stepFX.count(i+1)) {
                 const auto& fxList = p.stepFX.at(i+1);
                 if (std::find(fxList.begin(), fxList.end(), Pattern::FX_SLICE) != fxList.end()) {
                     c = GREEN;
                 }
            }
        }
        
        // Highlight Selected Step (FX or Edit Mode)
        if ((state.editor.showFxControls || state.editor.clipboard.isEditMode) && state.editor.selectedStep == i) {
            DrawRectangle(x-2, y-2, stepSize+4, stepSize+4, WHITE);
        }
        
        // Get playback cursor for live editing
        int playbackStep = engine.getPatternProgress(state.editor.currentPattern.name);
        
        // Playback Cursor Highlight (Yellow outline during playback)
        if (engine.isPlaying() && (i + 1) == playbackStep) {
            DrawRectangle(x-3, y-3, stepSize+6, stepSize+6, YELLOW);
        }

        // Draw background slot
        DrawRectangleRec({x, y, stepSize, stepSize}, DARKGRAY);
        
        // Draw active step with Nudge visual
        if (active) {
            // Check for Nudge FX to modify visual
            float offset = 0.5f; // Default center (full)
            if (p.stepFX.count(i+1)) {
                 for (int fx : p.stepFX.at(i+1)) {
                     if (fx == Pattern::FX_NUDGE) {
                         if (p.stepFXParams.count(i+1) && p.stepFXParams.at(i+1).count(Pattern::PAR_NUDGE_OFFSET)) {
                             offset = p.stepFXParams.at(i+1).at(Pattern::PAR_NUDGE_OFFSET);
                         }
                     }
                 }
            }
            
            // Calculate effective shape (Bipolar)
            // 0.5 = Full (drawX=x, drawW=stepSize)
            // > 0.5 = Trim Start (drawX shift right, width -)
            // < 0.5 = Trim End (drawX=x, width -)
            float drawX = x;
            float drawW = stepSize;
            
            if (offset == 0.0f) offset = 0.5f; // Handle unset/default 0 to be center
            
            if (offset > 0.5f) {
                // Right Nudge: Trim Start
                float norm = (offset - 0.5f) * 2.0f; // 0..1
                drawX = x + (stepSize * norm);
                drawW = stepSize * (1.0f - norm);
            } else if (offset < 0.5f) {
                 // Left Nudge: Trim End
                 float norm = offset * 2.0f; // 1..0 -> This norm is Length.
                 drawW = stepSize * norm;
            }
            
            if (drawW > 0) {
                DrawRectangleRec({drawX, y, drawW, stepSize}, c);
            }
        }
        
        Rectangle stepRect = {x, y, stepSize, stepSize};
        
        // Draw Pitch Text
        if (active && state.editor.showMelodicControls && p.stepPitches.count(i+1)) {
            int shift = p.stepPitches.at(i+1);
            int octave = 4 + (shift / 12);
            int noteIdx = (shift % 12 + 12) % 12;
            const char* nNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            char pText[8];
            snprintf(pText, 8, "%s%d", nNames[noteIdx], octave);
            
            int fontSize = (int)(stepSize * 0.4f);
            if (fontSize < 10) fontSize = 10;
            DrawText(pText, x + stepSize/2 - MeasureText(pText, fontSize)/2, y + stepSize/2 - fontSize/2, fontSize, BLACK);
        }
        
        // Draw Slice Index on step (if FX_SLICE is present)
        if (active && p.stepFX.count(i+1)) {
            const auto& fxList = p.stepFX.at(i+1);
            if (std::find(fxList.begin(), fxList.end(), Pattern::FX_SLICE) != fxList.end()) {
                if (p.stepFXParams.count(i+1) && p.stepFXParams.at(i+1).count(Pattern::PAR_SLICE_INDEX)) {
                    int sliceIdx = (int)p.stepFXParams.at(i+1).at(Pattern::PAR_SLICE_INDEX);
                    char sText[8];
                    snprintf(sText, 8, "S%d", sliceIdx);
                    int fontSize = (int)(stepSize * 0.35f);
                    if (fontSize < 8) fontSize = 8;
                    // Draw at bottom-right corner
                    DrawText(sText, x + stepSize - MeasureText(sText, fontSize) - 2, y + stepSize - fontSize - 2, fontSize, YELLOW);
                }
            }
        }
        
        bool inViewport = CheckCollisionPointRec(GetMousePosition(), {winRect.x, winRect.y+50, winRect.width, winRect.height-100});
        
        if (inViewport && CheckCollisionPointRec(GetMousePosition(), stepRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            
            // Modal Copy Logic
            if (state.editor.clipboard.isCopyMode) {
                // Copy this step
                int step = i + 1;
                state.editor.clipboard.active = state.editor.stepStates[i];
                
                if (p.stepPitches.count(step)) {
                     state.editor.clipboard.hasPitch = true;
                     state.editor.clipboard.pitch = p.stepPitches[step];
                } else state.editor.clipboard.hasPitch = false;
                
                if (p.stepVelocities.count(step)) {
                     state.editor.clipboard.hasVelocity = true;
                     state.editor.clipboard.velocity = p.stepVelocities[step];
                     // SYNC EDITOR
                     state.editor.currentVelocity = p.stepVelocities[step];
                } else {
                     state.editor.clipboard.hasVelocity = false;
                     state.editor.currentVelocity = 1.0f; // Default if none
                }
                
                state.editor.clipboard.fxList.clear();
                if (p.stepFX.count(step)) state.editor.clipboard.fxList = p.stepFX[step];
                
                state.editor.clipboard.fxParams.clear();
                if (p.stepFXParams.count(step)) state.editor.clipboard.fxParams = p.stepFXParams[step];
                
                state.editor.clipboard.hasData = true;
                state.editor.clipboard.isCopyMode = false; // Exit copy mode
                
                // USER REQUEST: Automatically enter Paste Mode
                state.editor.clipboard.isPasteMode = true; 
                
                state.editor.selectedStep = i; // Highlight source
                
                // SYNC PITCH IF EXISTS
                if (state.editor.clipboard.hasPitch) {
                     int shift = state.editor.clipboard.pitch;
                     state.editor.selectedOctave = 4 + (shift / 12);
                     state.editor.selectedNote = (shift % 12 + 12) % 12;
                }
                
                continue;
            }

            // Modal Paste Logic
            if (state.editor.clipboard.isPasteMode && state.editor.clipboard.hasData) {
                // Paste to this step
                int step = i + 1;
                
                state.editor.stepStates[i] = state.editor.clipboard.active;
                
                if (state.editor.clipboard.hasPitch) p.stepPitches[step] = state.editor.clipboard.pitch;
                else p.stepPitches.erase(step);
                
                if (state.editor.clipboard.hasVelocity) p.stepVelocities[step] = state.editor.clipboard.velocity;
                else p.stepVelocities.erase(step);
                
                if (!state.editor.clipboard.fxList.empty()) p.stepFX[step] = state.editor.clipboard.fxList;
                else p.stepFX.erase(step);
                
                if (!state.editor.clipboard.fxParams.empty()) p.stepFXParams[step] = state.editor.clipboard.fxParams;
                else p.stepFXParams.erase(step);
                
                engine.addPattern(p); // SYNC
                
                continue; // Don't toggle
            }
            
            // Edit Mode Logic (Select Only)
            if (state.editor.clipboard.isEditMode) {
                 if (state.editor.stepStates[i]) {
                     state.editor.selectedStep = i;
                     
                     // LOAD DATA INTO EDITOR
                     int step = i + 1;
                     
                     // Velocity
                     if (p.stepVelocities.count(step)) {
                         state.editor.currentVelocity = p.stepVelocities[step];
                     } else {
                         state.editor.currentVelocity = 1.0f; // Default
                     }
                     
                     // Pitch
                     if (p.stepPitches.count(step)) {
                         int shift = p.stepPitches[step];
                         state.editor.selectedOctave = 4 + (shift / 12);
                         state.editor.selectedNote = (shift % 12 + 12) % 12;
                     } // If no pitch, keep current (or default?)
                     
                 }
                 continue; // Don't toggle
            }

            // FX Mode: Select active step instead of toggling/deleting
            if (state.editor.showFxControls && state.editor.stepStates[i]) {
                if (state.editor.selectedStep != i) {
                    state.editor.selectedStep = i;
                    continue; // Just select, don't delete yet
                }
                // If already selected, proceed to delete/toggle logic below
            }

            bool wasActive = state.editor.stepStates[i];
            state.editor.stepStates[i] = !wasActive;
            
            // Apply Melodic / Velocity Data
            if (!wasActive) {
                 if (state.editor.showMelodicControls) {
                    int shift = (state.editor.selectedOctave - 4) * 12 + state.editor.selectedNote;
                    p.stepPitches[i+1] = shift;
                    
                    // In Melodic mode with slices, apply slice FX for melodic slice control
                    if (!p.sliceMarkers.empty() && state.editor.selectedSliceIndex >= 0 && 
                        state.editor.selectedSliceIndex < (int)p.sliceMarkers.size()) {
                        if (!p.stepFX.count(i+1)) p.stepFX[i+1] = std::vector<int>();
                        auto& fx = p.stepFX[i+1];
                        if (std::find(fx.begin(), fx.end(), Pattern::FX_SLICE) == fx.end()) {
                            fx.push_back(Pattern::FX_SLICE);
                        }
                        p.stepFXParams[i+1][Pattern::PAR_SLICE_INDEX] = (float)state.editor.selectedSliceIndex;
                        p.stepFXParams[i+1][Pattern::PAR_SLICE_CUTOFF] = state.editor.slicerCutoffEnabled ? 1.0f : 0.0f;
                    }
                 }
                 // Always apply velocity
                  p.stepVelocities[i+1] = state.editor.currentVelocity;
                  
                  // Apply Slicer (Slicer mode - non-melodic slice assignment)
                  if (state.editor.showSlicerControls) {
                      if (!p.stepFX.count(i+1)) p.stepFX[i+1] = std::vector<int>();
                      auto& fx = p.stepFX[i+1];
                      if (std::find(fx.begin(), fx.end(), Pattern::FX_SLICE) == fx.end()) {
                          fx.push_back(Pattern::FX_SLICE);
                      }
                      p.stepFXParams[i+1][Pattern::PAR_SLICE_INDEX] = (float)state.editor.selectedSliceIndex;
                      if (state.editor.slicerCutoffEnabled) {
                          p.stepFXParams[i+1][Pattern::PAR_SLICE_CUTOFF] = 1.0f;
                      } else {
                          p.stepFXParams[i+1][Pattern::PAR_SLICE_CUTOFF] = 0.0f;
                      }
                  }
                  
                  engine.addPattern(p); // SYNC
            } else {
                 // Removing step
                 p.stepPitches.erase(i+1);
                 p.stepVelocities.erase(i+1);
                 p.stepFX.erase(i+1);
                 
                 // If removing selected step, deselect?
                 if (state.editor.selectedStep == i) state.editor.selectedStep = -1;
                 
                 engine.addPattern(p); // SYNC
            }
            
            // USER REQUEST: Edit the last placed note.
            // If we just turned it ON, select it.
            if (!wasActive) {
                state.editor.selectedStep = i;
            }
            
            // If FX mode, select this step
            if (state.editor.showFxControls && state.editor.stepStates[i]) {
                state.editor.selectedStep = i;
                // No need to set single currentFxType
            }
        }
        
        // Right Click -> Delete / Clear Step
        if (inViewport && CheckCollisionPointRec(GetMousePosition(), stepRect) && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
             if (state.editor.stepStates[i]) {
                 state.editor.stepStates[i] = false;
                 p.stepPitches.erase(i+1);
                 p.stepVelocities.erase(i+1);
                 p.stepFX.erase(i+1);
                 
                 // If this was selected, unselect it? Or leave selection index but update type
                 if (state.editor.selectedStep == i) {
                     state.editor.selectedStep = -1;
                 }
                 engine.addPattern(p); // SYNC
             }
        }
    }
    
    startY += gridHeight + 20; // Dynamic spacing based on grid height 
    
    // Slice Selector Buttons (below grid, visible in any mode if slices exist)
    int sliceCount = (int)p.sliceMarkers.size();
    if (sliceCount > 0) {
        int buttonsPerRow = 8;
        float btnW = 35; float btnH = 25;
        
        DrawText("Slice:", winRect.x + 20, startY, 16, WHITE);
        
        float btnStartX = winRect.x + 80;
        for (int s = 0; s < sliceCount; ++s) {
            int row = s / buttonsPerRow;
            int col = s % buttonsPerRow;
            Rectangle sBtn = {btnStartX + col * (btnW + 3), startY + row * (btnH + 3), btnW, btnH};
            
            bool isSelected = (state.editor.selectedSliceIndex == s);
            DrawRectangleRec(sBtn, isSelected ? GREEN : DARKGRAY);
            DrawText(TextFormat("%d", s), sBtn.x + 12, sBtn.y + 5, 10, WHITE);
            
            if (CheckCollisionPointRec(GetMousePosition(), sBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                state.editor.selectedSliceIndex = s;
                
                // UX FIX: If a step is selected, update its slice index immediately
                if (state.editor.selectedStep != -1 && state.editor.stepStates[state.editor.selectedStep]) {
                    int step = state.editor.selectedStep + 1;
                    if (!p.stepFX.count(step)) p.stepFX[step] = std::vector<int>();
                    auto& fx = p.stepFX[step];
                    if (std::find(fx.begin(), fx.end(), Pattern::FX_SLICE) == fx.end()) {
                        fx.push_back(Pattern::FX_SLICE);
                    }
                    p.stepFXParams[step][Pattern::PAR_SLICE_INDEX] = (float)s;
                    engine.addPattern(p); // SYNC
                }
            }
        }
        startY += ((sliceCount - 1) / buttonsPerRow + 1) * (btnH + 3) + 10;
    }
    
    // Melodic Controls (NOW BELOW GRID)
    if (state.editor.showMelodicControls) {
        DrawText(TextFormat("Oct: %d", state.editor.selectedOctave), winRect.x + 20, startY + 10, 20, WHITE);
        
        Rectangle octDown = {winRect.x + 100, startY, 30, 30};
        DrawRectangleRec(octDown, DARKGRAY);
        DrawText("-", octDown.x + 10, octDown.y + 5, 20, WHITE);
        if (CheckCollisionPointRec(GetMousePosition(), octDown) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) state.editor.selectedOctave--;
        
        Rectangle octUp = {winRect.x + 140, startY, 30, 30};
        DrawRectangleRec(octUp, DARKGRAY);
        DrawText("+", octUp.x + 10, octUp.y + 5, 20, WHITE);
        if (CheckCollisionPointRec(GetMousePosition(), octUp) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) state.editor.selectedOctave++;
        
        const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        float keyX = winRect.x + 200;
        float keyY = startY; 
        float whiteKeyWidth = 25;
        float blackKeyWidth = 18;
        float keyHeight = 80;
        
        for (int i = 0; i < 12; ++i) {
            bool isBlack = (i==1 || i==3 || i==6 || i==8 || i==10);
            if (!isBlack) {
                Rectangle keyRect = {keyX, keyY, whiteKeyWidth, keyHeight};
                bool isSelected = (state.editor.selectedNote == i);
                DrawRectangleRec(keyRect, isSelected ? YELLOW : WHITE);
                DrawRectangleLinesEx(keyRect, 1, BLACK);
                if (CheckCollisionPointRec(GetMousePosition(), keyRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) state.editor.selectedNote = i;
                DrawText(notes[i], keyRect.x + 5, keyRect.y + keyHeight - 20, 10, BLACK);
                keyX += whiteKeyWidth;
            }
        }
        keyX = winRect.x + 200;
        for (int i = 0; i < 12; ++i) {
            bool isBlack = (i==1 || i==3 || i==6 || i==8 || i==10);
            if (isBlack) {
                 float xPos = 0;
                 if(i==1) xPos = whiteKeyWidth * 1 - (blackKeyWidth/2);
                 if(i==3) xPos = whiteKeyWidth * 2 - (blackKeyWidth/2);
                 if(i==6) xPos = whiteKeyWidth * 4 - (blackKeyWidth/2);
                 if(i==8) xPos = whiteKeyWidth * 5 - (blackKeyWidth/2);
                 if(i==10) xPos = whiteKeyWidth * 6 - (blackKeyWidth/2);
                 Rectangle keyRect = {winRect.x + 200 + xPos, keyY, blackKeyWidth, keyHeight * 0.6f};
                 bool isSelected = (state.editor.selectedNote == i);
                 DrawRectangleRec(keyRect, isSelected ? YELLOW : BLACK);
                 DrawRectangleLinesEx(keyRect, 1, DARKGRAY);
                 if (CheckCollisionPointRec(GetMousePosition(), keyRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) state.editor.selectedNote = i;
            }
        }
        startY += 90;
    }

    // FX Controls
    if (state.editor.showFxControls) {
        DrawText(TextFormat("Step: %d", state.editor.selectedStep + 1), winRect.x + 20, startY + 10, 20, WHITE);
        
        if (state.editor.selectedStep != -1 && state.editor.stepStates[state.editor.selectedStep]) {
            DrawText("FX Selection:", winRect.x + 20, startY + 25, 20, WHITE);
            
            // FX ListBox Logic
            struct FxOption { int id; std::string name; };
            std::vector<FxOption> availableFX = {
                {Pattern::FX_CUTOFF, "Cut Off"},
                {Pattern::FX_SLIDE, "Slide"},
                {Pattern::FX_STUTTER, "Stutter"},
                {Pattern::FX_NUDGE, "Nudge"}
            };
            
            auto& currentStepFX = p.stepFX[state.editor.selectedStep + 1]; // Get/Create vector
            
            // Define Boxes (Lowered further to startY + 70)
            float boxW = 140;
            float boxH = 100;
            Rectangle availBox = {winRect.x + 140, startY + 70, boxW, boxH};
            Rectangle appliedBox = {winRect.x + 300, startY + 70, boxW, boxH};
            
            // Draw Box Backgrounds
            DrawRectangleRec(availBox, BLACK);
            DrawRectangleLinesEx(availBox, 1, WHITE);
            DrawText("Available", availBox.x, availBox.y - 12, 10, LIGHTGRAY);
            
            DrawRectangleRec(appliedBox, BLACK);
            DrawRectangleLinesEx(appliedBox, 1, WHITE);
            DrawText("Applied", appliedBox.x, appliedBox.y - 12, 10, LIGHTGRAY);

            // Item Layout
            float availY = availBox.y + 5;
            float appliedY = appliedBox.y + 5;
            float itemH = 20;
            
            for (const auto& opt : availableFX) {
                bool isActive = std::find(currentStepFX.begin(), currentStepFX.end(), opt.id) != currentStepFX.end();
                
                Rectangle itemRect;
                bool isSelected = false;
                
                if (isActive) {
                    itemRect = {appliedBox.x + 5, appliedY, boxW - 10, itemH};
                    appliedY += itemH + 2;
                    if (state.editor.selectedAppliedFxId == opt.id) isSelected = true;
                } else {
                    itemRect = {availBox.x + 5, availY, boxW - 10, itemH};
                    availY += itemH + 2;
                    if (state.editor.selectedAvailableFxId == opt.id) isSelected = true;
                }
                
                // Draw Item Highlight
                if (isSelected) {
                    DrawRectangleRec(itemRect, ORANGE);
                } else if (CheckCollisionPointRec(GetMousePosition(), itemRect)) {
                    DrawRectangleRec(itemRect, {50, 50, 50, 255}); // Hover
                }
                
                DrawText(opt.name.c_str(), itemRect.x + 5, itemRect.y + 5, 10, isSelected ? BLACK : WHITE);
                
                // Interaction: Select on Click
                if (CheckCollisionPointRec(GetMousePosition(), itemRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    if (isActive) {
                        state.editor.selectedAppliedFxId = opt.id;
                        state.editor.selectedAvailableFxId = -1;
                    } else {
                        state.editor.selectedAvailableFxId = opt.id;
                        state.editor.selectedAppliedFxId = -1;
                    }
                }
            }
            
            // Add Button (Under Available)
            Rectangle addBtn = {availBox.x, availBox.y + boxH + 5, boxW, 25};
            DrawRectangleRec(addBtn, GRAY);
            DrawText("Add", addBtn.x + boxW/2 - 10, addBtn.y + 5, 10, WHITE);
            
            if (CheckCollisionPointRec(GetMousePosition(), addBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (state.editor.selectedAvailableFxId != -1) {
                    bool alreadyActive = std::find(currentStepFX.begin(), currentStepFX.end(), state.editor.selectedAvailableFxId) != currentStepFX.end();
                    if (!alreadyActive) {
                        currentStepFX.push_back(state.editor.selectedAvailableFxId);
                        state.editor.selectedAvailableFxId = -1; // Deselect after add
                        engine.addPattern(p); // SYNC
                    }
                }
            }

            // Remove Button (Under Applied)
            Rectangle removeBtn = {appliedBox.x, appliedBox.y + boxH + 5, boxW, 25};
            DrawRectangleRec(removeBtn, GRAY);
            DrawText("Remove", removeBtn.x + boxW/2 - 20, removeBtn.y + 5, 10, WHITE);
            
            if (CheckCollisionPointRec(GetMousePosition(), removeBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                 if (state.editor.selectedAppliedFxId != -1) {
                     currentStepFX.erase(std::remove(currentStepFX.begin(), currentStepFX.end(), state.editor.selectedAppliedFxId), currentStepFX.end());
                     state.editor.selectedAppliedFxId = -1; // Deselect after remove
                     engine.addPattern(p); // SYNC
                 }
            }
            
            // Parameter Panel
            if (state.editor.selectedAppliedFxId != -1) {
                float paramPanelY = removeBtn.y + removeBtn.height + 15;
                DrawText("FX Params:", winRect.x + 20, paramPanelY, 20, WHITE);
                
                if (state.editor.selectedAppliedFxId == Pattern::FX_STUTTER) {
                    // --- RATE CONTROL ---
                    DrawText("Rate:", winRect.x + 140, paramPanelY, 20, WHITE);
                    
                    // Value & Slider
                    float currentRate = 4.0f;
                    if (p.stepFXParams[state.editor.selectedStep + 1].count(Pattern::PAR_STUTTER_RATE)) {
                        currentRate = p.stepFXParams[state.editor.selectedStep + 1][Pattern::PAR_STUTTER_RATE];
                    }
                    
                    // Slider Area
                    Rectangle rateSlider = {winRect.x + 250, paramPanelY + 5, 150, 10};
                    DrawRectangleRec(rateSlider, DARKGRAY);
                    DrawRectangleLinesEx(rateSlider, 1, WHITE);
                    
                    // Handle Position
                    float minRate = 1.0f; float maxRate = 16.0f;
                    float rateNorm = (currentRate - minRate) / (maxRate - minRate);
                    if (rateNorm < 0) rateNorm = 0; if (rateNorm > 1) rateNorm = 1;
                    Rectangle rateHandle = {rateSlider.x + rateNorm * (rateSlider.width - 10), rateSlider.y - 2, 10, 14};
                    DrawRectangleRec(rateHandle, LIGHTGRAY);
                    
                    // Slider Interaction
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                        Vector2 mouse = GetMousePosition();
                        if (CheckCollisionPointRec(mouse, {rateSlider.x - 5, rateSlider.y - 5, rateSlider.width + 10, rateSlider.height + 10})) {
                             float newVal = minRate + ((mouse.x - rateSlider.x) / rateSlider.width) * (maxRate - minRate);
                             if (newVal < minRate) newVal = minRate;
                             if (newVal > maxRate) newVal = maxRate;
                             currentRate = newVal;
                             p.stepFXParams[state.editor.selectedStep + 1][Pattern::PAR_STUTTER_RATE] = currentRate;
                        }
                    }
                    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                        engine.addPattern(p); // SYNC on release
                    }
                    
                    // Rate Labels/Readout
                    DrawText(TextFormat("%.1f", currentRate), rateSlider.x + rateSlider.width + 10, paramPanelY, 10, WHITE);

                    // --- SPEED CONTROL ---
                    paramPanelY += 35;
                    DrawText("Speed:", winRect.x + 140, paramPanelY, 20, WHITE);
                    
                    // Value & Slider
                    float currentSpeed = 1.0f;
                    if (p.stepFXParams[state.editor.selectedStep + 1].count(Pattern::PAR_STUTTER_SPEED)) {
                        currentSpeed = p.stepFXParams[state.editor.selectedStep + 1][Pattern::PAR_STUTTER_SPEED];
                    }
                    
                    // Slider Area
                    Rectangle speedSlider = {winRect.x + 250, paramPanelY + 5, 150, 10};
                    DrawRectangleRec(speedSlider, DARKGRAY);
                    DrawRectangleLinesEx(speedSlider, 1, WHITE);
                    
                    // Handle Position
                    float minSpeed = 0.5f; float maxSpeed = 4.0f;
                    float speedNorm = (currentSpeed - minSpeed) / (maxSpeed - minSpeed);
                    if (speedNorm < 0) speedNorm = 0; if (speedNorm > 1) speedNorm = 1;
                    Rectangle speedHandle = {speedSlider.x + speedNorm * (speedSlider.width - 10), speedSlider.y - 2, 10, 14};
                    DrawRectangleRec(speedHandle, LIGHTGRAY);
                    
                    // Slider Interaction
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                        Vector2 mouse = GetMousePosition();
                        if (CheckCollisionPointRec(mouse, {speedSlider.x - 5, speedSlider.y - 5, speedSlider.width + 10, speedSlider.height + 10})) {
                             float newVal = minSpeed + ((mouse.x - speedSlider.x) / speedSlider.width) * (maxSpeed - minSpeed);
                             if (newVal < minSpeed) newVal = minSpeed;
                             if (newVal > maxSpeed) newVal = maxSpeed;
                             currentSpeed = newVal;
                             p.stepFXParams[state.editor.selectedStep + 1][Pattern::PAR_STUTTER_SPEED] = currentSpeed;
                        }
                    }
                    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                        engine.addPattern(p); // SYNC on release
                    }
                                        // Speed Readout
                    DrawText(TextFormat("%.2f", currentSpeed), speedSlider.x + speedSlider.width + 10, paramPanelY, 10, WHITE);
                } else if (state.editor.selectedAppliedFxId == Pattern::FX_SLIDE) {
                    // --- TIME CONTROL ---
                    DrawText("Time:", winRect.x + 140, paramPanelY, 20, WHITE);
                    
                    // Value & Slider
                    float currentTime = 1.0f;
                    if (p.stepFXParams[state.editor.selectedStep + 1].count(Pattern::PAR_SLIDE_TIME)) {
                        currentTime = p.stepFXParams[state.editor.selectedStep + 1][Pattern::PAR_SLIDE_TIME];
                    }
                    
                    // Slider Area
                    Rectangle timeSlider = {winRect.x + 250, paramPanelY + 5, 150, 10};
                    DrawRectangleRec(timeSlider, DARKGRAY);
                    DrawRectangleLinesEx(timeSlider, 1, WHITE);
                    
                    // Handle Position
                    float minTime = 0.1f; float maxTime = 1.0f;
                    float timeNorm = (currentTime - minTime) / (maxTime - minTime);
                    if (timeNorm < 0) timeNorm = 0; if (timeNorm > 1) timeNorm = 1;
                    Rectangle timeHandle = {timeSlider.x + timeNorm * (timeSlider.width - 10), timeSlider.y - 2, 10, 14};
                    DrawRectangleRec(timeHandle, LIGHTGRAY);
                    
                    // Slider Interaction
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                        Vector2 mouse = GetMousePosition();
                        if (CheckCollisionPointRec(mouse, {timeSlider.x - 5, timeSlider.y - 5, timeSlider.width + 10, timeSlider.height + 10})) {
                             float newVal = minTime + ((mouse.x - timeSlider.x) / timeSlider.width) * (maxTime - minTime);
                             if (newVal < minTime) newVal = minTime;
                             if (newVal > maxTime) newVal = maxTime;
                             currentTime = newVal;
                             p.stepFXParams[state.editor.selectedStep + 1][Pattern::PAR_SLIDE_TIME] = currentTime;
                        }
                    }
                    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                        engine.addPattern(p); // SYNC on release
                    }
                    
                    // Readout
                    DrawText(TextFormat("%.2f", currentTime), timeSlider.x + timeSlider.width + 10, paramPanelY, 10, WHITE);

                    // --- SQUELCH CONTROL ---
                    paramPanelY += 35;
                    DrawText("Squelch:", winRect.x + 140, paramPanelY, 20, WHITE);
                    
                    // Value & Slider
                    float currentSquelch = 0.0f;
                    if (p.stepFXParams[state.editor.selectedStep + 1].count(Pattern::PAR_SLIDE_SQUELCH)) {
                        currentSquelch = p.stepFXParams[state.editor.selectedStep + 1][Pattern::PAR_SLIDE_SQUELCH];
                    }
                    
                    // Slider Area
                    Rectangle squelchSlider = {winRect.x + 250, paramPanelY + 5, 150, 10};
                    DrawRectangleRec(squelchSlider, DARKGRAY);
                    DrawRectangleLinesEx(squelchSlider, 1, WHITE);
                    
                    // Handle Position
                    float minSquelch = 0.0f; float maxSquelch = 1.0f;
                    float squelchNorm = (currentSquelch - minSquelch) / (maxSquelch - minSquelch);
                    if (squelchNorm < 0) squelchNorm = 0; if (squelchNorm > 1) squelchNorm = 1;
                    Rectangle squelchHandle = {squelchSlider.x + squelchNorm * (squelchSlider.width - 10), squelchSlider.y - 2, 10, 14};
                    DrawRectangleRec(squelchHandle, LIGHTGRAY);
                    
                    // Slider Interaction
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                        Vector2 mouse = GetMousePosition();
                        if (CheckCollisionPointRec(mouse, {squelchSlider.x - 5, squelchSlider.y - 5, squelchSlider.width + 10, squelchSlider.height + 10})) {
                             float newVal = minSquelch + ((mouse.x - squelchSlider.x) / squelchSlider.width) * (maxSquelch - minSquelch);
                             if (newVal < minSquelch) newVal = minSquelch;
                             if (newVal > maxSquelch) newVal = maxSquelch;
                             currentSquelch = newVal;
                             p.stepFXParams[state.editor.selectedStep + 1][Pattern::PAR_SLIDE_SQUELCH] = currentSquelch;
                        }
                    }
                    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                        engine.addPattern(p); // SYNC on release
                    }
                    
                    // Readout
                    DrawText(TextFormat("%.2f", currentSquelch), squelchSlider.x + squelchSlider.width + 10, paramPanelY, 10, WHITE);
                
                } else if (state.editor.selectedAppliedFxId == Pattern::FX_NUDGE) { // NUDGE
                    DrawText("Nudge (Start/End):", winRect.x + 190, paramPanelY + 5, 10, WHITE);
                    
                    // Value & Slider
                    float currentOffset = 0.5f; // Center by default
                    if (p.stepFXParams[state.editor.selectedStep + 1].count(Pattern::PAR_NUDGE_OFFSET)) {
                        currentOffset = p.stepFXParams[state.editor.selectedStep + 1][Pattern::PAR_NUDGE_OFFSET];
                    }
                    
                    // Visual Indicator bar (Center = Full)
                    Rectangle offsetSlider = {winRect.x + 300, paramPanelY + 5, 150, 10};
                    DrawRectangleRec(offsetSlider, DARKGRAY);
                    DrawRectangleLinesEx(offsetSlider, 1, WHITE);
                    
                    // Center Tick
                    DrawRectangle(offsetSlider.x + offsetSlider.width/2 - 1, offsetSlider.y - 2, 2, 14, GRAY);
                    
                    // Visual Fill
                    if (currentOffset > 0.5f) {
                        // Right Side fill? Or "Gap" representation?
                        // User said: "slider bar should start in the middle and it goes left and right"
                    }
                    
                    // Handle
                    float minOff = 0.0f; float maxOff = 1.0f;
                    float offNorm = (currentOffset - minOff) / (maxOff - minOff);
                    if (offNorm < 0) offNorm = 0; if (offNorm > 1) offNorm = 1;
                    Rectangle offHandle = {offsetSlider.x + offNorm * (offsetSlider.width - 2), offsetSlider.y, 2, 10};
                    DrawRectangleRec(offHandle, ORANGE); 

                    // Slider Interaction
                    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                        Vector2 mouse = GetMousePosition();
                        if (CheckCollisionPointRec(mouse, {offsetSlider.x - 5, offsetSlider.y - 5, offsetSlider.width + 10, offsetSlider.height + 10})) {
                             float newVal = minOff + ((mouse.x - offsetSlider.x) / offsetSlider.width) * (maxOff - minOff);
                             if (newVal < minOff) newVal = minOff;
                             if (newVal > maxOff) newVal = maxOff;
                             currentOffset = newVal;
                             p.stepFXParams[state.editor.selectedStep + 1][Pattern::PAR_NUDGE_OFFSET] = currentOffset;
                        }
                    }
                    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                        engine.addPattern(p); // SYNC on release
                    }
                    
                    // Text Description based on side
                    if (currentOffset > 0.55f) DrawText(TextFormat("Start +%.0f%%", (currentOffset-0.5f)*200), offsetSlider.x + offsetSlider.width + 10, paramPanelY, 10, WHITE);
                    else if (currentOffset < 0.45f) DrawText(TextFormat("Len %.0f%%", currentOffset*200), offsetSlider.x + offsetSlider.width + 10, paramPanelY, 10, WHITE);
                    else DrawText("Full", offsetSlider.x + offsetSlider.width + 10, paramPanelY, 10, WHITE);
                
                } else {
                     DrawText("No params", winRect.x + 140, paramPanelY, 20, GRAY);
                }
            }
            
            // Cleanup empty entries map?
            if (currentStepFX.empty()) {
                p.stepFX.erase(state.editor.selectedStep + 1);
                engine.addPattern(p); // SYNC
            }
            
            startY += boxH + 110; // Adjust layout spacing (less space needed without buttons)
            
        } else {
             DrawText("Select an active step to edit FX", winRect.x + 20, startY + 25, 20, GRAY);
             startY += 40;
        }
        
        // Remove extra spacing added before
        // startY += 80; <-- Removed from previous logic
        startY += 10;
        startY += 50;
    }

    // Slicer Controls
    if (state.editor.showSlicerControls) {
        DrawText("Sample Slicer", winRect.x + 20, startY + 10, 20, WHITE);
        startY += 40;

        // Waveform Viewer
        Rectangle waveRect = {winRect.x + 20, startY, winRect.width - 40, 100};
        DrawRectangleRec(waveRect, BLACK);
        DrawRectangleLinesEx(waveRect, 1, GRAY);
        
        // Use editor's pattern buffer (loaded when Load button is clicked)
        if (p.sampleBuffer.getNumSamples() > 0) {
            int numSamples = p.sampleBuffer.getNumSamples();
            const float* data = p.sampleBuffer.getReadPointer(0);
            
            // Calculate visible sample range based on zoom and scroll
            float zoom = state.editor.waveformZoom;
            float viewWidth = 1.0f / zoom; // Fraction of total visible
            float scrollMax = 1.0f - viewWidth;
            if (scrollMax < 0) scrollMax = 0;
            state.editor.waveformScrollX = std::min(std::max(state.editor.waveformScrollX, 0.0f), scrollMax);
            
            int startSample = (int)(state.editor.waveformScrollX * numSamples);
            int endSample = (int)((state.editor.waveformScrollX + viewWidth) * numSamples);
            if (endSample > numSamples) endSample = numSamples;
            int visibleSamples = endSample - startSample;
            
            float midY = waveRect.y + waveRect.height / 2;
            float halfH = waveRect.height / 2.0f;
            
            // Draw Waveform (visible portion only)
            for (int x = 0; x < (int)waveRect.width; ++x) {
                float minVal = 0.0f;
                float maxVal = 0.0f;
                
                int sIdx = startSample + (int)((float)x / waveRect.width * visibleSamples);
                int eIdx = startSample + (int)((float)(x+1) / waveRect.width * visibleSamples);
                if (eIdx > endSample) eIdx = endSample;
                
                int step = std::max(1, (eIdx - sIdx) / 4);
                for (int s = sIdx; s < eIdx; s += step) {
                    if (s >= 0 && s < numSamples) {
                        float val = data[s];
                        if (val < minVal) minVal = val;
                        if (val > maxVal) maxVal = val;
                    }
                }
                
                DrawLine(waveRect.x + x, midY + minVal * halfH, waveRect.x + x, midY + maxVal * halfH, DARKGREEN);
            }
            
            // Draw Markers (only visible ones)
            for (int mFn = 0; mFn < (int)p.sliceMarkers.size(); ++mFn) {
                int sampleIdx = p.sliceMarkers[mFn];
                if (sampleIdx >= startSample && sampleIdx <= endSample) {
                    float xPos = (float)(sampleIdx - startSample) / visibleSamples * waveRect.width;
                    DrawLine(waveRect.x + xPos, waveRect.y, waveRect.x + xPos, waveRect.y + waveRect.height, RED);
                    DrawText(TextFormat("%d", mFn), waveRect.x + xPos + 2, waveRect.y + 2, 10, YELLOW);
                    
                    // Handle Delete (Right Click near marker)
                    if (CheckCollisionPointRec(GetMousePosition(), {waveRect.x + xPos - 5, waveRect.y, 10, waveRect.height}) && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                        p.sliceMarkers.erase(p.sliceMarkers.begin() + mFn);
                        mFn--; 
                        engine.addPattern(p); // SYNC 
                    }
                }
            }
            
            // Interaction: Left Click to Add Marker
            if (CheckCollisionPointRec(GetMousePosition(), waveRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                float localX = GetMousePosition().x - waveRect.x;
                int sampleIdx = startSample + (int)(localX / waveRect.width * visibleSamples);
                
                // Add and Sort
                p.sliceMarkers.push_back(sampleIdx);
                std::sort(p.sliceMarkers.begin(), p.sliceMarkers.end());
                engine.addPattern(p); // SYNC
            }
            
            // Horizontal Scrollbar (only if zoomed)
            if (zoom > 1.0f) {
                Rectangle scrollBarBg = {waveRect.x, waveRect.y + waveRect.height + 2, waveRect.width, 12};
                DrawRectangleRec(scrollBarBg, DARKGRAY);
                
                float thumbWidth = scrollBarBg.width / zoom;
                float thumbX = scrollBarBg.x + state.editor.waveformScrollX / (1.0f - viewWidth + 0.001f) * (scrollBarBg.width - thumbWidth);
                Rectangle scrollThumb = {thumbX, scrollBarBg.y + 2, thumbWidth, 8};
                DrawRectangleRec(scrollThumb, LIGHTGRAY);
                
                // Drag scrollbar
                static bool isDraggingScroll = false;
                static float dragStartX = 0;
                static float dragStartScroll = 0;
                
                if (CheckCollisionPointRec(GetMousePosition(), scrollBarBg) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    isDraggingScroll = true;
                    dragStartX = GetMousePosition().x;
                    dragStartScroll = state.editor.waveformScrollX;
                }
                if (isDraggingScroll && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                    float deltaX = GetMousePosition().x - dragStartX;
                    float deltaScroll = deltaX / (scrollBarBg.width - thumbWidth) * scrollMax;
                    state.editor.waveformScrollX = std::min(std::max(dragStartScroll + deltaScroll, 0.0f), scrollMax);
                }
                if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                    isDraggingScroll = false;
                }
            }
            
        } else {
            DrawText("No Sample Loaded", waveRect.x + 10, waveRect.y + 40, 20, DARKGRAY);
        }
        
        startY += (state.editor.waveformZoom > 1.0f) ? 125 : 110;
        
        // Controls: Clear Markers
        Rectangle clearBtn = {winRect.x + 20, startY, 100, 30};
        DrawRectangleRec(clearBtn, DARKGRAY);
        DrawText("Clear Slices", clearBtn.x + 10, clearBtn.y + 5, 10, WHITE);
        if (CheckCollisionPointRec(GetMousePosition(), clearBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            p.sliceMarkers.clear();
            engine.addPattern(p); // SYNC
        }
        
        // Cutoff Toggle
        Rectangle cutBtn = {winRect.x + 140, startY, 20, 20};
        DrawRectangleRec(cutBtn, state.editor.slicerCutoffEnabled ? GREEN : DARKGRAY);
        DrawText("Cut", cutBtn.x + 25, cutBtn.y + 2, 16, WHITE);
        if (CheckCollisionPointRec(GetMousePosition(), cutBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.editor.slicerCutoffEnabled = !state.editor.slicerCutoffEnabled;
        }
        
        // Zoom buttons (to the right of Cut)
        Rectangle zoomOutBtn = {winRect.x + 220, startY, 25, 25};
        Rectangle zoomInBtn = {winRect.x + 250, startY, 25, 25};
        DrawRectangleRec(zoomOutBtn, DARKGRAY);
        DrawRectangleRec(zoomInBtn, DARKGRAY);
        DrawText("-", zoomOutBtn.x + 9, zoomOutBtn.y + 4, 16, WHITE);
        DrawText("+", zoomInBtn.x + 8, zoomInBtn.y + 4, 16, WHITE);
        
        if (CheckCollisionPointRec(GetMousePosition(), zoomInBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.editor.waveformZoom = std::min(state.editor.waveformZoom * 1.5f, 20.0f);
        }
        if (CheckCollisionPointRec(GetMousePosition(), zoomOutBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.editor.waveformZoom = std::max(state.editor.waveformZoom / 1.5f, 1.0f);
            if (state.editor.waveformZoom <= 1.0f) state.editor.waveformScrollX = 0.0f;
        }
        
        startY += 40;
        
        // Add "Add Start Marker" button if 0 missing
        if (p.sliceMarkers.empty() || p.sliceMarkers[0] != 0) {
             Rectangle startMarkerBtn = {winRect.x + 20, startY, 120, 25};
             DrawRectangleRec(startMarkerBtn, GRAY);
             DrawText("Add Start Marker", startMarkerBtn.x + 5, startMarkerBtn.y + 5, 10, WHITE);
             if (CheckCollisionPointRec(GetMousePosition(), startMarkerBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                 p.sliceMarkers.insert(p.sliceMarkers.begin(), 0);
             }
             startY += 35;
        }

        startY += 10;
    }
    
    // Update Content Height
    state.editor.contentHeight = (startY + 50) - (winRect.y + 60) + state.editor.scrollOffsetY; 
    


    EndScissorMode();
    
    // Draw Scrollbar (Right side)
    if (state.editor.contentHeight > (winRect.height - 100)) {
        float viewH = winRect.height - 100;
        float ratio = viewH / state.editor.contentHeight;
        float barH = std::max(20.0f, viewH * ratio);
        float barY = (winRect.y + 50) + (state.editor.scrollOffsetY / (state.editor.contentHeight - viewH)) * (viewH - barH);
        
        if (state.editor.contentHeight - viewH <= 0.1f) barY = winRect.y + 50; // Safety
        
        DrawRectangle(winRect.x + winRect.width - 10, barY, 6, barH, Color{100, 100, 100, 200});
    }
    
    // --- FOOTER CONTROLS ---
    DrawRectangle(winRect.x, winRect.y + winRect.height - 50, winRect.width, 50, Color{30, 30, 30, 255}); // Footer BG

    // Copy Button (Left)
    Rectangle copyRect = {winRect.x + 20, winRect.y + winRect.height - 40, 60, 30};
    bool copyActive = state.editor.clipboard.isCopyMode;
    DrawRectangleRec(copyRect, copyActive ? ORANGE : DARKGRAY); // Orange when waiting for copy
    DrawText("Copy", copyRect.x + 10, copyRect.y + 5, 20, WHITE);
    
    if (CheckCollisionPointRec(GetMousePosition(), copyRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.clipboard.isCopyMode = !state.editor.clipboard.isCopyMode;
        state.editor.clipboard.isPasteMode = false;
        state.editor.clipboard.isEditMode = false;
    }

    // Paste Button (Left, next to Copy)
    Rectangle pasteRect = {winRect.x + 90, winRect.y + winRect.height - 40, 80, 30};
    bool canPaste = state.editor.clipboard.hasData;
    bool pasteActive = state.editor.clipboard.isPasteMode;
    
    // User requested "turn back to gray" if deactivated. 
    // We will use DARKGRAY for inactive, MAGENTA for Active. 
    // We rely on 'canPaste' only for clickability check, not color (to keep it gray).
    Color pasteColor = pasteActive ? MAGENTA : DARKGRAY; 
    
    DrawRectangleRec(pasteRect, pasteColor);
    DrawText("Paste", pasteRect.x + 5, pasteRect.y + 5, 20, WHITE);
    
    if (canPaste && CheckCollisionPointRec(GetMousePosition(), pasteRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.clipboard.isPasteMode = !state.editor.clipboard.isPasteMode;
        state.editor.clipboard.isCopyMode = false;
        state.editor.clipboard.isEditMode = false;
    }

    // Edit Button (Use Cyan for distinction)
    Rectangle editRect = {winRect.x + 180, winRect.y + winRect.height - 40, 60, 30};
    bool editActive = state.editor.clipboard.isEditMode;
    DrawRectangleRec(editRect, editActive ? SKYBLUE : DARKGRAY);
    DrawText("Edit", editRect.x + 10, editRect.y + 5, 20, WHITE);
    
    if (CheckCollisionPointRec(GetMousePosition(), editRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.clipboard.isEditMode = !state.editor.clipboard.isEditMode;
        state.editor.clipboard.isCopyMode = false;
        state.editor.clipboard.isPasteMode = false;
    }

    // Save Button (Fixed at Bottom Footer)
    Rectangle saveRect = {winRect.x + winRect.width - 100, winRect.y + winRect.height - 40, 80, 30};
    
    // Play Button (only visible in Shift mode during playback)
    if (state.isShiftMode && state.isPlaying) {
        Rectangle playBtn = {saveRect.x - 90, saveRect.y, 80, 30};
        DrawRectangleRec(playBtn, GREEN);
        DrawText("Play", playBtn.x + 18, playBtn.y + 5, 20, BLACK);
        
        if (CheckCollisionPointRec(GetMousePosition(), playBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            // --- SAVE THE PATTERN FIRST (same as Save button) ---
            p.name = state.editor.nameBuffer;
            
            std::string sPath = state.editor.samplePathBuffer;
            if (!sPath.empty() && !fs::exists(sPath)) {
                 std::vector<std::string> prefixes = {"../", "src/", "../src/", "samples/", "../samples/"};
                 for (const auto& pre : prefixes) {
                     if (fs::exists(pre + sPath)) {
                         sPath = pre + sPath;
                         break;
                     }
                 }
            }
            
            p.samplePath = sPath;
            p.bpm = state.bpm;
            p.steps = atoi(state.editor.stepsBuffer);
            
            p.activeSteps.clear();
            for (int i=0; i<64; ++i) {
                if (state.editor.stepStates[i]) {
                    p.activeSteps.push_back(i+1);
                }
            }
            
            if (p.samplePath != "") engine.loadSample(p);
            engine.addPattern(p);
            
            // --- NOW SELECT FOR PLAYBACK ---
            std::string patName = p.name;
            for (int colIdx = 0; colIdx < (int)state.columns.size(); ++colIdx) {
                for (int slotIdx = 0; slotIdx < (int)state.columns[colIdx].patternNames.size(); ++slotIdx) {
                    if (state.columns[colIdx].patternNames[slotIdx] == patName) {
                        state.activePatternSlots[colIdx] = slotIdx;
                        break;
                    }
                }
            }
            
            // Update engine with new active patterns
            std::vector<std::string> allActive;
            for (auto& pair : state.activePatternSlots) {
                int c = pair.first;
                int s = pair.second;
                if (c >= 0 && c < (int)state.columns.size() && s >= 0 && s < (int)state.columns[c].patternNames.size()) {
                    allActive.push_back(state.columns[c].patternNames[s]);
                }
            }
            engine.updateActivePatterns(allActive);
            
            // Turn off Shift mode
            state.isShiftMode = false;
            state.shiftEditingPatternName = "";
        }
    }
    
    // Footer BG already drawn above
    // DrawRectangle(winRect.x, winRect.y + winRect.height - 50, winRect.width, 50, Color{30, 30, 30, 255}); // Footer BG
    DrawRectangleRec(saveRect, BLUE);
    DrawText("Save", saveRect.x + 20, saveRect.y + 5, 20, WHITE);
    
    if (CheckCollisionPointRec(GetMousePosition(), saveRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Save back to pattern
        p.name = state.editor.nameBuffer;
        
        // Smart Load Logic
        std::string sPath = state.editor.samplePathBuffer;
        if (!sPath.empty() && !fs::exists(sPath)) {
             // Check common paths
             std::vector<std::string> prefixes = {"../", "src/", "../src/", "samples/", "../samples/"};
             for (const auto& pre : prefixes) {
                 if (fs::exists(pre + sPath)) {
                     sPath = pre + sPath;
                     break;
                 }
             }
        }
        
        p.samplePath = sPath;
        p.samplePath = sPath;
        p.bpm = state.bpm; // Inherit global BPM
        p.steps = atoi(state.editor.stepsBuffer);
        
        p.activeSteps.clear();
        for (int i=0; i<64; ++i) {
            if (state.editor.stepStates[i]) {
                p.activeSteps.push_back(i+1);
            }
        }
        
        // Register new pattern data
        // Correct order: Load Sample into 'p' THEN add to engine so buffer is copied
        if (p.samplePath != "") engine.loadSample(p);
        engine.addPattern(p);
        
        // Rename in columns if name changed
        std::string oldName = state.editor.originalName;
        if (p.name != oldName) {
            // Update all columns
            for (auto& col : state.columns) {
                std::replace(col.patternNames.begin(), col.patternNames.end(), oldName, p.name);
            }
            // Selection uses slot index now, no need to update names
        }
        
        // Refresh renderer - only close if NOT playing (live edit stays open)
        if (!engine.isPlaying()) {
            state.editor.isOpen = false;
        }
        state.editor.showFileBrowser = false;
        state.editor.focusedFieldId = -1;
    }
    
    // Draw Outline Last (to cover footer overlap)
    DrawRectangleLinesEx(winRect, 2, Color{200, 200, 200, 255});
}

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

void GuiRenderer::DrawTransportBar() {
    Rectangle rect = {0, (float)GetScreenHeight() - state.FOOTER_HEIGHT, (float)GetScreenWidth(), (float)state.FOOTER_HEIGHT};
    DrawRectangleRec(rect, Color{25, 25, 25, 255});
    
    float centerX = GetScreenWidth() / 2.0f;
    
    // Play
    Rectangle playRect = {centerX - 60, rect.y + 10, 40, 40};
    DrawRectangleRec(playRect, state.isPlaying ? GREEN : GRAY);
    DrawText(">", playRect.x + 15, playRect.y + 10, 20, BLACK);
    if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), playRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        std::vector<std::string> names;
        
        if (!state.activePatternSlots.empty()) {
            for (const auto& pair : state.activePatternSlots) {
                int colIdx = pair.first;
                int slotIdx = pair.second;
                if (colIdx >= 0 && colIdx < (int)state.columns.size() && slotIdx >= 0 && slotIdx < (int)state.columns[colIdx].patternNames.size()) {
                    std::string pName = state.columns[colIdx].patternNames[slotIdx];
                    names.push_back(pName);
                }
            }
        } else {
            // Fallback: Play ALL patterns (Song Mode)
            for (const auto& col : state.columns) {
                for (const auto& pName : col.patternNames) {
                    names.push_back(pName);
                }
            }
        }

        // Ensure patterns exist in engine
        for (const auto& pName : names) {
            if (engine.getPattern(pName) == nullptr) {
                Pattern p;
                p.name = pName;
                p.bpm = state.bpm;
                engine.addPattern(p);
            }
        }
        
        if (!names.empty()) engine.playMultiplePatterns(names);
    }
    
    // Stop
    Rectangle stopRect = {centerX, rect.y + 10, 40, 40};
    DrawRectangleRec(stopRect, RED);
    DrawRectangle(stopRect.x + 10, stopRect.y + 10, 20, 20, WHITE);
    if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), stopRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        engine.stop();
    }
    
    // BPM
    DrawText("BPM:", centerX + 100, rect.y + 10, 20, LIGHTGRAY);
    if (!state.editor.isOpen) {
        DrawTextInput({centerX + 150, rect.y + 10, 60, 25}, state.globalBpmBuffer, 5, 999, state.focusedFieldId);
    } else {
        DrawRectangleRec({centerX + 150, rect.y + 10, 60, 25}, LIGHTGRAY);
        DrawText(state.globalBpmBuffer, centerX + 155, rect.y + 15, 20, BLACK);
    }

    int newBpm = atoi(state.globalBpmBuffer);
    if (newBpm > 20 && newBpm < 300 && newBpm != state.bpm) {
        state.bpm = newBpm;
        engine.setBPM(newBpm);
    }
    
    // ---- TRACK COPY/PASTE BUTTONS (Left Side) ----
    // Copy Button
    Rectangle copyBtn = {20, rect.y + 15, 50, 30};
    DrawRectangleRec(copyBtn, state.trackClipboard.isCopyMode ? ORANGE : DARKGRAY);
    DrawText("Copy", copyBtn.x + 5, copyBtn.y + 8, 14, WHITE);
    
    if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), copyBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.trackClipboard.isCopyMode = !state.trackClipboard.isCopyMode;
        state.trackClipboard.isPasteMode = false;
    }
    
    // Paste Button
    Rectangle pasteBtn = {80, rect.y + 15, 55, 30};
    bool canPaste = state.trackClipboard.hasData;
    DrawRectangleRec(pasteBtn, state.trackClipboard.isPasteMode ? MAGENTA : (canPaste ? GRAY : DARKGRAY));
    DrawText("Paste", pasteBtn.x + 5, pasteBtn.y + 8, 14, WHITE);
    
    if (!state.editor.isOpen && canPaste && CheckCollisionPointRec(GetMousePosition(), pasteBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.trackClipboard.isPasteMode = !state.trackClipboard.isPasteMode;
        state.trackClipboard.isCopyMode = false;
    }
    
    // Edit Button (Only visible when playing and a pattern is selected)
    if (state.isPlaying && !state.activePatternSlots.empty()) {
        Rectangle editBtn = {145, rect.y + 15, 45, 30};
        DrawRectangleRec(editBtn, state.isLiveEditMode ? SKYBLUE : DARKGRAY);
        DrawText("Edit", editBtn.x + 5, editBtn.y + 8, 14, WHITE);
        
        if (CheckCollisionPointRec(GetMousePosition(), editBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            // Toggle Live Edit Mode (user must click a pattern to edit it)
            state.isLiveEditMode = !state.isLiveEditMode;
            
            if (!state.isLiveEditMode) {
                // Turn off Live Edit Mode also closes editor and resets Shift
                state.editor.isOpen = false;
                state.isShiftMode = false;
                state.shiftEditingPatternName = "";
            }
        }
        
        // Shift Button (only visible when Live Edit Mode is ON)
        if (state.isLiveEditMode) {
            Rectangle shiftBtn = {195, rect.y + 15, 45, 30};
            DrawRectangleRec(shiftBtn, state.isShiftMode ? ORANGE : DARKGRAY);
            DrawText("Shift", shiftBtn.x + 3, shiftBtn.y + 8, 12, WHITE);
            
            if (CheckCollisionPointRec(GetMousePosition(), shiftBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                state.isShiftMode = !state.isShiftMode;
                if (!state.isShiftMode) {
                    state.shiftEditingPatternName = ""; // Clear when turning off
                }
            }
        }
    }
    
    // ---- SETTINGS GEAR BUTTON (Right Side) ----
    float gearX = GetScreenWidth() - 50;
    Rectangle gearBtn = {gearX, rect.y + 15, 30, 30};
    DrawRectangleRec(gearBtn, state.settings.showSettingsMenu ? ORANGE : DARKGRAY);
    // Draw simple gear icon (asterisk-like)
    DrawText("*", gearBtn.x + 8, gearBtn.y + 3, 24, WHITE);
    
    if (CheckCollisionPointRec(GetMousePosition(), gearBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.settings.showSettingsMenu = !state.settings.showSettingsMenu;
        if (state.settings.showSettingsMenu) {
            // Refresh device list when opening
            state.settings.availableOutputDevices = engine.getAvailableOutputDevices();
            state.settings.currentDevice = engine.getCurrentOutputDevice();
        }
    }
    
    // Settings Popup Overlay
    if (state.settings.showSettingsMenu) {
        float popW = 350;
        float popH = 200;
        float popX = GetScreenWidth() - popW - 20;
        float popY = rect.y - popH - 10;
        
        // Background
        DrawRectangle(popX, popY, popW, popH, Color{40, 40, 40, 245});
        DrawRectangleLinesEx({popX, popY, popW, popH}, 2, WHITE);
        
        // Title
        DrawText("Audio Settings", popX + 10, popY + 10, 18, WHITE);
        
        // Close button
        Rectangle closeBtn = {popX + popW - 30, popY + 5, 25, 25};
        DrawRectangleRec(closeBtn, RED);
        DrawText("X", closeBtn.x + 7, closeBtn.y + 3, 18, WHITE);
        if (CheckCollisionPointRec(GetMousePosition(), closeBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.settings.showSettingsMenu = false;
        }
        
        // Current Device Label
        DrawText("Output Device:", popX + 10, popY + 40, 14, LIGHTGRAY);
        DrawText(state.settings.currentDevice.c_str(), popX + 120, popY + 40, 14, GREEN);
        
        // Device List
        float listY = popY + 65;
        for (size_t i = 0; i < state.settings.availableOutputDevices.size() && i < 5; ++i) {
            const auto& devName = state.settings.availableOutputDevices[i];
            Rectangle devBtn = {popX + 10, listY, popW - 20, 22};
            
            bool isCurrent = (devName == state.settings.currentDevice);
            DrawRectangleRec(devBtn, isCurrent ? Color{60, 120, 60, 255} : Color{60, 60, 60, 255});
            DrawText(devName.c_str(), devBtn.x + 5, devBtn.y + 4, 12, WHITE);
            
            if (!isCurrent && CheckCollisionPointRec(GetMousePosition(), devBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (engine.setOutputDevice(devName)) {
                    state.settings.currentDevice = devName;
                }
            }
            
            listY += 25;
        }
        
        if (state.settings.availableOutputDevices.empty()) {
            DrawText("No devices found", popX + 10, listY, 14, GRAY);
        }
    }
}
