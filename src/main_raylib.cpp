#include "GuiRenderer.h"
#include "GuiState.h"
#include "AudioEngine.h"
#include <iostream>
#include <algorithm>

// Design resolution - UI is authored at this size
static const int DESIGN_WIDTH = 960;
static const int DESIGN_HEIGHT = 540;

int main() {
    // Init Raylib
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    
#if defined(__ANDROID__)
    // On Android, use device screen size
    InitWindow(0, 0, "QC-33");
#else
    // On desktop, use default window size
    InitWindow(1000, 700, "QC-33");
#endif
    
    SetTargetFPS(60);

    // Create render texture at design resolution for UI scaling
    RenderTexture2D target = LoadRenderTexture(DESIGN_WIDTH, DESIGN_HEIGHT);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR); // Smooth scaling

    // Init Audio Asynchronously to prevent blocking startup on device timeouts
    AudioEngine audio;
    audio.initializeAsync();
    
    // Init State
    GuiState state;
    state.initDemo(); // Pre-populate some columns
    
    // Init Renderer
    GuiRenderer renderer(state, audio);
    
    // Main Loop
    while (!WindowShouldClose()) {
        // Calculate scaling to fit screen while maintaining aspect ratio
        float screenW = (float)GetScreenWidth();
        float screenH = (float)GetScreenHeight();
        float scaleX = screenW / DESIGN_WIDTH;
        float scaleY = screenH / DESIGN_HEIGHT;
        
        // Use min to fit entire UI on screen (may show letterbox bars)
        // This prevents cropping of transport bar, header, etc.
        float scale = std::min(scaleX, scaleY);
        
        float scaledW = DESIGN_WIDTH * scale;
        float scaledH = DESIGN_HEIGHT * scale;
        float offsetX = (screenW - scaledW) / 2.0f;
        float offsetY = (screenH - scaledH) / 2.0f;
        
        // Update with virtual mouse position
        Vector2 realMouse = GetMousePosition();
        Vector2 virtualMouse = {
            (realMouse.x - offsetX) / scale,
            (realMouse.y - offsetY) / scale
        };
        
        // Store virtual screen dimensions and mouse position in state for GUI code
        state.virtualWidth = DESIGN_WIDTH;
        state.virtualHeight = DESIGN_HEIGHT;
        state.virtualMouse = virtualMouse;
        state.uiScale = scale;
        
        renderer.Update();
        
        // Draw to render texture at design resolution
        BeginTextureMode(target);
            renderer.Draw();
        EndTextureMode();
        
        // Draw scaled render texture to screen
        BeginDrawing();
            ClearBackground(BLACK);
            
            // Source rectangle (flip Y because render textures are upside-down)
            Rectangle sourceRec = { 0, 0, (float)DESIGN_WIDTH, -(float)DESIGN_HEIGHT };
            
            // Destination rectangle (scaled and centered)
            Rectangle destRec = { offsetX, offsetY, scaledW, scaledH };
            
            DrawTexturePro(target.texture, sourceRec, destRec, {0, 0}, 0, WHITE);
        EndDrawing();
    }
    
    UnloadRenderTexture(target);
    audio.shutdown();
    CloseWindow();
    
    return 0;
}
