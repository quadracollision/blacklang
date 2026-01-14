#include "TransportBar.h"
#include "Widgets.h"
#include "FileBrowser.h"
#include "../GuiState.h"
#include "../AudioEngine.h"
#include "../ProjectFile.h"
#include "../tinyfiledialogs.h"
#include <cstdlib>
#include <cstring>

#if defined(__ANDROID__)
#include <android/log.h>
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "TransportBar", __VA_ARGS__)
#else
#define LOGD(...) 
#endif
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
    float gap = 5.0f;
    // Stop ends at centerX + 2.5 + 70 = centerX + 72.5
    Rectangle recBtn = {centerX + 72.5f + gap, rect.y, btnW, btnH};
    
    bool isArmed = state.recorder.isArmed;
    bool isRecording = state.recorder.isRecording;
    
    // Draw Record Button based on state
    if (isRecording) {
        // Full red when actively recording
        DrawRectangleRec(recBtn, Color{200, 50, 50, 255});
        DrawCircle((int)(recBtn.x + 30), (int)(recBtn.y + 30), 18, RED);
    } else if (isArmed) {
        // Orange/Yellow when armed (waiting for Play)
        DrawRectangleRec(recBtn, Color{200, 150, 50, 255});
        DrawCircle((int)(recBtn.x + 30), (int)(recBtn.y + 30), 14, ORANGE);
        DrawCircleLines((int)(recBtn.x + 30), (int)(recBtn.y + 30), 18, WHITE);
    } else {
        // Gray button with Red Dot (idle)
        DrawRectangleRec(recBtn, LIGHTGRAY);
        DrawCircle((int)(recBtn.x + 30), (int)(recBtn.y + 30), 12, RED);
    }
    
    // Handle Record Button Click
    if (state.isClickAvailable() && !state.editor.isOpen &&
        CheckCollisionPointRec(state.getMousePosition(), recBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick();
        if (isRecording) {
            // Stop in-memory recording and show review
            state.recorder.isRecording = false;
            engine.stopInMemoryRecording();
            engine.stop();
            state.recorder.showReview = true;
            state.recorder.justOpenedReview = true;  // Prevent click-through
        } else if (isArmed) {
            // Disarm
            state.recorder.isArmed = false;
            engine.disarmRecording();
        } else {
            // Open Mode Selection
            state.recorder.showModeSelection = true;
        }
    }
}

void TransportBar::DrawPlayStop() {
    Rectangle rect = {0, (float)state.getScreenHeight() - state.FOOTER_HEIGHT, (float)state.getScreenWidth(), (float)state.FOOTER_HEIGHT};
    float centerX = state.getScreenWidth() / 2.0f;
    float btnH = (float)state.FOOTER_HEIGHT;  // Full height
    float btnW = 70;  // Wider buttons
    
    // Play
    float gap = 5.0f;
    Rectangle playRect = {centerX - btnW - (gap/2), rect.y, btnW, btnH};
    DrawRectangleRec(playRect, state.isPlaying ? GREEN : GRAY);
    const char* playTxt = ">";
    int playTxtW = MeasureTextApp(playTxt, 24);
    DrawTextApp(playTxt, playRect.x + (playRect.width - playTxtW)/2, playRect.y + (playRect.height - 24)/2, 24, BLACK);
    if (state.isClickAvailable() && !state.editor.isOpen &&
        CheckCollisionPointRec(state.getMousePosition(), playRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick();
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
        
        // NEW: Start in-memory recording if armed
        if (state.recorder.isArmed) {
            engine.startInMemoryRecording();
            state.recorder.isRecording = true;
        }
        
        if (!names.empty()) engine.playMultiplePatterns(names);
    }
    
    // Stop
    // Stop
    // Gap 5px between Play and Stop means Offset 2.5 from center
    Rectangle stopRect = {centerX + (gap/2), rect.y, btnW, btnH};
    DrawRectangleRec(stopRect, RED);
    DrawRectangle((int)(stopRect.x + 25), (int)(stopRect.y + 20), 20, 20, WHITE);
    if (state.isClickAvailable() && !state.editor.isOpen &&
        CheckCollisionPointRec(state.getMousePosition(), stopRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick();
        // If in-memory recording, stop and show review
        if (state.recorder.isRecording) {
            engine.stopInMemoryRecording();
            state.recorder.isRecording = false;
            state.recorder.showReview = true;
            state.recorder.justOpenedReview = true;
        }
        
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
        const char* txt = "COPY";
        int txtW = MeasureTextApp(txt, 16);
        DrawTextApp(txt, copyBtn.x + (copyBtn.width - txtW)/2, copyBtn.y + (copyBtn.height - 16)/2, 16, WHITE);
        
        if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), copyBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.trackClipboard.isSelectingSource = true;
        }
    } else {
        // Active "Copy" State or "Cancel" button
        DrawRectangleRec(copyBtn, ORANGE); 
        DrawTextApp("Cancel", copyBtn.x + 15, copyBtn.y + 20, 18, WHITE);
         if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), copyBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.trackClipboard.isSelectingSource = false;
            state.trackClipboard.isPasting = false;
        }
    }
}

