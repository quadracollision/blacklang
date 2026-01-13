#include "RecordingUI.h"
#include "../GuiState.h"
#include "../AudioEngine.h"
#include "Widgets.h"
#include <cmath>
#include <algorithm>
#include <filesystem>

namespace gui {

RecordingUI::RecordingUI(GuiState& s, AudioEngine& e) : state(s), engine(e) {}

void RecordingUI::Draw() {
    if (state.recorder.showModeSelection) {
        DrawModeSelection();
    } else if (state.recorder.showReview) {
        DrawReviewUI();
    }
}

void RecordingUI::DrawModeSelection() {
    float screenW = (float)state.getScreenWidth();
    float screenH = (float)state.getScreenHeight();
    
    // Full-screen overlay
    DrawRectangle(0, 0, (int)screenW, (int)screenH, Color{0, 0, 0, 200});
    
    // Main Panel (gray box - smaller)
    float panelW = 400;
    float panelH = 240;
    float panelX = screenW/2 - panelW/2;
    float panelY = screenH/2 - panelH/2;
    
    Rectangle panel = {panelX, panelY, panelW, panelH};
    DrawRectangleRec(panel, Color{40, 40, 45, 255});
    DrawRectangleLinesEx(panel, 2, Color{80, 80, 90, 255});
    
    // Title
    const char* title = "New Recording";
    int titleW = MeasureText(title, 24);
    DrawText(title, (int)(screenW/2 - titleW/2), (int)(panelY + 20), 24, WHITE);
    
    // Save As textbox
    DrawText("Save as:", (int)(panelX + 30), (int)(panelY + 60), 14, LIGHTGRAY);
    Rectangle nameBox = {panelX + 30, panelY + 80, panelW - 60, 28};
    DrawRectangleRec(nameBox, Color{50, 50, 55, 255});
    DrawRectangleLinesEx(nameBox, 1, Color{80, 80, 90, 255});
    DrawTextInput(nameBox, state.recorder.filenameBuffer, 63, 601, state.focusedFieldId, state.getMousePosition());
    
    // Mode Buttons
    float btnW = 150;
    float btnH = 55;
    float gap = 20;
    float startX = screenW/2 - btnW - gap/2;
    float startY = panelY + 130;
    
    // Whole Mix Button
    Rectangle wholeBtn = {startX, startY, btnW, btnH};
    bool wholeHover = CheckCollisionPointRec(state.getMousePosition(), wholeBtn);
    DrawRectangleRec(wholeBtn, wholeHover ? Color{70, 70, 180, 255} : Color{50, 50, 130, 255});
    DrawRectangleLinesEx(wholeBtn, 2, wholeHover ? WHITE : Color{100, 100, 200, 255});
    
    const char* wholeText = "Whole Mix";
    int wholeTextW = MeasureText(wholeText, 18);
    DrawText(wholeText, (int)(wholeBtn.x + btnW/2 - wholeTextW/2), (int)(wholeBtn.y + btnH/2 - 9), 18, WHITE);
    
    // Stems Button
    Rectangle stemsBtn = {startX + btnW + gap, startY, btnW, btnH};
    bool stemsHover = CheckCollisionPointRec(state.getMousePosition(), stemsBtn);
    DrawRectangleRec(stemsBtn, stemsHover ? Color{70, 180, 70, 255} : Color{50, 130, 50, 255});
    DrawRectangleLinesEx(stemsBtn, 2, stemsHover ? WHITE : Color{100, 200, 100, 255});
    
    const char* stemsText = "Stems";
    int stemsTextW = MeasureText(stemsText, 18);
    DrawText(stemsText, (int)(stemsBtn.x + btnW/2 - stemsTextW/2), (int)(stemsBtn.y + btnH/2 - 9), 18, WHITE);
    
    // Cancel Button
    Rectangle cancelBtn = {screenW/2 - 40, panelY + panelH - 40, 80, 28};
    bool cancelHover = CheckCollisionPointRec(state.getMousePosition(), cancelBtn);
    DrawRectangleRec(cancelBtn, cancelHover ? Color{80, 80, 85, 255} : Color{60, 60, 65, 255});
    DrawRectangleLinesEx(cancelBtn, 1, GRAY);
    const char* cancelText = "Cancel";
    int cancelTextW = MeasureText(cancelText, 14);
    DrawText(cancelText, (int)(cancelBtn.x + 40 - cancelTextW/2), (int)(cancelBtn.y + 7), 14, LIGHTGRAY);
    
    // Handle Clicks - consume all clicks in this overlay
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick(); // Consume click for this overlay
        if (wholeHover) {
            state.recorder.recordStems = false;
            state.recorder.showModeSelection = false;
            state.recorder.isArmed = true;
            engine.armRecording(false);
        } else if (stemsHover) {
            state.recorder.recordStems = true;
            state.recorder.showModeSelection = false;
            state.recorder.isArmed = true;
            engine.armRecording(true);
        } else if (cancelHover) {
            state.recorder.showModeSelection = false;
        }
    }
}

