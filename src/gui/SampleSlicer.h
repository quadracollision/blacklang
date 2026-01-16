#pragma once

#include <raylib.h>

// Forward declarations
class AudioEngine;
struct GuiState;
struct Pattern;

namespace gui {

/**
 * SampleSlicer - Self-contained component for waveform visualization and slice editing.
 * 
 * Usage: Call Draw() when showSlicerControls is true in PatternEditor.
 * Returns the height used by the controls for layout purposes.
 */
class SampleSlicer {
public:
    SampleSlicer(GuiState& state, AudioEngine& engine);
    
    /**
     * Draw the slicer controls.
     * @param area Available drawing area
     * @param pattern Pattern to edit
     * @param inputBlocked Whether input should be blocked (e.g., modal open)
     * @return Height used by the controls
     */
    float Draw(Rectangle area, Pattern& pattern, bool inputBlocked);
    
private:
    GuiState& state;
    AudioEngine& engine;
    
    // Internal drawing helpers
    void DrawWaveform(Rectangle waveRect, Pattern& pattern, int startSample, int endSample);
    void DrawSliceMarkers(Rectangle waveRect, Pattern& pattern, int startSample, int endSample, bool inputBlocked, bool isInViewport);
    void DrawScrollbar(Rectangle waveRect, float zoom, bool inputBlocked, bool isInViewport);
    void DrawControlButtons(Rectangle area, Pattern& pattern, bool inputBlocked, bool isInViewport, float& startY);
    
    // Interaction handlers
    void HandleWaveformClick(Rectangle waveRect, Pattern& pattern, int startSample, int visibleSamples, bool inputBlocked, bool isInViewport);
    void HandlePlayModeClick(Rectangle waveRect, Pattern& pattern, bool inputBlocked, bool isInViewport);
    
    // Scrollbar state (moved from static locals)
    bool isDraggingScroll = false;
    float dragStartX = 0;
    float dragStartScroll = 0;
};

} // namespace gui
