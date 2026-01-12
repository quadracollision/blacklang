#include "TrackFX.h"
#include "../AudioEngine.h"
#include <cstdio>
#include <string>
#include <cmath>
#include <algorithm> // for std::max

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
    int nameWidth = MeasureText(effect->getName().c_str(), 20);
    DrawText(effect->getName().c_str(), bounds.x + 20, currentY, 20, ORANGE);
    
    // On/Off Toggle (Styled like Recording Menu but larger)
    float switchW = 100.0f;
    float switchH = 30.0f;
    Rectangle switchRect = {bounds.x + 20.0f + nameWidth + 20.0f, currentY - 5, switchW, switchH};
    
    DrawRectangleRec(switchRect, GRAY);
    
    Rectangle offRect = {switchRect.x, switchRect.y, switchW/2, switchH};
    Rectangle onRect = {switchRect.x + switchW/2, switchRect.y, switchW/2, switchH};
    
    bool isActive = effect->isActive();
    
    if (!isActive) {
        // OFF is Selected (White)
        DrawRectangleRec(offRect, WHITE);
        DrawText("OFF", offRect.x + 10, offRect.y + 8, 10, BLACK);
        
        // ON is Unselected (Gray)
        DrawText("ON", onRect.x + 15, onRect.y + 8, 10, WHITE);
    } else {
        // OFF is Unselected (Gray)
        DrawText("OFF", offRect.x + 10, offRect.y + 8, 10, WHITE);
        
        // ON is Selected (White)
        DrawRectangleRec(onRect, WHITE);
        DrawText("ON", onRect.x + 15, onRect.y + 8, 10, BLACK);
    }
    
    // Interaction
    if (state.editor.isOpen == false && !col.isDragging) {
        if (CheckCollisionPointRec(state.getMousePosition(), offRect) && CheckCollisionRecs(offRect, clipRect)) {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) anyInteraction = true;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) effect->setActive(false);
        }
        if (CheckCollisionPointRec(state.getMousePosition(), onRect) && CheckCollisionRecs(onRect, clipRect)) {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) anyInteraction = true;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) effect->setActive(true);
        }
    }

    currentY += 40;
    
    // Params
    for (int i = 0; i < effect->getNumParams(); ++i) {
        fx::TrackFXParam& param = effect->getParam(i);
        
        // Label
        DrawText(param.name.c_str(), bounds.x + 20, currentY, 10, GRAY);
        
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
        DrawText(buf, sliderRect.x + sliderRect.width - MeasureText(buf, 10) - 5, currentY, 10, WHITE);
        
        // Interaction
        Rectangle hitTest = {sliderRect.x - 10, sliderRect.y - 10, sliderRect.width + 20, sliderRect.height + 20};
        bool isInteracting = (!col.isDragging && CheckCollisionPointRec(state.getMousePosition(), hitTest) && CheckCollisionRecs(hitTest, clipRect));
        
        if (state.editor.isOpen == false && isInteracting && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
             anyInteraction = true;
             float mouseX = state.getMousePosition().x;
             float rawNorm = (mouseX - sliderRect.x) / sliderRect.width;
             if (rawNorm < 0.0f) rawNorm = 0.0f;
             if (rawNorm > 1.0f) rawNorm = 1.0f;
             
             float newValue = param.min + (rawNorm * (param.max - param.min));
             effect->setParam(i, newValue);
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
    DrawText("<", backBtn.x + 10, backBtn.y + 5, 20, WHITE);
    
    if (canInteract && CheckCollisionPointRec(state.getMousePosition(), backBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        col.mixerMode = false;
        col.isDragging = false; 
        return;
    }
    
    DrawText("Channel Strip", bounds.x + 45, bounds.y + 10, 20, LIGHTGRAY);
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
            if (fxIndex < trackBus->effects.size() && trackBus->effects[fxIndex]) {
                // Use first 3 chars of name
                static char nameBuf[4]; 
                std::string n = trackBus->effects[fxIndex]->getName();
                snprintf(nameBuf, 4, "%s", n.c_str());
                label = nameBuf;
            } else {
                label = "+";
            }
        }
        
        // Centered Text
        DrawText(label, tabRect.x + (tabWidth/2) - MeasureText(label, 10)/2, tabRect.y + 12, 10, isSelected ? WHITE : GRAY);
        
        if (canInteract && CheckCollisionPointRec(state.getMousePosition(), tabRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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
        DrawText("PAN", panRect.x, panRect.y - 15, 10, GRAY);
        DrawRectangleRec(panRect, Color{10, 10, 10, 255});
        float panHandleX = panRect.x + (trackBus->pan * panRect.width);
        DrawRectangle(panHandleX - 5, panRect.y, 10, panRect.height, ORANGE);
        
        Rectangle hitTestPan = {panRect.x, panRect.y - 10, panRect.width, panRect.height + 20};
        if (canInteract && !col.isDragging && CheckCollisionPointRec(state.getMousePosition(), hitTestPan) && CheckCollisionRecs(hitTestPan, scrollArea)) {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                 isInteracting = true; // Capture interaction
                 float mouseX = state.getMousePosition().x;
                 float newPan = (mouseX - panRect.x) / panRect.width;
                 if (newPan < 0.0f) newPan = 0.0f;
                 if (newPan > 1.0f) newPan = 1.0f;
                 col.pan = newPan;
                 trackBus->pan = newPan;
            }
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { // Double click reset
                 static double lastPanClick = 0;
                 if (GetTime() - lastPanClick < 0.3) { col.pan = 0.5f; trackBus->pan = 0.5f; }
                 lastPanClick = GetTime();
            }
        }
        
        currentY += 80;
        
        // VOLUME
        float faderHeight = 250.0f; 
        Rectangle volRect = {bounds.x + 40, currentY + 20, bounds.width - 80, faderHeight};
        DrawText("VOL", volRect.x, volRect.y - 15, 10, GRAY);
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
        DrawText(buf, volRect.x + 10, volRect.y + volRect.height + 10, 20, Color{255,255,255,100});

        Rectangle hitTestVol = {volRect.x - 15, volRect.y, volRect.width + 30, volRect.height};
        if (canInteract && !col.isDragging && CheckCollisionPointRec(state.getMousePosition(), hitTestVol) && CheckCollisionRecs(hitTestVol, scrollArea)) {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                isInteracting = true; // Capture interaction
                float mouseY = state.getMousePosition().y;
                float rawVal = ((volRect.y + volRect.height) - mouseY) / volRect.height;
                if (rawVal < 0.0f) rawVal = 0.0f; if (rawVal > 1.0f) rawVal = 1.0f;
                col.volume = rawVal;
                trackBus->volume = rawVal;
            }
        }
        currentY += faderHeight + 60;
        
    } else {
        // --- FX EDITOR ---
        int fxIndex = col.selectedFXSlot - 1;
        
        // Ensure vector is big enough (pad with nulls)
        while (trackBus->effects.size() <= fxIndex) {
            trackBus->effects.push_back(nullptr);
        }
        
        if (trackBus->effects[fxIndex]) {
            // Draw Parameters (and get interaction state)
            Rectangle contentRect = {bounds.x, currentY, bounds.width, 0};
            if (DrawGenericFXEditor(contentRect, scrollArea, trackBus->effects[fxIndex], col, state)) {
                isInteracting = true;
            }
            currentY += (trackBus->effects[fxIndex]->getNumParams() * 60) + 50; 
            
            // Remove Button
            Rectangle removeBtn = {bounds.x + bounds.width/2 - 40, currentY, 80, 30};
            DrawRectangleRec(removeBtn, RED);
            DrawText("Remove", removeBtn.x + 10, removeBtn.y + 8, 10, WHITE);
            if (canInteract && CheckCollisionPointRec(state.getMousePosition(), removeBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                 trackBus->effects[fxIndex] = nullptr;
                 isInteracting = true; // Button click counts as interaction
            }
            currentY += 50;
            
        } else {
            // Empty Slot -> Add Buttons
            DrawText("Add Effect:", bounds.x + 20, currentY, 20, GRAY);
            currentY += 40;
            
            // Delay Button
            Rectangle btnDelay = {bounds.x + 20, currentY, 120, 30};
            DrawRectangleRec(btnDelay, DARKGRAY);
            DrawText("+ Delay", btnDelay.x + 20, btnDelay.y + 8, 10, WHITE);
            
            if (canInteract && CheckCollisionPointRec(state.getMousePosition(), btnDelay) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                 trackBus->effects[fxIndex] = fx::CreateTrackEffect(fx::FX_DELAY);
                 isInteracting = true;
            }
            
            // Reverb Button
            Rectangle btnReverb = {bounds.x + 160, currentY, 120, 30};
            DrawRectangleRec(btnReverb, DARKGRAY);
            DrawText("+ Reverb", btnReverb.x + 20, btnReverb.y + 8, 10, WHITE);
            
            if (canInteract && CheckCollisionPointRec(state.getMousePosition(), btnReverb) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                 trackBus->effects[fxIndex] = fx::CreateTrackEffect(fx::FX_REVERB);
                 isInteracting = true;
            }
            
            // Compressor Button
            Rectangle btnComp = {bounds.x + 20, currentY + 40, 120, 30};
            DrawRectangleRec(btnComp, DARKGRAY);
            DrawText("+ Compressor", btnComp.x + 10, btnComp.y + 8, 10, WHITE);
            
            if (canInteract && CheckCollisionPointRec(state.getMousePosition(), btnComp) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                 trackBus->effects[fxIndex] = fx::CreateTrackEffect(fx::FX_COMPRESSOR);
                 isInteracting = true;
            }
            
            currentY += 100;
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
}

} // namespace gui
