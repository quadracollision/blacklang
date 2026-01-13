#pragma once

#include <raylib.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <string>
#include <vector>

// Forward declarations
class AudioEngine;
struct GuiState;

namespace gui {

class RecordingUI {
public:
    RecordingUI(GuiState& state, AudioEngine& engine);
    
    // Main draw function - shows mode selection or review based on state
    void Draw();
    
private:
    GuiState& state;
    AudioEngine& engine;
    
    // Sub-draw functions
    void DrawModeSelection();
    void DrawReviewUI();
    void DrawWaveform(Rectangle area, const juce::AudioBuffer<float>& buffer, int sampleCount, int64_t playhead);
    
    // UI State
    float reviewScrollY = 0.0f;
    int selectedStemIndex = 0; // -1 = master, 0+ = stems
};

} // namespace gui
