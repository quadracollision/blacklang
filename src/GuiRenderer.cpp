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
        if (CheckCollisionPointRec(GetMousePosition(), headerRect)) {
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
    
    float y = col.bounds.y + 40;
    
    for (size_t i = 0; i < col.patternNames.size(); ++i) {
        Rectangle patRect = {
            col.bounds.x + 5,
            y,
            col.bounds.width - 10,
            (float)state.PATTERN_HEIGHT
        };
        y += state.PATTERN_HEIGHT + 5; // Advance Y
        
        // Selection Logic
        bool isSelected = (state.activePatterns.count(index) && state.activePatterns[index] == col.patternNames[i]);
        
        // Skip drawing/interaction if completely covered by add button
        bool overlapsButton = CheckCollisionRecs(patRect, addBtnRect);
                                  
        // Skip drawing if being dragged
        if (state.drag.isDragging && 
            state.drag.sourceColumnIndex == index && 
            state.drag.patternName == col.patternNames[i]) {
            // Draw placeholder
            DrawRectangleRoundedLines(patRect, 0.1f, 4, 2.0f, DARKGRAY);
        } else {
            // Check if holding this specific pattern
            bool isHoldingThis = state.drag.isHolding && 
                                 state.drag.sourceColumnIndex == index && 
                                 state.drag.patternName == col.patternNames[i];
            
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
                
                // Interaction - only if not overlapping button
                if (CheckCollisionPointRec(GetMousePosition(), patRect)) {
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        // Logic:
                        // 1. If Double Click -> Edit
                        // 2. If Single Click -> Select/Queue
                        // 3. If Hold -> Drag
                        
                        // We need to differentiate single click vs hold/drag.
                        // Standard: MouseDown starts potential hold. MouseUp confirms click if not held long.
                        // But DoubleClick needs fast successive clicks.
                        
                        // Let's use MouseDown for potential hold + selection
                        // Immediate Selection feeling:
                        // Toggle Selection Logic
                        bool wasSelected = (state.activePatterns.count(index) && state.activePatterns[index] == col.patternNames[i]);
                        
                        if (wasSelected) {
                            state.activePatterns.erase(index);
                        } else {
                            state.activePatterns[index] = col.patternNames[i];
                        }
                        
                        // Sync with Audio Engine
                        if (engine.isPlaying()) {
                            std::vector<std::string> allActive;
                            for (auto& pair : state.activePatterns) allActive.push_back(pair.second);
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
    DrawRectangleRounded(addBtnRect, 0.2f, 4, Color{50, 50, 50, 255});
    DrawText("+ Add", addBtnRect.x + 40, addBtnRect.y + 5, 10, WHITE);
    
    if (CheckCollisionPointRec(GetMousePosition(), addBtnRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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
        (float)GetScreenHeight()/2 - 200, 
        500, 400
    };
    DrawRectangleRec(winRect, Color{30, 30, 30, 255});
    DrawRectangleLinesEx(winRect, 2, LIGHTGRAY);
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
    DrawTextInput({winRect.x + 100, startY, 240, 30}, state.editor.samplePathBuffer, 255, 1, state.editor.focusedFieldId);
    
    Rectangle loadBtnRect = {winRect.x + 350, startY, 50, 30};
    DrawRectangleRounded(loadBtnRect, 0.2f, 4, DARKGRAY);
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
         // Invert delta so dragging UP increases value (standard knob behavior often maps Up/Right to +)
         // But usually vertical sliders: Up is -, Down is +.
         // For a knob, typically dragging UP increases? Or Right?
         // Let's make Drag UP -> Increase (+), Drag DOWN -> Decrease (-).
         // Mouse Y increases downwards. So delta < 0 is UP.
         // So substracting delta adds value if moving up.
         state.editor.currentVelocity -= delta * 0.02f; // Sensitivity
         
         if (state.editor.currentVelocity < 0.0f) state.editor.currentVelocity = 0.0f;
         if (state.editor.currentVelocity > 1.0f) state.editor.currentVelocity = 1.0f;
    }
    DrawText(TextFormat("%d%%", (int)(state.editor.currentVelocity*100)), knobX + 25, startY + 5, 10, LIGHTGRAY);
    
    // Melodic Toggle
    Rectangle melodyToggleRect = {winRect.x + 380, startY, 80, 30};
    DrawRectangleRounded(melodyToggleRect, 0.2f, 4, state.editor.showMelodicControls ? BLUE : DARKGRAY);
    DrawText("Melodic", melodyToggleRect.x + 8, melodyToggleRect.y + 5, 10, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), melodyToggleRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.showMelodicControls = !state.editor.showMelodicControls;
    }
    
    startY += 50; 
    
    // Editor Grid (Moved ABOVE Keyboard)
    Pattern& p = state.editor.currentPattern;
    Rectangle gridRect = {winRect.x + 20, startY, winRect.width - 40, 150};
    
    int stepCount = atoi(state.editor.stepsBuffer);
    if (stepCount <= 0) stepCount = 16;
    if (stepCount > 64) stepCount = 64;
    int cols = std::min(16, stepCount);
    int rows = (stepCount + cols - 1) / cols;
    float cellW = gridRect.width / cols;
    float cellH = gridRect.height / rows;
    float stepSize = std::min(cellW, cellH) * 0.9f;
    
    for (int i = 0; i < stepCount; ++i) {
        int col = i % cols;
        int row = i / cols;
        float x = gridRect.x + col * cellW + (cellW - stepSize)/2;
        float y = gridRect.y + row * cellH + (cellH - stepSize)/2;
        
        bool active = state.editor.stepStates[i];
        Color c = active ? RED : DARKGRAY;
        if (active && state.editor.showMelodicControls && p.stepPitches.count(i+1)) c = ORANGE;

        Rectangle stepRect = {x, y, stepSize, stepSize};
        DrawRectangleRec(stepRect, c);
        
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
        
        bool inViewport = CheckCollisionPointRec(GetMousePosition(), {winRect.x, winRect.y+50, winRect.width, winRect.height-100});
        
        if (inViewport && CheckCollisionPointRec(GetMousePosition(), stepRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            bool wasActive = state.editor.stepStates[i];
            state.editor.stepStates[i] = !wasActive;
            
            // Apply Melodic / Velocity Data
            if (!wasActive) {
                 if (state.editor.showMelodicControls) {
                    int shift = (state.editor.selectedOctave - 4) * 12 + state.editor.selectedNote;
                    p.stepPitches[i+1] = shift;
                 }
                 // Always apply velocity
                 p.stepVelocities[i+1] = state.editor.currentVelocity;
            } else {
                 // Removing step
                 p.stepPitches.erase(i+1);
                 p.stepVelocities.erase(i+1);
            }
        }
    }
    
    startY += 160; 
    
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
    
    // Save Button (Fixed at Bottom Footer, outside scrubber)
    Rectangle saveRect = {winRect.x + winRect.width - 100, winRect.y + winRect.height - 40, 80, 30};
    DrawRectangle(winRect.x, winRect.y + winRect.height - 50, winRect.width, 50, Color{30, 30, 30, 255}); // Footer BG
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
            if (state.editor.stepStates[i]) p.activeSteps.push_back(i+1);
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
            // Update selection if needed
            for (auto& pair : state.activePatterns) {
                if (pair.second == oldName) pair.second = p.name;
            }
        }
        
        // Refresh renderer
        state.editor.isOpen = false;
        state.editor.showFileBrowser = false;
        state.editor.focusedFieldId = -1;
    }
}

void GuiRenderer::DrawPatternBox(const std::string& name, Rectangle bounds, bool selected) {
    Color bgColor = selected ? Color{58, 123, 213, 255} : Color{60, 60, 60, 255};
    DrawRectangleRounded(bounds, 0.1f, 4, bgColor);
    DrawRectangleRoundedLines(bounds, 0.1f, 4, 1.0f, selected ? WHITE : GRAY);
    
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
        
        DrawRectangleRec({x, y, size, size}, c);
        
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
    DrawRectangleRounded(playRect, 0.2f, 4, state.isPlaying ? GREEN : GRAY);
    DrawText(">", playRect.x + 15, playRect.y + 10, 20, BLACK);
    if (CheckCollisionPointRec(GetMousePosition(), playRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (!state.activePatterns.empty()) {
            std::vector<std::string> names;
            for (const auto& pair : state.activePatterns) names.push_back(pair.second);
            engine.playMultiplePatterns(names);
        }
    }
    
    // Stop
    Rectangle stopRect = {centerX, rect.y + 10, 40, 40};
    DrawRectangleRounded(stopRect, 0.2f, 4, RED);
    DrawRectangle(stopRect.x + 10, stopRect.y + 10, 20, 20, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), stopRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        engine.stop();
    }
    
    // BPM
    DrawText("BPM:", centerX + 100, rect.y + 10, 20, LIGHTGRAY);
    DrawTextInput({centerX + 150, rect.y + 10, 60, 25}, state.globalBpmBuffer, 5, 999, state.focusedFieldId);
    
    // Process BPM change on Enter or Focus loss (simplified: just parse every frame if valid)
    // Or just on Enter?
    // Let's parse continually if valid number
    int newBpm = atoi(state.globalBpmBuffer);
    if (newBpm > 20 && newBpm < 300 && newBpm != state.bpm) {
        state.bpm = newBpm;
        engine.setBPM(newBpm);
    }
}
