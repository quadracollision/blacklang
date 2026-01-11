#include "TransportBar.h"
#include "Widgets.h"
#include "../GuiState.h"
#include "../AudioEngine.h"
#include "../ProjectFile.h"
#include "../tinyfiledialogs.h"
#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace gui {

TransportBar::TransportBar(GuiState& s, AudioEngine& e) : state(s), engine(e) {}

void TransportBar::Draw() {
    Rectangle rect = {0, (float)state.getScreenHeight() - state.FOOTER_HEIGHT, (float)state.getScreenWidth(), (float)state.FOOTER_HEIGHT};
    DrawRectangleRec(rect, Color{25, 25, 25, 255});
    
    DrawPlayStop();
    DrawRecording();
    DrawCopyPaste();
    DrawEditShift();
    DrawSyncButton();
    DrawSettingsButton();
    DrawSettingsPopup();
}

void TransportBar::DrawRecording() {
    Rectangle rect = {0, (float)state.getScreenHeight() - state.FOOTER_HEIGHT, (float)state.getScreenWidth(), (float)state.FOOTER_HEIGHT};
    float centerX = state.getScreenWidth() / 2.0f;
    float btnH = (float)state.FOOTER_HEIGHT;
    float btnW = 60;
    
    // Record Button (Beside Stop)
    Rectangle recBtn = {centerX + 80, rect.y, btnW, btnH};
    bool isRecording = state.recorder.isRecording;
    
    if (isRecording) {
        // Red background when recording
        DrawRectangleRec(recBtn, Color{200, 50, 50, 255});
        DrawCircle(recBtn.x + 30, recBtn.y + 30, 18, RED);
    } else {
        // Gray button with Red Dot
        DrawRectangleRec(recBtn, LIGHTGRAY);
        DrawCircle(recBtn.x + 30, recBtn.y + 30, 12, RED);
    }
    
    if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), recBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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
        DrawText("Name:", panelX + 10, panelY + 12, 14, WHITE);
        
        Rectangle nameBox = {panelX + 60, panelY + 8, 100, 24};
        DrawTextInput(nameBox, state.recorder.filenameBuffer, 63, 500, state.focusedFieldId, state.getMousePosition());

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
        
        if (CheckCollisionPointRec(state.getMousePosition(), wholeRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.recorder.recordStems = false;
        }
        if (CheckCollisionPointRec(state.getMousePosition(), stemsRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.recorder.recordStems = true;
        }
        
        // Save Button
        Rectangle saveBtn = {panelX + 280, panelY + 8, 50, 24};
        DrawRectangleRec(saveBtn, GREEN);
        DrawText("Save", saveBtn.x + 10, saveBtn.y + 5, 14, BLACK);
        
        if (CheckCollisionPointRec(state.getMousePosition(), saveBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            // Start Recording
            state.recorder.isRecording = true;
            state.recorder.showControls = false;
            
            // Call Engine
            engine.startRecording(state.recorder.filenameBuffer, state.recorder.recordStems);
        }
    }
}

void TransportBar::DrawPlayStop() {
    Rectangle rect = {0, (float)state.getScreenHeight() - state.FOOTER_HEIGHT, (float)state.getScreenWidth(), (float)state.FOOTER_HEIGHT};
    float centerX = state.getScreenWidth() / 2.0f;
    float btnH = (float)state.FOOTER_HEIGHT;  // Full height
    float btnW = 70;  // Wider buttons
    
    // Play
    Rectangle playRect = {centerX - btnW - 5, rect.y, btnW, btnH};
    DrawRectangleRec(playRect, state.isPlaying ? GREEN : GRAY);
    DrawText(">", playRect.x + 28, playRect.y + 18, 24, BLACK);
    if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), playRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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
    Rectangle stopRect = {centerX + 5, rect.y, btnW, btnH};
    DrawRectangleRec(stopRect, RED);
    DrawRectangle(stopRect.x + 25, stopRect.y + 20, 20, 20, WHITE);
    if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), stopRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        engine.stop();
    }
}

