#pragma once
#include <raylib.h>
#include <string>

class FontManager {
public:
    static FontManager& Get();

    void Init();
    void Draw(const char* text, int x, int y, int fontSize, Color color);
    int Measure(const char* text, int fontSize);
    
    bool IsFontLoaded() const { return currentFont.texture.id != 0; }

private:
    FontManager() = default;
    Font currentFont = { 0 };
    Font fallbackFont = { 0 }; // Anonymous Pro
};
