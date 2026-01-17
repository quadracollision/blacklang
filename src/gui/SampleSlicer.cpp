#include "SampleSlicer.h"
#include "Widgets.h"
#include "../GuiState.h"
#include "../AudioEngine.h"
#include "../Pattern.h"
#include <algorithm>
#include <cmath>

namespace gui {

SampleSlicer::SampleSlicer(GuiState& s, AudioEngine& e) : state(s), engine(e) {}

float SampleSlicer::Draw(Rectangle area, Pattern& pattern, bool inputBlocked) {
    float startY = area.y;
    Rectangle winRect = area;
    
    // Define safe viewport for interaction (exclude header and footer)
    Rectangle viewportRect = {winRect.x, winRect.y, winRect.width, winRect.height};
    bool isInViewport = CheckCollisionPointRec(state.getMousePosition(), viewportRect);
    
    DrawTextApp("Sample Slicer", winRect.x + 20, startY + 10, 24, WHITE);
    startY += 45;

    // Waveform Viewer - larger for touch
    Rectangle waveRect = {winRect.x + 20, startY, winRect.width - 40, 150};
    DrawRectangleRec(waveRect, BLACK);
    DrawRectangleLinesEx(waveRect, 2, GRAY);
    
    // Use editor's pattern buffer
    if (pattern.sampleBuffer.getNumSamples() > 0) {
        int numSamples = pattern.sampleBuffer.getNumSamples();
        
        // Safety: Ensure selected slice index is valid
        if (state.editor.selectedSliceIndex >= (int)pattern.sliceMarkers.size()) {
            state.editor.selectedSliceIndex = 0;
        }
        
        // Calculate visible sample range based on zoom and scroll
        float zoom = state.editor.waveformZoom;
        float viewWidth = 1.0f / zoom;
        float scrollMax = 1.0f - viewWidth;
        if (scrollMax < 0) scrollMax = 0;
        state.editor.waveformScrollX = std::min(std::max(state.editor.waveformScrollX, 0.0f), scrollMax);
        
        int startSample = (int)(state.editor.waveformScrollX * numSamples);
        int endSample = (int)((state.editor.waveformScrollX + viewWidth) * numSamples);
        if (endSample > numSamples) endSample = numSamples;
        int visibleSamples = endSample - startSample;
        
        // Draw waveform
        DrawWaveform(waveRect, pattern, startSample, endSample);
        
        // Draw slice markers
        DrawSliceMarkers(waveRect, pattern, startSample, endSample, inputBlocked, isInViewport);
        
        // Handle adding markers (only in Slice mode, not Play mode)
        if (!inputBlocked && isInViewport && !state.editor.slicerPlayModeEnabled && 
            CheckCollisionPointRec(state.getMousePosition(), waveRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            float localX = state.getMousePosition().x - waveRect.x;
            int sampleIdx = startSample + (int)(localX / waveRect.width * visibleSamples);
            
            pattern.sliceMarkers.push_back(sampleIdx);
            std::sort(pattern.sliceMarkers.begin(), pattern.sliceMarkers.end());
            engine.addPattern(pattern);
        }
        
        // Handle play mode interaction
        HandlePlayModeClick(waveRect, pattern, inputBlocked, isInViewport);
        
        // Scrollbar (only if zoomed)
        if (zoom > 1.0f) {
            DrawScrollbar(waveRect, zoom, inputBlocked, isInViewport);
        }
        
    } else {
        DrawTextApp("No Sample Loaded", waveRect.x + 10, waveRect.y + 60, 20, DARKGRAY);
    }
    
    // Move past waveform and scrollbar
    startY += (pattern.sampleBuffer.getNumSamples() > 0 && state.editor.waveformZoom > 1.0f) ? 185 : 165;
    
    // Draw control buttons
    DrawControlButtons(area, pattern, inputBlocked, isInViewport, startY);
    
    startY += 10;
    
    // Return total height used
    return startY - area.y;
}

void SampleSlicer::DrawWaveform(Rectangle waveRect, Pattern& pattern, int startSample, int endSample) {
    int numSamples = pattern.sampleBuffer.getNumSamples();
    const float* data = pattern.sampleBuffer.getReadPointer(0);
    int visibleSamples = endSample - startSample;
    
    float midY = waveRect.y + waveRect.height / 2;
    float halfH = waveRect.height / 2.0f;
    
    // Sort slice markers for binary search
    std::vector<int> sortedMarkers = pattern.sliceMarkers;
    // Ensure bounds are included for search logic
    if (sortedMarkers.empty() || sortedMarkers.front() != 0) sortedMarkers.insert(sortedMarkers.begin(), 0);
    if (sortedMarkers.back() != numSamples) sortedMarkers.push_back(numSamples);
    
    for (int x = 0; x < (int)waveRect.width; ++x) {
        float minVal = 0.0f;
        float maxVal = 0.0f;
        
        int sIdx = startSample + (int)((float)x / waveRect.width * visibleSamples);
        int eIdx = startSample + (int)((float)(x+1) / waveRect.width * visibleSamples);
        if (eIdx > endSample) eIdx = endSample;
        
        int step = std::max(1, (eIdx - sIdx) / 4);
        
        // Calculate Envelope for this pixel (using sIdx)
        float envelope = 1.0f;
        
        // Determine Slice Context
        int currentSliceStart = 0;
        int currentSliceEnd = numSamples;
        
        float activeFadeIn = 0.0f;
        float activeFadeOut = 0.0f;
        
        if (pattern.fadeSlices) {
             // Slice Mode: Default to 0.0f, lookup override
             auto it = std::upper_bound(sortedMarkers.begin(), sortedMarkers.end(), sIdx);
             int sliceIndex = 0;
             if (it != sortedMarkers.begin()) {
                 sliceIndex = (int)std::distance(sortedMarkers.begin(), std::prev(it));
                 currentSliceStart = *std::prev(it);
                 currentSliceEnd = (it != sortedMarkers.end()) ? *it : numSamples;
             }
             
             if (pattern.sliceFadeIns.count(sliceIndex)) activeFadeIn = pattern.sliceFadeIns.at(sliceIndex);
             if (pattern.sliceFadeOuts.count(sliceIndex)) activeFadeOut = pattern.sliceFadeOuts.at(sliceIndex);
        } else {
             // Global Mode
             currentSliceStart = 0;
             currentSliceEnd = numSamples;
             activeFadeIn = pattern.fadeIn;
             activeFadeOut = pattern.fadeOut;
        }
        
        double duration = (double)(currentSliceEnd - currentSliceStart);
        if (duration < 100.0) duration = 100.0;
        
        // Apply Fade In
        if (activeFadeIn > 0.001f) {
            double fadeInLen = duration * activeFadeIn;
            double pos = (double)(sIdx - currentSliceStart);
            if (pos < fadeInLen && pos >= 0) {
                envelope *= (float)(pos / fadeInLen);
            }
        }
        
        // Apply Fade Out
        if (activeFadeOut > 0.001f) {
            double fadeOutLen = duration * activeFadeOut;
            double distFromEnd = (double)(currentSliceEnd - sIdx);
            if (distFromEnd < fadeOutLen && distFromEnd >= 0) {
                envelope *= (float)(distFromEnd / fadeOutLen);
            }
        }
        
        // Anti-click fix for visuals: use squared envelope for better visual tapering?
        // Or linear mapping is fine.
        
        for (int s = sIdx; s < eIdx; s += step) {
            if (s >= 0 && s < numSamples) {
                float val = data[s];
                if (val < minVal) minVal = val;
                if (val > maxVal) maxVal = val;
            }
        }
        
        minVal *= envelope;
        maxVal *= envelope;
        
        DrawLine(waveRect.x + x, midY + minVal * halfH, waveRect.x + x, midY + maxVal * halfH, DARKGREEN);
    }
}

void SampleSlicer::DrawSliceMarkers(Rectangle waveRect, Pattern& pattern, int startSample, int endSample, bool inputBlocked, bool isInViewport) {
    int visibleSamples = endSample - startSample;
    
    for (int mFn = 0; mFn < (int)pattern.sliceMarkers.size(); ++mFn) {
        int sampleIdx = pattern.sliceMarkers[mFn];
        if (sampleIdx >= startSample && sampleIdx <= endSample) {
            float xPos = (float)(sampleIdx - startSample) / visibleSamples * waveRect.width;
            DrawLine(waveRect.x + xPos, waveRect.y, waveRect.x + xPos, waveRect.y + waveRect.height, RED);
            int textWidth = MeasureTextApp(TextFormat("%d", mFn), 10);
            DrawTextApp(TextFormat("%d", mFn), waveRect.x + xPos + 2, waveRect.y + 2, 10, YELLOW);
            
            // Handle Delete (Click on Number) - only in Slice mode
            if (!inputBlocked && isInViewport && !state.editor.slicerPlayModeEnabled && 
                CheckCollisionPointRec(state.getMousePosition(), {waveRect.x + xPos, waveRect.y, (float)textWidth + 4, 15}) && 
                IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                pattern.sliceMarkers.erase(pattern.sliceMarkers.begin() + mFn);
                mFn--;
                engine.addPattern(pattern);
            }
            
            // Handle Delete (Right Click near marker)
            if (!inputBlocked && isInViewport && 
                CheckCollisionPointRec(state.getMousePosition(), {waveRect.x + xPos - 5, waveRect.y, 10, waveRect.height}) && 
                IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                pattern.sliceMarkers.erase(pattern.sliceMarkers.begin() + mFn);
                mFn--;
                engine.addPattern(pattern);
            }
        }
    }
}

void SampleSlicer::DrawScrollbar(Rectangle waveRect, float zoom, bool inputBlocked, bool isInViewport) {
    float viewWidth = 1.0f / zoom;
    float scrollMax = 1.0f - viewWidth;
    if (scrollMax < 0) scrollMax = 0;
    
    Rectangle scrollBarBg = {waveRect.x, waveRect.y + waveRect.height + 5, waveRect.width, 25};
    DrawRectangleRec(scrollBarBg, DARKGRAY);
    
    float thumbWidth = scrollBarBg.width / zoom;
    float thumbX = scrollBarBg.x + state.editor.waveformScrollX / (1.0f - viewWidth + 0.001f) * (scrollBarBg.width - thumbWidth);
    Rectangle scrollThumb = {thumbX, scrollBarBg.y + 3, thumbWidth, 19};
    DrawRectangleRec(scrollThumb, LIGHTGRAY);
    
    // Drag scrollbar
    if (!inputBlocked && isInViewport && CheckCollisionPointRec(state.getMousePosition(), scrollBarBg) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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

void SampleSlicer::DrawControlButtons(Rectangle area, Pattern& pattern, bool inputBlocked, bool isInViewport, float& startY) {
    Rectangle winRect = area;
    
    // Clear Button
    Rectangle clearBtn = {winRect.x + 20, startY, 140, 45};
    DrawRectangleRec(clearBtn, RED);
    const char* clearTxt = "Clear";
    int clearW = MeasureTextApp(clearTxt, 16);
    DrawTextApp(clearTxt, clearBtn.x + (clearBtn.width - clearW)/2, clearBtn.y + 12, 16, WHITE);
    
    if (!inputBlocked && isInViewport && CheckCollisionPointRec(state.getMousePosition(), clearBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        pattern.sliceMarkers.clear();
        
        // Remove steps that have FX_SLICE
        std::vector<int> stepsToRemove;
        for (auto& stepPair : pattern.stepFX) {
            const auto& fxList = stepPair.second;
            if (std::find(fxList.begin(), fxList.end(), Pattern::FX_SLICE) != fxList.end()) {
                stepsToRemove.push_back(stepPair.first);
            }
        }
        
        for (int step : stepsToRemove) {
            pattern.stepPitches.erase(step);
            pattern.stepVelocities.erase(step);
            pattern.stepFX.erase(step);
            pattern.stepFXParams.erase(step);
            state.editor.stepStates[step - 1] = false;
            if (state.editor.selectedStep == step - 1) {
                state.editor.selectedStep = -1;
            }
        }
        
        engine.addPattern(pattern);
    }
    
    // Cutoff Toggle
    Rectangle cutBtn = {winRect.x + 170, startY, 80, 45};
    DrawRectangleRec(cutBtn, state.editor.slicerCutoffEnabled ? GREEN : DARKGRAY);
    const char* cutTxt = "Cut";
    int cutW = MeasureTextApp(cutTxt, 16);
    DrawTextApp(cutTxt, cutBtn.x + (cutBtn.width - cutW)/2, cutBtn.y + 12, 16, WHITE);
    
    if (!inputBlocked && isInViewport && CheckCollisionPointRec(state.getMousePosition(), cutBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.slicerCutoffEnabled = !state.editor.slicerCutoffEnabled;
    }
    
    // Play/Slice Mode Switch
    float switchStartX = winRect.x + 260;
    Rectangle sliceRect = {switchStartX, startY, 70, 45};
    Rectangle playRect = {switchStartX + 70, startY, 70, 45};
    
    if (!state.editor.slicerPlayModeEnabled) {
        DrawRectangleRec(sliceRect, WHITE);
        DrawRectangleRec(playRect, GRAY);
        const char* sliceTxt = "Slice";
        int sliceW = MeasureTextApp(sliceTxt, 16);
        DrawTextApp(sliceTxt, sliceRect.x + (sliceRect.width - sliceW)/2, sliceRect.y + 12, 16, BLACK);
        const char* playTxt = "Play";
        int playW = MeasureTextApp(playTxt, 16);
        DrawTextApp(playTxt, playRect.x + (playRect.width - playW)/2, playRect.y + 12, 16, WHITE);
    } else {
        DrawRectangleRec(sliceRect, GRAY);
        DrawRectangleRec(playRect, WHITE);
        const char* sliceTxt = "Slice";
        int sliceW = MeasureTextApp(sliceTxt, 16);
        DrawTextApp(sliceTxt, sliceRect.x + (sliceRect.width - sliceW)/2, sliceRect.y + 12, 16, WHITE);
        const char* playTxt = "Play";
        int playW = MeasureTextApp(playTxt, 16);
        DrawTextApp(playTxt, playRect.x + (playRect.width - playW)/2, playRect.y + 12, 16, BLACK);
    }
    
    if (!inputBlocked && isInViewport && CheckCollisionPointRec(state.getMousePosition(), sliceRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.slicerPlayModeEnabled = false;
    }
    if (!inputBlocked && isInViewport && CheckCollisionPointRec(state.getMousePosition(), playRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.slicerPlayModeEnabled = true;
    }
    
    // Zoom buttons
    Rectangle zoomOutBtn = {winRect.x + 410, startY, 45, 45};
    Rectangle zoomInBtn = {winRect.x + 460, startY, 45, 45};
    DrawRectangleRec(zoomOutBtn, DARKGRAY);
    DrawRectangleRec(zoomInBtn, DARKGRAY);
    DrawTextApp("-", zoomOutBtn.x + 16, zoomOutBtn.y + 10, 24, WHITE);
    DrawTextApp("+", zoomInBtn.x + 14, zoomInBtn.y + 10, 24, WHITE);
    
    if (!inputBlocked && isInViewport && CheckCollisionPointRec(state.getMousePosition(), zoomInBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.waveformZoom = std::min(state.editor.waveformZoom * 1.5f, 20.0f);
    }
    if (!inputBlocked && isInViewport && CheckCollisionPointRec(state.getMousePosition(), zoomOutBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.editor.waveformZoom = std::max(state.editor.waveformZoom / 1.5f, 1.0f);
        if (state.editor.waveformZoom <= 1.0f) state.editor.waveformScrollX = 0.0f;
    }
    
    startY += 55;
    
    // Bookend Button
    if (pattern.sampleBuffer.getNumSamples() > 0) {
        Rectangle bookendBtn = {winRect.x + 20, startY, 150, 45};
        DrawRectangleRec(bookendBtn, BLUE);
        const char* bookTxt = "Bookend";
        int bookW = MeasureTextApp(bookTxt, 16);
        DrawTextApp(bookTxt, bookendBtn.x + (bookendBtn.width - bookW)/2, bookendBtn.y + 12, 16, WHITE);
        
        if (!inputBlocked && isInViewport && CheckCollisionPointRec(state.getMousePosition(), bookendBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            bool changed = false;
            if (std::find(pattern.sliceMarkers.begin(), pattern.sliceMarkers.end(), 0) == pattern.sliceMarkers.end()) {
                pattern.sliceMarkers.push_back(0);
                changed = true;
            }
            int64_t total = pattern.sampleBuffer.getNumSamples();
            if (std::find(pattern.sliceMarkers.begin(), pattern.sliceMarkers.end(), total) == pattern.sliceMarkers.end()) {
                pattern.sliceMarkers.push_back(total);
                changed = true;
            }
            
            if (changed) {
                std::sort(pattern.sliceMarkers.begin(), pattern.sliceMarkers.end());
                engine.addPattern(pattern);
            }
        }
        startY += 55;
    }
    
    // --- SLICE SELECTOR ---
    if (!pattern.sliceMarkers.empty()) {
        DrawTextApp("SLICE:", winRect.x + 20, startY + 14, 20, WHITE);
        float sliceBtnX = winRect.x + 110; // Offset to fit longer label
        
        for (int i = 0; i < (int)pattern.sliceMarkers.size() && i < 8; ++i) { // Limit to 8 for now or until wrap logic
            Rectangle sBtn = {sliceBtnX, startY, 40, 40};
            bool isSelected = (state.editor.selectedSliceIndex == i);
            DrawRectangleRec(sBtn, isSelected ? GREEN : DARKGRAY);
            DrawRectangleLinesEx(sBtn, 1, WHITE);
            
            const char* sTxt = TextFormat("%d", i);
            int sW = MeasureTextApp(sTxt, 16);
            DrawTextApp(sTxt, sBtn.x + (sBtn.width - sW)/2, sBtn.y + 12, 16, WHITE);
            
            if (!inputBlocked && isInViewport && CheckCollisionPointRec(state.getMousePosition(), sBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                state.editor.selectedSliceIndex = i;
            }
            
            sliceBtnX += 45;
        }
        startY += 55;
    }
    
    // --- FADE CONTROLS ---
    DrawTextApp("FADE SETTINGS:", winRect.x + 20, startY + 10, 20, WHITE);
    startY += 35;
    
    // Fade Mode Switch (Global vs Slices) - Matches Play/Slice style
    float fadeSwitchX = winRect.x + 80;
    Rectangle globalRect = {fadeSwitchX, startY, 70, 30};
    Rectangle slicesRect = {fadeSwitchX + 70, startY, 70, 30};
    
    bool isSliceMode = pattern.fadeSlices;
    
    // Draw Switch
    if (isSliceMode) {
        DrawRectangleRec(globalRect, GRAY);
        DrawRectangleRec(slicesRect, WHITE);
        
        const char* gTxt = "GLOBAL";
        int gW = MeasureTextApp(gTxt, 10);
        DrawTextApp(gTxt, globalRect.x + (globalRect.width - gW)/2, globalRect.y + 10, 10, WHITE);
        
        const char* sTxt = "SLICES";
        int sW = MeasureTextApp(sTxt, 10);
        DrawTextApp(sTxt, slicesRect.x + (slicesRect.width - sW)/2, slicesRect.y + 10, 10, BLACK);
    } else {
        DrawRectangleRec(globalRect, WHITE);
        DrawRectangleRec(slicesRect, GRAY);
        
        const char* gTxt = "GLOBAL";
        int gW = MeasureTextApp(gTxt, 10);
        DrawTextApp(gTxt, globalRect.x + (globalRect.width - gW)/2, globalRect.y + 10, 10, BLACK);
        
        const char* sTxt = "SLICES";
        int sW = MeasureTextApp(sTxt, 10);
        DrawTextApp(sTxt, slicesRect.x + (slicesRect.width - sW)/2, slicesRect.y + 10, 10, WHITE);
    }
    
    DrawTextApp("FADE:", winRect.x + 20, startY + 8, 16, WHITE);
    
    // Interaction
    if (!inputBlocked && isInViewport && CheckCollisionPointRec(state.getMousePosition(), globalRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        pattern.fadeSlices = false;
        engine.addPattern(pattern);
    }
    if (!inputBlocked && isInViewport && CheckCollisionPointRec(state.getMousePosition(), slicesRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        pattern.fadeSlices = true;
        engine.addPattern(pattern);
    }
    
    startY += 40;
    
    // Determine which values to edit
    float* inVal = &pattern.fadeIn;
    float* outVal = &pattern.fadeOut;
    
    // UI Feedback: Show which slice is being edited
    if (pattern.fadeSlices) {
        // Ensure map entries exist or use defaults (0.0f for slice mode to avoid global bleed)
        if (pattern.sliceFadeIns.find(state.editor.selectedSliceIndex) == pattern.sliceFadeIns.end()) {
             pattern.sliceFadeIns[state.editor.selectedSliceIndex] = 0.0f;
        }
        if (pattern.sliceFadeOuts.find(state.editor.selectedSliceIndex) == pattern.sliceFadeOuts.end()) {
             pattern.sliceFadeOuts[state.editor.selectedSliceIndex] = 0.0f;
        }
        
        inVal = &pattern.sliceFadeIns[state.editor.selectedSliceIndex];
        outVal = &pattern.sliceFadeOuts[state.editor.selectedSliceIndex];
    }
    
    // Fade In Slider using DrawSlider widget
    DrawTextApp("IN:", winRect.x + 20, startY + 15, 20, WHITE);
    bool fadeInDragging = false;
    Rectangle fadeInRect = {winRect.x + 80, startY, 280, 50};
    float newFadeIn = DrawSlider(fadeInRect, *inVal, 0.0f, 0.5f, DARKGRAY, LIGHTGRAY, state.getMousePosition(), &fadeInDragging);
    if (fadeInDragging) {
        state.editor.scrollConsumed = true; // Block scroll while dragging
        *inVal = newFadeIn;
        engine.addPattern(pattern);
    }
    DrawTextApp(TextFormat("%.0f%%", *inVal * 100.0f), fadeInRect.x + fadeInRect.width + 15, startY + 18, 20, WHITE);
    startY += 65;
    
    // Fade Out Slider using DrawSlider widget
    DrawTextApp("OUT:", winRect.x + 15, startY + 15, 20, WHITE);
    bool fadeOutDragging = false;
    Rectangle fadeOutRect = {winRect.x + 80, startY, 280, 50};
    float newFadeOut = DrawSlider(fadeOutRect, *outVal, 0.0f, 0.5f, DARKGRAY, LIGHTGRAY, state.getMousePosition(), &fadeOutDragging);
    if (fadeOutDragging) {
        state.editor.scrollConsumed = true; // Block scroll while dragging
        *outVal = newFadeOut;
        engine.addPattern(pattern);
    }
    DrawTextApp(TextFormat("%.0f%%", *outVal * 100.0f), fadeOutRect.x + fadeOutRect.width + 15, startY + 18, 20, WHITE);
    startY += 65;
}

void SampleSlicer::HandlePlayModeClick(Rectangle waveRect, Pattern& pattern, bool inputBlocked, bool isInViewport) {
    if (!inputBlocked && isInViewport && state.editor.slicerPlayModeEnabled && 
        CheckCollisionPointRec(state.getMousePosition(), waveRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        
        if (pattern.sampleBuffer.getNumSamples() > 0) {
            int64_t totalSamples = pattern.sampleBuffer.getNumSamples();
            float zoom = state.editor.waveformZoom;
            float viewWidth = 1.0f / zoom;
            int64_t visibleSamples = (int64_t)(totalSamples * viewWidth);
            
            float scrollMax = 1.0f - viewWidth;
            if (scrollMax < 0) scrollMax = 0;
            if (state.editor.waveformScrollX > scrollMax) state.editor.waveformScrollX = scrollMax;
            
            int64_t startSample = (int64_t)(state.editor.waveformScrollX / (scrollMax + 0.001f) * (totalSamples - visibleSamples));
            if (startSample < 0) startSample = 0;
            
            float localX = state.getMousePosition().x - waveRect.x;
            int sampleIdx = startSample + (int)(localX / waveRect.width * visibleSamples);
            
            // Find slice
            int foundSlice = -1;
            if (sampleIdx >= 0 && !pattern.sliceMarkers.empty()) {
                for (size_t i = 0; i < pattern.sliceMarkers.size(); ++i) {
                    int64_t sStart = pattern.sliceMarkers[i];
                    int64_t sEnd = (i + 1 < pattern.sliceMarkers.size()) ? pattern.sliceMarkers[i+1] : pattern.sampleBuffer.getNumSamples();
                    
                    if (sampleIdx >= sStart && sampleIdx < sEnd) {
                        foundSlice = (int)i;
                        break;
                    }
                }
            }
            
            if (foundSlice != -1) {
                // Preview the slice
                engine.previewSlice(pattern, foundSlice, !state.editor.slicerCutoffEnabled);
                
                // Live Edit Mode: Assign slice to selected step
                if (state.isLiveEditMode && state.editor.selectedStep >= 0 && state.editor.selectedStep < 64) {
                    int step = state.editor.selectedStep + 1;
                    
                    if (std::find(pattern.stepFX[step].begin(), pattern.stepFX[step].end(), Pattern::FX_SLICE) == pattern.stepFX[step].end()) {
                        pattern.stepFX[step].push_back(Pattern::FX_SLICE);
                    }
                    
                    pattern.stepFXParams[step][Pattern::PAR_SLICE_INDEX] = (float)foundSlice;
                    if (state.editor.slicerCutoffEnabled) {
                        pattern.stepFXParams[step][Pattern::PAR_SLICE_CUTOFF] = 1.0f;
                    } else {
                        pattern.stepFXParams[step].erase(Pattern::PAR_SLICE_CUTOFF);
                    }
                    
                    state.editor.stepStates[state.editor.selectedStep] = true;
                    pattern.stepPitches[step] = 0;
                    pattern.stepVelocities[step] = 1.0f;
                    
                    engine.addPattern(pattern);
                }
            }
        }
    }
}

} // namespace gui