void TransportBar::DrawBPM() {
    // Moved to header
}

void TransportBar::DrawCopyPaste() {
    Rectangle rect = {0, (float)state.getScreenHeight() - state.FOOTER_HEIGHT, (float)state.getScreenWidth(), (float)state.FOOTER_HEIGHT};
    float btnH = (float)state.FOOTER_HEIGHT;
    float btnW = 80;
    float startX = 0; // Start at left
    
    // Copy Button
    Rectangle copyBtn = {startX, rect.y, btnW, btnH};
    bool isActive = state.trackClipboard.isSelectingSource;
    if (state.trackClipboard.isPasting) isActive = true;
    
    // If not active, draw normal button
    if (!state.trackClipboard.isSelectingSource && !state.trackClipboard.isPasting) {
        DrawRectangleRec(copyBtn, DARKGRAY);
        DrawText("Copy", copyBtn.x + 20, copyBtn.y + 20, 18, WHITE);
        
        if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), copyBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.trackClipboard.isSelectingSource = true;
        }
    } else {
        // Active "Copy" State or "Cancel" button
        DrawRectangleRec(copyBtn, ORANGE); 
        DrawText("Cancel", copyBtn.x + 15, copyBtn.y + 20, 18, WHITE);
         if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), copyBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.trackClipboard.isSelectingSource = false;
            state.trackClipboard.isPasting = false;
        }
    }
}

void TransportBar::DrawEditShift() {
    Rectangle rect = {0, (float)state.getScreenHeight() - state.FOOTER_HEIGHT, (float)state.getScreenWidth(), (float)state.FOOTER_HEIGHT};
    float btnH = (float)state.FOOTER_HEIGHT;
    float btnW = 80;
    float startX = 85; // After Copy button
    
    if (state.isPlaying && !state.activePatternSlots.empty()) {
        Rectangle editBtn = {startX, rect.y, btnW, btnH};
        DrawRectangleRec(editBtn, state.isLiveEditMode ? SKYBLUE : DARKGRAY);
        DrawText("Edit", editBtn.x + 22, editBtn.y + 20, 18, WHITE);
        
        if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), editBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.isLiveEditMode = !state.isLiveEditMode;
            
            if (!state.isLiveEditMode) {
                state.editor.isOpen = false;
                state.isShiftMode = false;
                state.shiftEditingPatternName = "";
            }
        }
        
        if (state.isLiveEditMode) {
            Rectangle shiftBtn = {startX + btnW + 5, rect.y, btnW, btnH};
            DrawRectangleRec(shiftBtn, state.isShiftMode ? ORANGE : DARKGRAY);
            DrawText("Shift", shiftBtn.x + 20, shiftBtn.y + 20, 18, WHITE);
            
            if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), shiftBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                state.isShiftMode = !state.isShiftMode;
                if (!state.isShiftMode) {
                    state.shiftEditingPatternName = "";
                }
            }
        }
    }
}

