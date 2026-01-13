#pragma once

#include "GuiState.h"
#include "AudioEngine.h"
#include "gui/TransportBar.h"
#include "gui/TrackView.h"
#include "gui/DragDrop.h"
#include "gui/PatternEditor.h"
#include "gui/RecordingUI.h"

class GuiRenderer {
public:
    GuiRenderer(GuiState& state, AudioEngine& engine);
    
    void Update();
    void Draw();
    
private:
    GuiState& state;
    AudioEngine& engine;
    
    // Modular GUI components
    gui::TransportBar transportBar;
    gui::TrackView trackView;
    gui::PatternEditor patternEditor;
    gui::RecordingUI recordingUI;
    
    Font font;
    Texture2D patternIcon;
    
    void DrawColumn(int index, PatternColumn& col);
    void DrawPatternBox(const std::string& name, Rectangle bounds, bool selected);
    void DrawTransportBar();
    void DrawPatternEditor();
    
    // Input handling helpers
    void HandleDragAndDrop();
    void HandlePatternClick(const std::string& name, int colIndex);
    
    // Helper to draw step grid
    void DrawStepGrid(Rectangle bounds, const Pattern& pattern, int activeStep = -1);
};
