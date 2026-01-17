#pragma once

#include <raylib.h>
#include <cstring>

// Text input widget with focus management
bool DrawTextInput(Rectangle rect, char* buffer, size_t maxLen, int fieldId, int& focusedId, Vector2 mousePos = {-1,-1}, bool inputBlocked = false);

// Simple button that returns true when clicked
bool DrawButton(Rectangle rect, const char* text, Color bgColor = DARKGRAY, Color textColor = WHITE, Vector2 mousePos = {-1,-1}, int fontSize = 14);

// Touch-friendly slider widget returns new value
// If outIsDragging is provided, it will be set to true when the slider is being actively manipulated
float DrawSlider(Rectangle rect, float value, float minVal, float maxVal, Color trackColor = DARKGRAY, Color handleColor = LIGHTGRAY, Vector2 mousePos = {-1,-1}, bool* outIsDragging = nullptr);
