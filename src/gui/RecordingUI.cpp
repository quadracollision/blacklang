#include "RecordingUI.h"
#include "../GuiState.h"
#include "../AudioEngine.h"
#include "FileBrowser.h"
#include "Widgets.h"
#include "../platform/AndroidBridge.h"
#include <iostream>
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
    int titleW = MeasureTextApp(title, 24);
    DrawTextApp(title, (int)(screenW/2 - titleW/2), (int)(panelY + 20), 24, WHITE);
    
    // Save As textbox
    DrawTextApp("Save as:", (int)(panelX + 30), (int)(panelY + 60), 14, LIGHTGRAY);
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
    int wholeTextW = MeasureTextApp(wholeText, 18);
    DrawTextApp(wholeText, (int)(wholeBtn.x + (wholeBtn.width - wholeTextW) / 2), (int)(wholeBtn.y + (wholeBtn.height - 18) / 2), 18, WHITE);
    
    // Stems Button
    Rectangle stemsBtn = {startX + btnW + gap, startY, btnW, btnH};
    bool stemsHover = CheckCollisionPointRec(state.getMousePosition(), stemsBtn);
    DrawRectangleRec(stemsBtn, stemsHover ? Color{70, 180, 70, 255} : Color{50, 130, 50, 255});
    DrawRectangleLinesEx(stemsBtn, 2, stemsHover ? WHITE : Color{100, 200, 100, 255});
    
    const char* stemsText = "Stems";
    int stemsTextW = MeasureTextApp(stemsText, 18);
    DrawTextApp(stemsText, (int)(stemsBtn.x + (stemsBtn.width - stemsTextW) / 2), (int)(stemsBtn.y + (stemsBtn.height - 18) / 2), 18, WHITE);
    
    // Cancel Button
    Rectangle cancelBtn = {screenW/2 - 40, panelY + panelH - 40, 80, 28};
    bool cancelHover = CheckCollisionPointRec(state.getMousePosition(), cancelBtn);
    DrawRectangleRec(cancelBtn, cancelHover ? Color{80, 80, 85, 255} : Color{60, 60, 65, 255});
    DrawRectangleLinesEx(cancelBtn, 1, GRAY);
    const char* cancelText = "Cancel";
    int cancelTextW = MeasureTextApp(cancelText, 14);
    DrawTextApp(cancelText, (int)(cancelBtn.x + (cancelBtn.width - cancelTextW) / 2), (int)(cancelBtn.y + (cancelBtn.height - 14) / 2), 14, LIGHTGRAY);
    
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
    
    // Full-screen overlay (Standard Editor Style)
    DrawRectangle(0, 0, (int)screenW, (int)screenH, Color{25, 25, 30, 255});
    
    // Header panel (60px)
    DrawRectangle(0, 0, (int)screenW, 60, Color{35, 35, 40, 255});
    
    // Title
    const char* title = "Review Recording";
    DrawTextApp(title, 20, 18, 24, WHITE);
    
    // Filename Input in Header (Right side of title)
    float titleW = MeasureTextApp(title, 24);
    DrawTextApp("Name:", 20 + titleW + 30, 22, 16, LIGHTGRAY);
    
    Rectangle nameBox = {20 + titleW + 85, 16, 250, 28};
    DrawRectangleRec(nameBox, Color{50, 50, 55, 255});
    DrawRectangleLinesEx(nameBox, 1, Color{80, 80, 90, 255});
    DrawTextInput(nameBox, state.recorder.filenameBuffer, 63, 600, state.focusedFieldId, state.getMousePosition());
    
    // Main Waveform Area (Full Screen minus Header/Footer)
    float waveX = 10;
    float waveY = 70;
    float waveW = screenW - 20;
    float waveH = screenH - 140; // Leave room for footer
    
    Rectangle waveArea = {waveX, waveY, waveW, waveH};
    DrawRectangleRec(waveArea, BLACK);
    DrawRectangleLinesEx(waveArea, 1, GRAY);
    
    // Footer Background
    float footerY = screenH - 60;
    DrawRectangle(0, (int)footerY, (int)screenW, 60, Color{30, 30, 30, 255});
    
    // Draw waveform
    const auto& masterBuffer = engine.getRecordedMaster();
    int sampleCount = engine.getRecordedSampleCount();
    int64_t playhead = state.recorder.previewPosition;
    
    if (sampleCount > 0) {
        // Zoom/Scroll Logic (Slicer Style)
        float zoom = state.recorder.waveformZoom;
        if (zoom < 1.0f) zoom = 1.0f;
        
        // Handle Mouse Wheel Zoom
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            float mouseRelX = (state.getMousePosition().x - waveArea.x) / waveArea.width;
            if (mouseRelX < 0) mouseRelX = 0; if (mouseRelX > 1) mouseRelX = 1;
            
            float oldZoom = state.recorder.waveformZoom;
            if (wheel > 0) state.recorder.waveformZoom *= 1.1f;
            else state.recorder.waveformZoom /= 1.1f;
            
            if (state.recorder.waveformZoom < 1.0f) state.recorder.waveformZoom = 1.0f;
            
            // Center zoom on mouse
            float viewW = 1.0f / state.recorder.waveformZoom;
            float oldViewW = 1.0f / oldZoom;
            float centerRatio = state.recorder.waveformScrollX + (mouseRelX * oldViewW);
            state.recorder.waveformScrollX = centerRatio - (mouseRelX * viewW);
        }
        
        // Clamp Scroll
        float viewWidth = 1.0f / state.recorder.waveformZoom;
        float maxScroll = 1.0f - viewWidth;
        if (state.recorder.waveformScrollX < 0) state.recorder.waveformScrollX = 0;
        if (state.recorder.waveformScrollX > maxScroll) state.recorder.waveformScrollX = maxScroll;
        
        // Draw Waveform
        DrawWaveform(waveArea, masterBuffer, sampleCount, playhead);
        
        // Scrollbar (if zoomed)
        if (state.recorder.waveformZoom > 1.01f) {
            Rectangle scrollBarBg = {waveArea.x, waveArea.y + waveArea.height - 15, waveArea.width, 15};
            DrawRectangleRec(scrollBarBg, Color{40, 40, 40, 200});
            
            float thumbW = scrollBarBg.width * viewWidth;
            float thumbX = scrollBarBg.x + (state.recorder.waveformScrollX / (maxScroll + 0.0001f)) * (scrollBarBg.width - thumbW);
            
            Rectangle thumb = {thumbX, scrollBarBg.y + 2, thumbW, 11};
            DrawRectangleRec(thumb, LIGHTGRAY);
            
            // Drag Scrollbar Logic
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(state.getMousePosition(), scrollBarBg)) {
                state.recorder.isDraggingScroll = true;
                state.recorder.dragStartX = state.getMousePosition().x;
                state.recorder.dragStartScrollX = state.recorder.waveformScrollX;
            }
        }
        
    } else {
        const char* noData = "No recording data";
        int noDataW = MeasureTextApp(noData, 20);
        DrawTextApp(noData, (int)(waveArea.x + waveArea.width/2 - noDataW/2), (int)(waveArea.y + waveArea.height/2), 20, GRAY);
    }
    
    // --- Footer Controls (Slicer Style) ---
    float btnH = 45;
    float btnY = footerY + (60 - btnH) / 2; // Center vertically in footer
    
    // Left: (Empty for now)
    
    // Right: Action Buttons
    float btnW = 140;
    float gap = 20;
    float totalBtnWidth = 3 * btnW + 2 * gap;
    float btnX = screenW - totalBtnWidth - 40; // Right aligned with margin
    
    // Preview / Stop
    Rectangle previewBtn = {btnX, btnY, btnW, btnH};
    bool isPreviewing = state.recorder.isPreviewing;
    
    if (isPreviewing) {
        DrawRectangleRec(previewBtn, RED);
        const char* stopTxt = "Stop";
        int stopTxtW = MeasureTextApp(stopTxt, 20);
        DrawTextApp(stopTxt, (int)(previewBtn.x + (previewBtn.width - stopTxtW) / 2), (int)(previewBtn.y + (previewBtn.height - 20) / 2), 20, WHITE);
    } else {
        DrawRectangleRec(previewBtn, LIME);
        const char* prevTxt = "Preview";
        int prevTxtW = MeasureTextApp(prevTxt, 20);
        DrawTextApp(prevTxt, (int)(previewBtn.x + (previewBtn.width - prevTxtW) / 2), (int)(previewBtn.y + (previewBtn.height - 20) / 2), 20, BLACK);
    }
    
    // Save
    Rectangle saveBtn = {btnX + btnW + gap, btnY, btnW, btnH};
    DrawRectangleRec(saveBtn, BLUE);
    const char* saveLabel = state.recorder.recordStems ? "Save All" : "Save";
    int saveLabelW = MeasureTextApp(saveLabel, 20);
    DrawTextApp(saveLabel, (int)(saveBtn.x + (saveBtn.width - saveLabelW) / 2), (int)(saveBtn.y + (saveBtn.height - 20) / 2), 20, WHITE);
    
    // Discard
    Rectangle discardBtn = {btnX + 2 * (btnW + gap), btnY, btnW, btnH};
    DrawRectangleRec(discardBtn, Color{150, 50, 50, 255});
    const char* discardLabel = "Discard";
    int discardLabelW = MeasureTextApp(discardLabel, 20);
    DrawTextApp(discardLabel, (int)(discardBtn.x + (discardBtn.width - discardLabelW) / 2), (int)(discardBtn.y + (discardBtn.height - 20) / 2), 20, WHITE);
    
    
    // Handle Input
    if (state.recorder.justOpenedReview) {
        state.recorder.justOpenedReview = false;
        return; 
    }
    
    // --- Waveform Overlay Controls (Zoom) ---
    // Floating buttons top-right of waveform
    float zoomBtnSize = 40;
    float zoomBtnMargin = 10;
    float zoomX = waveArea.x + waveArea.width - zoomBtnSize - zoomBtnMargin;
    float zoomY = waveArea.y + zoomBtnMargin;
    
    Rectangle zoomInBtn = {zoomX, zoomY, zoomBtnSize, zoomBtnSize};
    Rectangle zoomOutBtn = {zoomX - zoomBtnSize - 10, zoomY, zoomBtnSize, zoomBtnSize};
    
    // Draw Zoom Buttons
    DrawRectangleRec(zoomInBtn, Color{60, 60, 60, 200});
    DrawRectangleLinesEx(zoomInBtn, 1, WHITE);
    const char* plusTxt = "+";
    int plusTxtW = MeasureTextApp(plusTxt, 30);
    DrawTextApp(plusTxt, (int)(zoomInBtn.x + (zoomInBtn.width - plusTxtW) / 2), (int)(zoomInBtn.y + (zoomInBtn.height - 30) / 2), 30, WHITE);
    
    DrawRectangleRec(zoomOutBtn, Color{60, 60, 60, 200});
    DrawRectangleLinesEx(zoomOutBtn, 1, WHITE);
    const char* minusTxt = "-";
    int minusTxtW = MeasureTextApp(minusTxt, 30);
    DrawTextApp(minusTxt, (int)(zoomOutBtn.x + (zoomOutBtn.width - minusTxtW) / 2), (int)(zoomOutBtn.y + (zoomOutBtn.height - 30) / 2), 30, WHITE);

    // --- Interaction Logic ---
    
    // Handle Input
    if (state.recorder.justOpenedReview) {
        state.recorder.justOpenedReview = false;
        return; 
    }
    
    // Scrollbar Dragging (Explicit scrollbar)
    if (state.recorder.isDraggingScroll && !state.recorder.isDraggingWaveform) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float delta = state.getMousePosition().x - state.recorder.dragStartX;
            float viewWidth = 1.0f / state.recorder.waveformZoom;
            float maxScroll = 1.0f - viewWidth;
            float tracksWidth = waveArea.width - (waveArea.width * viewWidth); // Width of track area
            
            if (tracksWidth > 0.1f) { 
                float scrollDelta = delta / tracksWidth * maxScroll;
                state.recorder.waveformScrollX = state.recorder.dragStartScrollX + scrollDelta;
                // Clamp
                if (state.recorder.waveformScrollX < 0) state.recorder.waveformScrollX = 0;
                if (state.recorder.waveformScrollX > maxScroll) state.recorder.waveformScrollX = maxScroll;
            }
        } else {
            state.recorder.isDraggingScroll = false;
        }
    }
    
    // Waveform Dragging (Drag to Scroll)
    if (state.recorder.isDraggingWaveform) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float deltaPix = state.recorder.dragStartX - state.getMousePosition().x; // Drag LEFT to move view RIGHT (scroll increases)
            
            // Convert pixel delta to normalized scroll delta
            // Total width in pixels = waveArea.width * zoom
            // Scroll delta = deltaPix / TotalWidth
            float totalWidth = waveArea.width * state.recorder.waveformZoom;
            float scrollDelta = deltaPix / totalWidth; 
            
            state.recorder.waveformScrollX = state.recorder.dragStartScrollX + scrollDelta;
            
            float viewWidth = 1.0f / state.recorder.waveformZoom;
            float maxScroll = 1.0f - viewWidth;
            
            if (state.recorder.waveformScrollX < 0) state.recorder.waveformScrollX = 0;
            if (state.recorder.waveformScrollX > maxScroll) state.recorder.waveformScrollX = maxScroll;
            
        } else {
            state.recorder.isDraggingWaveform = false;
        }
    }
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick();
        
        // Check Zoom Buttons first
        if (CheckCollisionPointRec(state.getMousePosition(), zoomInBtn)) {
            state.recorder.waveformZoom *= 1.5f;
             // Clamp center? Simple zoom in is fine.
        }
        else if (CheckCollisionPointRec(state.getMousePosition(), zoomOutBtn)) {
            state.recorder.waveformZoom /= 1.5f;
            if (state.recorder.waveformZoom < 1.0f) state.recorder.waveformZoom = 1.0f;
            // Re-clamp scroll if we zoomed out
            float viewWidth = 1.0f / state.recorder.waveformZoom;
            float maxScroll = 1.0f - viewWidth;
            if (state.recorder.waveformScrollX > maxScroll) state.recorder.waveformScrollX = maxScroll;
        }
        else if (CheckCollisionPointRec(state.getMousePosition(), previewBtn)) {
            if (isPreviewing) {
                engine.stopRecordingPreview();
                state.recorder.isPreviewing = false;
            } else {
                engine.startRecordingPreview();
                state.recorder.isPreviewing = true;
            }
        }
        else if (CheckCollisionPointRec(state.getMousePosition(), saveBtn)) {
             // Save Logic
             std::string recordingsDir = "recordings";
             
             std::string filename = state.recorder.filenameBuffer;
             if (filename.empty()) filename = "recording";
             
             #if defined(__ANDROID__)
            // ANDROID: Direct Export (Bypass File Browser)
            // 1. Save locally to app-specific cache
            std::string cacheDir = "/data/data/com.quadracollision.blacklang/get_files/cache"; // standard cache location
            // Actually better to use the safe path we found before, but treat as temp
            std::string tempPath = "/storage/emulated/0/Android/data/com.quadracollision.blacklang/files/Recordings/" + filename + ".wav";
            
            // Ensure dir exists
            std::filesystem::create_directories("/storage/emulated/0/Android/data/com.quadracollision.blacklang/files/Recordings");

            if (engine.saveRecordingWrapper(tempPath)) {
                 platform::LaunchFileSaver(tempPath, filename + ".wav");
                 // platform::ShowToast("Saving..."); // Optional, system picker is obvious enough
                 
                 // Clear buffers immediately as we are done
                 engine.clearRecordedBuffers();
            } else {
                 platform::ShowToast("Internal Save Failed!");
            }
            #else
            // DESKTOP: Use File Browser
            // Open File Browser for Save (Master Mix)
            FileBrowser::Open(state, PatternEditorState::BrowserMode::RecordingSave);
             
            // CRITICAL: DO NOT clear buffers here. The FileBrowser needs the data to save!
            // We will clear them in FileBrowser::Draw after successful save.
            #endif
            
            state.recorder.showReview = false;
            state.recorder.isPreviewing = false;
            engine.stopRecordingPreview();
            
            engine.disarmRecording();
            state.recorder.isArmed = false;
            state.recorder.isRecording = false;
        }
        else if (CheckCollisionPointRec(state.getMousePosition(), discardBtn)) {
            engine.clearRecordedBuffers();
            state.recorder.showReview = false;
            state.recorder.isPreviewing = false;
            engine.stopRecordingPreview();
            
            engine.disarmRecording();
            state.recorder.isArmed = false;
            state.recorder.isRecording = false;
        }
        else if (CheckCollisionPointRec(state.getMousePosition(), waveArea) && sampleCount > 0) {
            // Start Drag on Waveform
            state.recorder.isDraggingWaveform = true;
            state.recorder.dragStartX = state.getMousePosition().x;
            state.recorder.dragStartScrollX = state.recorder.waveformScrollX;
        }
    }
    
    // Release Logic (Seek if it was a Tap)
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
         if (CheckCollisionPointRec(state.getMousePosition(), waveArea) && sampleCount > 0 && !state.recorder.isDraggingScroll) {
             // If we were dragging waveform but didn't move much, treat as seek
             // Or better: If `isDraggingWaveform` was true, check distance
             // But simpler: If state.recorder.isDraggingWaveform is true, we check if mouse moved.
             
             if (state.recorder.isDraggingWaveform) {
                 float dist = fabs(state.getMousePosition().x - state.recorder.dragStartX);
                 if (dist < 5.0f) { // Threshold for Tap vs Drag
                     // It was a Tap -> Seek
                     float clickX = state.getMousePosition().x - waveArea.x;
                     float normX = clickX / waveArea.width;
                     
                     float viewWidth = 1.0f / state.recorder.waveformZoom;
                     float actualPosNorm = state.recorder.waveformScrollX + (normX * viewWidth);
                     
                     int64_t seekSample = (int64_t)(actualPosNorm * sampleCount);
                     if (seekSample < 0) seekSample = 0;
                     if (seekSample >= sampleCount) seekSample = sampleCount - 1;
                     
                     engine.seekRecordingPreview(seekSample);
                     state.recorder.previewPosition = seekSample;
                 }
                 state.recorder.isDraggingWaveform = false;
             }
         }
    }
}

