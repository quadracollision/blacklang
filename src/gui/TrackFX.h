#pragma once

#include <raylib.h>
#include "../GuiState.h"

// Forward declaration
class AudioEngine;

namespace gui {

// Draws the Track Mixer / FX view within the given bounds
void DrawTrackMixer(const Rectangle& bounds, PatternColumn& col, GuiState& state, AudioEngine& engine);

} // namespace gui
