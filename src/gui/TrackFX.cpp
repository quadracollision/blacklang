#include "TrackFX.h"
#include "../AudioEngine.h"
#include <cstdio>
#include <string>

namespace gui {

// Helper to get touch-friendly scroll
static void HandleMixerScroll(const Rectangle& bounds, PatternColumn& col, GuiState& state) {
    Vector2 mouse = state.getMousePosition();
    
    // Check for scroll start
    if (CheckCollisionPointRec(mouse, bounds)) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            // Only start scrolling if we aren't clicking a control (checks passed from calling function would be better, 
            // but for now we'll assume background clicks are scrolls)
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

void DrawTrackMixer(const Rectangle& bounds, PatternColumn& col, GuiState& state, AudioEngine& engine) {
    bool canInteract = !state.editor.isOpen;
    
    // Get the track bus for this column
    AudioBus* trackBus = engine.getTrackBus(col.trackName);
    if (!trackBus) return;  // No bus assigned yet

    // 1. Background
    DrawRectangleRec(bounds, Color{20, 20, 25, 255});
    
    // 2. Header (Fixed)
    float headerH = 40.0f;
    Rectangle headerRect = {bounds.x, bounds.y, bounds.width, headerH};
    DrawRectangleRec(headerRect, Color{30, 30, 35, 255});
    
    Rectangle backBtn = {bounds.x + 5, bounds.y + 5, 30, 30};
    DrawRectangleRec(backBtn, Color{50, 50, 50, 255});
    DrawText("<", backBtn.x + 10, backBtn.y + 5, 20, WHITE);
    
    if (canInteract && CheckCollisionPointRec(state.getMousePosition(), backBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        col.mixerMode = false;
        col.isDragging = false; // Reset drag state
        return;
    }
    
    DrawText("Channel Strip", bounds.x + 45, bounds.y + 10, 20, LIGHTGRAY);
    DrawLine(bounds.x, bounds.y + headerH, bounds.x + bounds.width, bounds.y + headerH, DARKGRAY);

    // 3. Scrollable Area
    Rectangle scrollArea = {bounds.x, bounds.y + headerH, bounds.width, bounds.height - headerH};
    
    // Handle Scrolling input (before drawing controls so controls can steal focus if needed)
    // Note: In a full system, controls would consume the event. Here we simple check if mouse is on a control
    // inside the drawing loop is tricky. 
    // Simplified strategy: Handle scroll, but let controls override if they are clicked.
    // Actually, we'll run scroll logic first, but controls check IsMouseButtonPressed which is only true on first frame.
    // If we are dragging a slider, we shouldn't be scrolling.
    
    // Perform clipping
    BeginScissorMode((int)scrollArea.x, (int)scrollArea.y, (int)scrollArea.width, (int)scrollArea.height);
    
    float contentStartY = scrollArea.y + 20 - col.mixerScrollY; // Top of scrolling content
    float currentY = contentStartY;
    
    // --- PAN (Top) ---
    Rectangle panRect = {bounds.x + 20, currentY + 15, bounds.width - 40, 30};
    bool interactingWithPan = false;
    
    DrawText("PAN", panRect.x, panRect.y - 15, 10, GRAY);
    DrawRectangleRec(panRect, Color{10, 10, 10, 255}); // Track
    float panHandleX = panRect.x + (trackBus->pan * panRect.width);
    DrawRectangle(panHandleX - 5, panRect.y, 10, panRect.height, ORANGE); // Handle
    
    Rectangle hitTestPan = {panRect.x, panRect.y - 10, panRect.width, panRect.height + 20}; // Larger hit area
    if (canInteract && !col.isDragging && CheckCollisionPointRec(state.getMousePosition(), hitTestPan) && CheckCollisionRecs(hitTestPan, scrollArea)) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
             interactingWithPan = true;
             float mouseX = state.getMousePosition().x;
             float newPan = (mouseX - panRect.x) / panRect.width;
             if (newPan < 0.0f) newPan = 0.0f;
             if (newPan > 1.0f) newPan = 1.0f;
             col.pan = newPan;
             trackBus->pan = newPan;
        }
        // Double click reset
         if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
             static double lastPanClick = 0;
             double now = GetTime();
             if (now - lastPanClick < 0.3) {
                 col.pan = 0.5f;
                 trackBus->pan = 0.5f;
             }
             lastPanClick = now;
         }
    }
    
    currentY += 80;
    
    // --- VOLUME (Vertical) ---
    float faderHeight = 250.0f; 
    Rectangle volRect = {bounds.x + 40, currentY + 20, bounds.width - 80, faderHeight};
    bool interactingWithVol = false;
    
    DrawText("VOL", volRect.x, volRect.y - 15, 10, GRAY);
    DrawRectangleRec(volRect, Color{10, 10, 10, 255}); // Track
    
    float fillHeight = trackBus->volume * volRect.height;
    Rectangle fillRect = {volRect.x, volRect.y + volRect.height - fillHeight, volRect.width, fillHeight};
    
    Color volColor = GREEN;
    if (trackBus->volume > 0.8f) volColor = ORANGE;
    if (trackBus->volume > 0.95f) volColor = RED;
    
    DrawRectangleRec(fillRect, volColor);
    DrawRectangleLinesEx(volRect, 1, DARKGRAY);
    DrawRectangle(volRect.x - 5, fillRect.y, volRect.width + 10, 10, WHITE); // Handle
    
    // Value text
    char buf[32];
    sprintf(buf, "%d%%", (int)(trackBus->volume * 100));
    DrawText(buf, volRect.x + 10, volRect.y + volRect.height + 10, 20, Color{255,255,255,100});

    Rectangle hitTestVol = {volRect.x - 15, volRect.y, volRect.width + 30, volRect.height};
    if (canInteract && !col.isDragging && CheckCollisionPointRec(state.getMousePosition(), hitTestVol) && CheckCollisionRecs(hitTestVol, scrollArea)) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            interactingWithVol = true;
            float mouseY = state.getMousePosition().y;
            float bottomY = volRect.y + volRect.height;
            float rawVal = (bottomY - mouseY) / volRect.height;
            if (rawVal < 0.0f) rawVal = 0.0f;
            if (rawVal > 1.0f) rawVal = 1.0f;
            col.volume = rawVal;
            trackBus->volume = rawVal;
        }
    }
    
    currentY += faderHeight + 60;
    
    // --- FX INSERTS ---
    DrawText("FX INSERTS", bounds.x + 20, currentY, 10, DARKGRAY);
    currentY += 20;
    
    for (int i = 0; i < 4; ++i) { // Added one more slot
        Rectangle slotRect = {bounds.x + 10, currentY, bounds.width - 20, 35};
        DrawRectangleRec(slotRect, Color{15, 15, 20, 255});
        DrawRectangleLinesEx(slotRect, 1, Color{40, 40, 40, 255});
        DrawText("Empty", slotRect.x + 10, slotRect.y + 10, 12, Color{60, 60, 60, 255});
        currentY += 40;
    }
    
    col.mixerContentHeight = (currentY - contentStartY) + 50; // Total height + padding
    
    EndScissorMode();
    
    // Scroll handling (only if not using a control)
    if (canInteract && !interactingWithPan && !interactingWithVol) {
        // If we were dragging a control, don't start scrolling immediately
        // Just standard drag scrolling
        HandleMixerScroll(scrollArea, col, state);
    } else {
        col.isDragging = false; // Stop scroll drag if using control
    }
}

} // namespace gui