void RecordingUI::DrawWaveform(Rectangle area, const juce::AudioBuffer<float>& buffer, int sampleCount, int64_t playhead) {
    if (sampleCount <= 0 || buffer.getNumSamples() == 0) return;
    
    // Calculate visible range based on Zoom/Scroll
    float zoom = state.recorder.waveformZoom;
    if (zoom < 1.0f) zoom = 1.0f;
    
    float viewWidth = 1.0f / zoom;
    int visibleSampleCount = (int)(sampleCount * viewWidth);
    int startOffsetSample = (int)(state.recorder.waveformScrollX * sampleCount);
    
    // Clamp
    if (startOffsetSample < 0) startOffsetSample = 0;
    if (startOffsetSample + visibleSampleCount > sampleCount) startOffsetSample = sampleCount - visibleSampleCount;
    
    int width = (int)area.width;
    int samplesPerPixel = std::max(1, visibleSampleCount / width);
    
    float centerY = area.y + area.height / 2;
    float halfHeight = area.height / 2 - 5;
    
    // Draw visible waveform
    for (int x = 0; x < width; ++x) {
        int startSample = startOffsetSample + (x * samplesPerPixel);
        int endSample = std::min(startSample + samplesPerPixel, startOffsetSample + visibleSampleCount);
        
        if (endSample > sampleCount) endSample = sampleCount;
        if (startSample >= endSample) continue;
        
        float minVal = 0, maxVal = 0;
        // Simple peak preservation
        // Optimization: Don't iterate all samples if zoomed out too far, but here we prioritize accuracy for now
        // For performance on huge files we might need mipmaps, but for recording it's fine.
        int step = 1;
        if (samplesPerPixel > 100) step = samplesPerPixel / 50; // Skip some for perf if very zoomed out
        
        for (int s = startSample; s < endSample; s += step) {
            float sample = buffer.getSample(0, s); // Left channel
            if (sample < minVal) minVal = sample;
            if (sample > maxVal) maxVal = sample;
        }
        
        int y1 = (int)(centerY - maxVal * halfHeight);
        int y2 = (int)(centerY - minVal * halfHeight);
        if (y2 < y1) std::swap(y1, y2);
        
        // Ensure at least 1px height
        if (y2 == y1) y2++;
        
        DrawLine((int)(area.x + x), y1, (int)(area.x + x), y2, Color{100, 150, 255, 255});
    }
    
    // Draw playhead relative to view
    if (playhead >= startOffsetSample && playhead < startOffsetSample + visibleSampleCount) {
        float relPositions = (float)(playhead - startOffsetSample) / visibleSampleCount;
        float playheadX = area.x + relPositions * area.width;
        DrawLine((int)playheadX, (int)area.y, (int)playheadX, (int)(area.y + area.height), RED);
    }
}

} // namespace gui
