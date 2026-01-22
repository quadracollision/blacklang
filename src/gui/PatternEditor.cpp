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

PatternEditor::PatternEditor(GuiState& s, AudioEngine& e) : state(s), engine(e), fxControls(s, e), sampleSlicer(s, e) {}

bool PatternEditor::IsOpen() const {
    return state.editor.isOpen;
}

void PatternEditor::Draw() {
    // Block input when file browser is open or editor just opened (prevent click-through)
    bool inputBlocked = state.editor.showFileBrowser;
    
    // If editor just opened, block this frame's input and clear the flag
    if (state.editor.justOpened) {
        inputBlocked = true;
        // Clear flag when mouse button is released (user has let go of the click that opened editor)
        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            state.editor.justOpened = false;
        }
    }
    
    // Overlay
    DrawRectangle(0, 0, state.getScreenWidth(), state.getScreenHeight(), Color{0, 0, 0, 200});
    
    // Window - full screen width for touch (minimal margins)
    float winW = (float)state.getScreenWidth() - 10;
    float winH = (float)state.getScreenHeight() - 10;
    float winX = 5; 
    float winY = 5;
    
    Rectangle winRect = {winX, winY, winW, winH};
    DrawRectangleRec(winRect, Color{30, 30, 30, 255});
    
    // Scroll Logic
    Rectangle contentScissor = {winRect.x, winRect.y + 60, winRect.width, winRect.height - 110};
    BeginScissorMode((int)contentScissor.x, (int)contentScissor.y, (int)contentScissor.width, (int)contentScissor.height); 
    
    // Reset consumable flag at start of draw
    state.editor.scrollConsumed = false; 
    
    // ... drawing ...
    // Note: Scroll update moved to END of this function to check for consumption
    
    // If file browser is open, draw it and return early (don't draw pattern editor content)
    
    float startY = winRect.y + 70 - state.editor.scrollOffsetY; // Base Y with Scroll
    float rowHeight = 50; // Touch-friendly row spacing
    float labelX = winRect.x + 20;
    float labelWidth = state.isPortrait ? 80 : 100;  // Narrower labels in portrait
    float fieldX = winRect.x + labelWidth + 20;
    float fieldMaxWidth = winRect.width - labelWidth - 50;  // Dynamic field width
    int labelSize = state.isPortrait ? 18 : 22;
    int fieldH = 40;
    
    
    // Name - dynamic width
    float nameFieldWidth = std::min(280.0f, fieldMaxWidth);
    DrawTextApp("Name:", labelX, startY + 8, labelSize, WHITE);
    if (DrawTextInput({fieldX, startY, nameFieldWidth, (float)fieldH}, state.editor.nameBuffer, 63, 0, state.editor.focusedFieldId, state.getMousePosition(), inputBlocked)) {
        state.editor.scrollConsumed = true;
    }
    
    // Sample - responsive layout
    startY += rowHeight;
    DrawTextApp("Smpl:", labelX, startY + 8, state.isPortrait ? 16 : 20, WHITE);
    
    // User requested only filename display
    float loadBtnWidth = state.isPortrait ? 60 : 80;
    float sampleBoxWidth = std::min(300.0f, fieldMaxWidth - loadBtnWidth - 10);
    Rectangle sampleBox = {fieldX, startY, sampleBoxWidth, (float)fieldH};
    DrawRectangleRec(sampleBox, DARKGRAY);
    DrawRectangleLinesEx(sampleBox, 1, GRAY);
    
    std::string dispName = "";
    if (strlen(state.editor.samplePathBuffer) > 0) {
        dispName = fs::path(state.editor.samplePathBuffer).filename().string();
    }
    
    BeginScissorMode((int)sampleBox.x, (int)sampleBox.y, (int)sampleBox.width, (int)sampleBox.height);
    DrawTextApp(dispName.c_str(), sampleBox.x + 8, sampleBox.y + 12, 16, WHITE);
    EndScissorMode();
    
    // RESTORE MAIN CONTENT SCISSOR - critical to avoid "appearing through" header
    BeginScissorMode((int)contentScissor.x, (int)contentScissor.y, (int)contentScissor.width, (int)contentScissor.height);
    
    // Load Button - touch friendly, positioned after sample box
    Rectangle loadBtnRect = {fieldX + sampleBoxWidth + 10, startY, loadBtnWidth, (float)fieldH};
    DrawRectangleRec(loadBtnRect, BLUE);
    int loadFontSize = state.isPortrait ? 14 : 18;
    DrawTextApp("Load", loadBtnRect.x + (loadBtnRect.width - MeasureTextApp("Load", loadFontSize))/2, loadBtnRect.y + (loadBtnRect.height - loadFontSize)/2, loadFontSize, WHITE);
    
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), loadBtnRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
         state.consumeClick();
         FileBrowser::Open(state, PatternEditorState::BrowserMode::Samples);
    }
    
    // Steps
    startY += rowHeight;
    DrawTextApp("Steps:", labelX, startY + 8, labelSize, WHITE);
    if (DrawTextInput({fieldX, startY, 100, (float)fieldH}, state.editor.stepsBuffer, 4, 3, state.editor.focusedFieldId, state.getMousePosition(), inputBlocked)) {
        state.editor.scrollConsumed = true;
    }
    
    // Sync Base (for polyrhythm timing)
    startY += rowHeight;
    DrawTextApp("Sync:", labelX, startY + 8, labelSize, WHITE);
    if (DrawTextInput({fieldX, startY, 100, (float)fieldH}, state.editor.syncBaseBuffer, 4, 4, state.editor.focusedFieldId, state.getMousePosition(), inputBlocked)) {
        state.editor.scrollConsumed = true;
    }
    
    // Always Sync moved to footer for better touch access
    
    // Velocity Knob - larger for touch
    float knobX = fieldX + 260;
    float knobY = startY + 20;
    float radius = 25;
    DrawTextApp("Vel:", knobX - 80, startY + 8, labelSize, WHITE);
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
    DrawTextApp(TextFormat("%d%%", (int)(state.editor.currentVelocity*100)), knobX + 35, startY + 8, 16, LIGHTGRAY);
    
    // Mode Toggle Buttons Row - touch friendly horizontal layout
    startY += rowHeight + 10; // Move to next row
    
    // Calculate dynamic button sizes based on available width
    float availableWidth = winRect.width - 40;  // 20px padding each side
    float toggleGap = state.isPortrait ? 4 : 8;
    float toggleW = (availableWidth - toggleGap * 3) / 4;  // 4 buttons with 3 gaps
    float toggleH = state.isPortrait ? 40 : 50;
    float toggleStartX = labelX;
    int toggleFontSize = state.isPortrait ? 14 : 16;
    
    // Helper: Check if any mode is active
    bool anyModeActive = state.editor.showMelodicControls || state.editor.showFxControls || state.editor.showSlicerControls;
    
    // Step Toggle (default mode - just sample sequencing)
    Rectangle stepToggleRect = {toggleStartX, startY, toggleW, toggleH};
    DrawRectangleRec(stepToggleRect, !anyModeActive ? ORANGE : DARKGRAY);
    const char* stepTxt = "Step";
    int stepW = MeasureTextApp(stepTxt, toggleFontSize);
    DrawTextApp(stepTxt, stepToggleRect.x + (stepToggleRect.width - stepW)/2, stepToggleRect.y + (stepToggleRect.height - toggleFontSize)/2, toggleFontSize, WHITE);
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), stepToggleRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Disable all special modes to return to step mode
        state.editor.showMelodicControls = false;
        state.editor.showFxControls = false;
        state.editor.showSlicerControls = false;
    }
    
    // Melodic Toggle
    Rectangle melodyToggleRect = {toggleStartX + (toggleW + toggleGap), startY, toggleW, toggleH};
    DrawRectangleRec(melodyToggleRect, state.editor.showMelodicControls ? BLUE : DARKGRAY);
    const char* melTxt = state.isPortrait ? "Mel" : "Melody";
    int melW = MeasureTextApp(melTxt, toggleFontSize);
    DrawTextApp(melTxt, melodyToggleRect.x + (melodyToggleRect.width - melW)/2, melodyToggleRect.y + (melodyToggleRect.height - toggleFontSize)/2, toggleFontSize, WHITE);
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
    const char* fxTxt = "FX";
    int fxW = MeasureTextApp(fxTxt, toggleFontSize);
    DrawTextApp(fxTxt, fxToggleRect.x + (fxToggleRect.width - fxW)/2, fxToggleRect.y + (fxToggleRect.height - toggleFontSize)/2, toggleFontSize, WHITE);
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
    const char* slicerTxt = state.isPortrait ? "Slice" : "Slicer";
    int slicerW = MeasureTextApp(slicerTxt, toggleFontSize);
    DrawTextApp(slicerTxt, slicerToggleRect.x + (slicerToggleRect.width - slicerW)/2, slicerToggleRect.y + (slicerToggleRect.height - toggleFontSize)/2, toggleFontSize, WHITE);
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), slicerToggleRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.showSlicerControls = !state.editor.showSlicerControls;
        if (state.editor.showSlicerControls) {
            state.editor.showMelodicControls = false;
            state.editor.showFxControls = false;
        }
    }
    
    // Check for resample cycle completion (button moved to footer)
    // Handle both: UI-detected completion (via update) AND audio-thread completion (buffer full)
    if (engine.resampleManager.isRecording()) {
        int progress = engine.getPatternProgress(state.editor.currentPattern.name);
        engine.resampleManager.update(progress);  // Update state even if not completing
    }
    
    // Check if resampling completed (either way)
    if (engine.resampleManager.isComplete()) {
        // Cycle completed - transfer audio to pattern
        engine.resampleManager.transferToPattern(state.editor.currentPattern);
        engine.addPattern(state.editor.currentPattern);  // Sync to engine
        engine.stop();
        
        // Update sample path display
        strncpy(state.editor.samplePathBuffer, "[resampled]", 255);
    }
    
    startY += toggleH + 15; // Spacing before grid 
    
    // Editor Grid
    Pattern& p = state.editor.currentPattern;
    Rectangle gridRect = {winRect.x + 20, startY, winRect.width - 40, 150};
    
    int stepCount = atoi(state.editor.stepsBuffer);
    if (stepCount <= 0) stepCount = 16;
    if (stepCount > 64) stepCount = 64;
    
    // In portrait mode, use 4 columns (more rows, larger touch targets)
    // In landscape mode, use 16 columns
    int maxCols = state.isPortrait ? 4 : 16;
    int cols = std::min(maxCols, stepCount);
    int rows = (stepCount + cols - 1) / cols;
    float cellW = gridRect.width / cols;
    
    // Make steps tighter and stacked vertically with slight padding
    float stepSize = cellW * 0.95f; // 5% horizontal gap
    float verticalGap = 4.0f;
    float gridHeight = rows * stepSize + (rows > 0 ? (rows - 1) * verticalGap : 0); 
    
    // Update gridRect height to match content
    gridRect.height = gridHeight;
    
    // --- TAP VS DRAG DETECTION FOR STEP GRID ---
    // Check if current drag moved too far to be a tap
    if (state.editor.pendingStepClick >= 0 && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 currentPos = state.getMousePosition();
        float dx = currentPos.x - state.editor.stepClickStartPos.x;
        float dy = currentPos.y - state.editor.stepClickStartPos.y;
        float dist = sqrtf(dx*dx + dy*dy);
        if (dist > 15.0f) {  // 15px threshold for tap vs drag
            state.editor.stepClickMoved = true;
        }
    }
    
    // Process pending tap on release
    int tappedStep = -1;
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        if (state.editor.pendingStepClick >= 0 && !state.editor.stepClickMoved) {
            tappedStep = state.editor.pendingStepClick;  // This was a tap, process it
        }
        // Always reset pending click state on release
        state.editor.pendingStepClick = -1;
        state.editor.stepClickMoved = false;
    }
    
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
            DrawTextApp(pText, x + stepSize/2 - MeasureTextApp(pText, fontSize)/2, y + stepSize/2 - fontSize/2, fontSize, BLACK);
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
                    DrawTextApp(sText, x + stepSize - MeasureTextApp(sText, fontSize) - 2, y + stepSize - fontSize - 2, fontSize, YELLOW);
                }
            }
        }
        
        bool inViewport = CheckCollisionPointRec(state.getMousePosition(), {winRect.x, winRect.y+50, winRect.width, winRect.height-100});
        
        // On PRESS: Record pending click (don't process yet)
        if (!inputBlocked && inViewport && CheckCollisionPointRec(state.getMousePosition(), stepRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.editor.pendingStepClick = i;
            state.editor.stepClickStartPos = state.getMousePosition();
            state.editor.stepClickMoved = false;
        }
        
        // Process step action only if this step was TAPPED (not dragged)
        if (tappedStep == i) {
            
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
    
    // Draw beat sync dividers in the pattern editor grid
    int syncBase = p.syncBase;
    if (syncBase > 0 && syncBase < stepCount) {
        for (int i = syncBase; i < stepCount; i += syncBase) {
            int col = i % cols;
            int row = i / cols;
            
            // Draw at left edge of this step
            float lineX = gridRect.x + col * cellW;
            float lineY = gridRect.y + row * (stepSize + verticalGap);
            
            // Use teal color, matching step height
            DrawLineEx({lineX, lineY}, {lineX, lineY + stepSize}, 2.0f, Color{0, 180, 180, 255});
        }
    }
    
    startY += gridHeight + 20; // Dynamic spacing based on grid height 
    
    // Slice Selector Buttons (below grid, visible in any mode if slices exist)
    int sliceCount = (int)p.sliceMarkers.size();
    if (sliceCount > 0) {
        int buttonsPerRow = 8;
        float btnW = 35; float btnH = 25;
        
        DrawTextApp("Slice:", winRect.x + 20, startY, 16, WHITE);
        
        float btnStartX = winRect.x + 100; // Increased from 80 to prevent overlap
        for (int s = 0; s < sliceCount; ++s) {
            int row = s / buttonsPerRow;
            int col = s % buttonsPerRow;
            Rectangle sBtn = {btnStartX + col * (btnW + 3), startY + row * (btnH + 3), btnW, btnH};
            
            bool isSelected = (state.editor.selectedSliceIndex == s);
            DrawRectangleRec(sBtn, isSelected ? GREEN : DARKGRAY);
            const char* sIdxTxt = TextFormat("%d", s);
            int sIdxW = MeasureTextApp(sIdxTxt, 10);
            DrawTextApp(sIdxTxt, sBtn.x + (sBtn.width - sIdxW)/2, sBtn.y + (sBtn.height - 10)/2, 10, WHITE);
            
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
        // Portrait mode: Stack octave controls and piano vertically
        // Landscape mode: Octave on left, piano to right
        
        if (state.isPortrait) {
            // Row 1: Octave controls centered
            float octRowY = startY;
            float octStartX = winRect.x + 20;
            DrawTextApp(TextFormat("Oct: %d", state.editor.selectedOctave), octStartX, octRowY + 12, 20, WHITE);
            
            Rectangle octDown = {octStartX + 100, octRowY, 60, 45};
            DrawRectangleRec(octDown, DARKGRAY);
            DrawTextApp("-", octDown.x + 25, octDown.y + 10, 24, WHITE);
            if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), octDown) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) state.editor.selectedOctave--;
            
            Rectangle octUp = {octStartX + 170, octRowY, 60, 45};
            DrawRectangleRec(octUp, DARKGRAY);
            DrawTextApp("+", octUp.x + 25, octUp.y + 10, 24, WHITE);
            if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), octUp) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) state.editor.selectedOctave++;
            
            startY += 55; // Move to next row
            
            // Row 2: Full-width piano keyboard
            const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            float pianoWidth = winRect.width - 40;
            float keyX = winRect.x + 20;
            float keyY = startY;
            float whiteKeyWidth = pianoWidth / 7;  // 7 white keys
            float blackKeyWidth = whiteKeyWidth * 0.65f;
            float keyHeight = 120; // Taller keys for Android touch (was 80)
            
            // White keys first
            float whiteX = keyX;
            for (int i = 0; i < 12; ++i) {
                bool isBlack = (i==1 || i==3 || i==6 || i==8 || i==10);
                if (!isBlack) {
                    Rectangle keyRect = {whiteX, keyY, whiteKeyWidth, keyHeight};
                    bool isSelected = (state.editor.selectedNote == i);
                    DrawRectangleRec(keyRect, isSelected ? YELLOW : WHITE);
                    DrawRectangleLinesEx(keyRect, 2, BLACK);
                    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), keyRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        state.editor.selectedNote = i;
                        if (state.editor.selectedStep != -1 && state.editor.stepStates[state.editor.selectedStep]) {
                            int shift = (state.editor.selectedOctave - 4) * 12 + i;
                            p.stepPitches[state.editor.selectedStep + 1] = shift;
                            engine.addPattern(p);
                        }
                    }
                    int fontSize = (int)(whiteKeyWidth * 0.35f);
                    if (fontSize < 10) fontSize = 10;
                    DrawTextApp(notes[i], keyRect.x + (keyRect.width - MeasureTextApp(notes[i], fontSize))/2, keyRect.y + keyHeight - fontSize - 5, fontSize, BLACK);
                    whiteX += whiteKeyWidth;
                }
            }
            
            // Black keys on top
            for (int i = 0; i < 12; ++i) {
                bool isBlack = (i==1 || i==3 || i==6 || i==8 || i==10);
                if (isBlack) {
                    float xPos = 0;
                    if(i==1) xPos = whiteKeyWidth * 1 - (blackKeyWidth/2);
                    if(i==3) xPos = whiteKeyWidth * 2 - (blackKeyWidth/2);
                    if(i==6) xPos = whiteKeyWidth * 4 - (blackKeyWidth/2);
                    if(i==8) xPos = whiteKeyWidth * 5 - (blackKeyWidth/2);
                    if(i==10) xPos = whiteKeyWidth * 6 - (blackKeyWidth/2);
                    Rectangle keyRect = {keyX + xPos, keyY, blackKeyWidth, keyHeight * 0.6f};
                    bool isSelected = (state.editor.selectedNote == i);
                    DrawRectangleRec(keyRect, isSelected ? YELLOW : BLACK);
                    DrawRectangleLinesEx(keyRect, 1, DARKGRAY);
                    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), keyRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        state.editor.selectedNote = i;
                        if (state.editor.selectedStep != -1 && state.editor.stepStates[state.editor.selectedStep]) {
                            int shift = (state.editor.selectedOctave - 4) * 12 + i;
                            p.stepPitches[state.editor.selectedStep + 1] = shift;
                            engine.addPattern(p);
                        }
                    }
                }
            }
            startY += keyHeight + 10;
            
        } else {
            // Landscape mode: Original horizontal layout
            DrawTextApp(TextFormat("Oct: %d", state.editor.selectedOctave), winRect.x + 20, startY + 15, 24, WHITE);
            
            Rectangle octDown = {winRect.x + 140, startY, 50, 50};
            DrawRectangleRec(octDown, DARKGRAY);
            DrawTextApp("-", octDown.x + 20, octDown.y + 12, 28, WHITE);
            if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), octDown) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) state.editor.selectedOctave--;
            
            Rectangle octUp = {winRect.x + 200, startY, 50, 50};
            DrawRectangleRec(octUp, DARKGRAY);
            DrawTextApp("+", octUp.x + 20, octUp.y + 12, 28, WHITE);
            if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), octUp) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) state.editor.selectedOctave++;
            
            const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            float keyX = winRect.x + 270;
            float keyY = startY; 
            float whiteKeyWidth = 45;
            float blackKeyWidth = 30;
            float keyHeight = 100;
            
            // White keys
            for (int i = 0; i < 12; ++i) {
                bool isBlack = (i==1 || i==3 || i==6 || i==8 || i==10);
                if (!isBlack) {
                    Rectangle keyRect = {keyX, keyY, whiteKeyWidth, keyHeight};
                    bool isSelected = (state.editor.selectedNote == i);
                    DrawRectangleRec(keyRect, isSelected ? YELLOW : WHITE);
                    DrawRectangleLinesEx(keyRect, 2, BLACK);
                    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), keyRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        state.editor.selectedNote = i;
                        if (state.editor.selectedStep != -1 && state.editor.stepStates[state.editor.selectedStep]) {
                            int shift = (state.editor.selectedOctave - 4) * 12 + i;
                            p.stepPitches[state.editor.selectedStep + 1] = shift;
                            engine.addPattern(p);
                        }
                    }
                    DrawTextApp(notes[i], keyRect.x + 12, keyRect.y + keyHeight - 25, 16, BLACK);
                    keyX += whiteKeyWidth;
                }
            }
            
            // Black keys
            keyX = winRect.x + 270;
            for (int i = 0; i < 12; ++i) {
                bool isBlack = (i==1 || i==3 || i==6 || i==8 || i==10);
                if (isBlack) {
                    float xPos = 0;
                    if(i==1) xPos = whiteKeyWidth * 1 - (blackKeyWidth/2);
                    if(i==3) xPos = whiteKeyWidth * 2 - (blackKeyWidth/2);
                    if(i==6) xPos = whiteKeyWidth * 4 - (blackKeyWidth/2);
                    if(i==8) xPos = whiteKeyWidth * 5 - (blackKeyWidth/2);
                    if(i==10) xPos = whiteKeyWidth * 6 - (blackKeyWidth/2);
                    Rectangle keyRect = {keyX + xPos, keyY, blackKeyWidth, keyHeight * 0.6f};
                    bool isSelected = (state.editor.selectedNote == i);
                    DrawRectangleRec(keyRect, isSelected ? YELLOW : BLACK);
                    DrawRectangleLinesEx(keyRect, 1, DARKGRAY);
                    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), keyRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        state.editor.selectedNote = i;
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
    }

    // FX Controls (Modular)
    if (state.editor.showFxControls) {
        Rectangle fxArea = {winRect.x + 20, startY, winRect.width - 40, 0};
        // Define parent scissor rect based on PatternEditor's viewport
        Rectangle parentScissor = {(float)(int)winRect.x, (float)(int)winRect.y + 50, (float)(int)winRect.width, (float)(int)winRect.height - 100};
        float heightUsed = fxControls.Draw(fxArea, state.editor.currentPattern, parentScissor);
        startY += heightUsed;
    }

    // Slicer Controls (now extracted to SampleSlicer component)
    if (state.editor.showSlicerControls) {
        Rectangle slicerArea = {winRect.x, startY, winRect.width, winRect.height - startY - 60};
        float heightUsed = sampleSlicer.Draw(slicerArea, p, inputBlocked);
        startY += heightUsed;
    }
    
    // Update Content Height
    state.editor.contentHeight = (startY + 50) - (winRect.y + 60) + state.editor.scrollOffsetY; 
    


    EndScissorMode();
    
    // --- HEADER / FOOTER OVERLAYS (Drawn last to block scrolling items) ---
    
    if (!state.editor.showFileBrowser) {
        // 1. Header Bar
        DrawRectangle(winRect.x, winRect.y, winRect.width, 60, Color{30, 30, 30, 255});
        DrawTextApp("Edit Pattern", winRect.x + 20, winRect.y + 15, 24, WHITE);
        
        // Close Button (Fixed in Header)
        Rectangle closeRect = {winRect.x + winRect.width - 55, winRect.y + 5, 50, 50};
        DrawRectangleRec(closeRect, RED);
        DrawTextApp("X", closeRect.x + 18, closeRect.y + 12, 24, WHITE);
        
        if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), closeRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (state.editor.isPreviewing) engine.stop();
            state.editor.isPreviewing = false;
            state.editor.isOpen = false;
            state.editor.showFileBrowser = false;
            state.editor.focusedFieldId = -1;
        }
    }

    // 2. Footer Bar
    DrawRectangle(winRect.x, winRect.y + winRect.height - 65, winRect.width, 65, Color{30, 30, 30, 255}); // Footer BG

    // Footer buttons - dynamically sized
    float footerY = winRect.y + winRect.height - 60;
    float footerBtnH = state.isPortrait ? 55 : 45;
    float footerGap = state.isPortrait ? 5 : 8;
    int footerFontSize = state.isPortrait ? 16 : 16;
    
    // Calculate button widths to fit screen
    float availFooterWidth = winRect.width - 20;  // 10px padding each side
    
    // 6 buttons in both modes: Copy/Paste, Edit, Commit, Play, Resample, Save
    int numButtons = 6;
    float footerBtnW = (availFooterWidth - (numButtons - 1) * footerGap) / numButtons;
    
    float btnX = winRect.x + 10;

    // Copy/Paste Toggle Button (single button)
    Rectangle copyPasteRect = {btnX, footerY, footerBtnW, footerBtnH};
    bool hasClipboard = state.editor.clipboard.hasData;
    bool copyActive = state.editor.clipboard.isCopyMode;
    bool pasteActive = state.editor.clipboard.isPasteMode;
    
    // Determine button state
    const char* cpTxt;
    Color cpColor;
    if (copyActive) {
        cpTxt = "Cancel";
        cpColor = ORANGE;
    } else if (hasClipboard && pasteActive) {
        cpTxt = "Cancel";
        cpColor = MAGENTA;
    } else if (hasClipboard) {
        cpTxt = "Paste";
        cpColor = MAGENTA;
    } else {
        cpTxt = "Copy";
        cpColor = DARKGRAY;
    }
    
    DrawRectangleRec(copyPasteRect, cpColor);
    int cpW = MeasureTextApp(cpTxt, footerFontSize);
    DrawTextApp(cpTxt, copyPasteRect.x + (copyPasteRect.width - cpW)/2, copyPasteRect.y + (copyPasteRect.height - footerFontSize)/2, footerFontSize, WHITE);
    
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), copyPasteRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (copyActive || pasteActive) {
            // Cancel mode
            state.editor.clipboard.isCopyMode = false;
            state.editor.clipboard.isPasteMode = false;
        } else if (hasClipboard) {
            // Enter paste mode
            state.editor.clipboard.isPasteMode = true;
            state.editor.clipboard.isCopyMode = false;
        } else {
            // Enter copy mode
            state.editor.clipboard.isCopyMode = true;
            state.editor.clipboard.isPasteMode = false;
        }
        state.editor.clipboard.isEditMode = false;
    }
    btnX += footerBtnW + footerGap;

    // Edit Button (select step to edit velocity/pitch)
    Rectangle editRect = {btnX, footerY, footerBtnW, footerBtnH};
    bool editActive = state.editor.clipboard.isEditMode;
    DrawRectangleRec(editRect, editActive ? SKYBLUE : DARKGRAY);
    const char* editTxt = "Edit";
    int editW = MeasureTextApp(editTxt, footerFontSize);
    DrawTextApp(editTxt, editRect.x + (editRect.width - editW)/2, editRect.y + (editRect.height - footerFontSize)/2, footerFontSize, WHITE);
    
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), editRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.clipboard.isEditMode = !state.editor.clipboard.isEditMode;
        state.editor.clipboard.isCopyMode = false;
        state.editor.clipboard.isPasteMode = false;
    }
    btnX += footerBtnW + footerGap;

    
    // Commit Button (Save without closing)
    Rectangle commitRect = {btnX, footerY, footerBtnW, footerBtnH};
    DrawRectangleRec(commitRect, Color{0, 150, 0, 255}); // Green
    const char* commitTxt = "Commit";
    int commitW = MeasureTextApp(commitTxt, footerFontSize);
    DrawTextApp(commitTxt, commitRect.x + (commitRect.width - commitW)/2, commitRect.y + (commitRect.height - footerFontSize)/2, footerFontSize, WHITE);
    
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), commitRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Save pattern without closing
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
        
        // Rename in columns if name changed
        std::string oldName = state.editor.originalName;
        if (p.name != oldName) {
            for (auto& col : state.columns) {
                std::replace(col.patternNames.begin(), col.patternNames.end(), oldName, p.name);
            }
            strncpy(state.editor.originalName, p.name.c_str(), sizeof(state.editor.originalName) - 1);
        }
        // Don't close - stay in editor
    }
    btnX += footerBtnW + footerGap;

    // Preview/Stop Button
    Rectangle previewRect = {btnX, footerY, footerBtnW, footerBtnH};
    
    if (state.editor.isPreviewing) {
        DrawRectangleRec(previewRect, RED);
        const char* stopTxt = "Stop";
        int stopW = MeasureTextApp(stopTxt, footerFontSize);
        DrawTextApp(stopTxt, previewRect.x + (previewRect.width - stopW)/2, previewRect.y + (previewRect.height - footerFontSize)/2, footerFontSize, WHITE);
    } else {
        DrawRectangleRec(previewRect, LIME);
        const char* prevTxt = state.isPortrait ? "Play" : "Preview";
        int prevW = MeasureTextApp(prevTxt, footerFontSize);
        DrawTextApp(prevTxt, previewRect.x + (previewRect.width - prevW)/2, previewRect.y + (previewRect.height - footerFontSize)/2, footerFontSize, BLACK);
    }
    
    if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), previewRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (state.editor.isPreviewing) {
            engine.stop();
            state.editor.isPreviewing = false;
        } else {
            p.name = state.editor.nameBuffer;
            p.steps = atoi(state.editor.stepsBuffer);
            p.syncBase = atoi(state.editor.syncBaseBuffer);
            if (p.syncBase <= 0) p.syncBase = p.steps;
            engine.addPattern(p);
            
            bool assigned = false;
            for (const auto& col : state.columns) {
                for (const auto& pn : col.patternNames) {
                    if (pn == p.name) {
                        engine.assignPatternToTrack(p.name, col.trackName);
                        assigned = true;
                        break;
                    }
                }
                if (assigned) break;
            }
            
            if (!assigned && state.editor.sourceColumnIndex >= 0 && state.editor.sourceColumnIndex < (int)state.columns.size()) {
                engine.assignPatternToTrack(p.name, state.columns[(size_t)state.editor.sourceColumnIndex].trackName);
                assigned = true;
            }
            
            if (!assigned && !state.columns.empty()) {
                engine.assignPatternToTrack(p.name, state.columns[0].trackName);
            }
            
            engine.playPattern(p.name);
            state.editor.isPreviewing = true;
        }
    }
    btnX += footerBtnW + footerGap;

    // Resample Button
    Rectangle resampleRect = {btnX, footerY, footerBtnW, footerBtnH};
    bool isResampling = engine.resampleManager.isRecording();
        
        if (isResampling) {
            DrawRectangleRec(resampleRect, RED);
            const char* recTxt = "REC";
            int recW = MeasureTextApp(recTxt, footerFontSize);
            DrawTextApp(recTxt, resampleRect.x + (resampleRect.width - recW)/2, resampleRect.y + (resampleRect.height - footerFontSize)/2, footerFontSize, WHITE);
        } else {
            DrawRectangleRec(resampleRect, Color{255, 140, 0, 255});
            const char* resTxt = "Resamp";
            int resW = MeasureTextApp(resTxt, footerFontSize);
            DrawTextApp(resTxt, resampleRect.x + (resampleRect.width - resW)/2, resampleRect.y + (resampleRect.height - footerFontSize)/2, footerFontSize, WHITE);
        }
        
        if (!inputBlocked && CheckCollisionPointRec(state.getMousePosition(), resampleRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (!isResampling) {
                int steps = atoi(state.editor.stepsBuffer);
                if (steps <= 0) steps = 16;
                engine.resampleManager.start(steps, engine.getBPM(), 44100.0);
                engine.playPattern(state.editor.currentPattern.name);
            } else {
                engine.resampleManager.stop();
                engine.stop();
            }
    }
    btnX += footerBtnW + footerGap;

    // Save+Exit Button (draws in the last button slot)
    Rectangle saveRect = {btnX, footerY, footerBtnW, footerBtnH};
    const char* saveTxt = state.isPortrait ? "Save" : "Save+Exit";
    DrawRectangleRec(saveRect, BLUE);
    int saveW = MeasureTextApp(saveTxt, 14); // Keep 14 to fit
    DrawTextApp(saveTxt, saveRect.x + (saveRect.width - saveW)/2, saveRect.y + (saveRect.height - 14)/2, 14, WHITE);
    
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
        
        // Always close on Save+Exit
        if (state.editor.isPreviewing) engine.stop();
        state.editor.isPreviewing = false;
        state.editor.isOpen = false;
        state.editor.showFileBrowser = false;
        state.editor.focusedFieldId = -1;
    }
    
    // Draw Outline Last (to cover footer overlap)
    DrawRectangleLinesEx(winRect, 2, Color{200, 200, 200, 255});
    
    // --- LATE UPDATE SCROLL LOGIC ---
    // We update scroll offset here because children (like FXControls) might have consumed the scroll event.
    Rectangle scrollArea = {winRect.x, winRect.y + 60, winRect.width, winRect.height - 110};
    float maxScroll = std::max(0.0f, state.editor.contentHeight - (winRect.height - 100));
    
    if (!state.editor.scrollConsumed && !state.editor.showFileBrowser) {
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




} // namespace gui
