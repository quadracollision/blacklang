#pragma once

#include "../GuiState.h"
#include "../AudioEngine.h"

namespace ProjectBrowser {
    void Init(GuiState& state, bool saveMode);
    void Refresh(GuiState& state);
    void NavigateTo(GuiState& state, const std::string& path);
    void GoUp(GuiState& state);
    bool Draw(GuiState& state, AudioEngine& engine); // Returns true if input consumed
}
