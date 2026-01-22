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
    Rectangle rect = {0, (float)state.getScreenHeight() - state.getFooterHeight(), (float)state.getScreenWidth(), (float)state.getFooterHeight()};
    DrawRectangleRec(rect, Color{25, 25, 25, 255});
    
    DrawAddTrackButton(); // Index 0
    DrawCopyPaste();      // Index 1
    DrawPlayStop();       // Index 2 (Play) & 3 (Stop)
    DrawRecording();      // Index 4
    DrawSyncButton();     // Index 5
    DrawSettingsButton(); // Index 6
    DrawSettingsPopup();
}

void TransportBar::DrawAddTrackButton() {
    float footerH = (float)state.getFooterHeight();
    float y = (float)state.getScreenHeight() - footerH;
    float btnW = (float)state.getScreenWidth() / 7.0f;
    float x = 0.0f; // Index 0
    
    Rectangle rect = {x, y, btnW, footerH};
    
    DrawRectangleRec(rect, Color{40, 40, 40, 255});
    DrawTextApp("+", rect.x + (btnW - 14)/2, rect.y + footerH/2 - 12, 24, GRAY);
    
    // Add gap line
    DrawLine(rect.x + rect.width, rect.y, rect.x + rect.width, rect.y + rect.height, BLACK);
    
    if (state.isClickAvailable() && !state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), rect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick();
        // Add a new track
        char nameBuf[32];
        snprintf(nameBuf, 32, "Track %d", (int)state.columns.size() + 1);
        
        PatternColumn newCol;
        newCol.trackName = nameBuf;
        for(int i=0; i<16; i++) {
             newCol.patternNames.push_back("");
             newCol.slotSyncEnabled.push_back(false);
        }
        newCol.bounds = {0,0,0,0}; // Will be calculated by Layout
        
        state.columns.push_back(newCol);
    }
}

