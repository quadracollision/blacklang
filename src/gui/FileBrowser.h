#pragma once
#include "../GuiState.h"
#include "../AudioEngine.h"

namespace FileBrowser {
    void Open(GuiState& state, PatternEditorState::BrowserMode mode);
    bool Draw(GuiState& state, AudioEngine& engine);
    void Refresh(GuiState& state);
    void NavigateTo(GuiState& state, const std::string& path);
}
