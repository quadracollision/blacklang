#pragma once

#include <raylib.h>
#include <cstring>

// Text input widget with focus management
void DrawTextInput(Rectangle rect, char* buffer, size_t maxLen, int fieldId, int& focusedId);

// Simple button that returns true when clicked
bool DrawButton(Rectangle rect, const char* text, Color bgColor = DARKGRAY, Color textColor = WHITE);

// Slider widget returns new value
float DrawSlider(Rectangle rect, float value, float minVal, float maxVal, Color trackColor = DARKGRAY, Color handleColor = LIGHTGRAY);
