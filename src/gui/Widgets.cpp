#include "Widgets.h"
#include "AppFont.h"
#include "../FilePicker.h"
#include <cstring>

bool DrawTextInput(Rectangle rect, char* buffer, size_t maxLen, int fieldId, int& focusedId, Vector2 mousePos, bool inputBlocked) {
    Vector2 m = (mousePos.x < 0) ? GetMousePosition() : mousePos;
    bool wasFocused = (focusedId == fieldId);
    bool clicked = false;
    
    if (!inputBlocked && CheckCollisionPointRec(m, rect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        focusedId = fieldId;
        clicked = true;
        // Always show keyboard on click to ensure it comes up if dismissed
        FilePicker::showKeyboard();
    }
    
    bool isFocused = (focusedId == fieldId);
    DrawRectangleRec(rect, isFocused ? WHITE : LIGHTGRAY);
    DrawRectangleLinesEx(rect, 1, isFocused ? BLUE : DARKGRAY);
    
    // Improved vertical centering for text input
    int fontSize = 20;
    DrawTextApp(buffer, (int)(rect.x + 5), (int)(rect.y + (rect.height - fontSize) / 2), fontSize, BLACK);
    
    if (isFocused) {
        // Blinking Cursor
        if ((int)(GetTime() * 2) % 2 == 0) {
            int textW = MeasureTextApp(buffer, fontSize);
            float cursorX = rect.x + 5 + textW + 2;
            DrawLineV({cursorX, rect.y + 4}, {cursorX, rect.y + rect.height - 4}, BLACK);
        }

#if defined(__ANDROID__)
        // Android Proxy Input ONLY
        int aKey, aChar;
        while (FilePicker::AndroidGetInput(aKey, aChar)) {
            if (aKey == 259) { // Backspace
                 size_t len = strlen(buffer);
                 if (len > 0) buffer[len-1] = '\0';
            } else if (aKey == 257) { // Enter
                 clicked = true;
                 focusedId = -1;
                 FilePicker::hideKeyboard();
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
        
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            focusedId = -1;
            clicked = true;
        }
#endif
    }
    return clicked;
}

bool DrawButton(Rectangle rect, const char* text, Color bgColor, Color textColor, Vector2 mousePos, int fontSize) {
    Vector2 m = (mousePos.x < 0) ? GetMousePosition() : mousePos;
    bool hovered = CheckCollisionPointRec(m, rect);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    
    Color bg = hovered ? ColorBrightness(bgColor, 0.2f) : bgColor;
    DrawRectangleRec(rect, bg);
    DrawRectangleLinesEx(rect, 1, WHITE);
    
    int textWidth = MeasureTextApp(text, fontSize);
    DrawTextApp(text, (int)(rect.x + (rect.width - textWidth) / 2), (int)(rect.y + (rect.height - fontSize) / 2), fontSize, textColor);
    
    return clicked;
}

// Touch-friendly slider widget with exclusive locking and relative drag
// Returns new value. Sets outIsDragging to true if actively being manipulated.
// Uses relative drag - doesn't jump to click position, follows finger/mouse movement.
float DrawSlider(Rectangle rect, float value, float minVal, float maxVal, Color trackColor, Color handleColor, Vector2 mousePos, bool* outIsDragging) {
    // Static variables for drag state - per-slider tracking
    static float activeSliderX = -999999.0f;
    static float activeSliderY = -999999.0f;
    static bool anySliderActive = false;
    static float grabOffsetX = 0.0f;  // Offset from handle center when grabbed
    static float initialMouseX = 0.0f;
    static float initialMouseY = 0.0f;
    static bool thresholdPassed = false;  // Whether we've determined this is a horizontal drag
    static bool verticalWon = false;       // Whether vertical movement won (scrolling)
    
    Vector2 mouse = (mousePos.x < 0) ? GetMousePosition() : mousePos;
    
    // Draw track
    DrawRectangleRec(rect, trackColor);
    DrawRectangleLinesEx(rect, 2, WHITE);
    
    // Normalize value
    float norm = (value - minVal) / (maxVal - minVal);
    if (norm < 0) norm = 0;
    if (norm > 1) norm = 1;
    
    // Larger handle for touch (50px wide, extends more above/below track)
    float handleWidth = 50.0f;
    float handleX = rect.x + norm * (rect.width - handleWidth);
    Rectangle handle = {
        handleX, 
        rect.y - 8, 
        handleWidth, 
        rect.height + 16
    };
    
    // Extended hit area for easier touch (larger padding for fat fingers)
    Rectangle hitArea = {
        rect.x - 15, 
        rect.y - 25, 
        rect.width + 30, 
        rect.height + 50
    };
    
    bool isHovering = CheckCollisionPointRec(mouse, hitArea);
    bool isDragging = false;
    bool isThisSliderActive = (activeSliderX == rect.x && activeSliderY == rect.y);
    float newValue = value;
    
    // Reset lock when mouse is released
    if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        anySliderActive = false;
        activeSliderX = -999999.0f;
        activeSliderY = -999999.0f;
        thresholdPassed = false;
        verticalWon = false;
    }
    
    // Lock to this slider on initial press
    if (isHovering && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !anySliderActive) {
        activeSliderX = rect.x;
        activeSliderY = rect.y;
        anySliderActive = true;
        isThisSliderActive = true;
        initialMouseX = mouse.x;
        initialMouseY = mouse.y;
        thresholdPassed = false;
        verticalWon = false;
        
        // Calculate grab offset from handle center
        float handleCenterX = handleX + handleWidth / 2.0f;
        grabOffsetX = mouse.x - handleCenterX;
    }
    
    // Only respond if this is the active slider and vertical didn't win
    if (isThisSliderActive && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !verticalWon) {
        float deltaX = std::abs(mouse.x - initialMouseX);
        float deltaY = std::abs(mouse.y - initialMouseY);

        // Threshold check: Must move enough in one direction to determine intent
        // Movement threshold of 8px before deciding direction
        if (!thresholdPassed) {
            if (deltaX > 8.0f || deltaY > 8.0f) {
               // Movement detected - determine direction
               if (deltaX > deltaY * 1.2f) {
                   // Horizontal wins (with 20% bias to help horizontal detection)
                   thresholdPassed = true;
               } else {
                   // Vertical wins -> Release lock so parent can scroll
                   verticalWon = true;
                   anySliderActive = false;
                   activeSliderX = -999999.0f;
                   activeSliderY = -999999.0f;
               }
            }
        }

        if (thresholdPassed) {
            isDragging = true;
            
            // Apply mouse position adjusted by grab offset
            float targetHandleCenterX = mouse.x - grabOffsetX;
            float targetHandleX = targetHandleCenterX - handleWidth / 2.0f;
            float newNorm = (targetHandleX - rect.x) / (rect.width - handleWidth);
            if (newNorm < 0) newNorm = 0;
            if (newNorm > 1) newNorm = 1;
            newValue = minVal + newNorm * (maxVal - minVal);
        }
    }
    
    // Draw handle with visual feedback when dragging
    Color actualHandleColor = isDragging ? WHITE : handleColor;
    DrawRectangleRec(handle, actualHandleColor);
    DrawRectangleLinesEx(handle, 2, isDragging ? GREEN : WHITE);
    
    // Output dragging state if requested
    if (outIsDragging) {
        *outIsDragging = isDragging;
    }
    
    return newValue;
}