void TransportBar::DrawRecording() {
    float footerH = (float)state.getFooterHeight();
    float y = (float)state.getScreenHeight() - footerH;
    float btnW = (float)state.getScreenWidth() / 7.0f;
    float x = btnW * 4; // Index 4 (After Stop)
    
    Rectangle recBtn = {x, y, btnW, footerH};
    
    bool isArmed = state.recorder.isArmed;
    bool isRecording = state.recorder.isRecording;
    
    Color bg = LIGHTGRAY;
    if (isRecording) bg = Color{200, 50, 50, 255};
    else if (isArmed) bg = Color{200, 150, 50, 255};
    
    DrawRectangleRec(recBtn, bg);
    
    if (isRecording) {
        DrawCircle((int)(recBtn.x + btnW/2), (int)(recBtn.y + footerH/2), 18, RED);
    } else if (isArmed) {
        DrawCircle((int)(recBtn.x + btnW/2), (int)(recBtn.y + footerH/2), 14, ORANGE);
        DrawCircleLines((int)(recBtn.x + btnW/2), (int)(recBtn.y + footerH/2), 18, WHITE);
    } else {
        DrawCircle((int)(recBtn.x + btnW/2), (int)(recBtn.y + footerH/2), 12, RED);
    }
    
    // Add gap line
    DrawLine(recBtn.x + recBtn.width, recBtn.y, recBtn.x + recBtn.width, recBtn.y + recBtn.height, BLACK);
    
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
    float footerH = (float)state.getFooterHeight();
    float y = (float)state.getScreenHeight() - footerH;
    float btnW = (float)state.getScreenWidth() / 7.0f;
    
    // Play: Index 2
    float playX = btnW * 2;
    Rectangle playRect = {playX, y, btnW, footerH};
    
    DrawRectangleRec(playRect, state.isPlaying ? GREEN : GRAY);
    const char* playTxt = ">";
    int playTxtW = MeasureTextApp(playTxt, 30);
    DrawTextApp(playTxt, playRect.x + (playRect.width - playTxtW)/2, playRect.y + (playRect.height - 30)/2, 30, BLACK);
    
    DrawLine(playRect.x + playRect.width, playRect.y, playRect.x + playRect.width, playRect.y + playRect.height, BLACK);

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
    
    // Stop: Index 3
    float stopX = btnW * 3;
    Rectangle stopRect = {stopX, y, btnW, footerH};
    
    DrawRectangleRec(stopRect, RED);
    DrawRectangle((int)(stopRect.x + (btnW-20)/2), (int)(stopRect.y + (footerH-20)/2), 20, 20, WHITE);
    
    DrawLine(stopRect.x + stopRect.width, stopRect.y, stopRect.x + stopRect.width, stopRect.y + stopRect.height, BLACK);

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
    float footerH = (float)state.getFooterHeight();
    float y = (float)state.getScreenHeight() - footerH;
    float btnW = (float)state.getScreenWidth() / 7.0f;
    float x = btnW * 1; // Index 1 (After Add)
    
    Rectangle copyBtn = {x, y, btnW, footerH};
    
    bool isActive = state.trackClipboard.isSelectingSource;
    if (state.trackClipboard.isPasting) isActive = true;
    
    // Draw button
    if (!state.trackClipboard.isSelectingSource && !state.trackClipboard.isPasting) {
        DrawRectangleRec(copyBtn, DARKGRAY);
        const char* txt = "COPY";
        int txtW = MeasureTextApp(txt, 18);
        DrawTextApp(txt, copyBtn.x + (copyBtn.width - txtW)/2, copyBtn.y + (copyBtn.height - 18)/2, 18, WHITE);
        
        if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), copyBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.trackClipboard.isSelectingSource = true;
        }
    } else {
        // Active "Copy" State
        DrawRectangleRec(copyBtn, ORANGE); 
        const char* txt = "CANCEL";
        int txtW = MeasureTextApp(txt, 18);
        DrawTextApp(txt, copyBtn.x + (copyBtn.width - txtW)/2, copyBtn.y + (copyBtn.height - 18)/2, 18, WHITE);
        
         if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), copyBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.trackClipboard.isSelectingSource = false;
            state.trackClipboard.isPasting = false;
        }
    }
    
    DrawLine(copyBtn.x + copyBtn.width, copyBtn.y, copyBtn.x + copyBtn.width, copyBtn.y + copyBtn.height, BLACK);
}

void TransportBar::DrawEditShift() {
    Rectangle rect = {0, (float)state.getScreenHeight() - state.FOOTER_HEIGHT, (float)state.getScreenWidth(), (float)state.FOOTER_HEIGHT};
    // Variables removed as they are no longer used.

    
    // Edit button deprecated and removed per user request.
    // Logic was: if (state.isPlaying && !state.activePatternSlots.empty()) { ... Edit/Shift ... }
}

