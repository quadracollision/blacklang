#include "TransportBar.h"
#include "Widgets.h"
#include "../GuiState.h"
#include "../AudioEngine.h"
#include "../ProjectFile.h"
#include "../tinyfiledialogs.h"
#include <cstdlib>

namespace gui {

TransportBar::TransportBar(GuiState& s, AudioEngine& e) : state(s), engine(e) {}

void TransportBar::Draw() {
    Rectangle rect = {0, (float)GetScreenHeight() - state.FOOTER_HEIGHT, (float)GetScreenWidth(), (float)state.FOOTER_HEIGHT};
    DrawRectangleRec(rect, Color{25, 25, 25, 255});
    
    DrawPlayStop();
    DrawRecording();
    DrawBPM();
    DrawCopyPaste();
    DrawEditShift();
    DrawSettingsButton();
    DrawSettingsPopup();
}

void TransportBar::DrawRecording() {
    Rectangle rect = {0, (float)GetScreenHeight() - state.FOOTER_HEIGHT, (float)GetScreenWidth(), (float)state.FOOTER_HEIGHT};
    float centerX = GetScreenWidth() / 2.0f;
    
    // Record Button (Beside Stop)
    Rectangle recBtn = {centerX + 60, rect.y + 10, 40, 40};
    bool isRecording = state.recorder.isRecording;
    
    if (isRecording) {
        // Red Circle when recording
        DrawCircle(recBtn.x + 20, recBtn.y + 20, 18, RED);
    } else {
        // Gray button with Red Dot
        DrawRectangleRec(recBtn, LIGHTGRAY);
        DrawCircle(recBtn.x + 20, recBtn.y + 20, 10, RED);
    }
    
    if (!state.editor.isOpen && CheckCollisionPointRec(GetMousePosition(), recBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (state.recorder.isRecording) {
            // Stop Recording
            state.recorder.isRecording = false;
            state.recorder.showControls = false;
            engine.stopRecording();
        } else {
            // Open Controls
            state.recorder.showControls = !state.recorder.showControls;
        }
    }
    
    // Recording Controls (Stems/Whole, Name)
    if (state.recorder.showControls && !state.recorder.isRecording) {
        float panelX = recBtn.x + 50;
        float panelY = rect.y + 10;
        float panelW = 340;
        float panelH = 40;
        
        // Panel Background
        DrawRectangle(panelX, panelY, panelW, panelH, DARKGRAY);
        DrawRectangleLines(panelX, panelY, panelW, panelH, LIGHTGRAY);
        
        // Name Input
        // "small text box with a black background and white boarder"
        DrawText("Name:", panelX + 10, panelY + 12, 14, WHITE);
        
        Rectangle nameBox = {panelX + 60, panelY + 8, 100, 24};
        DrawRectangleRec(nameBox, BLACK);
        DrawRectangleLinesEx(nameBox, 1, WHITE);
        DrawText(state.recorder.filenameBuffer, nameBox.x + 5, nameBox.y + 4, 14, WHITE);
        
        // Simple cursor simulation (blink?) or just static for now
        if ((int)(GetTime() * 2) % 2 == 0) {
            int len = MeasureText(state.recorder.filenameBuffer, 14);
            DrawText("|", nameBox.x + 5 + len, nameBox.y + 4, 14, WHITE);
        }
        
        // Handle Input (basic)
        int key = GetCharPressed();
        while (key > 0) {
            if ((key >= 32) && (key <= 125)) {
                int len = strlen(state.recorder.filenameBuffer);
                if (len < 63) {
                    state.recorder.filenameBuffer[len] = (char)key;
                    state.recorder.filenameBuffer[len+1] = '\0';
                }
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
             int len = strlen(state.recorder.filenameBuffer);
             if (len > 0) state.recorder.filenameBuffer[len-1] = '\0';
        }

        // Switch: "Stems" or "Whole"
        Rectangle switchRect = {panelX + 170, panelY + 8, 100, 24};
        
        // Draw Switch (Toggle Look)
        // [ Whole | Stems ]
        DrawRectangleRec(switchRect, GRAY);
        
        Rectangle wholeRect = {switchRect.x, switchRect.y, 50, 24};
        Rectangle stemsRect = {switchRect.x + 50, switchRect.y, 50, 24};
        
        if (!state.recorder.recordStems) {
            DrawRectangleRec(wholeRect, WHITE);
            DrawText("Whole", wholeRect.x + 5, wholeRect.y + 5, 12, BLACK);
            DrawText("Stems", stemsRect.x + 5, stemsRect.y + 5, 12, WHITE);
        } else {
            DrawRectangleRec(stemsRect, WHITE);
            DrawText("Whole", wholeRect.x + 5, wholeRect.y + 5, 12, WHITE);
            DrawText("Stems", stemsRect.x + 5, stemsRect.y + 5, 12, BLACK);
        }
        
        if (CheckCollisionPointRec(GetMousePosition(), wholeRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.recorder.recordStems = false;
        }
        if (CheckCollisionPointRec(GetMousePosition(), stemsRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.recorder.recordStems = true;
        }
        
        // Save Button
        Rectangle saveBtn = {panelX + 280, panelY + 8, 50, 24};
        DrawRectangleRec(saveBtn, GREEN);
        DrawText("Save", saveBtn.x + 10, saveBtn.y + 5, 14, BLACK);
        
        if (CheckCollisionPointRec(GetMousePosition(), saveBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            // Start Recording
            state.recorder.isRecording = true;
            state.recorder.showControls = false;
            
            // Call Engine
            engine.startRecording(state.recorder.filenameBuffer, state.recorder.recordStems);
        }
    }
}

void TransportBar::DrawPlayStop() {
    Rectangle rect = {0, (float)GetScreenHeight() - state.FOOTER_HEIGHT, (float)GetScreenWidth(), (float)state.FOOTER_HEIGHT};
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
                    // Assign pattern to track
                    engine.assignPatternToTrack(pName, state.columns[colIdx].trackName);
                }
            }
        } else {
            // All patterns mode - assign each pattern to its column's track
            for (size_t colIdx = 0; colIdx < state.columns.size(); ++colIdx) {
                const auto& col = state.columns[colIdx];
                for (const auto& pName : col.patternNames) {
                    if (!pName.empty()) {
                        names.push_back(pName);
                        // Assign pattern to track
                        engine.assignPatternToTrack(pName, col.trackName);
                    }
                }
            }
        }

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
}

void TransportBar::DrawBPM() {
    Rectangle rect = {0, (float)GetScreenHeight() - state.FOOTER_HEIGHT, (float)GetScreenWidth(), (float)state.FOOTER_HEIGHT};
    
    // Move to left side, after Edit/Shift (approx 250px)
    float bpmX = 260.0f; 
    
    DrawText("BPM:", bpmX, rect.y + 10, 20, LIGHTGRAY);
    if (!state.editor.isOpen) {
        DrawTextInput({bpmX + 50, rect.y + 10, 60, 25}, state.globalBpmBuffer, 5, 999, state.focusedFieldId);
    } else {
        DrawRectangleRec({bpmX + 50, rect.y + 10, 60, 25}, LIGHTGRAY);
        DrawText(state.globalBpmBuffer, bpmX + 55, rect.y + 15, 20, BLACK);
    }

    int newBpm = atoi(state.globalBpmBuffer);
    if (newBpm > 20 && newBpm < 300 && newBpm != state.bpm) {
        state.bpm = newBpm;
        engine.setBPM(newBpm);
    }
}

void TransportBar::DrawCopyPaste() {
    Rectangle rect = {0, (float)GetScreenHeight() - state.FOOTER_HEIGHT, (float)GetScreenWidth(), (float)state.FOOTER_HEIGHT};
    
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
}

void TransportBar::DrawEditShift() {
    Rectangle rect = {0, (float)GetScreenHeight() - state.FOOTER_HEIGHT, (float)GetScreenWidth(), (float)state.FOOTER_HEIGHT};
    
    if (state.isPlaying && !state.activePatternSlots.empty()) {
        Rectangle editBtn = {145, rect.y + 15, 45, 30};
        DrawRectangleRec(editBtn, state.isLiveEditMode ? SKYBLUE : DARKGRAY);
        DrawText("Edit", editBtn.x + 5, editBtn.y + 8, 14, WHITE);
        
        if (CheckCollisionPointRec(GetMousePosition(), editBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.isLiveEditMode = !state.isLiveEditMode;
            
            if (!state.isLiveEditMode) {
                state.editor.isOpen = false;
                state.isShiftMode = false;
                state.shiftEditingPatternName = "";
            }
        }
        
        if (state.isLiveEditMode) {
            Rectangle shiftBtn = {195, rect.y + 15, 45, 30};
            DrawRectangleRec(shiftBtn, state.isShiftMode ? ORANGE : DARKGRAY);
            DrawText("Shift", shiftBtn.x + 3, shiftBtn.y + 8, 12, WHITE);
            
            if (CheckCollisionPointRec(GetMousePosition(), shiftBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                state.isShiftMode = !state.isShiftMode;
                if (!state.isShiftMode) {
                    state.shiftEditingPatternName = "";
                }
            }
        }
    }
}

void TransportBar::DrawSettingsButton() {
    Rectangle rect = {0, (float)GetScreenHeight() - state.FOOTER_HEIGHT, (float)GetScreenWidth(), (float)state.FOOTER_HEIGHT};
    
    // Move next to BPM (BPM input ends around 260+50+60 = 370)
    float gearX = 380.0f;
    Rectangle gearBtn = {gearX, rect.y + 15, 30, 30};
    bool isOpen = (state.settings.activePopup != PopupType::None);
    DrawRectangleRec(gearBtn, isOpen ? ORANGE : DARKGRAY);
    DrawText("*", gearBtn.x + 8, gearBtn.y + 3, 24, WHITE);
    
    if (CheckCollisionPointRec(GetMousePosition(), gearBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (isOpen) {
            state.settings.activePopup = PopupType::None;
            state.settings.showSettingsMenu = false;
        } else {
            state.settings.activePopup = PopupType::Main;
            state.settings.showSettingsMenu = true;
        }
    }
}

void TransportBar::DrawSettingsPopup() {
    if (state.settings.activePopup == PopupType::None) return;
    
    Rectangle rect = {0, (float)GetScreenHeight() - state.FOOTER_HEIGHT, (float)GetScreenWidth(), (float)state.FOOTER_HEIGHT};
    
    float popW = 350;
    float popH = 200;
    // Align with new gear button (x=380)
    float popX = 380.0f;
    float popY = rect.y - popH - 10;
    
    DrawRectangle(popX, popY, popW, popH, Color{40, 40, 40, 245});
    DrawRectangleLinesEx({popX, popY, popW, popH}, 2, WHITE);
    
    // Header
    const char* title = "Settings";
    if (state.settings.activePopup == PopupType::Project) title = "Project";
    else if (state.settings.activePopup == PopupType::Audio) title = "Audio Device";
    
    DrawText(title, popX + 10, popY + 10, 18, WHITE);
    
    // Close Button (X)
    Rectangle closeBtn = {popX + popW - 30, popY + 5, 25, 25};
    DrawRectangleRec(closeBtn, RED);
    DrawText("X", closeBtn.x + 7, closeBtn.y + 3, 18, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), closeBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.settings.activePopup = PopupType::None;
        state.settings.showSettingsMenu = false;
    }

    // Back Button (if submenu)
    if (state.settings.activePopup != PopupType::Main) {
        Rectangle backBtn = {popX + popW - 60, popY + 5, 25, 25};
        DrawRectangleRec(backBtn, DARKGRAY);
        DrawText("<", backBtn.x + 8, backBtn.y + 3, 18, WHITE);
        if (CheckCollisionPointRec(GetMousePosition(), backBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.settings.activePopup = PopupType::Main;
        }
    }
    
    float currentY = popY + 45;
    
    if (state.settings.activePopup == PopupType::Main) {
        // --- Main Menu ---
        Rectangle projBtn = {popX + 20, currentY, popW - 40, 30};
        DrawRectangleRec(projBtn, DARKGRAY);
        DrawText("Project...", projBtn.x + 10, projBtn.y + 5, 14, WHITE);
        if (CheckCollisionPointRec(GetMousePosition(), projBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.settings.activePopup = PopupType::Project;
        }
        
        Rectangle audioBtn = {popX + 20, currentY + 40, popW - 40, 30};
        DrawRectangleRec(audioBtn, DARKGRAY);
        DrawText("Audio Settings...", audioBtn.x + 10, audioBtn.y + 5, 14, WHITE);
        if (CheckCollisionPointRec(GetMousePosition(), audioBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.settings.activePopup = PopupType::Audio;
            state.settings.availableOutputDevices = engine.getAvailableOutputDevices();
            state.settings.currentDevice = engine.getCurrentOutputDevice();
        }
    }
    else if (state.settings.activePopup == PopupType::Project) {
        // --- Project Menu ---
        DrawText("Manage Project:", popX + 20, currentY, 14, LIGHTGRAY);
        currentY += 30;
        
        Rectangle saveBtn = {popX + 20, currentY, 140, 30};
        DrawRectangleRec(saveBtn, DARKGRAY);
        DrawText("Save Project", saveBtn.x + 25, saveBtn.y + 8, 14, WHITE);
        if (CheckCollisionPointRec(GetMousePosition(), saveBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            const char* filters[] = { "*.json" };
            const char* path = tinyfd_saveFileDialog("Save Project", "project.json", 1, filters, "JSON Project Files");
            if (path) {
                // Convert Layout
                std::vector<SerializedColumn> cols;
                for (const auto& col : state.columns) {
                    cols.push_back({col.title, col.patternNames});
                }
                ProjectFile::save(path, engine.getPatterns(), state.activeChain, cols);
                state.settings.activePopup = PopupType::None; // Close after action
                state.settings.showSettingsMenu = false;
            }
        }
        
        Rectangle loadBtn = {popX + 170, currentY, 140, 30};
        DrawRectangleRec(loadBtn, DARKGRAY);
        DrawText("Load Project", loadBtn.x + 25, loadBtn.y + 8, 14, WHITE);
        if (CheckCollisionPointRec(GetMousePosition(), loadBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            const char* filters[] = { "*.json" };
            const char* path = tinyfd_openFileDialog("Load Project", "", 1, filters, "JSON Project Files", 0);
            if (path) {
                std::map<std::string, Pattern> patterns;
                std::vector<SerializedColumn> loadedCols;
                
                if (ProjectFile::load(path, patterns, state.activeChain, loadedCols)) {
                    engine.stop();
                    for (auto& [name, pat] : patterns) {
                        if (!pat.samplePath.empty()) {
                            engine.loadSample(pat); 
                        }
                        engine.addPattern(pat);
                    }
                    state.columns.clear();
                    state.activePatternSlots.clear();
                    
                    if (!loadedCols.empty()) {
                        for (const auto& sCol : loadedCols) {
                            state.columns.push_back({sCol.title, sCol.patternNames, {0,0,0,0}, 0.0f});
                        }
                    } else {
                        state.columns.resize(4);
                        int colIdx = 0;
                        for (const auto& [name, pat] : patterns) {
                           state.columns[colIdx].patternNames.push_back(name);
                        }
                    }
                    state.settings.activePopup = PopupType::None; // Close after action
                    state.settings.showSettingsMenu = false;
                }
            }
        }
    }
    else if (state.settings.activePopup == PopupType::Audio) {
        // --- Audio Menu ---
        DrawText("Output Device:", popX + 20, currentY, 14, LIGHTGRAY);
        
        // Show switching indicator
        if (state.settings.isSwitchingDevice) {
            DrawText("Switching...", popX + 130, currentY, 14, ORANGE);
        } else {
            DrawText(state.settings.currentDevice.c_str(), popX + 130, currentY, 14, GREEN);
        }
        
        float listY = currentY + 30;
        for (size_t i = 0; i < state.settings.availableOutputDevices.size() && i < 5; ++i) {
            const auto& devName = state.settings.availableOutputDevices[i];
            Rectangle devBtn = {popX + 20, listY, popW - 40, 22};
            
            bool isCurrent = (devName == state.settings.currentDevice);
            bool isSwitching = state.settings.isSwitchingDevice;
            
            // Gray out if switching
            Color bgColor = isCurrent ? Color{60, 120, 60, 255} : 
                           isSwitching ? Color{40, 40, 40, 255} : Color{60, 60, 60, 255};
            Color textColor = isSwitching ? GRAY : WHITE;
            
            DrawRectangleRec(devBtn, bgColor);
            DrawText(devName.c_str(), devBtn.x + 5, devBtn.y + 4, 12, textColor);
            
            // Only allow clicking if not currently switching and not the current device
            if (!isCurrent && !isSwitching && CheckCollisionPointRec(GetMousePosition(), devBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                state.settings.isSwitchingDevice = true;
                
                // Use async switching - capture state pointer
                GuiState* statePtr = &state;
                engine.setOutputDeviceAsync(devName, [statePtr, devName](bool success) {
                    if (success) {
                        statePtr->settings.currentDevice = devName;
                    }
                    statePtr->settings.isSwitchingDevice = false;
                });
            }
            listY += 25;
        }
         if (state.settings.availableOutputDevices.empty()) {
            DrawText("No devices found", popX + 20, listY, 14, GRAY);
        }
    }
}

} // namespace gui
