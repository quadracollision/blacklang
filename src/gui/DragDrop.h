#pragma once

#include <raylib.h>

// Forward declarations
struct GuiState;

namespace gui {

// Handle drag and drop logic for patterns
void HandleDragAndDrop(GuiState& state);

// Draw the dragged pattern ghost if active
void DrawDragGhost(GuiState& state);

} // namespace gui
