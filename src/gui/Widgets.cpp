#include "Widgets.h"
#include "../FilePicker.h"
#include <cstring>

void DrawTextInput(Rectangle rect, char* buffer, size_t maxLen, int fieldId, int& focusedId, Vector2 mousePos) {
    Vector2 m = (mousePos.x < 0) ? GetMousePosition() : mousePos;
    bool wasFocused = (focusedId == fieldId);
    bool isFocused = wasFocused;
    
    if (CheckCollisionPointRec(m, rect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        focusedId = fieldId;
        isFocused = true;
        // Always show keyboard on click to ensure it comes up if dismissed
        FilePicker::showKeyboard();
    }
    
    DrawRectangleRec(rect, isFocused ? WHITE : LIGHTGRAY);
    DrawRectangleLinesEx(rect, 1, isFocused ? BLUE : DARKGRAY);
    DrawText(buffer, rect.x + 5, rect.y + 5, 20, BLACK);
    
    if (isFocused) {
        // Blinking Cursor
        if ((int)(GetTime() * 2) % 2 == 0) {
            int textW = MeasureText(buffer, 20);
            DrawLine(rect.x + 5 + textW + 2, rect.y + 5, rect.x + 5 + textW + 2, rect.y + 25, BLACK);
        }

#if defined(__ANDROID__)
        // Android Proxy Input ONLY
        int aKey, aChar;
        while (FilePicker::AndroidGetInput(aKey, aChar)) {
            if (aKey == 259) { // Backspace
                 size_t len = strlen(buffer);
                 if (len > 0) buffer[len-1] = '\0';
            } else if (aChar >= 32 && aChar <= 125 && strlen(buffer) < maxLen) {
                size_t len = strlen(buffer);
                buffer[len] = (char)aChar;
                buffer[len+1] = '\0';
            }
        }
#else
        // Desktop / Standard Raylib Input
        int key = GetCharPressed();
        while (key > 0) {
            if ((key >= 32) && (key <= 125) && (strlen(buffer) < maxLen)) {
                size_t len = strlen(buffer);
                buffer[len] = (char)key;
                buffer[len+1] = '\0';
            }
            key = GetCharPressed();
        }
        
        if (IsKeyPressed(KEY_BACKSPACE)) {
            size_t len = strlen(buffer);
            if (len > 0) buffer[len-1] = '\0';
        }
#endif
    }
}

bool DrawButton(Rectangle rect, const char* text, Color bgColor, Color textColor, Vector2 mousePos) {
    Vector2 m = (mousePos.x < 0) ? GetMousePosition() : mousePos;
    bool hovered = CheckCollisionPointRec(m, rect);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    
    Color bg = hovered ? ColorBrightness(bgColor, 0.2f) : bgColor;
    DrawRectangleRec(rect, bg);
    DrawRectangleLinesEx(rect, 1, WHITE);
    
    int textWidth = MeasureText(text, 14);
    DrawText(text, rect.x + (rect.width - textWidth) / 2, rect.y + (rect.height - 14) / 2, 14, textColor);
    
    return clicked;
}

float DrawSlider(Rectangle rect, float value, float minVal, float maxVal, Color trackColor, Color handleColor, Vector2 mousePos) {
    DrawRectangleRec(rect, trackColor);
    DrawRectangleLinesEx(rect, 1, WHITE);
    
    float norm = (value - minVal) / (maxVal - minVal);
    if (norm < 0) norm = 0;
    if (norm > 1) norm = 1;
    
    Rectangle handle = {rect.x + norm * (rect.width - 10), rect.y - 2, 10, rect.height + 4};
    DrawRectangleRec(handle, handleColor);
    
    // Handle interaction
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = (mousePos.x < 0) ? GetMousePosition() : mousePos;
        if (CheckCollisionPointRec(mouse, {rect.x - 5, rect.y - 5, rect.width + 10, rect.height + 10})) {
            float newNorm = (mouse.x - rect.x) / rect.width;
            if (newNorm < 0) newNorm = 0;
            if (newNorm > 1) newNorm = 1;
            return minVal + newNorm * (maxVal - minVal);
        }
    }
    
    return value;
}
