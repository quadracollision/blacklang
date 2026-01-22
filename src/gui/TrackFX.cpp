#include "TrackFX.h"
#include "../AudioEngine.h"
#include <cstdio>
#include <string>
#include <cmath>
#include <algorithm> // for std::max
#if defined(__ANDROID__)
#include <android/log.h>
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "BlackLang_FX", __VA_ARGS__)
#else
#include <cstdio>
#define LOGD(...) printf(__VA_ARGS__); printf("\n")
#endif

namespace gui {

// Helper to get touch-friendly scroll
static void HandleMixerScroll(const Rectangle& bounds, PatternColumn& col, GuiState& state) {
    Vector2 mouse = state.getMousePosition();
    
    // Check for scroll start
    if (CheckCollisionPointRec(mouse, bounds)) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            // Only start scrolling if we aren't clicking a control
            // (We rely on control interaction flags handled in main draw)
            col.isDragging = true;
            col.dragStartY = mouse.y;
            col.dragStartScroll = col.mixerScrollY;
        }
    }
    
    // Update scroll
    if (col.isDragging) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float delta = col.dragStartY - mouse.y;
            col.mixerScrollY = col.dragStartScroll + delta;
        } else {
            col.isDragging = false;
        }
    }
    
    // Mouse wheel fallback
    if (CheckCollisionPointRec(mouse, bounds)) {
        col.mixerScrollY -= GetMouseWheelMove() * 30.0f;
    }
    
    // Clamp
    float maxScroll = std::max(0.0f, col.mixerContentHeight - bounds.height);
    if (col.mixerScrollY < 0) col.mixerScrollY = 0;
    if (col.mixerScrollY > maxScroll) col.mixerScrollY = maxScroll;
}

static bool DrawGenericFXEditor(const Rectangle& bounds, const Rectangle& clipRect, std::shared_ptr<fx::TrackEffect> effect, PatternColumn& col, GuiState& state) {
    float startY = bounds.y;
    float currentY = startY;
    bool anyInteraction = false;
    
    // FX Title
    int nameWidth = MeasureTextApp(effect->getName().c_str(), 20);
    DrawTextApp(effect->getName().c_str(), bounds.x + 20, currentY, 20, ORANGE);

    currentY += 40;
    
    // Params
    for (int i = 0; i < effect->getNumParams(); ++i) {
        fx::TrackFXParam& param = effect->getParam(i);
        
        // Label
        DrawTextApp(param.name.c_str(), bounds.x + 20, currentY, 16, GRAY);
        
        // Slider Background
        Rectangle sliderRect = {bounds.x + 20, currentY + 15, bounds.width - 40, 30};
        DrawRectangleRec(sliderRect, Color{10, 10, 10, 255});
        DrawRectangleLinesEx(sliderRect, 1, DARKGRAY);
        
        // Slider Handle
        float normalized = (param.value - param.min) / (param.max - param.min);
        float handleX = sliderRect.x + (normalized * sliderRect.width);
        DrawRectangle(handleX - 5, sliderRect.y, 10, sliderRect.height, ORANGE);
        
        // Value Text
        char buf[32];
        sprintf(buf, "%.1f%s", param.value, param.suffix.c_str());
        DrawTextApp(buf, sliderRect.x + sliderRect.width - MeasureTextApp(buf, 14) - 5, currentY, 14, WHITE);
        
        // Interaction - only start drag if click was available
        Rectangle hitTest = {sliderRect.x - 10, sliderRect.y - 10, sliderRect.width + 20, sliderRect.height + 20};
        bool mouseInHitbox = (!col.isDragging && CheckCollisionPointRec(state.getMousePosition(), hitTest) && CheckCollisionPointRec(state.getMousePosition(), clipRect));
        
        if (state.editor.isOpen == false) {
             std::string paramId = col.trackName + "_FX_View_P" + std::to_string(i);
             
             // 1. Is locked to something else?
             bool isLockedToOther = (!state.drag.activeControlId.empty() && state.drag.activeControlId != paramId);
             
             // 2. Start Condition: Clicked THIS slider (and allowed)
             bool startInteract = (!isLockedToOther && mouseInHitbox && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && state.isClickAvailable());
             
             // 3. Continue Condition: Locked to THIS slider and holding
             bool continueInteract = (state.drag.activeControlId == paramId && IsMouseButtonDown(MOUSE_LEFT_BUTTON));

             if (startInteract || continueInteract) {
                 if (startInteract) {
                     LOGD("FX Slider Interact Start: %s", paramId.c_str());
                     state.consumeClick();
                     state.drag.activeControlId = paramId; // Lock
                 }
                 
                 anyInteraction = true;
                 float mouseX = state.getMousePosition().x;
                 float rawNorm = (mouseX - sliderRect.x) / sliderRect.width;
                 if (rawNorm < 0.0f) rawNorm = 0.0f;
                 if (rawNorm > 1.0f) rawNorm = 1.0f;
                 
                 float newValue = param.min + (rawNorm * (param.max - param.min));
                 effect->setParam(i, newValue);
             }
        }
        
        currentY += 60;
    }
    return anyInteraction;
}