void TransportBar::DrawSettingsButton() {
    Rectangle rect = {0, (float)state.getScreenHeight() - state.FOOTER_HEIGHT, (float)state.getScreenWidth(), (float)state.FOOTER_HEIGHT};
    float btnH = (float)state.FOOTER_HEIGHT;
    float btnW = 60;
    
    // Far right
    Rectangle gearBtn = {state.getScreenWidth() - btnW, rect.y, btnW, btnH};
    bool isOpen = (state.settings.activePopup != PopupType::None);
    DrawRectangleRec(gearBtn, isOpen ? ORANGE : DARKGRAY);
    DrawText("*", gearBtn.x + 22, gearBtn.y + 15, 30, WHITE);
    
    if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), gearBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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
    
    Rectangle rect = {0, (float)state.getScreenHeight() - state.FOOTER_HEIGHT, (float)state.getScreenWidth(), (float)state.FOOTER_HEIGHT};
    
    float popW = 350;
    float popH = 200;
    // Align with new gear button (Right side)
    float popX = state.getScreenWidth() - popW - 10;
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
    if (CheckCollisionPointRec(state.getMousePosition(), closeBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.settings.activePopup = PopupType::None;
        state.settings.showSettingsMenu = false;
    }

    // Back Button (if submenu)
    if (state.settings.activePopup != PopupType::Main) {
        Rectangle backBtn = {popX + popW - 60, popY + 5, 25, 25};
        DrawRectangleRec(backBtn, DARKGRAY);
        DrawText("<", backBtn.x + 8, backBtn.y + 3, 18, WHITE);
        if (CheckCollisionPointRec(state.getMousePosition(), backBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.settings.activePopup = PopupType::Main;
        }
    }
    
    float currentY = popY + 45;
    
    if (state.settings.activePopup == PopupType::Main) {
        // --- Main Menu ---
        Rectangle projBtn = {popX + 20, currentY, popW - 40, 30};
        DrawRectangleRec(projBtn, DARKGRAY);
        DrawText("Project...", projBtn.x + 10, projBtn.y + 5, 14, WHITE);
        if (CheckCollisionPointRec(state.getMousePosition(), projBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.settings.activePopup = PopupType::Project;
        }
        
        Rectangle audioBtn = {popX + 20, currentY + 40, popW - 40, 30};
        DrawRectangleRec(audioBtn, DARKGRAY);
        DrawText("Audio Settings...", audioBtn.x + 10, audioBtn.y + 5, 14, WHITE);
        if (CheckCollisionPointRec(state.getMousePosition(), audioBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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
        if (CheckCollisionPointRec(state.getMousePosition(), saveBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            // Open project browser in save mode
            state.projectBrowser.isOpen = true;
            state.projectBrowser.isSaveMode = true;
            if (state.projectBrowser.currentPath.empty()) {
                state.projectBrowser.currentPath = std::filesystem::current_path().string();
            }
            state.projectBrowser.fileList.clear();
            state.projectBrowser.dirList.clear();
            state.projectBrowser.scrollY = 0;
            // Refresh will be called on first draw
            state.settings.activePopup = PopupType::None;
            state.settings.showSettingsMenu = false;
        }
        
        Rectangle loadBtn = {popX + 170, currentY, 140, 30};
        DrawRectangleRec(loadBtn, DARKGRAY);
        DrawText("Load Project", loadBtn.x + 25, loadBtn.y + 8, 14, WHITE);
        if (CheckCollisionPointRec(state.getMousePosition(), loadBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            // Open project browser in load mode
            state.projectBrowser.isOpen = true;
            state.projectBrowser.isSaveMode = false;
            if (state.projectBrowser.currentPath.empty()) {
                state.projectBrowser.currentPath = std::filesystem::current_path().string();
            }
            state.projectBrowser.fileList.clear();
            state.projectBrowser.dirList.clear();
            state.projectBrowser.scrollY = 0;
            memset(state.projectBrowser.selectedFile, 0, sizeof(state.projectBrowser.selectedFile));
            state.settings.activePopup = PopupType::None;
            state.settings.showSettingsMenu = false;
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
            if (!isCurrent && !isSwitching && CheckCollisionPointRec(state.getMousePosition(), devBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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



void TransportBar::DrawSyncButton() {
    // Placement: After Edit/Shift.
    // Edit/Shift logic is in DrawEditShift.
    // Let's place it at a fixed comfortable position or relative.
    // Right of center (Play/Stop/Rec).
    // Play/Stop is Center - 60 to Center + 60.
    // Rec is Center + 80.
    // Edit/Shift is left aligned? No.
    // Let's put Sync at Center + 160 (Right of Record)
    
    float centerX = state.getScreenWidth() / 2.0f;
    float x = centerX + 160;
    float y = (float)state.getScreenHeight() - state.FOOTER_HEIGHT + 10;
    float w = 60;
    float h = 40;
    
    Rectangle btn = {x, y, w, h};
    
    DrawRectangleRec(btn, LIGHTGRAY);
    DrawText("SYNC", x + 10, y + 12, 16, BLACK);
    
    if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        engine.scheduleResync();
    }
}

} // namespace gui
