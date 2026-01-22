#include "GuiRenderer.h"
#include "GuiState.h"
#include "AudioEngine.h"
#include "platform/CrashLogger.h"
#include <iostream>
#include <algorithm>


// Design resolution - UI is authored at this size
static const int DESIGN_WIDTH = 960;
static const int DESIGN_HEIGHT = 540;

int main() {
    // Initialize crash logger FIRST, before anything else
    crash::initCrashLogger();
    crash::logMessage("main() started");
    
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

    // Init Audio AFTER Raylib to avoid device conflict
    AudioEngine audio;
    if (!audio.initialize()) {
        std::cerr << "Failed to init audio" << std::endl;
        UnloadRenderTexture(target);
        return 1;
    }
    
    // Init State
    GuiState state;
    state.initDemo(); // Pre-populate some columns
    
    // Init Renderer
    GuiRenderer renderer(state, audio);
    
    // Track orientation for render texture recreation
    bool lastPortrait = false;
    
    // Main Loop
    while (!WindowShouldClose()) {
        // Detect orientation
        float screenW = (float)GetScreenWidth();
        float screenH = (float)GetScreenHeight();
        bool isPortrait = screenH > screenW;
        
        // Recreate render texture if orientation changed
        if (isPortrait != lastPortrait) {
            UnloadRenderTexture(target);
            if (isPortrait) {
                target = LoadRenderTexture(DESIGN_HEIGHT, DESIGN_WIDTH);  // 540x960 for portrait
            } else {
                target = LoadRenderTexture(DESIGN_WIDTH, DESIGN_HEIGHT);  // 960x540 for landscape
            }
            SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
            lastPortrait = isPortrait;
        }
        
        // Get current virtual dimensions
        int virtualW = isPortrait ? DESIGN_HEIGHT : DESIGN_WIDTH;
        int virtualH = isPortrait ? DESIGN_WIDTH : DESIGN_HEIGHT;
        
        // Calculate scaling to fit screen while maintaining aspect ratio
        float scaleX = screenW / virtualW;
        float scaleY = screenH / virtualH;
        
        // Use min to fit entire UI on screen (may show letterbox bars)
        // This prevents cropping of transport bar, header, etc.
        float scale = std::min(scaleX, scaleY);
        
        float scaledW = virtualW * scale;
        float scaledH = virtualH * scale;
        float offsetX = (screenW - scaledW) / 2.0f;
        float offsetY = (screenH - scaledH) / 2.0f;
        
        // Update with virtual mouse position
        Vector2 realMouse = GetMousePosition();
        Vector2 virtualMouse = {
            (realMouse.x - offsetX) / scale,
            (realMouse.y - offsetY) / scale
        };
        
        // Store virtual screen dimensions and mouse position in state for GUI code
        state.virtualWidth = virtualW;
        state.virtualHeight = virtualH;
        state.virtualMouse = virtualMouse;
        state.uiScale = scale;
        state.isPortrait = isPortrait;
        
        renderer.Update();
        
        // Draw to render texture at design resolution
        BeginTextureMode(target);
            renderer.Draw();
        EndTextureMode();
        
        // Draw scaled render texture to screen
        BeginDrawing();
            ClearBackground(BLACK);
            
            // Source rectangle (flip Y because render textures are upside-down)
            Rectangle sourceRec = { 0, 0, (float)virtualW, -(float)virtualH };
            
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
