#include "PatternEditor.h"
#include "Widgets.h"
#include "../GuiState.h"
#include "../AudioEngine.h"
#include "../FilePicker.h"
#include "FileBrowser.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;

namespace gui {

PatternEditor::PatternEditor(GuiState& s, AudioEngine& e) : state(s), engine(e), fxControls(s, e) {}

bool PatternEditor::IsOpen() const {
    return state.editor.isOpen;
}

void PatternEditor::Draw() {
    // Block input when file browser is open
    bool inputBlocked = state.editor.showFileBrowser;
    
    // Overlay
    DrawRectangle(0, 0, state.getScreenWidth(), state.getScreenHeight(), Color{0, 0, 0, 200});
    
    // Window - full screen width for touch (minimal margins)
    float winW = (float)state.getScreenWidth() - 10;
    float winH = (float)state.getScreenHeight() - 10;
    float winX = 5; 
    float winY = 5;
    
    Rectangle winRect = {winX, winY, winW, winH};
    DrawRectangleRec(winRect, Color{30, 30, 30, 255});
    DrawText("Edit Pattern", winRect.x + 20, winRect.y + 15, 24, WHITE);
    
    // Close Button - touch friendly (50x50)
    Rectangle closeRect = {winRect.x + winRect.width - 55, winRect.y + 5, 50, 50};
    DrawRectangleRec(closeRect, RED);
    DrawText("X", closeRect.x + 18, closeRect.y + 12, 24, WHITE);
    
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), closeRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.isOpen = false;
        state.editor.showFileBrowser = false;
        state.editor.focusedFieldId = -1;
    }

    // Scroll Logic
    BeginScissorMode((int)winRect.x, (int)winRect.y + 60, (int)winRect.width, (int)winRect.height - 80); 
    
    // Reset consumable flag at start of draw
    state.editor.scrollConsumed = false; 
    
    // ... drawing ...\
    // Note: Scroll update moved to END of this function to check for consumption
    
    float startY = winRect.y + 70 - state.editor.scrollOffsetY; // Base Y with Scroll
    float rowHeight = 50; // Touch-friendly row spacing
    float labelX = winRect.x + 20;
    float fieldX = winRect.x + 120;
    int labelSize = 22;
    int fieldH = 40;
    
    // Name
    DrawText("Name:", labelX, startY + 8, labelSize, WHITE);
    DrawTextInput({fieldX, startY, 280, (float)fieldH}, state.editor.nameBuffer, 63, 0, state.editor.focusedFieldId, state.getMousePosition());
    
    // Sample
    startY += rowHeight;
    DrawText("Sample:", labelX, startY + 8, labelSize, WHITE);
    
    // User requested only filename display
    Rectangle sampleBox = {fieldX, startY, 300, (float)fieldH};
    DrawRectangleRec(sampleBox, DARKGRAY);
    DrawRectangleLinesEx(sampleBox, 1, GRAY);
    
    std::string dispName = "";
    if (strlen(state.editor.samplePathBuffer) > 0) {
        dispName = fs::path(state.editor.samplePathBuffer).filename().string();
    }
    
    DrawText(dispName.c_str(), sampleBox.x + 8, sampleBox.y + 12, 16, WHITE);
    
    // Load Button - touch friendly
    Rectangle loadBtnRect = {fieldX + 310, startY, 80, (float)fieldH};
    DrawRectangleRec(loadBtnRect, BLUE);
    DrawText("Load", loadBtnRect.x + 20, loadBtnRect.y + 10, 18, WHITE);
    
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), loadBtnRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
         FileBrowser::Init(state);
         state.editor.showFileBrowser = true;
    }
    
    // Steps
    startY += rowHeight;
    DrawText("Steps:", labelX, startY + 8, labelSize, WHITE);
    DrawTextInput({fieldX, startY, 100, (float)fieldH}, state.editor.stepsBuffer, 4, 3, state.editor.focusedFieldId, state.getMousePosition());
    
    // Sync Base (for polyrhythm timing)
    startY += rowHeight;
    DrawText("Sync:", labelX, startY + 8, labelSize, WHITE);
    DrawTextInput({fieldX, startY, 100, (float)fieldH}, state.editor.syncBaseBuffer, 4, 4, state.editor.focusedFieldId, state.getMousePosition());
    
    // Always Sync moved to footer for better touch access
    
    // Velocity Knob - larger for touch
    float knobX = fieldX + 200;
    float knobY = startY + 20;
    float radius = 25;
    DrawText("Vel:", knobX - 60, startY + 8, labelSize, WHITE);
    DrawCircle(knobX, knobY, radius, DARKGRAY);
    DrawCircleLines(knobX, knobY, radius, WHITE);
    
    // Draw Value Indicator (Line angle)
    float angle = -135.0f + (state.editor.currentVelocity * 270.0f);
    Vector2 center = {knobX, knobY};
    Vector2 end = {center.x + cosf(angle*DEG2RAD)*radius, center.y + sinf(angle*DEG2RAD)*radius};
    DrawLineEx(center, end, 3.0f, RED);
    
    // Interaction - larger hit area
    Rectangle knobRect = {knobX - radius - 10, knobY - radius - 10, (radius+10)*2, (radius+10)*2};
    
    // Start drag
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), knobRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.isDraggingVelocity = true;
        state.editor.scrollConsumed = true; // Prevent scroll while dragging
    }
    
    // Stop drag
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        state.editor.isDraggingVelocity = false;
    }
    
    // Process Drag
    if (state.editor.isDraggingVelocity) {
         state.editor.scrollConsumed = true; // Keep blocking scroll during entire drag
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
    DrawText(TextFormat("%d%%", (int)(state.editor.currentVelocity*100)), knobX + 35, startY + 8, 16, LIGHTGRAY);
    
    // Mode Toggle Buttons Row - touch friendly horizontal layout
    startY += rowHeight + 10; // Move to next row
    float toggleW = 90;
    float toggleH = 50;
    float toggleGap = 8;
    float toggleStartX = labelX;
    
    // Helper: Check if any mode is active
    bool anyModeActive = state.editor.showMelodicControls || state.editor.showFxControls || state.editor.showSlicerControls;
    
    // Step Toggle (default mode - just sample sequencing)
    Rectangle stepToggleRect = {toggleStartX, startY, toggleW, toggleH};
    DrawRectangleRec(stepToggleRect, !anyModeActive ? ORANGE : DARKGRAY);
    DrawText("Step", stepToggleRect.x + 22, stepToggleRect.y + 14, 20, WHITE);
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), stepToggleRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Disable all special modes to return to step mode
        state.editor.showMelodicControls = false;
        state.editor.showFxControls = false;
        state.editor.showSlicerControls = false;
    }
    
    // Melodic Toggle
    Rectangle melodyToggleRect = {toggleStartX + (toggleW + toggleGap), startY, toggleW, toggleH};
    DrawRectangleRec(melodyToggleRect, state.editor.showMelodicControls ? BLUE : DARKGRAY);
    DrawText("Melodic", melodyToggleRect.x + 8, melodyToggleRect.y + 14, 18, WHITE);
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), melodyToggleRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.showMelodicControls = !state.editor.showMelodicControls;
        if (state.editor.showMelodicControls) {
            state.editor.showFxControls = false;
            state.editor.showSlicerControls = false;
        }
    }

    // FX Toggle
    Rectangle fxToggleRect = {toggleStartX + (toggleW + toggleGap) * 2, startY, toggleW, toggleH}; 
    DrawRectangleRec(fxToggleRect, state.editor.showFxControls ? VIOLET : DARKGRAY);
    DrawText("FX", fxToggleRect.x + 35, fxToggleRect.y + 14, 20, WHITE);
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), fxToggleRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.showFxControls = !state.editor.showFxControls;
        if (state.editor.showFxControls) {
            state.editor.showMelodicControls = false;
            state.editor.showSlicerControls = false;
        }
    }
    
    // Slicer Toggle
    Rectangle slicerToggleRect = {toggleStartX + (toggleW + toggleGap) * 3, startY, toggleW, toggleH};
    DrawRectangleRec(slicerToggleRect, state.editor.showSlicerControls ? GREEN : DARKGRAY);
    DrawText("Slicer", slicerToggleRect.x + 15, slicerToggleRect.y + 14, 18, WHITE);
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), slicerToggleRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.showSlicerControls = !state.editor.showSlicerControls;
        if (state.editor.showSlicerControls) {
            state.editor.showMelodicControls = false;
            state.editor.showFxControls = false;
        }
    }
    
    startY += toggleH + 15; // Spacing before grid 
    
    // Editor Grid
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
        
        bool inViewport = CheckCollisionPointRec(state.getMousePosition(), {winRect.x, winRect.y+50, winRect.width, winRect.height-100});
        
        if (!inputBlocked && inViewport && CheckCollisionPointRec(state.getMousePosition(), stepRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            
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
        if (!inputBlocked && inViewport && CheckCollisionPointRec(state.getMousePosition(), stepRect) && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
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
            
            if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), sBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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
        DrawText(TextFormat("Oct: %d", state.editor.selectedOctave), winRect.x + 20, startY + 15, 24, WHITE);
        
        // Octave buttons - touch friendly
        Rectangle octDown = {winRect.x + 120, startY, 50, 50};
        DrawRectangleRec(octDown, DARKGRAY);
        DrawText("-", octDown.x + 18, octDown.y + 10, 28, WHITE);
        if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), octDown) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) state.editor.selectedOctave--;
        
        Rectangle octUp = {winRect.x + 180, startY, 50, 50};
        DrawRectangleRec(octUp, DARKGRAY);
        DrawText("+", octUp.x + 15, octUp.y + 10, 28, WHITE);
        if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), octUp) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) state.editor.selectedOctave++;
        
        const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        float keyX = winRect.x + 250;
        float keyY = startY; 
        float whiteKeyWidth = 45;  // Increased from 25
        float blackKeyWidth = 30;  // Increased from 18
        float keyHeight = 100;     // Increased from 80
        
        // White keys first
        for (int i = 0; i < 12; ++i) {
            bool isBlack = (i==1 || i==3 || i==6 || i==8 || i==10);
            if (!isBlack) {
                Rectangle keyRect = {keyX, keyY, whiteKeyWidth, keyHeight};
                bool isSelected = (state.editor.selectedNote == i);
                DrawRectangleRec(keyRect, isSelected ? YELLOW : WHITE);
                DrawRectangleLinesEx(keyRect, 2, BLACK);
                if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), keyRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    state.editor.selectedNote = i;
                    // Update selected step's pitch if in edit mode
                    if (state.editor.selectedStep != -1 && state.editor.stepStates[state.editor.selectedStep]) {
                        int shift = (state.editor.selectedOctave - 4) * 12 + i;
                        p.stepPitches[state.editor.selectedStep + 1] = shift;
                        engine.addPattern(p);
                    }
                }
                DrawText(notes[i], keyRect.x + 12, keyRect.y + keyHeight - 25, 16, BLACK);
                keyX += whiteKeyWidth;
            }
        }
        
        // Black keys on top
        keyX = winRect.x + 250;
        for (int i = 0; i < 12; ++i) {
            bool isBlack = (i==1 || i==3 || i==6 || i==8 || i==10);
            if (isBlack) {
                 float xPos = 0;
                 if(i==1) xPos = whiteKeyWidth * 1 - (blackKeyWidth/2);
                 if(i==3) xPos = whiteKeyWidth * 2 - (blackKeyWidth/2);
                 if(i==6) xPos = whiteKeyWidth * 4 - (blackKeyWidth/2);
                 if(i==8) xPos = whiteKeyWidth * 5 - (blackKeyWidth/2);
                 if(i==10) xPos = whiteKeyWidth * 6 - (blackKeyWidth/2);
                 Rectangle keyRect = {winRect.x + 250 + xPos, keyY, blackKeyWidth, keyHeight * 0.6f};
                 bool isSelected = (state.editor.selectedNote == i);
                 DrawRectangleRec(keyRect, isSelected ? YELLOW : BLACK);
                 DrawRectangleLinesEx(keyRect, 1, DARKGRAY);
                 if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), keyRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                     state.editor.selectedNote = i;
                     // Update selected step's pitch if in edit mode
                     if (state.editor.selectedStep != -1 && state.editor.stepStates[state.editor.selectedStep]) {
                         int shift = (state.editor.selectedOctave - 4) * 12 + i;
                         p.stepPitches[state.editor.selectedStep + 1] = shift;
                         engine.addPattern(p);
                     }
                 }
            }
        }
        startY += keyHeight + 10;
    }

    // Draw File Browser (Modal)
    bool browserBusy = false;
    if (state.editor.showFileBrowser) {
        browserBusy = DrawFileBrowser(winRect);
    }

    if (browserBusy) return; // Block input to pattern editor!

    // FX Controls (Modular)
    if (state.editor.showFxControls) {
        Rectangle fxArea = {winRect.x + 20, startY, winRect.width - 40, 0};
        // Define parent scissor rect based on PatternEditor's viewport
        Rectangle parentScissor = {(float)(int)winRect.x, (float)(int)winRect.y + 50, (float)(int)winRect.width, (float)(int)winRect.height - 100};
        float heightUsed = fxControls.Draw(fxArea, state.editor.currentPattern, parentScissor);
        startY += heightUsed;
    }

    // Slicer Controls
    if (state.editor.showSlicerControls) {
        DrawText("Sample Slicer", winRect.x + 20, startY + 10, 24, WHITE);
        startY += 45;

        // Waveform Viewer - larger for touch
        Rectangle waveRect = {winRect.x + 20, startY, winRect.width - 40, 150};
        DrawRectangleRec(waveRect, BLACK);
        DrawRectangleLinesEx(waveRect, 2, GRAY);
        
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
                    int textWidth = MeasureText(TextFormat("%d", mFn), 10);
                    DrawText(TextFormat("%d", mFn), waveRect.x + xPos + 2, waveRect.y + 2, 10, YELLOW);
                    
                    // Handle Delete (Click on Number)
                    // Only active if NOT in Play Mode
                    if (!inputBlocked && !state.editor.slicerPlayModeEnabled && CheckCollisionPointRec(state.getMousePosition(), {waveRect.x + xPos, waveRect.y, (float)textWidth + 4, 15}) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        p.sliceMarkers.erase(p.sliceMarkers.begin() + mFn);
                        mFn--; 
                        engine.addPattern(p); // SYNC 
                    }
                    
                    // Handle Delete (Right Click near marker - kept for alt method)
                    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), {waveRect.x + xPos - 5, waveRect.y, 10, waveRect.height}) && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                        p.sliceMarkers.erase(p.sliceMarkers.begin() + mFn);
                        mFn--; 
                        engine.addPattern(p); // SYNC 
                    }
                }
            }
            
            // Interaction: Left Click to Add Marker (Only if NOT in Play Mode)
            if (!inputBlocked && !state.editor.slicerPlayModeEnabled && CheckCollisionPointRec(state.getMousePosition(), waveRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                float localX = state.getMousePosition().x - waveRect.x;
                int sampleIdx = startSample + (int)(localX / waveRect.width * visibleSamples);
                
                // Add and Sort
                p.sliceMarkers.push_back(sampleIdx);
                std::sort(p.sliceMarkers.begin(), p.sliceMarkers.end());
                engine.addPattern(p); // SYNC
            }
            
            // Horizontal Scrollbar (only if zoomed) - touch friendly
            if (zoom > 1.0f) {
                Rectangle scrollBarBg = {waveRect.x, waveRect.y + waveRect.height + 5, waveRect.width, 25};
                DrawRectangleRec(scrollBarBg, DARKGRAY);
                
                float thumbWidth = scrollBarBg.width / zoom;
                float thumbX = scrollBarBg.x + state.editor.waveformScrollX / (1.0f - viewWidth + 0.001f) * (scrollBarBg.width - thumbWidth);
                Rectangle scrollThumb = {thumbX, scrollBarBg.y + 3, thumbWidth, 19};
                DrawRectangleRec(scrollThumb, LIGHTGRAY);
                
                // Drag scrollbar
                static bool isDraggingScroll = false;
                static float dragStartX = 0;
                static float dragStartScroll = 0;
                
                if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), scrollBarBg) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    isDraggingScroll = true;
                    dragStartX = state.getMousePosition().x;
                    dragStartScroll = state.editor.waveformScrollX;
                }
                if (isDraggingScroll && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                    float deltaX = state.getMousePosition().x - dragStartX;
                    float deltaScroll = deltaX / (scrollBarBg.width - thumbWidth) * scrollMax;
                    state.editor.waveformScrollX = std::min(std::max(dragStartScroll + deltaScroll, 0.0f), scrollMax);
                }
                if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                    isDraggingScroll = false;
                }
            }
            
        } else {
            DrawText("No Sample Loaded", waveRect.x + 10, waveRect.y + 60, 20, DARKGRAY);
        }
        
        // Move past the waveform (150px) and scrollbar area
        startY += (p.sampleBuffer.getNumSamples() > 0 && state.editor.waveformZoom > 1.0f) ? 185 : 165;
        
        // Controls: Clear Markers - touch friendly
        Rectangle clearBtn = {winRect.x + 20, startY, 140, 45};
        DrawRectangleRec(clearBtn, RED);
        DrawText("Clear Slices", clearBtn.x + 15, clearBtn.y + 12, 16, WHITE);
        if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), clearBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            p.sliceMarkers.clear();
            
            // UX SYNC: Remove entire steps that have FX_SLICE
            std::vector<int> stepsToRemove;
            for (auto& stepPair : p.stepFX) {
                const auto& fxList = stepPair.second;
                if (std::find(fxList.begin(), fxList.end(), Pattern::FX_SLICE) != fxList.end()) {
                    stepsToRemove.push_back(stepPair.first);
                }
            }
            
            for (int step : stepsToRemove) {
                p.stepPitches.erase(step);
                p.stepVelocities.erase(step);
                p.stepFX.erase(step);
                p.stepFXParams.erase(step);
                
                // Update UI state
                state.editor.stepStates[step - 1] = false;
                if (state.editor.selectedStep == step - 1) {
                    state.editor.selectedStep = -1;
                }
            }
            
            engine.addPattern(p); // SYNC
        }
        
        // Cutoff Toggle - touch friendly
        Rectangle cutBtn = {winRect.x + 170, startY, 80, 45};
        DrawRectangleRec(cutBtn, state.editor.slicerCutoffEnabled ? GREEN : DARKGRAY);
        DrawText("Cut", cutBtn.x + 25, cutBtn.y + 12, 18, WHITE);
        if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), cutBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.editor.slicerCutoffEnabled = !state.editor.slicerCutoffEnabled;
        }
        
        // Play/Slice Mode Switch - touch friendly
        float switchStartX = winRect.x + 260;
        Rectangle sliceRect = {switchStartX, startY, 70, 45};
        Rectangle playRect = {switchStartX + 70, startY, 70, 45};
        
        if (!state.editor.slicerPlayModeEnabled) {
            DrawRectangleRec(sliceRect, WHITE);
            DrawRectangleRec(playRect, GRAY);
            DrawText("Slice", sliceRect.x + 12, sliceRect.y + 12, 18, BLACK);
            DrawText("Play", playRect.x + 15, playRect.y + 12, 18, WHITE);
        } else {
            DrawRectangleRec(sliceRect, GRAY);
            DrawRectangleRec(playRect, WHITE);
            DrawText("Slice", sliceRect.x + 12, sliceRect.y + 12, 18, WHITE);
            DrawText("Play", playRect.x + 15, playRect.y + 12, 18, BLACK);
        }
        
        if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), sliceRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.editor.slicerPlayModeEnabled = false;
        }
        if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), playRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.editor.slicerPlayModeEnabled = true;
        }
        
        // Play Mode Interaction: Right Click or Click in Mode
        if (!inputBlocked && state.editor.slicerPlayModeEnabled && CheckCollisionPointRec(state.getMousePosition(), waveRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
             if (p.sampleBuffer.getNumSamples() > 0) {
                 int64_t totalSamples = p.sampleBuffer.getNumSamples();
                 float zoom = state.editor.waveformZoom;
                 float viewWidth = 1.0f / zoom;
                 int64_t visibleSamples = (int64_t)(totalSamples * viewWidth);
                 // Scroll mapping
                 float scrollMax = 1.0f - viewWidth;
                 if (scrollMax < 0) scrollMax = 0;
                 // Clamp scroll
                 if (state.editor.waveformScrollX > scrollMax) state.editor.waveformScrollX = scrollMax;
                 
                 int64_t startSample = (int64_t)(state.editor.waveformScrollX / (scrollMax + 0.001f) * (totalSamples - visibleSamples));
                 if (startSample < 0) startSample = 0;
                 
                 float localX = state.getMousePosition().x - waveRect.x;
                 int sampleIdx = startSample + (int)(localX / waveRect.width * visibleSamples);
                 
                 // Find Slice
                 int foundSlice = -1;
                 if (sampleIdx >= 0) {
                     // Check if valid start
                     if (!p.sliceMarkers.empty()) {
                         for (size_t i = 0; i < p.sliceMarkers.size(); ++i) {
                             int64_t sStart = p.sliceMarkers[i];
                             int64_t sEnd = (i + 1 < p.sliceMarkers.size()) ? p.sliceMarkers[i+1] : p.sampleBuffer.getNumSamples();
                             
                             if (sampleIdx >= sStart && sampleIdx < sEnd) {
                                 foundSlice = (int)i;
                                 break;
                             }
                         }
                     }
                 }
                 
                 if (foundSlice != -1) {
                     // Preview
                     engine.previewSlice(p, foundSlice, !state.editor.slicerCutoffEnabled);
                     
                     // Record if Live Edit Mode is ON
                     if (state.isLiveEditMode && state.editor.selectedStep >= 0 && state.editor.selectedStep < 64) {
                          // Assign slice to selected step
                          int step = state.editor.selectedStep + 1;
                          
                          // Add Slice FX
                          if (std::find(p.stepFX[step].begin(), p.stepFX[step].end(), Pattern::FX_SLICE) == p.stepFX[step].end()) {
                              p.stepFX[step].push_back(Pattern::FX_SLICE);
                          }
                          
                          // Set Index Param
                          p.stepFXParams[step][Pattern::PAR_SLICE_INDEX] = (float)foundSlice;
                          if (state.editor.slicerCutoffEnabled) {
                              p.stepFXParams[step][Pattern::PAR_SLICE_CUTOFF] = 1.0f;
                          } else {
                              p.stepFXParams[step].erase(Pattern::PAR_SLICE_CUTOFF);
                          }
                          
                          // Activate step if not active?
                          state.editor.stepStates[state.editor.selectedStep] = true;
                          p.stepPitches[step] = 0; // Default C
                          p.stepVelocities[step] = 1.0f;
                          
                          engine.addPattern(p);
                     }
                 }
             }
        }

        
        // Zoom buttons (to the right of Play Mode) - touch friendly
        Rectangle zoomOutBtn = {winRect.x + 410, startY, 45, 45};
        Rectangle zoomInBtn = {winRect.x + 460, startY, 45, 45};
        DrawRectangleRec(zoomOutBtn, DARKGRAY);
        DrawRectangleRec(zoomInBtn, DARKGRAY);
        DrawText("-", zoomOutBtn.x + 16, zoomOutBtn.y + 10, 24, WHITE);
        DrawText("+", zoomInBtn.x + 14, zoomInBtn.y + 10, 24, WHITE);
        
        if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), zoomInBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.editor.waveformZoom = std::min(state.editor.waveformZoom * 1.5f, 20.0f);
        }
        if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), zoomOutBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.editor.waveformZoom = std::max(state.editor.waveformZoom / 1.5f, 1.0f);
            if (state.editor.waveformZoom <= 1.0f) state.editor.waveformScrollX = 0.0f;
        }
        
        startY += 55;
        
        // Bookend Button (Add Start & End Markers) - touch friendly
        if (p.sampleBuffer.getNumSamples() > 0) {
             Rectangle bookendBtn = {winRect.x + 20, startY, 150, 45};
             DrawRectangleRec(bookendBtn, BLUE);
             DrawText("Bookend", bookendBtn.x + 35, bookendBtn.y + 12, 18, WHITE);
             if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), bookendBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                 bool changed = false;
                 // Add Start (0)
                 if (std::find(p.sliceMarkers.begin(), p.sliceMarkers.end(), 0) == p.sliceMarkers.end()) {
                     p.sliceMarkers.push_back(0);
                     changed = true;
                 }
                 // Add End (Total Samples)
                 int64_t total = p.sampleBuffer.getNumSamples();
                 if (std::find(p.sliceMarkers.begin(), p.sliceMarkers.end(), total) == p.sliceMarkers.end()) {
                     p.sliceMarkers.push_back(total);
                     changed = true;
                 }
                 
                 if (changed) {
                     std::sort(p.sliceMarkers.begin(), p.sliceMarkers.end());
                     engine.addPattern(p);
                 }
             }
             startY += 55;
        }

        startY += 10;
    }
    
    // Update Content Height
    state.editor.contentHeight = (startY + 50) - (winRect.y + 60) + state.editor.scrollOffsetY; 
    


    EndScissorMode();
    
    // --- FOOTER CONTROLS ---
    DrawRectangle(winRect.x, winRect.y + winRect.height - 55, winRect.width, 55, Color{30, 30, 30, 255}); // Footer BG

    // Copy Button (Left) - larger for touch
    Rectangle copyRect = {winRect.x + 10, winRect.y + winRect.height - 48, 80, 40};
    bool copyActive = state.editor.clipboard.isCopyMode;
    DrawRectangleRec(copyRect, copyActive ? ORANGE : DARKGRAY);
    DrawText("Copy", copyRect.x + 18, copyRect.y + 10, 20, WHITE);
    
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), copyRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.clipboard.isCopyMode = !state.editor.clipboard.isCopyMode;
        state.editor.clipboard.isPasteMode = false;
        state.editor.clipboard.isEditMode = false;
    }

    // Paste Button - larger for touch
    Rectangle pasteRect = {winRect.x + 100, winRect.y + winRect.height - 48, 90, 40};
    bool canPaste = state.editor.clipboard.hasData;
    bool pasteActive = state.editor.clipboard.isPasteMode;
    
    Color pasteColor = pasteActive ? MAGENTA : DARKGRAY; 
    
    DrawRectangleRec(pasteRect, pasteColor);
    DrawText("Paste", pasteRect.x + 18, pasteRect.y + 10, 20, WHITE);
    
    if (!inputBlocked && canPaste && CheckCollisionPointRec(state.getMousePosition(), pasteRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.clipboard.isPasteMode = !state.editor.clipboard.isPasteMode;
        state.editor.clipboard.isCopyMode = false;
        state.editor.clipboard.isEditMode = false;
    }

    // Edit Button (Use Cyan for distinction) - larger for touch
    Rectangle editRect = {winRect.x + 200, winRect.y + winRect.height - 45, 80, 40};
    bool editActive = state.editor.clipboard.isEditMode;
    DrawRectangleRec(editRect, editActive ? SKYBLUE : DARKGRAY);
    DrawText("Edit", editRect.x + 18, editRect.y + 10, 20, WHITE);
    
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), editRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.clipboard.isEditMode = !state.editor.clipboard.isEditMode;
        state.editor.clipboard.isCopyMode = false;
        state.editor.clipboard.isPasteMode = false;
    }

    // Preview Button - plays just this pattern
    Rectangle previewRect = {winRect.x + 290, winRect.y + winRect.height - 45, 100, 40};
    DrawRectangleRec(previewRect, LIME);
    DrawText("Preview", previewRect.x + 12, previewRect.y + 10, 20, BLACK);
    
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), previewRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Save current pattern state first
        p.name = state.editor.nameBuffer;
        p.steps = atoi(state.editor.stepsBuffer);
        p.syncBase = atoi(state.editor.syncBaseBuffer);
        if (p.syncBase <= 0) p.syncBase = p.steps;
        engine.addPattern(p);
        
        // Ensure track assignment matches the column this pattern belongs to
        for (const auto& col : state.columns) {
            bool found = false;
            for (const auto& pn : col.patternNames) {
                if (pn == p.name) {
                    engine.assignPatternToTrack(p.name, col.trackName);
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        
        // Play just this pattern
        engine.playPattern(p.name);
    }

    // Per-Slot Sync Toggle - in footer for easy touch access
    // Find which slot this pattern is in and toggle that slot's sync
    Rectangle syncRect = {winRect.x + 400, winRect.y + winRect.height - 45, 80, 40};
    
    // Find slot for current pattern
    int foundCol = -1, foundSlot = -1;
    for (size_t c = 0; c < state.columns.size() && foundCol < 0; c++) {
        for (size_t s = 0; s < state.columns[c].patternNames.size(); s++) {
            if (state.columns[c].patternNames[s] == p.name) {
                foundCol = (int)c;
                foundSlot = (int)s;
                break;
            }
        }
    }
    
    bool slotSyncEnabled = false;
    if (foundCol >= 0 && foundSlot >= 0) {
        PatternColumn& pc = state.columns[(size_t)foundCol];
        // Ensure slotSyncEnabled vector is large enough
        while (pc.slotSyncEnabled.size() < pc.patternNames.size()) {
            pc.slotSyncEnabled.push_back(false);
        }
        if (foundSlot < (int)pc.slotSyncEnabled.size()) {
            slotSyncEnabled = pc.slotSyncEnabled[(size_t)foundSlot];
        }
    }
    
    DrawRectangleRec(syncRect, slotSyncEnabled ? Color{0, 180, 0, 255} : DARKGRAY);
    DrawText("Sync", syncRect.x + 18, syncRect.y + 10, 20, WHITE);
    
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), syncRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (foundCol >= 0 && foundSlot >= 0) {
            PatternColumn& pc = state.columns[(size_t)foundCol];
            // Ensure vector is large enough
            while (pc.slotSyncEnabled.size() < pc.patternNames.size()) {
                pc.slotSyncEnabled.push_back(false);
            }
            pc.slotSyncEnabled[(size_t)foundSlot] = !pc.slotSyncEnabled[(size_t)foundSlot];
        }
    }

    // Save Button (Fixed at Bottom Footer) - larger for touch
    Rectangle saveRect = {winRect.x + winRect.width - 100, winRect.y + winRect.height - 45, 90, 40};
    
    // Play Button (only visible in Shift mode during playback)
    if (state.isShiftMode && state.isPlaying) {
        Rectangle playBtn = {saveRect.x - 90, saveRect.y, 80, 30};
        DrawRectangleRec(playBtn, GREEN);
        DrawText("Play", playBtn.x + 18, playBtn.y + 5, 20, BLACK);
        
        if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), playBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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
            p.syncBase = atoi(state.editor.syncBaseBuffer);
            
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
            // Update engine with new active patterns
            std::vector<std::pair<std::string, std::string>> allActive;
            for (auto& pair : state.activePatternSlots) {
                int c = pair.first;
                int s = pair.second;
                if (c >= 0 && c < (int)state.columns.size() && s >= 0 && s < (int)state.columns[c].patternNames.size()) {
                    allActive.push_back({state.columns[c].patternNames[s], state.columns[c].trackName});
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
    
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), saveRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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
        p.syncBase = atoi(state.editor.syncBaseBuffer);
        
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
    
    // --- LATE UPDATE SCROLL LOGIC ---
    // We update scroll offset here because children (like FXControls) might have consumed the scroll event.
    Rectangle scrollArea = {winRect.x, winRect.y + 60, winRect.width, winRect.height - 110};
    float maxScroll = std::max(0.0f, state.editor.contentHeight - (winRect.height - 100));
    
    if (!state.editor.scrollConsumed) {
        // Mouse wheel (desktop)
        state.editor.scrollOffsetY -= GetMouseWheelMove() * 30.0f;
        
        // Touch drag scrolling
        if (CheckCollisionPointRec(state.getMousePosition(), scrollArea)) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                state.editor.editorDragging = true;
                state.editor.editorDragStartY = state.getMousePosition().y;
                state.editor.editorDragStartScrollY = state.editor.scrollOffsetY;
            }
        }
    }
    
    // Update drag (even if scrollConsumed, continue tracking existing drag)
    if (state.editor.editorDragging && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float deltaY = state.editor.editorDragStartY - state.getMousePosition().y;
        state.editor.scrollOffsetY = state.editor.editorDragStartScrollY + deltaY;
    }
    
    // End drag
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        state.editor.editorDragging = false;
    }
    
    // Clamp scroll
    if (state.editor.scrollOffsetY < 0) state.editor.scrollOffsetY = 0;
    if (state.editor.scrollOffsetY > maxScroll) state.editor.scrollOffsetY = maxScroll;
}

bool PatternEditor::DrawFileBrowser(Rectangle winRect) {
    return FileBrowser::Draw(state, engine);
}


} // namespace gui