void DrawTrackMixer(const Rectangle& bounds, PatternColumn& col, GuiState& state, AudioEngine& engine) {
    bool canInteract = !state.editor.isOpen;
    
    // Get the track bus for this column
    AudioBus* trackBus = engine.getTrackBus(col.trackName);
    if (!trackBus) return;  // No bus assigned yet
    
    // 1. Background
    DrawRectangleRec(bounds, Color{20, 20, 25, 255});
    
    // 2. Header (Fixed)
    float headerH = 40.0f;
    float tabH = 40.0f;
    float topOffset = headerH + tabH;
    
    Rectangle headerRect = {bounds.x, bounds.y, bounds.width, headerH};
    DrawRectangleRec(headerRect, Color{30, 30, 35, 255});
    
    Rectangle backBtn = {bounds.x + 5, bounds.y + 5, 30, 30};
    DrawRectangleRec(backBtn, Color{50, 50, 50, 255});
    DrawTextApp("<", backBtn.x + 10, backBtn.y + 5, 20, WHITE);
    
    if (canInteract && state.isClickAvailable() && CheckCollisionPointRec(state.getMousePosition(), backBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick();
        col.mixerMode = false;
        col.isDragging = false; 
        return;
    }
    
    std::string header = col.title;
    DrawTextApp(header.c_str(), bounds.x + 45, bounds.y + 10, 20, LIGHTGRAY);
    DrawLine(bounds.x, bounds.y + headerH, bounds.x + bounds.width, bounds.y + headerH, DARKGRAY);

    // 3. FX Tabs (Fixed below header)
    Rectangle tabArea = {bounds.x, bounds.y + headerH, bounds.width, tabH};
    DrawRectangleRec(tabArea, Color{25, 25, 30, 255});
    
    const int maxSlots = 4; // Vol + 3 Inserts
    float tabWidth = bounds.width / maxSlots;
    
    for (int i = 0; i < maxSlots; ++i) {
        Rectangle tabRect = {bounds.x + (i * tabWidth), bounds.y + headerH, tabWidth, tabH};
        bool isSelected = (col.selectedFXSlot == i);
        
        DrawRectangleRec(tabRect, isSelected ? Color{60, 60, 70, 255} : Color{35, 35, 40, 255});
        DrawRectangleLinesEx(tabRect, 1, Color{20, 20, 20, 255});
        
        const char* label = "Empty";
        if (i == 0) label = "MIX";
        else {
            int fxIndex = i - 1;
            std::lock_guard<std::mutex> lock(trackBus->effectsMutex);
            if (fxIndex < trackBus->effects.size() && trackBus->effects[fxIndex]) {
                // Use first 3 chars of name
                char nameBuf[4]; 
                std::string n = trackBus->effects[fxIndex]->getName();
                snprintf(nameBuf, 4, "%s", n.c_str());
                label = nameBuf;
            } else {
                label = "+";
            }
        }
        
        // Centered Text
        DrawTextApp(label, tabRect.x + (tabWidth/2) - MeasureTextApp(label, 12)/2, tabRect.y + 12, 12, isSelected ? WHITE : GRAY);
        
        if (canInteract && state.isClickAvailable() && CheckCollisionPointRec(state.getMousePosition(), tabRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.consumeClick();
            col.selectedFXSlot = i;
            col.mixerScrollY = 0; // Reset scroll on switch
        }
    }
    
    // 4. Content Area (Scrollable)
    Rectangle scrollArea = {bounds.x, bounds.y + topOffset, bounds.width, bounds.height - topOffset};
    BeginScissorMode((int)scrollArea.x, (int)scrollArea.y, (int)scrollArea.width, (int)scrollArea.height);
    
    float contentStartY = scrollArea.y + 20 - col.mixerScrollY;
    float currentY = contentStartY;
    
    bool isInteracting = false; // Flag to track if we are touching a control
    
    if (col.selectedFXSlot == 0) {
        // --- VOLUME / PAN EDITOR ---
        
        // PAN
        Rectangle panRect = {bounds.x + 20, currentY + 15, bounds.width - 40, 30};
        DrawTextApp("PAN", panRect.x, panRect.y - 15, 16, GRAY);
        DrawRectangleRec(panRect, Color{10, 10, 10, 255});
        float panHandleX = panRect.x + (trackBus->pan * panRect.width);
        DrawRectangle(panHandleX - 5, panRect.y, 10, panRect.height, ORANGE);
        
        Rectangle hitTestPan = {panRect.x, panRect.y - 10, panRect.width, panRect.height + 20};
        if (canInteract && CheckCollisionPointRec(state.getMousePosition(), hitTestPan) && CheckCollisionPointRec(state.getMousePosition(), scrollArea)) {
             
             std::string panId = col.trackName + "_Pan";
             
             bool isLockedToOther = (!state.drag.activeControlId.empty() && state.drag.activeControlId != panId);
             bool startInteract = (!isLockedToOther && !col.isDragging && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && state.isClickAvailable());
             bool continueInteract = (state.drag.activeControlId == panId && IsMouseButtonDown(MOUSE_LEFT_BUTTON));

            if (startInteract || continueInteract) {
                 if (startInteract) {
                     LOGD("Pan Interact Start: %s", panId.c_str());
                     state.consumeClick();
                     state.drag.activeControlId = panId; // Lock
                 }
                 
                 isInteracting = true; // Capture interaction
                 float mouseX = state.getMousePosition().x;
                 float newPan = (mouseX - panRect.x) / panRect.width;
                 if (newPan < 0.0f) newPan = 0.0f;
                 if (newPan > 1.0f) newPan = 1.0f;
                 col.pan = newPan;
                 trackBus->pan = newPan;
            }
            
            // Double click reset pan
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && state.isClickAvailable()) {
                static std::string lastClickId = "";
                static double lastClickTime = 0;
                
                std::string currentId = col.trackName + "_Pan";
                double now = GetTime();
                
                if (currentId == lastClickId && (now - lastClickTime < 0.3)) { 
                    state.consumeClick();
                    col.pan = 0.5f; trackBus->pan = 0.5f; 
                    lastClickId = ""; // Reset to prevent triple click
                } else {
                    lastClickId = currentId;
                    lastClickTime = now;
                }
            }
        } 
        else if (state.drag.activeControlId == (col.trackName + "_Pan") && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
             // Handle 'continueInteract' even if mouse left the rect
             isInteracting = true;
             float mouseX = state.getMousePosition().x;
             float newPan = (mouseX - panRect.x) / panRect.width;
             if (newPan < 0.0f) newPan = 0.0f;
             if (newPan > 1.0f) newPan = 1.0f;
             col.pan = newPan;
             trackBus->pan = newPan;
        }
        
        currentY += 80;
        
        // VOLUME
        float faderHeight = 250.0f; 
        Rectangle volRect = {bounds.x + 40, currentY + 20, bounds.width - 80, faderHeight};
        DrawTextApp("VOL", volRect.x, volRect.y - 15, 16, GRAY);
        DrawRectangleRec(volRect, Color{10, 10, 10, 255});
        
        float fillHeight = trackBus->volume * volRect.height;
        Rectangle fillRect = {volRect.x, volRect.y + volRect.height - fillHeight, volRect.width, fillHeight};
        Color volColor = (trackBus->volume > 0.95f) ? RED : ((trackBus->volume > 0.8f) ? ORANGE : GREEN);
        
        DrawRectangleRec(fillRect, volColor);
        DrawRectangleLinesEx(volRect, 1, DARKGRAY);
        DrawRectangle(volRect.x - 5, fillRect.y, volRect.width + 10, 10, WHITE);
        
        // Value
        char buf[32];
        sprintf(buf, "%d%%", (int)(trackBus->volume * 100));
        DrawTextApp(buf, volRect.x + 10, volRect.y + volRect.height + 10, 20, Color{255,255,255,100});

        // Track volume slider drag
        Rectangle hitTestVol = {volRect.x - 15, volRect.y, volRect.width + 30, volRect.height};
        if (canInteract && CheckCollisionPointRec(state.getMousePosition(), hitTestVol) && CheckCollisionPointRec(state.getMousePosition(), scrollArea)) {
             
             std::string volId = col.trackName + "_Vol";
             
             bool isLockedToOther = (!state.drag.activeControlId.empty() && state.drag.activeControlId != volId);
             bool startInteract = (!isLockedToOther && !col.isDragging && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && state.isClickAvailable());
             bool continueInteract = (state.drag.activeControlId == volId && IsMouseButtonDown(MOUSE_LEFT_BUTTON));
             
             if (startInteract || continueInteract) {
                 if (startInteract) {
                     LOGD("Vol Interact Start: %s", volId.c_str());
                     state.consumeClick();
                     state.drag.activeControlId = volId; // Lock
                 }
                 
                 isInteracting = true; // Capture interaction
                 float mouseY = state.getMousePosition().y;
                 float rawVal = ((volRect.y + volRect.height) - mouseY) / volRect.height;
                 if (rawVal < 0.0f) rawVal = 0.0f; if (rawVal > 1.0f) rawVal = 1.0f;
                 col.volume = rawVal;
                 trackBus->volume = rawVal;
             }
        } 
        else if (state.drag.activeControlId == (col.trackName + "_Vol") && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
              // Handle 'continueInteract' even if mouse left the rect
              isInteracting = true;
              float mouseY = state.getMousePosition().y;
              float rawVal = ((volRect.y + volRect.height) - mouseY) / volRect.height;
              if (rawVal < 0.0f) rawVal = 0.0f; if (rawVal > 1.0f) rawVal = 1.0f;
              col.volume = rawVal;
              trackBus->volume = rawVal;
        }
        currentY += faderHeight + 60;
        
    } else {
        // --- FX EDITOR ---
        int fxIndex = col.selectedFXSlot - 1;
        
        std::shared_ptr<fx::TrackEffect> targetEffect = nullptr;
        {
            std::lock_guard<std::mutex> lock(trackBus->effectsMutex);
            // Ensure vector is big enough (pad with nulls)
            while (trackBus->effects.size() <= fxIndex) {
                trackBus->effects.push_back(nullptr);
            }
            targetEffect = trackBus->effects[fxIndex];
        }
        
        if (targetEffect) {
            // Draw Parameters
            Rectangle contentRect = {bounds.x, currentY, bounds.width, 0};
            
            // Branch for EQ vs Generic
            if (targetEffect->getType() == fx::FX_EQ) {
                // --- EQ EDITOR ---
                std::shared_ptr<fx::TrackEffect> effect = targetEffect;
                
                // 1. Band Selection (Param 0)
                fx::TrackFXParam& bandsParam = effect->getParam(0);
                DrawTextApp("Bands:", contentRect.x + 20, currentY, 10, GRAY);
                
                const char* bandOpt[] = {"3", "6", "12"};
                for (int i=0; i<3; ++i) {
                     Rectangle optRect = {contentRect.x + 60 + (i*40), currentY - 5, 30, 20};
                     bool isSelected = ((int)bandsParam.value == i);
                     DrawRectangleRec(optRect, isSelected ? WHITE : Color{40, 40, 40, 255});
                     DrawTextApp(bandOpt[i], optRect.x + 10, optRect.y + 5, 10, isSelected ? BLACK : WHITE);
                     
                     if (canInteract && state.isClickAvailable() && CheckCollisionPointRec(state.getMousePosition(), optRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                         state.consumeClick();
                         effect->setParam(0, (float)i);
                     }
                }
                currentY += 40;
                
                // 2. Bands Grid (Highs on Top, Rows of 3)
                int mode = (int)bandsParam.value;
                int numBands = (mode == 0) ? 3 : (mode == 1 ? 6 : 12);
                
                // We display active bands in REVERSE order (Highs first)
                // But laid out Left->Right. 
                // Wait, typically EQ is Low->High L->R. User said "Highs on top, lows on bottom".
                // Logic: 
                // Row 1 (Top): High Bands
                // Row 2: Mid Bands
                // Row 3 (Bottom): Low Bands
                
                int bandsPerRow = 3;
                int rows = (numBands + bandsPerRow - 1) / bandsPerRow;
                
                // Active bands start at index 1 in params (index 0 is band count)
                // The params are ordered Low -> High by default setup.
                // So index 1 = Lowest, index numBands = Highest.
                
                // We want Top Row to have Highest indices.
                // Bottom Row to have Lowest indices.
                
                for (int r = 0; r < rows; ++r) {
                    // Row 0 is Top. We want the highest bands here.
                    // Total bands = numBands.
                    // Row 0 starts at: numBands - bandsPerRow.
                    // Actually, let's just reverse iterate chunks.
                    
                    int rowStartIndex = numBands - (r + 1) * bandsPerRow;
                    if (rowStartIndex < 0) rowStartIndex = 0; // Should handle clean multiples
                    
                    // But actually, for 12 bands:
                    // R0: 10, 11, 12 (Highs)
                    // R1: 7, 8, 9
                    // ...
                    // R3: 1, 2, 3 (Lows)
                    
                    // Correction: The bands are indices 1..12 in params.
                    // Let's iterate rows.
                    
                    for (int c = 0; c < bandsPerRow; ++c) {
                        // We want high frequency on RIGHT of row? Or Left? 
                        // Typically EQ is Low->High. 
                        // If "Highs on Top", maybe Highs are L->R? 
                        // "rows of 3... highs on top"
                        // Simplest: Row 0 has the 3 highest bands. Row Last has 3 lowest.
                        // Within a row, usually Low->High (Left->Right).
                        
                        // Example 6 band:
                        // Row 0: Band 4, 5, 6
                        // Row 1: Band 1, 2, 3
                        
                        // Calculate band index for this slot
                        // Total rows: rows. Current row: r (0 is top).
                        // Row index from bottom: (rows - 1) - r
                        
                        int rowFromBottom = (rows - 1) - r;
                        int bandIndexInSet = (rowFromBottom * bandsPerRow) + c; // 0-based index in active bands
                        
                        if (bandIndexInSet >= numBands) continue;
                        
                        // Param index = bandIndexInSet + 1 (skip band count param)
                        int pIndex = bandIndexInSet + 1;
                        fx::TrackFXParam& param = effect->getParam(pIndex);
                        
                        // Retrieve frequency for label
                        float freq = 0.0f;
                        if (auto eqEffect = std::dynamic_pointer_cast<fx::EQEffect>(effect)) {
                            // We need to access the band info directly or reconstruct frequency logic
                            // Reconstruct logic for simplicity (matches EQEffect::setupBands)
                            int mode = (int)bandsParam.value;
                            std::vector<float> freqs;
                            if (mode == 0) freqs = {100.0f, 1000.0f, 10000.0f};
                            else if (mode == 1) freqs = {60.0f, 200.0f, 600.0f, 2000.0f, 6000.0f, 12000.0f};
                            else freqs = {30.0f, 60.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 12000.0f, 16000.0f, 20000.0f};
                            
                            if (bandIndexInSet < (int)freqs.size()) {
                                freq = freqs[bandIndexInSet];
                            }
                        }
                        
                        // Format frequency string
                        char labelBuf[16];
                        if (freq >= 1000.0f) {
                            sprintf(labelBuf, "%.1fk", freq / 1000.0f);
                        } else {
                            sprintf(labelBuf, "%.0fHz", freq);
                        }
                        
                        // Dynamic Layout for Narrow Tracks
                        float colWidth = contentRect.width / 3.0f;
                        float sliderW = colWidth - 6.0f; // Maximize width (3px padding per side)
                        
                        // Center in column
                        Rectangle bandRect = {contentRect.x + (c * colWidth), currentY, colWidth, 70};
                        
                        // Centered Text
                        int labelW = MeasureTextApp(labelBuf, 10);
                        DrawTextApp(labelBuf, bandRect.x + (colWidth - labelW)/2, bandRect.y, 10, GRAY);
                        
                        // Knob Logic (Vertical Slider visual for EQ gain usually best, or Knob)
                        // Make slider taller and wider for touch
                        Rectangle sliderRect = {bandRect.x + (colWidth - sliderW)/2, bandRect.y + 15, sliderW, 50};
                        
                        DrawRectangleRec(sliderRect, Color{20, 20, 20, 255});
                        DrawRectangleLinesEx(sliderRect, 1, DARKGRAY);
                        
                        // Center zero
                        float range = param.max - param.min;
                        float norm = (param.value - param.min) / range;
                        float zeroNorm = (0.0f - param.min) / range;
                        
                        float zeroY = sliderRect.y + sliderRect.height * (1.0f - zeroNorm);
                        float valY = sliderRect.y + sliderRect.height * (1.0f - norm);
                        
                        // Fill from zero to val
                        if (norm > zeroNorm) {
                            DrawRectangle(sliderRect.x + 1, valY, sliderRect.width - 2, zeroY - valY, ORANGE);
                        } else {
                            DrawRectangle(sliderRect.x + 1, zeroY, sliderRect.width - 2, valY - zeroY, ORANGE);
                        }
                        
                        // Center Line
                        DrawLine(sliderRect.x, zeroY, sliderRect.x + sliderRect.width, zeroY, WHITE);
                        
                        // Value Text
                        char buf[16];
                        sprintf(buf, "%+.0f", param.value);
                        DrawTextApp(buf, sliderRect.x + 5, sliderRect.y + sliderRect.height + 2, 10, WHITE);
                        
                        // Interaction
                        Rectangle hitTest = {sliderRect.x - 5, sliderRect.y - 5, sliderRect.width + 10, sliderRect.height + 10};
                        
                        // Track drag
                        std::string eqParamId = col.trackName + "_EQ_P" + std::to_string(pIndex);
                        bool isLockedToOther = (!state.drag.activeControlId.empty() && state.drag.activeControlId != eqParamId);
                        
                        bool startInteract = (!isLockedToOther && !col.isDragging && CheckCollisionPointRec(state.getMousePosition(), hitTest) && CheckCollisionPointRec(state.getMousePosition(), scrollArea) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && state.isClickAvailable());
                        bool continueInteract = (state.drag.activeControlId == eqParamId && IsMouseButtonDown(MOUSE_LEFT_BUTTON));
                        
                        if (startInteract || continueInteract) {
                            if (startInteract) {
                                state.consumeClick();
                                state.drag.activeControlId = eqParamId;
                            }
                            isInteracting = true;
                            float mouseY = state.getMousePosition().y;
                            float rawNorm = 1.0f - ((mouseY - sliderRect.y) / sliderRect.height);
                            if (rawNorm < 0.0f) rawNorm = 0.0f; if (rawNorm > 1.0f) rawNorm = 1.0f;
                            float newVal = param.min + (rawNorm * range);
                            // Snap to 0
                            if (std::abs(newVal) < 0.5f) newVal = 0.0f;
                            effect->setParam(pIndex, newVal);
                        }
                    }
                    currentY += 80;
                }
                
            } else if (targetEffect->getType() == fx::FX_FILTER) {
                // --- FILTER EDITOR (Custom UI) ---
                
                // 1. Cutoff (Param 0) & Reso (Param 1) - Draw as Sliders
                for (int i=0; i<2; ++i) {
                     fx::TrackFXParam& param = targetEffect->getParam(i);
                     DrawTextApp(param.name.c_str(), contentRect.x + 20, currentY, 16, GRAY);
                     
                     // Slider
                     Rectangle sliderRect = {contentRect.x + 20, currentY + 15, contentRect.width - 40, 30};
                     DrawRectangleRec(sliderRect, Color{10, 10, 10, 255});
                     DrawRectangleLinesEx(sliderRect, 1, DARKGRAY);
                     
                     float range = param.max - param.min;
                     // Use log scale for cutoff? For now linear to match backend expectations unless we change mapping
                     float norm = (param.value - param.min) / range;
                     
                     float handleX = sliderRect.x + (norm * sliderRect.width);
                     DrawRectangle(handleX - 5, sliderRect.y, 10, sliderRect.height, ORANGE);
                     
                     // Text
                     char buf[32];
                     if (i==0) sprintf(buf, "%.0f%s", param.value, param.suffix.c_str());
                     else sprintf(buf, "%.1f", param.value);
                     DrawTextApp(buf, sliderRect.x + sliderRect.width - MeasureTextApp(buf, 14) - 5, currentY, 14, WHITE);
                     
                     // Interaction
                     std::string paramId = col.trackName + "_Filt_P" + std::to_string(i);
                     Rectangle hitTest = {sliderRect.x - 10, sliderRect.y - 10, sliderRect.width + 20, sliderRect.height + 20};
                     
                     bool isLockedToOther = (!state.drag.activeControlId.empty() && state.drag.activeControlId != paramId);
                     bool startInteract = (!isLockedToOther && !col.isDragging && CheckCollisionPointRec(state.getMousePosition(), hitTest) && CheckCollisionPointRec(state.getMousePosition(), scrollArea) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && state.isClickAvailable());
                     bool continueInteract = (state.drag.activeControlId == paramId && IsMouseButtonDown(MOUSE_LEFT_BUTTON));
                     
                     if (startInteract || continueInteract) {
                         if (startInteract) {
                             state.consumeClick();
                             state.drag.activeControlId = paramId;
                         }
                         isInteracting = true;
                         float mouseX = state.getMousePosition().x;
                         float rawNorm = (mouseX - sliderRect.x) / sliderRect.width;
                         if (rawNorm < 0.0f) rawNorm = 0.0f; if (rawNorm > 1.0f) rawNorm = 1.0f;
                         float newVal = param.min + (rawNorm * range);
                         targetEffect->setParam(i, newVal);
                     }
                     currentY += 60;
                }
                
                // 2. Filter Type (Param 2) - CYCLIC SELECTOR (StepFX Style)
                fx::TrackFXParam& typeParam = targetEffect->getParam(2);
                DrawTextApp("Type:", contentRect.x + 20, currentY, 16, GRAY);
                currentY += 25;
                
                const char* types[] = {"Low Pass", "High Pass", "Band Pass"};
                int currentType = (int)typeParam.value;
                if (currentType < 0) currentType = 0; if (currentType > 2) currentType = 2;
                
                Rectangle selectorRect = {contentRect.x + 20, currentY, 200.0f, 35.0f};
                
                // Draw Black Box (StepFX Style)
                DrawRectangleRec(selectorRect, BLACK);
                DrawRectangleLinesEx(selectorRect, 2, WHITE);
                
                const char* typeText = types[currentType];
                DrawTextApp(typeText, selectorRect.x + 20, selectorRect.y + 10, 16, WHITE);
                
                // Draw Arrow
                DrawTextApp("v", selectorRect.x + selectorRect.width - 20, selectorRect.y + 10, 12, LIGHTGRAY);
                
                if (canInteract && state.isClickAvailable() && CheckCollisionPointRec(state.getMousePosition(), selectorRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    state.consumeClick();
                    // Cycle: 0 -> 1 -> 2 -> 0
                    int nextType = (currentType + 1) % 3;
                    targetEffect->setParam(2, (float)nextType);
                    isInteracting = true;
                }
                
                currentY += 60;

            } else {
                // Generic Editor
                if (DrawGenericFXEditor(contentRect, scrollArea, targetEffect, col, state)) {
                    isInteracting = true;
                }
                currentY += (targetEffect->getNumParams() * 60); 
            }
            
            currentY += 50; // Padding before remove parameters 
            
            // Remove Button + Toggle (Side by Side)
            float totalW = 170.0f; // 80 + 10 + 80
            float startX = bounds.x + (bounds.width - totalW) / 2.0f;
            
            Rectangle removeBtn = {startX, currentY, 80, 30};
            DrawRectangleRec(removeBtn, RED);
            DrawTextApp("Remove", removeBtn.x + 10, removeBtn.y + 8, 10, WHITE);
            if (canInteract && state.isClickAvailable() && CheckCollisionPointRec(state.getMousePosition(), removeBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                 state.consumeClick();
                 state.drag.activeControlId = "FX_REMOVE"; // Prevent click-through
                 std::lock_guard<std::mutex> lock(trackBus->effectsMutex);
                 trackBus->effects[fxIndex] = nullptr;
                 isInteracting = true; // Button click counts as interaction
            }
            
            // Toggle Switch
            if (targetEffect) {
                float switchW = 80.0f;
                float switchH = 30.0f;
                Rectangle switchRect = {startX + 90, currentY, switchW, switchH};
                Rectangle offRect = {switchRect.x, switchRect.y, switchW/2, switchH};
                Rectangle onRect = {switchRect.x + switchW/2, switchRect.y, switchW/2, switchH};
                
                bool isActive = targetEffect->isActive();
                if (!isActive) {
                    DrawRectangleRec(offRect, WHITE);
                    DrawTextApp("OFF", offRect.x + 5, offRect.y + 8, 10, BLACK);
                    DrawRectangleRec(onRect, GRAY);
                    DrawTextApp("ON", onRect.x + 10, onRect.y + 8, 10, WHITE);
                } else {
                    DrawRectangleRec(offRect, GRAY);
                    DrawTextApp("OFF", offRect.x + 5, offRect.y + 8, 10, WHITE);
                    DrawRectangleRec(onRect, WHITE);
                    DrawTextApp("ON", onRect.x + 10, onRect.y + 8, 10, BLACK);
                }
                
                // Toggle Interaction
                if (canInteract && state.isClickAvailable()) {
                    if (CheckCollisionPointRec(state.getMousePosition(), offRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        state.consumeClick();
                        targetEffect->setActive(false);
                        isInteracting = true;
                    }
                    if (CheckCollisionPointRec(state.getMousePosition(), onRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        state.consumeClick();
                        targetEffect->setActive(true);
                        isInteracting = true;
                    }
                }
            }
            currentY += 50;
            
        } else {
            // Empty Slot -> Vertical Scrolling Effect List
            DrawTextApp("Add Effect:", bounds.x + 20, currentY, 20, GRAY);
            currentY += 35;
            
            // Effect button list with vertical scroll (uses existing mixerScrollY)
            float btnH = 45.0f;
            float btnGap = 5.0f;
            int fontSize = 18;
            
            // Define all effects
            struct FXButton { const char* label; fx::FXType type; };
            FXButton effects[] = {
                {"Delay", fx::FX_DELAY},
                {"Reverb", fx::FX_REVERB},
                {"Compressor", fx::FX_COMPRESSOR},
                {"EQ", fx::FX_EQ},
                {"Saturation", fx::FX_SATURATION},
                {"Overdrive", fx::FX_OVERDRIVE},
                {"Chorus", fx::FX_CHORUS},
                {"Flanger", fx::FX_FLANGER},
                {"Bitcrush", fx::FX_BITCRUSH},
                {"Filter", fx::FX_FILTER}
            };
            int numEffects = 10;
            
            // Draw buttons vertically (scrolling handled by mixerScrollY via parent)
            for (int i = 0; i < numEffects; ++i) {
                Rectangle btnRect = {bounds.x + 10, currentY, bounds.width - 20, btnH};
                
                DrawRectangleRec(btnRect, Color{50, 50, 60, 255});
                DrawRectangleLinesEx(btnRect, 1, Color{80, 80, 90, 255});
                
                // Centered text
                int textW = MeasureTextApp(effects[i].label, fontSize);
                DrawTextApp(effects[i].label, btnRect.x + (btnRect.width - textW)/2, btnRect.y + (btnH - fontSize)/2, fontSize, WHITE);
                
                // Click handling (Trigger on RELEASE to allow scrolling)
                // Only trigger if we are NOT dragging OR if duplicate small drag (< 10px) which counts as click
                bool releasedOver = CheckCollisionPointRec(state.getMousePosition(), btnRect) && 
                                   CheckCollisionPointRec(state.getMousePosition(), scrollArea) &&
                                   IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
                
                // Allow "sloppy clicks" where user moved mouse slightly (< 10px)
                float dragDist = std::abs(state.getMousePosition().y - col.dragStartY);
                bool isSmallDrag = (col.isDragging && dragDist < 10.0f);
                                   
                if (canInteract && releasedOver && (!col.isDragging || isSmallDrag) && !state.drag.isDragging && state.drag.activeControlId.empty()) {
                    // state.consumeClick();
                    std::lock_guard<std::mutex> lock(trackBus->effectsMutex);
                    trackBus->effects[fxIndex] = fx::CreateTrackEffect(effects[i].type);
                    isInteracting = true;
                }
                
                currentY += btnH + btnGap;
            }
        }
    }
    
    col.mixerContentHeight = (currentY - contentStartY) + 20;
    
    EndScissorMode();
    
    // Scroll handling (simplified, controls handle their own lock)
    if (!isInteracting) {
        HandleMixerScroll(scrollArea, col, state);
    } else {
        col.isDragging = false; // Force stop scrolling if we started interacting
    }
    
    // Unlock on Release - only if the interaction was in THIS mixer panel
    // Check if activeControlId belongs to this track (starts with track name)
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        bool isMyControl = state.drag.activeControlId.rfind(col.trackName, 0) == 0;
        bool isFxRemove = (state.drag.activeControlId == "FX_REMOVE");
        bool mouseInBounds = CheckCollisionPointRec(state.getMousePosition(), bounds);
        if (isMyControl || isFxRemove || (mouseInBounds && state.drag.activeControlId.empty())) {
            state.drag.activeControlId = "";
        }
    }
}

} // namespace gui
