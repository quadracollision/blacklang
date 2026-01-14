#pragma once
#include <raylib.h>
#include "FontManager.h"

// Wrapper to draw text using FontManager
inline void DrawTextApp(const char* text, int x, int y, int fontSize, Color color) {
    FontManager::Get().Draw(text, x, y, fontSize, color);
}

inline int MeasureTextApp(const char* text, int fontSize) {
    return FontManager::Get().Measure(text, fontSize);
}