void TransportBar::DrawSettingsButton() {
    float footerH = (float)state.getFooterHeight();
    float y = (float)state.getScreenHeight() - footerH;
    float btnW = (float)state.getScreenWidth() / 7.0f;
    float x = btnW * 6; // Index 6 (Last)
    
    Rectangle gearBtn = {x, y, btnW, footerH};
    bool isOpen = (state.settings.activePopup != PopupType::None);
    DrawRectangleRec(gearBtn, isOpen ? ORANGE : DARKGRAY);
    const char* gearTxt = "*";
    int gearW = MeasureTextApp(gearTxt, 30);
    DrawTextApp(gearTxt, gearBtn.x + (gearBtn.width - gearW)/2, gearBtn.y + (gearBtn.height - 30)/2, 30, WHITE);
    
    DrawLine(gearBtn.x, gearBtn.y, gearBtn.x, gearBtn.y + gearBtn.height, BLACK);
    
    // Fix: Allow clicking to CLOSE even if click was consumed by the early-pass overlay blocker
    // The GuiRenderer consumes clicks when activePopup is set, so we must bypass isClickAvailable check if isOpen is true
    bool canClick = (state.isClickAvailable() || isOpen) && !state.editor.isOpen;
    
    if (canClick && CheckCollisionPointRec(state.getMousePosition(), gearBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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
    
    float footerH = (float)state.getFooterHeight();
    Rectangle rect = {0, (float)state.getScreenHeight() - footerH, (float)state.getScreenWidth(), footerH};
    
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
        
        // New Project Button
        currentY += 50;
        Rectangle newBtn = {popX + 20, currentY, popW - 40, 30};
        DrawRectangleRec(newBtn, Color{100, 50, 50, 255});  // Reddish to indicate destructive action
        const char* newTxt = "NEW PROJECT";
        int newW = MeasureTextApp(newTxt, 14);
        DrawTextApp(newTxt, newBtn.x + (newBtn.width - newW)/2, newBtn.y + 8, 14, WHITE);
        
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(state.getMousePosition(), newBtn)) {
            state.consumeClick();
            state.settings.activePopup = PopupType::NewProjectConfirm;
        }
    }
    else if (state.settings.activePopup == PopupType::NewProjectConfirm) {
        // --- New Project Confirmation ---
        DrawTextApp("Start a new project?", popX + 20, currentY, 16, WHITE);
        currentY += 25;
        DrawTextApp("This will clear all patterns and tracks.", popX + 20, currentY, 12, GRAY);
        currentY += 40;
        
        // Yes Button
        Rectangle yesBtn = {popX + 60, currentY, 120, 35};
        DrawRectangleRec(yesBtn, Color{150, 50, 50, 255});  // Red for destructive
        const char* yesTxt = "YES";
        int yesW = MeasureTextApp(yesTxt, 16);
        DrawTextApp(yesTxt, yesBtn.x + (yesBtn.width - yesW)/2, yesBtn.y + 10, 16, WHITE);
        
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(state.getMousePosition(), yesBtn)) {
            state.consumeClick();
            
            // Clear the project
            engine.stop();
            engine.clearAllPatterns();
            state.columns.clear();
            state.activePatternSlots.clear();
            
            // Create one default track
            state.columns.push_back({
                "Track 1",
                std::vector<std::string>(16, ""),
                std::vector<bool>(16, false),
                {0, 0, 0, 0},
                0.0f,
                false,
                "Track_0",
                1.0f,
                0.5f
            });
            
            state.settings.activePopup = PopupType::None;
            state.settings.showSettingsMenu = false;
        }
        
        // No Button
        Rectangle noBtn = {popX + 270, currentY, 120, 35};
        DrawRectangleRec(noBtn, DARKGRAY);
        const char* noTxt = "NO";
        int noW = MeasureTextApp(noTxt, 16);
        DrawTextApp(noTxt, noBtn.x + (noBtn.width - noW)/2, noBtn.y + 10, 16, WHITE);
        
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(state.getMousePosition(), noBtn)) {
            state.consumeClick();
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
    float footerH = (float)state.getFooterHeight();
    float y = (float)state.getScreenHeight() - footerH;
    float btnW = (float)state.getScreenWidth() / 7.0f;
    float x = btnW * 5; // Index 5 (After Rec)
    
    Rectangle btn = {x, y, btnW, footerH};
    DrawRectangleRec(btn, LIGHTGRAY);
    
    const char* txt = "SYNC";
    int txtW = MeasureTextApp(txt, 16);
    DrawTextApp(txt, btn.x + (btn.width - txtW)/2, btn.y + (btn.height - 16)/2, 16, BLACK);
    
    DrawLine(btn.x + btn.width, btn.y, btn.x + btn.width, btn.y + btn.height, BLACK);
    
    if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (state.isPlaying) {
             // ... sync logic ...
            std::vector<std::string> patternsWithBeatsync;
            for (const auto& col : state.columns) {
                for (const std::string& patName : col.patternNames) {
                    if (!patName.empty()) {
                        patternsWithBeatsync.push_back(patName);
                    }
                }
            }
            if (!patternsWithBeatsync.empty()) {
                engine.syncPatternsWithBeatsync(patternsWithBeatsync);
            }
        }
    }
}

} // namespace gui

