#pragma once

#include <raylib.h>
#include <string>

// Forward declarations
class AudioEngine;
struct GuiState;
struct Pattern;

namespace gui {

// Draw a pattern box widget (used in track columns)
void DrawPatternBox(const std::string& name, Rectangle bounds, bool selected, int progress = -1);

// Draw a step sequencer grid
void DrawStepGrid(Rectangle bounds, const Pattern& pattern, int activeStep, GuiState& state);

} // namespace gui