void RecordingUI::DrawReviewUI() {
    float screenW = (float)state.getScreenWidth();
    float screenH = (float)state.getScreenHeight();
    
    // Full-screen overlay matching app style
    DrawRectangle(0, 0, (int)screenW, (int)screenH, Color{25, 25, 30, 255});
    
    // Header panel
    DrawRectangle(0, 0, (int)screenW, 60, Color{35, 35, 40, 255});
    const char* title = "Recording Review";
    int titleW = MeasureText(title, 24);
    DrawText(title, (int)(screenW/2 - titleW/2), 18, 24, WHITE);
    
    // Info Panel (left side)
    float infoPanelX = 20;
    float infoPanelY = 75;
    float infoPanelW = 200;
    float infoPanelH = screenH - 170;
    
    DrawRectangle((int)infoPanelX, (int)infoPanelY, (int)infoPanelW, (int)infoPanelH, Color{35, 35, 40, 255});
    DrawRectangleLinesEx({infoPanelX, infoPanelY, infoPanelW, infoPanelH}, 1, Color{60, 60, 70, 255});
    
    // Info content
    int sampleCount = engine.getRecordedSampleCount();
    double sampleRate = engine.getRecordedSampleRate();
    double duration = sampleCount > 0 ? (double)sampleCount / sampleRate : 0;
    
    DrawText("Recording Info", (int)(infoPanelX + 10), (int)(infoPanelY + 15), 16, WHITE);
    DrawLine((int)(infoPanelX + 10), (int)(infoPanelY + 38), (int)(infoPanelX + infoPanelW - 10), (int)(infoPanelY + 38), Color{60, 60, 70, 255});
    
    // Mode
    const char* modeLabel = "Mode:";
    const char* modeValue = state.recorder.recordStems ? "Stems" : "Whole Mix";
    DrawText(modeLabel, (int)(infoPanelX + 10), (int)(infoPanelY + 50), 14, LIGHTGRAY);
    DrawText(modeValue, (int)(infoPanelX + 10), (int)(infoPanelY + 68), 14, WHITE);
    
    // Duration
    char durText[32];
    snprintf(durText, 32, "%.1f sec", duration);
    DrawText("Duration:", (int)(infoPanelX + 10), (int)(infoPanelY + 95), 14, LIGHTGRAY);
    DrawText(durText, (int)(infoPanelX + 10), (int)(infoPanelY + 113), 14, WHITE);
    
    // Sample Rate
    char srText[32];
    snprintf(srText, 32, "%.0f Hz", sampleRate);
    DrawText("Sample Rate:", (int)(infoPanelX + 10), (int)(infoPanelY + 140), 14, LIGHTGRAY);
    DrawText(srText, (int)(infoPanelX + 10), (int)(infoPanelY + 158), 14, WHITE);
    
    // Samples
    char samplesText[32];
    snprintf(samplesText, 32, "%d", sampleCount);
    DrawText("Samples:", (int)(infoPanelX + 10), (int)(infoPanelY + 185), 14, LIGHTGRAY);
    DrawText(samplesText, (int)(infoPanelX + 10), (int)(infoPanelY + 203), 14, WHITE);
    
    // Filename Input (in info panel)
    DrawText("Filename:", (int)(infoPanelX + 10), (int)(infoPanelY + 240), 14, LIGHTGRAY);
    Rectangle nameBox = {infoPanelX + 10, infoPanelY + 260, infoPanelW - 20, 28};
    DrawRectangleRec(nameBox, Color{50, 50, 55, 255});
    DrawRectangleLinesEx(nameBox, 1, Color{80, 80, 90, 255});
    DrawTextInput(nameBox, state.recorder.filenameBuffer, 63, 600, state.focusedFieldId, state.getMousePosition());
    
    // Waveform Area (right side)
    float waveX = infoPanelX + infoPanelW + 15;
    float waveY = 75;
    float waveW = screenW - waveX - 20;
    float waveH = screenH - 170;
    
    Rectangle waveArea = {waveX, waveY, waveW, waveH};
    DrawRectangleRec(waveArea, Color{30, 30, 35, 255});
    DrawRectangleLinesEx(waveArea, 1, Color{60, 60, 70, 255});
    
    // Draw waveform
    const auto& masterBuffer = engine.getRecordedMaster();
    int64_t playhead = state.recorder.previewPosition;
    
    if (sampleCount > 0) {
        DrawWaveform(waveArea, masterBuffer, sampleCount, playhead);
    } else {
        const char* noData = "No recording data";
        int noDataW = MeasureText(noData, 18);
        DrawText(noData, (int)(waveArea.x + waveArea.width/2 - noDataW/2), (int)(waveArea.y + waveArea.height/2 - 9), 18, GRAY);
    }
    
    // Footer with buttons
    float footerY = screenH - 75;
    DrawRectangle(0, (int)footerY, (int)screenW, 75, Color{35, 35, 40, 255});
    
    float btnH = 45;
    float btnW = 110;
    float gap = 15;
    float totalBtnW = btnW * 3 + gap * 2;
    float btnStartX = screenW/2 - totalBtnW/2;
    float btnY = footerY + 15;
    
    // Preview Button
    Rectangle previewBtn = {btnStartX, btnY, btnW, btnH};
    bool previewHover = CheckCollisionPointRec(state.getMousePosition(), previewBtn);
    bool isPreviewing = state.recorder.isPreviewing;
    DrawRectangleRec(previewBtn, isPreviewing ? Color{200, 120, 50, 255} : (previewHover ? Color{70, 70, 170, 255} : Color{50, 50, 130, 255}));
    DrawRectangleLinesEx(previewBtn, 1, isPreviewing ? ORANGE : Color{100, 100, 200, 255});
    const char* previewText = isPreviewing ? "Stop" : "Preview";
    int previewTextW = MeasureText(previewText, 16);
    DrawText(previewText, (int)(previewBtn.x + btnW/2 - previewTextW/2), (int)(previewBtn.y + 14), 16, WHITE);
    
    // Save Button
    Rectangle saveBtn = {btnStartX + btnW + gap, btnY, btnW, btnH};
    bool saveHover = CheckCollisionPointRec(state.getMousePosition(), saveBtn);
    DrawRectangleRec(saveBtn, saveHover ? Color{70, 170, 70, 255} : Color{50, 130, 50, 255});
    DrawRectangleLinesEx(saveBtn, 1, Color{100, 200, 100, 255});
    const char* saveText = state.recorder.recordStems ? "Save All" : "Save";
    int saveTextW = MeasureText(saveText, 16);
    DrawText(saveText, (int)(saveBtn.x + btnW/2 - saveTextW/2), (int)(saveBtn.y + 14), 16, WHITE);
    
    // Discard Button
    Rectangle discardBtn = {btnStartX + (btnW + gap) * 2, btnY, btnW, btnH};
    bool discardHover = CheckCollisionPointRec(state.getMousePosition(), discardBtn);
    DrawRectangleRec(discardBtn, discardHover ? Color{170, 70, 70, 255} : Color{130, 50, 50, 255});
    DrawRectangleLinesEx(discardBtn, 1, Color{200, 100, 100, 255});
    const char* discardText = "Discard";
    int discardTextW = MeasureText(discardText, 16);
    DrawText(discardText, (int)(discardBtn.x + btnW/2 - discardTextW/2), (int)(discardBtn.y + 14), 16, WHITE);
    
    // Handle Clicks - consume ALL clicks when this overlay is open
    // Clear justOpened flag on first frame to prevent click-through
    if (state.recorder.justOpenedReview) {
        state.recorder.justOpenedReview = false;
        return; // Skip click handling on first frame
    }
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick(); // Consume click for this overlay
        if (previewHover) {
            if (isPreviewing) {
                engine.stopRecordingPreview();
                state.recorder.isPreviewing = false;
            } else {
                engine.startRecordingPreview();
                state.recorder.isPreviewing = true;
            }
        } else if (saveHover) {
            // Save recording to recordings folder
            std::string recordingsDir = "recordings";
            
            #if defined(__ANDROID__)
            recordingsDir = "/storage/emulated/0/Music";
            #else
            std::filesystem::create_directories(recordingsDir);
            #endif
            
            std::string filename = state.recorder.filenameBuffer;
            if (filename.empty()) filename = "recording";
            
            if (state.recorder.recordStems) {
                engine.saveRecordedStems(recordingsDir, filename);
            } else {
                engine.saveRecordedAudio(recordingsDir + "/" + filename + ".wav");
            }
            
            engine.clearRecordedBuffers();
            state.recorder.showReview = false;
            state.recorder.isPreviewing = false;
            engine.stopRecordingPreview();
            
            // Disarm recording after saving (User Request: "not-recording mode")
            engine.disarmRecording();
            state.recorder.isArmed = false;
            state.recorder.isRecording = false;
        } else if (discardHover) {
            engine.clearRecordedBuffers();
            state.recorder.showReview = false;
            state.recorder.isPreviewing = false;
            engine.stopRecordingPreview();
            
            // Disarm on discard too, per user request
            engine.disarmRecording();
            state.recorder.isArmed = false;
            state.recorder.isRecording = false;
        } else if (CheckCollisionPointRec(state.getMousePosition(), waveArea) && sampleCount > 0) {
            // Waveform click for seeking
            float clickX = state.getMousePosition().x - waveArea.x;
            float norm = clickX / waveArea.width;
            int64_t seekSample = (int64_t)(norm * sampleCount);
            engine.seekRecordingPreview(seekSample);
            state.recorder.previewPosition = seekSample;
        }
        // All clicks consumed - overlay is modal
    }
}

