#include "GuiRenderer.h"
#include "GuiState.h"
#include "AudioEngine.h"
#include <iostream>

int main() {
    // Init Raylib
    const int screenWidth = 1000;
    const int screenHeight = 700;
    
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "Quadracollision BlackLang");
    SetTargetFPS(60);

    // Init Audio AFTER Raylib to avoid device conflict
    AudioEngine audio;
    if (!audio.initialize()) {
        std::cerr << "Failed to init audio" << std::endl;
        return 1;
    }
    
    // Init State
    GuiState state;
    state.initDemo(); // Pre-populate some columns
    
    // Init Renderer
    GuiRenderer renderer(state, audio);
    
    // Main Loop
    while (!WindowShouldClose()) {
        // Update
        renderer.Update();
        
        // Draw
        BeginDrawing();
            renderer.Draw();
        EndDrawing();
    }
    
    audio.shutdown();
    CloseWindow();
    
    return 0;
}
