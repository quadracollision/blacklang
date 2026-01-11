#pragma once
#include "../GuiState.h"
#include "../AudioEngine.h"

namespace FileBrowser {
    void Init(GuiState& state);
    bool Draw(GuiState& state, AudioEngine& engine);
    void Refresh(GuiState& state);
    void NavigateTo(GuiState& state, const std::string& path);
}
