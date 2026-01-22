#pragma once

#include <raylib.h>
#include <string>
#include <vector>

// Forward declarations
class AudioEngine;
struct GuiState;

namespace gui {

class TransportBar {
public:
    TransportBar(GuiState& state, AudioEngine& engine);
    void Draw();
    
private:
    GuiState& state;
    AudioEngine& engine;
    
    void DrawPlayStop();
    void DrawAddTrackButton();
    void DrawBPM();
    void DrawCopyPaste();
    void DrawEditShift();
    void DrawSyncButton();  // Sync patterns with beatsync enabled
    void DrawSettingsButton();
    void DrawSettingsPopup();
    void DrawRecording();
};

} // namespace gui
