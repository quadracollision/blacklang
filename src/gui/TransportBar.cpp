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
    DrawBPM();
    DrawCopyPaste();
    DrawEditShift();
    DrawSettingsButton();
    DrawSettingsPopup();
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
                }
            }
        } else {
            for (const auto& col : state.columns) {
                for (const auto& pName : col.patternNames) {
                    names.push_back(pName);
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
    float centerX = GetScreenWidth() / 2.0f;
    
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
    
    float gearX = GetScreenWidth() - 50;
    Rectangle gearBtn = {gearX, rect.y + 15, 30, 30};
    DrawRectangleRec(gearBtn, state.settings.showSettingsMenu ? ORANGE : DARKGRAY);
    DrawText("*", gearBtn.x + 8, gearBtn.y + 3, 24, WHITE);
    
    if (CheckCollisionPointRec(GetMousePosition(), gearBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.settings.showSettingsMenu = !state.settings.showSettingsMenu;
        if (state.settings.showSettingsMenu) {
            state.settings.availableOutputDevices = engine.getAvailableOutputDevices();
            state.settings.currentDevice = engine.getCurrentOutputDevice();
        }
    }
}

void TransportBar::DrawSettingsPopup() {
    if (!state.settings.showSettingsMenu) return;
    
    Rectangle rect = {0, (float)GetScreenHeight() - state.FOOTER_HEIGHT, (float)GetScreenWidth(), (float)state.FOOTER_HEIGHT};
    
    float popW = 350;
    float popH = 200;
    float popX = GetScreenWidth() - popW - 20;
    float popY = rect.y - popH - 10;
    
    DrawRectangle(popX, popY, popW, popH, Color{40, 40, 40, 245});
    DrawRectangleLinesEx({popX, popY, popW, popH}, 2, WHITE);
    
    DrawText("Audio Settings", popX + 10, popY + 10, 18, WHITE);
    
    Rectangle closeBtn = {popX + popW - 30, popY + 5, 25, 25};
    DrawRectangleRec(closeBtn, RED);
    DrawText("X", closeBtn.x + 7, closeBtn.y + 3, 18, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), closeBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.settings.showSettingsMenu = false;
    }
    
    float currentY = popY + 40;
    
    // --- Projects ---
    DrawText("Project:", popX + 10, currentY, 14, LIGHTGRAY);
    
    Rectangle saveBtn = {popX + 100, currentY - 5, 80, 25};
    DrawRectangleRec(saveBtn, DARKGRAY);
    DrawText("Save", saveBtn.x + 20, saveBtn.y + 5, 14, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), saveBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        const char* filters[] = { "*.json" };
        const char* path = tinyfd_saveFileDialog(
            "Save Project",
            "project.json",
            1,
            filters,
            "JSON Project Files"
        );
        if (path) {
            // Convert Layout
            std::vector<SerializedColumn> cols;
            for (const auto& col : state.columns) {
                cols.push_back({col.title, col.patternNames});
            }
            ProjectFile::save(path, engine.getPatterns(), state.activeChain, cols);
        }
    }
    
    Rectangle loadBtn = {popX + 190, currentY - 5, 80, 25};
    DrawRectangleRec(loadBtn, DARKGRAY);
    DrawText("Load", loadBtn.x + 20, loadBtn.y + 5, 14, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), loadBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        const char* filters[] = { "*.json" };
        const char* path = tinyfd_openFileDialog(
            "Load Project",
            "",
            1,
            filters,
            "JSON Project Files",
            0
        );
        if (path) {
            std::map<std::string, Pattern> patterns;
            std::vector<SerializedColumn> loadedCols;
            
            if (ProjectFile::load(path, patterns, state.activeChain, loadedCols)) {
                // Clear existing
                engine.stop();
                // We need a way to clear engine patterns roughly or just overwrite
                // Currently engine.addPattern overwrites.
                
                // Load into Engine
                for (auto& [name, pat] : patterns) {
                    engine.addPattern(pat);
                    engine.loadSample(pat); // Reload audio
                }
                
                // Update UI state
                state.columns.clear();
                state.activePatternSlots.clear();
                
                // Restore Layout
                if (!loadedCols.empty()) {
                    for (const auto& sCol : loadedCols) {
                        state.columns.push_back({sCol.title, sCol.patternNames, {0,0,0,0}, 0.0f});
                    }
                } else {
                    // Fallback if loading old project without layout
                    state.columns.resize(4);
                    int colIdx = 0;
                    for (const auto& [name, pat] : patterns) {
                       state.columns[colIdx].patternNames.push_back(name);
                    }
                }
            }
        }
    }
    
    currentY += 40;

    DrawText("Output Device:", popX + 10, currentY, 14, LIGHTGRAY);
    DrawText(state.settings.currentDevice.c_str(), popX + 120, currentY, 14, GREEN);
    
    float listY = currentY + 25;
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

} // namespace gui
