#include "RecordingDialog.h"
#include "Widgets.h"
#include "../FilePicker.h"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <raymath.h>

namespace gui {

void DrawRecordingDialog(GuiState& state, AudioEngine& engine) {
    if (!state.recorder.showDialog) return;
    
    // Dim background
    DrawRectangle(0, 0, state.getScreenWidth(), state.getScreenHeight(), Color{0, 0, 0, 180});
    
    // Dialog Box
    int width = 500;
    int height = 400;
    int x = (state.getScreenWidth() - width) / 2;
    int y = (state.getScreenHeight() - height) / 2;
    
    Rectangle bounds = {(float)x, (float)y, (float)width, (float)height};
    DrawRectangleRec(bounds, Color{30, 30, 30, 255});
    DrawRectangleLinesEx(bounds, 2, Color{60, 60, 60, 255});
    
    // Close Button (only if not recording)
    if (!state.recorder.isRecording) {
        Rectangle closeBtn = {bounds.x + bounds.width - 30, bounds.y + 5, 25, 25};
        DrawText("X", closeBtn.x + 8, closeBtn.y + 2, 20, GRAY);
        if (CheckCollisionPointRec(state.getMousePosition(), closeBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.recorder.showDialog = false;
        }
    }
    
    // Title
    DrawText("Recording", bounds.x + 20, bounds.y + 20, 24, WHITE);
    
    float startY = bounds.y + 60;
    
    if (state.recorder.isRecording) {
        // --- RECORDING IN PROGRESS ---
        double duration = GetTime() - state.recorder.recordingStartTime;
        int minutes = (int)duration / 60;
        int seconds = (int)duration % 60;
        int ms = (int)((duration - (int)duration) * 100);
        
        const char* timeText = TextFormat("%02d:%02d.%02d", minutes, seconds, ms);
        int textW = MeasureText(timeText, 60);
        DrawText(timeText, bounds.x + (width - textW)/2, startY + 50, 60, RED);
        
        DrawText("Recording in progress...", bounds.x + (width - MeasureText("Recording in progress...", 20))/2, startY + 120, 20, WHITE);
        
        // Stop Button
        Rectangle stopBtn = {bounds.x + (width - 120)/2, startY + 200, 120, 50};
        DrawRectangleRec(stopBtn, RED);
        DrawText("STOP", stopBtn.x + 35, stopBtn.y + 15, 20, WHITE);
        
        if (CheckCollisionPointRec(state.getMousePosition(), stopBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            engine.stopRecording();
            state.recorder.isRecording = false;
            state.recorder.finished = true;
        }
        
    } else if (state.recorder.finished) {
        // --- FINISHED / EXPORT ---
        DrawText("Recording Complete!", bounds.x + (width - MeasureText("Recording Complete!", 24))/2, startY + 20, 24, GREEN);
        
        float listY = startY + 60;
        DrawText("Generated Files:", bounds.x + 30, listY, 20, GRAY);
        listY += 30;
        
        // List files available for export
        // Logic: if mix was enabled -> [BaseName]_master.wav
        // if stems enabled -> [BaseName]_[TrackName].wav for all tracks
        // For simplicity, just show generic "Master Mix" and "All Stems" buttons?
        // Or actually list them based on our knowledge.
        
        std::vector<std::string> filesToExport;
        if (state.recorder.recordMix) {
            filesToExport.push_back(state.recorder.lastRecordingPath + (state.recorder.recordStems ? "_master.wav" : ".wav"));
        }
        if (state.recorder.recordStems) {
            // We just note that stems are available. Handling list of all stems here might be too long for the UI.
            // Maybe just "Export Stems (Separate)" or just listing them?
            // Let's implement export button for Master and generic "Export Stems" hint.
        }

        // Export Master Button
        if (state.recorder.recordMix) {
            Rectangle expBtn = {bounds.x + 30, listY, 200, 30};
            DrawRectangleRec(expBtn, Color{40, 40, 50, 255});
            DrawRectangleLinesEx(expBtn, 1, WHITE);
            DrawText("Export Master Mix", expBtn.x + 10, expBtn.y + 5, 20, WHITE);
            
            if (CheckCollisionPointRec(state.getMousePosition(), expBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                std::string path = state.recorder.lastRecordingPath + (state.recorder.recordStems ? "_master.wav" : ".wav");
                FilePicker::exportFile(path, "master_mix.wav");
            }
            listY += 40;
        }
        
        // Export Stems (Batch? Or One by one? We can't easily GUI list 16 tracks here without logic)
        // Let's iterate engine tracks to find potential files
        if (state.recorder.recordStems) {
             DrawText("Stems available:", bounds.x + 30, listY, 20, GRAY);
             listY += 25;
             
             // Simple Scrollable list? Or just a few
             // Just show "Export [TrackName]" for active tracks
             for (const auto& pair : engine.getPatterns()) { // Not patterns... track names!
                 // AudioEngine doesn't expose list of tracks easily outside of patternToTrack map
                 // We can infer from GuiState columns!
             }
             
             for (auto& col : state.columns) {
                 Rectangle trkBtn = {bounds.x + 30, listY, 200, 30};
                 DrawRectangleRec(trkBtn, Color{40, 40, 50, 255});
                 DrawText(("Export " + col.title).c_str(), trkBtn.x + 5, trkBtn.y + 5, 16, WHITE);
                 
                 if (CheckCollisionPointRec(state.getMousePosition(), trkBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                     std::string path = state.recorder.lastRecordingPath + "_" + col.title + ".wav";
                     // Check if exists?
                     FilePicker::exportFile(path, col.title + ".wav");
                 }
                 listY += 35;
                 if (listY > bounds.y + height - 60) break; // clipped
             }
        }
        
        // New Recording Button (Reset)
        Rectangle resetBtn = {bounds.x + bounds.width - 140, bounds.y + bounds.height - 50, 120, 40};
        DrawRectangleRec(resetBtn, Color{60, 60, 60, 255});
        DrawText("Back", resetBtn.x + 40, resetBtn.y + 10, 20, WHITE);
         if (CheckCollisionPointRec(state.getMousePosition(), resetBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
             state.recorder.finished = false;
         }
        
    } else {
        // --- CONFIGURATION ---
        
        // Checkboxes
        DrawText("Options:", bounds.x + 30, startY, 20, GRAY);
        
        Rectangle mixCheck = {bounds.x + 30, startY + 30, 20, 20};
        DrawRectangleRec(mixCheck, state.recorder.recordMix ? GREEN : DARKGRAY);
        DrawText("Record Master Mix", mixCheck.x + 30, mixCheck.y, 20, WHITE);
        if (CheckCollisionPointRec(state.getMousePosition(), mixCheck) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.recorder.recordMix = !state.recorder.recordMix;
        }
        
        Rectangle stemCheck = {bounds.x + 30, startY + 70, 20, 20};
        DrawRectangleRec(stemCheck, state.recorder.recordStems ? GREEN : DARKGRAY);
        DrawText("Record Track Stems", stemCheck.x + 30, stemCheck.y, 20, WHITE);
        if (CheckCollisionPointRec(state.getMousePosition(), stemCheck) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.recorder.recordStems = !state.recorder.recordStems;
        }
        
        // Start Button
        Rectangle startBtn = {bounds.x + (width - 150)/2, startY + 150, 150, 60};
        bool enabled = state.recorder.recordMix || state.recorder.recordStems;
        DrawRectangleRec(startBtn, enabled ? RED : DARKGRAY);
        DrawText("START REC", startBtn.x + 20, startBtn.y + 20, 20, WHITE);
        
        if (enabled && CheckCollisionPointRec(state.getMousePosition(), startBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            // Generate filename based on timestamp
            std::time_t t = std::time(nullptr);
            std::tm tm = *std::localtime(&t);
            std::stringstream ss;
            ss << std::put_time(&tm, "Rec_%Y%m%d_%H%M%S");
            
            std::string baseName = ss.str();
            std::string basePath = FilePicker::getWritablePath() + baseName;
            
            state.recorder.lastRecordingPath = basePath;
            state.recorder.recordingStartTime = GetTime();
            
            engine.startRecording(basePath, state.recorder.recordMix, state.recorder.recordStems);
            state.recorder.isRecording = true;
            state.recorder.showDialog = false; // Hide dialog so user can interact with app
        }
    }
}

} // namespace gui
