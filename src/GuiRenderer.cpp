#include "GuiRenderer.h"
#include "gui/Widgets.h"
#include "gui/DragDrop.h"
#include "gui/ProjectBrowser.h"
#include "gui/RecordingUI.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <raymath.h>

namespace fs = std::filesystem;

GuiRenderer::GuiRenderer(GuiState& s, AudioEngine& e) 
    : state(s), engine(e), transportBar(s, e), trackView(s, e), patternEditor(s, e), recordingUI(s, e) {
    font = GetFontDefault();
}
#include "tinyfiledialogs.h"

void GuiRenderer::Update() {
    // Use modular drag and drop handler - DEPRECATED, handled in TrackView
    // gui::HandleDragAndDrop(state);
    
    // Sync playback state
    state.isPlaying = engine.isPlaying();
}

void GuiRenderer::Draw() {
    // Reset click consumption for this frame
    state.resetClickState();
    
    // =============================
    // EARLY INPUT PASS: Consume clicks for overlays BEFORE main view processes input
    // This prevents clicks from passing through to elements behind overlays
    // =============================
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Recording overlays (highest priority)
        if (state.recorder.showModeSelection || state.recorder.showReview) {
            state.consumeClick();
        }
        // Project browser
        else if (state.projectBrowser.isOpen) {
            state.consumeClick();
        }
        // Pattern editor
        else if (state.editor.isOpen) {
            state.consumeClick();
        }
        // Settings popup
        else if (state.settings.activePopup != PopupType::None) {
            state.consumeClick();
        }
    }
    
    ClearBackground(Color{20, 20, 20, 255});
    
    // Draw Headers
    Rectangle headerRect = {0, 0, (float)state.getScreenWidth(), (float)state.HEADER_HEIGHT};
    DrawRectangleRec(headerRect, Color{30, 30, 30, 255});
    DrawText("QC-33", 20, 15, 30, WHITE);
    
    // BPM Display in header (top right)
    float bpmX = state.getScreenWidth() - 150;
    DrawText("BPM:", bpmX, 15, 20, LIGHTGRAY);
    if (!state.editor.isOpen) {
        DrawTextInput({bpmX + 55, 10, 80, 35}, state.globalBpmBuffer, 5, 999, state.focusedFieldId, state.getMousePosition());
    } else {
        DrawRectangleRec({bpmX + 55, 10, 80, 35}, LIGHTGRAY);
        DrawText(state.globalBpmBuffer, bpmX + 65, 18, 20, BLACK);
    }
    
    // Update BPM if changed
    int newBpm = atoi(state.globalBpmBuffer);
    if (newBpm > 20 && newBpm < 300 && newBpm != state.bpm) {
        state.bpm = newBpm;
        engine.setBPM(newBpm);
    }
    
    // Main View Area
    Rectangle viewRect = {0, (float)state.HEADER_HEIGHT, (float)state.getScreenWidth(), (float)state.getScreenHeight() - state.HEADER_HEIGHT - state.FOOTER_HEIGHT};
    
    // Calculate Content Width
    float startX = 20;
    float contentW = startX + state.columns.size() * (state.COLUMN_WIDTH + 10) + 60; // + Add Button + Margin
    state.mainContentWidth = contentW;
    
    // Scroll Input (Shift + Wheel)
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
        state.mainScrollX -= GetMouseWheelMove() * 30.0f;
    }
    
    // Clamp Scroll
    float maxScroll = std::max(0.0f, state.mainContentWidth - state.getScreenWidth());
    if (state.mainScrollX < 0) state.mainScrollX = 0;
    if (state.mainScrollX > maxScroll) state.mainScrollX = maxScroll;
    
    // Draw Track View (Columns + Global Drag Logic)
    trackView.Draw();
    
    // Draw Horizontal Scrollbar (if needed)
    if (state.mainContentWidth > state.getScreenWidth()) {
        float barH = 10;
        float barY = viewRect.y + viewRect.height - barH - 5;
        
        float viewRatio = state.getScreenWidth() / state.mainContentWidth;
        float thumbW = std::max(30.0f, state.getScreenWidth() * viewRatio);
        float thumbX = (state.mainScrollX / (state.mainContentWidth - state.getScreenWidth())) * (state.getScreenWidth() - thumbW);
        
        // Track
        DrawRectangle(0, barY, state.getScreenWidth(), barH, Color{20, 20, 20, 200});
        // Thumb
        Rectangle thumbRect = {thumbX, barY, thumbW, barH};
        DrawRectangleRec(thumbRect, Color{100, 100, 100, 255});
        
        // Drag Scrollbar Logic
        static bool isDraggingScroll = false;
        static float dragOffsetX = 0;
        
        if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), thumbRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            isDraggingScroll = true;
            dragOffsetX = state.getMousePosition().x - thumbRect.x;
        }
        
        if (isDraggingScroll) {
            if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) isDraggingScroll = false;
            else {
                float targetX = state.getMousePosition().x - dragOffsetX;
                float pct = targetX / (state.getScreenWidth() - thumbW);
                state.mainScrollX = pct * (state.mainContentWidth - state.getScreenWidth());
                if (state.mainScrollX < 0) state.mainScrollX = 0;
                if (state.mainScrollX > maxScroll) state.mainScrollX = maxScroll;
            }
        }
    }
    
    // Use modular transport bar
    transportBar.Draw();
    
    // Draw dragged item using modular DragDrop - DEPRECATED, handled in TrackView
    // gui::DrawDragGhost(state);
    
    // Use modular pattern editor
    if (state.editor.isOpen) {
        patternEditor.Draw();
    }
    
    // Draw project browser (for save/load project)
    if (state.projectBrowser.isOpen) {
        // Initialize on first open
        static bool wasOpen = false;
        if (!wasOpen) {
            ProjectBrowser::Refresh(state);
        }
        wasOpen = state.projectBrowser.isOpen;
        ProjectBrowser::Draw(state, engine);
    }
    
    // Draw RecordingUI (mode selection and review overlays)
    recordingUI.Draw();
}

void GuiRenderer::DrawColumn(int index, PatternColumn& col) {
    trackView.DrawColumn(index, col);
}

// Legacy drawing methods removed - delegated to TrackView and StepGrid modules
/*
void GuiRenderer::DrawPatternBox(const std::string& name, Rectangle bounds, bool selected) {
    // Deprecated
}

void GuiRenderer::DrawStepGrid(Rectangle bounds, const Pattern& pattern, int activeStep) {
    // Deprecated
}
*/

// DrawTransportBar moved to gui/TransportBar.cpp
// The transportBar.Draw() call replaces this 188-line function