void TransportBar::DrawEditShift() {
    Rectangle rect = {0, (float)state.getScreenHeight() - state.FOOTER_HEIGHT, (float)state.getScreenWidth(), (float)state.FOOTER_HEIGHT};
    // Variables removed as they are no longer used.

    
    // Edit button deprecated and removed per user request.
    // Logic was: if (state.isPlaying && !state.activePatternSlots.empty()) { ... Edit/Shift ... }
}

void TransportBar::DrawSettingsButton() {
    Rectangle rect = {0, (float)state.getScreenHeight() - state.FOOTER_HEIGHT, (float)state.getScreenWidth(), (float)state.FOOTER_HEIGHT};
    float btnH = (float)state.FOOTER_HEIGHT;
    float btnW = 60;
    
    // Far right
    // Far right - add margin
    Rectangle gearBtn = {state.getScreenWidth() - btnW - 5, rect.y, btnW, btnH};
    bool isOpen = (state.settings.activePopup != PopupType::None);
    DrawRectangleRec(gearBtn, isOpen ? ORANGE : DARKGRAY);
    const char* gearTxt = "*";
    int gearW = MeasureTextApp(gearTxt, 30);
    // Removed +5 offset
    DrawTextApp(gearTxt, gearBtn.x + (gearBtn.width - gearW)/2, gearBtn.y + (gearBtn.height - 30)/2, 30, WHITE);
    
    if (state.isClickAvailable() && !state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), gearBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick();
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
    
    // CRITICAL: Consume ALL clicks when settings popup is open to prevent click-through
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick();
    }
    
    Rectangle rect = {0, (float)state.getScreenHeight() - state.FOOTER_HEIGHT, (float)state.getScreenWidth(), (float)state.FOOTER_HEIGHT};
    
    float popW = 450;
    float popH = 300; // Increased height for better spacing
    // Align with new gear button (Right side)
    float popX = state.getScreenWidth() - popW - 10;
    float popH_rect = rect.y - popH - 10; float popY = popH_rect; // Fix variable reuse
    
    DrawRectangle(popX, popY, popW, popH, Color{40, 40, 40, 245});
    DrawRectangleLinesEx({popX, popY, popW, popH}, 2, WHITE);
    
    // Header
    const char* title = "Settings";
    if (state.settings.activePopup == PopupType::Project) title = "Project";
    else if (state.settings.activePopup == PopupType::Audio) title = "Audio Device";
    
    DrawTextApp(title, popX + 10, popY + 10, 18, WHITE);
    
    // Close Button (X)
    Rectangle closeBtn = {popX + popW - 30, popY + 5, 25, 25};
    DrawRectangleRec(closeBtn, RED);
    DrawTextApp("X", closeBtn.x + 7, closeBtn.y + 3, 18, WHITE);
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(state.getMousePosition(), closeBtn)) {
        state.consumeClick();
        state.settings.activePopup = PopupType::None;
        state.settings.showSettingsMenu = false;
    }

    // Back Button (if submenu)
    if (state.settings.activePopup != PopupType::Main) {
        Rectangle backBtn = {popX + popW - 60, popY + 5, 25, 25};
        DrawRectangleRec(backBtn, DARKGRAY);
        DrawTextApp("<", backBtn.x + 8, backBtn.y + 3, 18, WHITE);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(state.getMousePosition(), backBtn)) {
            state.consumeClick();
            state.settings.activePopup = PopupType::Main;
        }
    }
    
    float currentY = popY + 45;
    
    if (state.settings.activePopup == PopupType::Main) {
        // --- Main Menu ---
        Rectangle projBtn = {popX + 20, currentY, popW - 40, 30};
        DrawRectangleRec(projBtn, DARKGRAY);
        DrawTextApp("Project...", projBtn.x + 10, projBtn.y + 5, 14, WHITE);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(state.getMousePosition(), projBtn)) {
            state.consumeClick();
            state.settings.activePopup = PopupType::Project;
        }
        
        Rectangle audioBtn = {popX + 20, currentY + 40, popW - 40, 30};
        DrawRectangleRec(audioBtn, DARKGRAY);
        DrawTextApp("Audio Settings...", audioBtn.x + 10, audioBtn.y + 5, 14, WHITE);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(state.getMousePosition(), audioBtn)) {
            state.consumeClick();
            state.settings.activePopup = PopupType::Audio;
            state.settings.availableOutputDevices = engine.getAvailableOutputDevices();
            state.settings.currentDevice = engine.getCurrentOutputDevice();
        }
    }
    else if (state.settings.activePopup == PopupType::Project) {
        // --- Project Menu ---
        DrawTextApp("Manage Project:", popX + 20, currentY, 14, LIGHTGRAY);
        currentY += 30;
        
        // Save Button
        Rectangle saveBtn = {popX + 20, currentY, 200, 30};
        DrawRectangleRec(saveBtn, DARKGRAY);
        const char* saveTxt = "SAVE PROJECT";
        int saveW = MeasureTextApp(saveTxt, 14);
        DrawTextApp(saveTxt, saveBtn.x + (saveBtn.width - saveW)/2, saveBtn.y + 8, 14, WHITE);
        
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(state.getMousePosition(), saveBtn)) {
            state.consumeClick();
            LOGD("TransportBar: Save Project clicked");
            FileBrowser::Open(state, PatternEditorState::BrowserMode::ProjectSave);
            state.settings.activePopup = PopupType::None;
            state.settings.showSettingsMenu = false;
        }
        
        // Load Button
        Rectangle loadBtn = {popX + 230, currentY, 200, 30};
        DrawRectangleRec(loadBtn, DARKGRAY);
        const char* loadTxt = "LOAD PROJECT";
        int loadW = MeasureTextApp(loadTxt, 14);
        DrawTextApp(loadTxt, loadBtn.x + (loadBtn.width - loadW)/2, loadBtn.y + 8, 14, WHITE);
        
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(state.getMousePosition(), loadBtn)) {
            state.consumeClick();
            FileBrowser::Open(state, PatternEditorState::BrowserMode::ProjectLoad);
            state.settings.activePopup = PopupType::None;
            state.settings.showSettingsMenu = false;
        }
    }
    else if (state.settings.activePopup == PopupType::Audio) {
        // --- Audio Menu ---
        DrawTextApp("Output Device:", popX + 20, currentY, 14, LIGHTGRAY);
        currentY += 20; // New Line
        
        // Show switching indicator
        if (state.settings.isSwitchingDevice) {
            DrawTextApp("Switching...", popX + 20, currentY, 14, ORANGE);
        } else {
            DrawTextApp(state.settings.currentDevice.c_str(), popX + 20, currentY, 14, GREEN);
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
            DrawTextApp(devName.c_str(), devBtn.x + 5, devBtn.y + 4, 12, textColor);
            
            // Only allow clicking if not currently switching and not the current device
            if (!isCurrent && !isSwitching && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(state.getMousePosition(), devBtn)) {
                state.consumeClick();
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
            DrawTextApp("No devices found", popX + 20, listY, 14, GRAY);
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
    // Rec starts at centerX + 77.5. Width 60. Ends at + 137.5.
    // Sync starts at + 137.5 + 5 = 142.5.
    float x = centerX + 142.5f;
    float y = (float)state.getScreenHeight() - state.FOOTER_HEIGHT; // Match others
    float w = 60;
    float h = (float)state.FOOTER_HEIGHT; // Match others
    
    Rectangle btn = {x, y, w, h};
    
    DrawRectangleRec(btn, LIGHTGRAY);
    // Reduced font size to fit new font
    const char* txt = "SYNC";
    int txtW = MeasureTextApp(txt, 12);
    DrawTextApp(txt, x + (w - txtW)/2, y + (h - 12)/2, 12, BLACK);
    
    if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        engine.scheduleResync();
    }
}

} // namespace gui
