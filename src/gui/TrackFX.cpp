#include "TrackFX.h"
#include "../AudioEngine.h"
#include <cstdio>
#include <string>

namespace gui {

void DrawTrackMixer(const Rectangle& bounds, PatternColumn& col, GuiState& state, AudioEngine& engine) {
    bool canInteract = !state.editor.isOpen;
    
    // Get the track bus for this column
    AudioBus* trackBus = engine.getTrackBus(col.trackName);
    if (!trackBus) return;  // No bus assigned yet

    // 1. Background
    DrawRectangleRec(bounds, Color{20, 20, 25, 255});
    
    // 2. Header / Back Button
    Rectangle backBtn = {bounds.x + 5, bounds.y + 5, 30, 30};
    DrawRectangleRec(backBtn, Color{40, 40, 40, 255});
    DrawText("<", backBtn.x + 10, backBtn.y + 5, 20, WHITE);
    
    if (canInteract && CheckCollisionPointRec(GetMousePosition(), backBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        col.mixerMode = false;
        return;
    }
    
    DrawText("Channel Strip", bounds.x + 45, bounds.y + 10, 20, LIGHTGRAY);
    
    // 3. Pan Control (Top)
    float startY = bounds.y + 50;
    Rectangle panRect = {bounds.x + 20, startY, bounds.width - 40, 30};
    
    // Pan Label
    DrawText("PAN", panRect.x, panRect.y - 15, 10, GRAY);
    
    // Pan Slider Logic
    DrawRectangleRec(panRect, Color{10, 10, 10, 255}); // Track
    float panHandleX = panRect.x + (trackBus->pan * panRect.width);
    DrawRectangle(panHandleX - 5, panRect.y, 10, panRect.height, ORANGE); // Handle
    
    // Pan Interaction
    if (canInteract && CheckCollisionPointRec(GetMousePosition(), panRect)) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
             float mouseX = GetMousePosition().x;
             float newPan = (mouseX - panRect.x) / panRect.width;
             if (newPan < 0.0f) newPan = 0.0f;
             if (newPan > 1.0f) newPan = 1.0f;
             
             // Update both column state and bus
             col.pan = newPan;
             trackBus->pan = newPan;
        }
    }
    
    // Reset Pan on Double Click
    if (canInteract && CheckCollisionPointRec(GetMousePosition(), panRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
         static double lastPanClick = 0;
         double now = GetTime();
         if (now - lastPanClick < 0.3) {
             col.pan = 0.5f;
             trackBus->pan = 0.5f;
         }
         lastPanClick = now;
    }

    // 4. Volume Fader (Vertical, Fixed Height)
    float volY = startY + 50;
    float faderHeight = 180.0f; // Fixed height, not filling screen
    Rectangle volRect = {
        bounds.x + 40, 
        volY, 
        bounds.width - 80, 
        faderHeight
    };
    
    // Volume Label
    DrawText("VOL", volRect.x, volRect.y - 15, 10, GRAY);
    
    // Fader Track
    DrawRectangleRec(volRect, Color{10, 10, 10, 255});
    
    // Fader Fill logic
    float fillHeight = trackBus->volume * volRect.height;
    Rectangle fillRect = {
        volRect.x,
        volRect.y + volRect.height - fillHeight,
        volRect.width,
        fillHeight
    };
    
    // Gradient Color
    Color volColor = GREEN;
    if (trackBus->volume > 0.8f) volColor = ORANGE;
    if (trackBus->volume > 0.95f) volColor = RED;
    
    DrawRectangleRec(fillRect, volColor);
    DrawRectangleLinesEx(volRect, 1, DARKGRAY);
    
    // Fader Handle
    DrawRectangle(volRect.x - 5, fillRect.y, volRect.width + 10, 10, WHITE);
    
    // Interaction
    Rectangle hitTestVol = {volRect.x - 10, volRect.y, volRect.width + 20, volRect.height};
    if (canInteract && CheckCollisionPointRec(GetMousePosition(), hitTestVol)) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float mouseY = GetMousePosition().y;
            float bottomY = volRect.y + volRect.height;
            float rawVal = (bottomY - mouseY) / volRect.height;
            
            if (rawVal < 0.0f) rawVal = 0.0f;
            if (rawVal > 1.0f) rawVal = 1.0f;
            
            // Update both column state and bus
            col.volume = rawVal;
            trackBus->volume = rawVal;
        }
    }
    
    // Display Values
    char buf[32];
    sprintf(buf, "%d%%", (int)(trackBus->volume * 100));
    DrawText(buf, volRect.x + 10, volRect.y + volRect.height + 5, 20, Color{255,255,255,100});
    
    // 5. FX Slots (Placeholders for future)
    float fxY = volY + faderHeight + 40;
    DrawText("FX INSERTS", bounds.x + 20, fxY - 20, 10, DARKGRAY);
    
    for (int i = 0; i < 3; ++i) {
        Rectangle slotRect = {bounds.x + 10, fxY + (i * 35), bounds.width - 20, 30};
        DrawRectangleRec(slotRect, Color{15, 15, 20, 255});
        DrawRectangleLinesEx(slotRect, 1, Color{40, 40, 40, 255});
        DrawText("Empty", slotRect.x + 10, slotRect.y + 8, 12, Color{60, 60, 60, 255});
    }
    
}

} // namespace gui