void RecordingUI::DrawWaveform(Rectangle area, const juce::AudioBuffer<float>& buffer, int sampleCount, int64_t playhead) {
    if (sampleCount <= 0 || buffer.getNumSamples() == 0) return;
    
    int width = (int)area.width;
    int samplesPerPixel = std::max(1, sampleCount / width);
    
    float centerY = area.y + area.height / 2;
    float halfHeight = area.height / 2 - 5;
    
    // Draw waveform
    for (int x = 0; x < width; ++x) {
        int startSample = x * samplesPerPixel;
        int endSample = std::min(startSample + samplesPerPixel, sampleCount);
        
        float minVal = 0, maxVal = 0;
        for (int s = startSample; s < endSample; ++s) {
            float sample = buffer.getSample(0, s); // Left channel
            if (sample < minVal) minVal = sample;
            if (sample > maxVal) maxVal = sample;
        }
        
        int y1 = (int)(centerY - maxVal * halfHeight);
        int y2 = (int)(centerY - minVal * halfHeight);
        if (y2 < y1) std::swap(y1, y2);
        
        DrawLine((int)(area.x + x), y1, (int)(area.x + x), y2, Color{100, 150, 255, 255});
    }
    
    // Draw playhead
    if (playhead >= 0 && playhead < sampleCount) {
        float playheadX = area.x + ((float)playhead / sampleCount) * area.width;
        DrawLine((int)playheadX, (int)area.y, (int)playheadX, (int)(area.y + area.height), RED);
    }
}

} // namespace gui
