#pragma once

#include "Arrangement.h"
#include "../GuiState.h"
#include "../AudioEngine.h"
#include "../ArrangementPlayer.h"
#include <raylib.h>

namespace gui {

// Snap values for clip placement
enum SnapValue {
    SNAP_FREE = 0,
    SNAP_1_1 = 16,   // 1 bar = 16 beats (4/4 time, 4 beats per bar, 4 bars)
    SNAP_1_2 = 8,    // Half bar
    SNAP_1_4 = 4,    // Quarter (1 beat)
    SNAP_1_8 = 2,    // Eighth
    SNAP_1_16 = 1,   // Sixteenth
    SNAP_1_32 = 0,   // Thirty-second (use 0.5 in code)
    SNAP_1_64 = 0    // Sixty-fourth (use 0.25 in code)
};

class ArrangementView {
public:
    ArrangementView(GuiState& s, AudioEngine& e);
    
    void Draw(Rectangle bounds);
    
    // Convert between screen and timeline coordinates
    float beatToPixel(double beat, float startX);
    double pixelToBeat(float px, float startX);
    
    // Snap helper
    double snapBeat(double beat);
    
    // Access to player for external control
    ArrangementPlayer& getPlayer() { return player; }
    
private:
    GuiState& state;
    AudioEngine& engine;
    ArrangementPlayer player;
    
    // View State
    float scrollX = 0.0f;
    float scrollY = 0.0f;
    float pixelsPerBeat = 20.0f; // Zoom level
    
    // Drag-to-scroll state
    bool timelineDragActive = false;
    Vector2 timelineDragStart = {0, 0};
    float timelineScrollStartX = 0.0f;
    float timelineScrollStartY = 0.0f;
    
    // Snap setting
    int snapIndex = 4; // Default: 1/4 (index into snap options array)
    
    // Clip Selection
    int selectedTrackIdx = -1;
    int selectedClipIdx = -1;
    
    // Clip Drag
    bool clipDragActive = false;
    bool clipDragThresholdMet = false; // Only move clip after threshold
    Vector2 clipDragStartPos = {0, 0};
    double clipDragStartBeat = 0.0;
    double clipOriginalBeat = 0.0;
    
    // Pinch Zoom
    bool pinchActive = false;
    float lastPinchDist = 0.0f;
    
    void DrawGrid(Rectangle bounds);
    void DrawTracks(Rectangle bounds);
    void DrawTransportBar(Rectangle bounds);
    void DrawPlayhead(Rectangle bounds);
    void HandleInput(Rectangle bounds);
};

} // namespace gui

