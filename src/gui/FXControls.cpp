#include "FXControls.h"
#include "Widgets.h"
#include "../GuiState.h"
#include "../AudioEngine.h"
#include "../Pattern.h"
#include <algorithm>
#include <cstdio>

namespace gui {

FXControls::FXControls(GuiState& s, AudioEngine& e) : state(s), engine(e) {}

void FXControls::DrawFXSlider(Rectangle rect, const char* label, float* value, float min, float max) {
    DrawText(label, rect.x - 100, rect.y, 16, WHITE);
    
    DrawRectangleRec(rect, DARKGRAY);
    DrawRectangleLinesEx(rect, 1, WHITE);
    
    float norm = (*value - min) / (max - min);
    if (norm < 0) norm = 0;
    if (norm > 1) norm = 1;
    Rectangle handle = {rect.x + norm * (rect.width - 10), rect.y - 2, 10, 14};
    DrawRectangleRec(handle, LIGHTGRAY);
    
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, {rect.x - 5, rect.y - 5, rect.width + 10, rect.height + 10})) {
            float newVal = min + ((mouse.x - rect.x) / rect.width) * (max - min);
            if (newVal < min) newVal = min;
            if (newVal > max) newVal = max;
            *value = newVal;
        }
    }
    
    DrawText(TextFormat("%.2f", *value), rect.x + rect.width + 10, rect.y, 10, WHITE);
}

float FXControls::Draw(Rectangle area, Pattern& pattern) {
    float startY = area.y;
    Pattern& p = pattern;
    
    DrawText(TextFormat("Step: %d", state.editor.selectedStep + 1), area.x, startY + 10, 20, WHITE);
    
    if (state.editor.selectedStep != -1 && state.editor.stepStates[state.editor.selectedStep]) {
        DrawText("FX Selection:", area.x, startY + 25, 20, WHITE);
        
        // FX ListBox with all implemented FX
        struct FxOption { int id; const char* name; };
        std::vector<FxOption> availableFX = {
            {Pattern::FX_CUTOFF, "Cut Off"},
            {Pattern::FX_SLIDE, "Slide"},
            {Pattern::FX_STUTTER, "Stutter"},
            {Pattern::FX_NUDGE, "Nudge"}
            // Future: Add more FX here as they're implemented
        };
        
        auto& currentStepFX = p.stepFX[state.editor.selectedStep + 1];
        
        float boxW = 140;
        float boxH = 100;
        Rectangle availBox = {area.x + 120, startY + 70, boxW, boxH};
        Rectangle appliedBox = {area.x + 280, startY + 70, boxW, boxH};
        
        // Draw Available Box
        DrawRectangleRec(availBox, BLACK);
        DrawRectangleLinesEx(availBox, 1, WHITE);
        DrawText("Available", availBox.x, availBox.y - 12, 10, LIGHTGRAY);
        
        // Draw Applied Box
        DrawRectangleRec(appliedBox, BLACK);
        DrawRectangleLinesEx(appliedBox, 1, WHITE);
        DrawText("Applied", appliedBox.x, appliedBox.y - 12, 10, LIGHTGRAY);

        float availY = availBox.y + 5;
        float appliedY = appliedBox.y + 5;
        float itemH = 20;
        
        for (const auto& opt : availableFX) {
            bool isActive = std::find(currentStepFX.begin(), currentStepFX.end(), opt.id) != currentStepFX.end();
            
            Rectangle itemRect;
            bool isSelected = false;
            
            if (isActive) {
                itemRect = {appliedBox.x + 5, appliedY, boxW - 10, itemH};
                appliedY += itemH + 2;
                if (state.editor.selectedAppliedFxId == opt.id) isSelected = true;
            } else {
                itemRect = {availBox.x + 5, availY, boxW - 10, itemH};
                availY += itemH + 2;
                if (state.editor.selectedAvailableFxId == opt.id) isSelected = true;
            }
            
            if (isSelected) {
                DrawRectangleRec(itemRect, ORANGE);
            } else if (CheckCollisionPointRec(GetMousePosition(), itemRect)) {
                DrawRectangleRec(itemRect, {50, 50, 50, 255});
            }
            
            DrawText(opt.name, itemRect.x + 5, itemRect.y + 5, 10, isSelected ? BLACK : WHITE);
            
            if (CheckCollisionPointRec(GetMousePosition(), itemRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (isActive) {
                    state.editor.selectedAppliedFxId = opt.id;
                    state.editor.selectedAvailableFxId = -1;
                } else {
                    state.editor.selectedAvailableFxId = opt.id;
                    state.editor.selectedAppliedFxId = -1;
                }
            }
        }
        
        // Add Button
        Rectangle addBtn = {availBox.x, availBox.y + boxH + 5, boxW, 25};
        DrawRectangleRec(addBtn, GRAY);
        DrawText("Add", addBtn.x + boxW/2 - 10, addBtn.y + 5, 10, WHITE);
        
        if (CheckCollisionPointRec(GetMousePosition(), addBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (state.editor.selectedAvailableFxId != -1) {
                bool alreadyActive = std::find(currentStepFX.begin(), currentStepFX.end(), state.editor.selectedAvailableFxId) != currentStepFX.end();
                if (!alreadyActive) {
                    currentStepFX.push_back(state.editor.selectedAvailableFxId);
                    state.editor.selectedAvailableFxId = -1;
                    engine.addPattern(p);
                }
            }
        }

        // Remove Button
        Rectangle removeBtn = {appliedBox.x, appliedBox.y + boxH + 5, boxW, 25};
        DrawRectangleRec(removeBtn, GRAY);
        DrawText("Remove", removeBtn.x + boxW/2 - 20, removeBtn.y + 5, 10, WHITE);
        
        if (CheckCollisionPointRec(GetMousePosition(), removeBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (state.editor.selectedAppliedFxId != -1) {
                currentStepFX.erase(std::remove(currentStepFX.begin(), currentStepFX.end(), state.editor.selectedAppliedFxId), currentStepFX.end());
                state.editor.selectedAppliedFxId = -1;
                engine.addPattern(p);
            }
        }
        
        // Parameter Panel
        if (state.editor.selectedAppliedFxId != -1) {
            float paramPanelY = removeBtn.y + removeBtn.height + 15;
            DrawText("FX Params:", area.x, paramPanelY, 20, WHITE);
            
            int step = state.editor.selectedStep + 1;
            
            if (state.editor.selectedAppliedFxId == Pattern::FX_STUTTER) {
                DrawStutterParams(area.x, paramPanelY, p, step);
            } else if (state.editor.selectedAppliedFxId == Pattern::FX_SLIDE) {
                DrawSlideParams(area.x, paramPanelY, p, step);
            } else if (state.editor.selectedAppliedFxId == Pattern::FX_NUDGE) {
                DrawNudgeParams(area.x, paramPanelY, p, step);
            } else {
                DrawText("No params", area.x + 120, paramPanelY, 20, GRAY);
            }
        }
        
        // Cleanup empty entries
        if (currentStepFX.empty()) {
            p.stepFX.erase(state.editor.selectedStep + 1);
            engine.addPattern(p);
        }
        
        startY += boxH + 110;
    } else {
        DrawText("Select an active step to edit FX", area.x, startY + 25, 20, GRAY);
        startY += 40;
    }
    
    startY += 60;
    return startY - area.y;
}

void FXControls::DrawStutterParams(float x, float paramPanelY, Pattern& p, int step) {
    // Rate Control
    DrawText("Rate:", x + 120, paramPanelY, 20, WHITE);
    
    float currentRate = 4.0f;
    if (p.stepFXParams[step].count(Pattern::PAR_STUTTER_RATE)) {
        currentRate = p.stepFXParams[step][Pattern::PAR_STUTTER_RATE];
    }
    
    Rectangle rateSlider = {x + 230, paramPanelY + 5, 150, 10};
    DrawRectangleRec(rateSlider, DARKGRAY);
    DrawRectangleLinesEx(rateSlider, 1, WHITE);
    
    float minRate = 1.0f, maxRate = 16.0f;
    float rateNorm = (currentRate - minRate) / (maxRate - minRate);
    if (rateNorm < 0) rateNorm = 0;
    if (rateNorm > 1) rateNorm = 1;
    Rectangle rateHandle = {rateSlider.x + rateNorm * (rateSlider.width - 10), rateSlider.y - 2, 10, 14};
    DrawRectangleRec(rateHandle, LIGHTGRAY);
    
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, {rateSlider.x - 5, rateSlider.y - 5, rateSlider.width + 10, rateSlider.height + 10})) {
            float newVal = minRate + ((mouse.x - rateSlider.x) / rateSlider.width) * (maxRate - minRate);
            if (newVal < minRate) newVal = minRate;
            if (newVal > maxRate) newVal = maxRate;
            p.stepFXParams[step][Pattern::PAR_STUTTER_RATE] = newVal;
        }
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        engine.addPattern(p);
    }
    
    DrawText(TextFormat("%.1f", p.stepFXParams[step].count(Pattern::PAR_STUTTER_RATE) ? 
        p.stepFXParams[step][Pattern::PAR_STUTTER_RATE] : 4.0f), rateSlider.x + rateSlider.width + 10, paramPanelY, 10, WHITE);

    // Speed Control
    paramPanelY += 35;
    DrawText("Speed:", x + 120, paramPanelY, 20, WHITE);
    
    float currentSpeed = 1.0f;
    if (p.stepFXParams[step].count(Pattern::PAR_STUTTER_SPEED)) {
        currentSpeed = p.stepFXParams[step][Pattern::PAR_STUTTER_SPEED];
    }
    
    Rectangle speedSlider = {x + 230, paramPanelY + 5, 150, 10};
    DrawRectangleRec(speedSlider, DARKGRAY);
    DrawRectangleLinesEx(speedSlider, 1, WHITE);
    
    float minSpeed = 0.5f, maxSpeed = 4.0f;
    float speedNorm = (currentSpeed - minSpeed) / (maxSpeed - minSpeed);
    if (speedNorm < 0) speedNorm = 0;
    if (speedNorm > 1) speedNorm = 1;
    Rectangle speedHandle = {speedSlider.x + speedNorm * (speedSlider.width - 10), speedSlider.y - 2, 10, 14};
    DrawRectangleRec(speedHandle, LIGHTGRAY);
    
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, {speedSlider.x - 5, speedSlider.y - 5, speedSlider.width + 10, speedSlider.height + 10})) {
            float newVal = minSpeed + ((mouse.x - speedSlider.x) / speedSlider.width) * (maxSpeed - minSpeed);
            if (newVal < minSpeed) newVal = minSpeed;
            if (newVal > maxSpeed) newVal = maxSpeed;
            p.stepFXParams[step][Pattern::PAR_STUTTER_SPEED] = newVal;
        }
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        engine.addPattern(p);
    }
    
    DrawText(TextFormat("%.2f", p.stepFXParams[step].count(Pattern::PAR_STUTTER_SPEED) ? 
        p.stepFXParams[step][Pattern::PAR_STUTTER_SPEED] : 1.0f), speedSlider.x + speedSlider.width + 10, paramPanelY, 10, WHITE);
}

void FXControls::DrawSlideParams(float x, float paramPanelY, Pattern& p, int step) {
    // Time Control
    DrawText("Time:", x + 120, paramPanelY, 20, WHITE);
    
    float currentTime = 1.0f;
    if (p.stepFXParams[step].count(Pattern::PAR_SLIDE_TIME)) {
        currentTime = p.stepFXParams[step][Pattern::PAR_SLIDE_TIME];
    }
    
    Rectangle timeSlider = {x + 230, paramPanelY + 5, 150, 10};
    DrawRectangleRec(timeSlider, DARKGRAY);
    DrawRectangleLinesEx(timeSlider, 1, WHITE);
    
    float minTime = 0.1f, maxTime = 1.0f;
    float timeNorm = (currentTime - minTime) / (maxTime - minTime);
    if (timeNorm < 0) timeNorm = 0;
    if (timeNorm > 1) timeNorm = 1;
    Rectangle timeHandle = {timeSlider.x + timeNorm * (timeSlider.width - 10), timeSlider.y - 2, 10, 14};
    DrawRectangleRec(timeHandle, LIGHTGRAY);
    
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, {timeSlider.x - 5, timeSlider.y - 5, timeSlider.width + 10, timeSlider.height + 10})) {
            float newVal = minTime + ((mouse.x - timeSlider.x) / timeSlider.width) * (maxTime - minTime);
            if (newVal < minTime) newVal = minTime;
            if (newVal > maxTime) newVal = maxTime;
            p.stepFXParams[step][Pattern::PAR_SLIDE_TIME] = newVal;
        }
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        engine.addPattern(p);
    }
    
    DrawText(TextFormat("%.2f", p.stepFXParams[step].count(Pattern::PAR_SLIDE_TIME) ? 
        p.stepFXParams[step][Pattern::PAR_SLIDE_TIME] : 1.0f), timeSlider.x + timeSlider.width + 10, paramPanelY, 10, WHITE);

    // Squelch Control
    paramPanelY += 35;
    DrawText("Squelch:", x + 120, paramPanelY, 20, WHITE);
    
    float currentSquelch = 0.0f;
    if (p.stepFXParams[step].count(Pattern::PAR_SLIDE_SQUELCH)) {
        currentSquelch = p.stepFXParams[step][Pattern::PAR_SLIDE_SQUELCH];
    }
    
    Rectangle squelchSlider = {x + 230, paramPanelY + 5, 150, 10};
    DrawRectangleRec(squelchSlider, DARKGRAY);
    DrawRectangleLinesEx(squelchSlider, 1, WHITE);
    
    float minSquelch = 0.0f, maxSquelch = 1.0f;
    float squelchNorm = (currentSquelch - minSquelch) / (maxSquelch - minSquelch);
    if (squelchNorm < 0) squelchNorm = 0;
    if (squelchNorm > 1) squelchNorm = 1;
    Rectangle squelchHandle = {squelchSlider.x + squelchNorm * (squelchSlider.width - 10), squelchSlider.y - 2, 10, 14};
    DrawRectangleRec(squelchHandle, LIGHTGRAY);
    
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, {squelchSlider.x - 5, squelchSlider.y - 5, squelchSlider.width + 10, squelchSlider.height + 10})) {
            float newVal = minSquelch + ((mouse.x - squelchSlider.x) / squelchSlider.width) * (maxSquelch - minSquelch);
            if (newVal < minSquelch) newVal = minSquelch;
            if (newVal > maxSquelch) newVal = maxSquelch;
            p.stepFXParams[step][Pattern::PAR_SLIDE_SQUELCH] = newVal;
        }
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        engine.addPattern(p);
    }
    
    DrawText(TextFormat("%.2f", p.stepFXParams[step].count(Pattern::PAR_SLIDE_SQUELCH) ? 
        p.stepFXParams[step][Pattern::PAR_SLIDE_SQUELCH] : 0.0f), squelchSlider.x + squelchSlider.width + 10, paramPanelY, 10, WHITE);
}

void FXControls::DrawNudgeParams(float x, float paramPanelY, Pattern& p, int step) {
    DrawText("Nudge (Start/End):", x + 170, paramPanelY + 5, 10, WHITE);
    
    float currentOffset = 0.5f;
    if (p.stepFXParams[step].count(Pattern::PAR_NUDGE_OFFSET)) {
        currentOffset = p.stepFXParams[step][Pattern::PAR_NUDGE_OFFSET];
    }
    
    Rectangle offsetSlider = {x + 280, paramPanelY + 5, 150, 10};
    DrawRectangleRec(offsetSlider, DARKGRAY);
    DrawRectangleLinesEx(offsetSlider, 1, WHITE);
    
    // Center Tick
    DrawRectangle(offsetSlider.x + offsetSlider.width/2 - 1, offsetSlider.y - 2, 2, 14, GRAY);
    
    // Handle
    float minOff = 0.0f, maxOff = 1.0f;
    float offNorm = (currentOffset - minOff) / (maxOff - minOff);
    if (offNorm < 0) offNorm = 0;
    if (offNorm > 1) offNorm = 1;
    Rectangle offHandle = {offsetSlider.x + offNorm * (offsetSlider.width - 2), offsetSlider.y, 2, 10};
    DrawRectangleRec(offHandle, ORANGE);

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = GetMousePosition();
        if (CheckCollisionPointRec(mouse, {offsetSlider.x - 5, offsetSlider.y - 5, offsetSlider.width + 10, offsetSlider.height + 10})) {
            float newVal = minOff + ((mouse.x - offsetSlider.x) / offsetSlider.width) * (maxOff - minOff);
            if (newVal < minOff) newVal = minOff;
            if (newVal > maxOff) newVal = maxOff;
            p.stepFXParams[step][Pattern::PAR_NUDGE_OFFSET] = newVal;
        }
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        engine.addPattern(p);
    }
    
    // Text Description
    float offset = p.stepFXParams[step].count(Pattern::PAR_NUDGE_OFFSET) ? 
        p.stepFXParams[step][Pattern::PAR_NUDGE_OFFSET] : 0.5f;
    if (offset > 0.55f) {
        DrawText(TextFormat("Start +%.0f%%", (offset-0.5f)*200), offsetSlider.x + offsetSlider.width + 10, paramPanelY, 10, WHITE);
    } else if (offset < 0.45f) {
        DrawText(TextFormat("Len %.0f%%", offset*200), offsetSlider.x + offsetSlider.width + 10, paramPanelY, 10, WHITE);
    } else {
        DrawText("Full", offsetSlider.x + offsetSlider.width + 10, paramPanelY, 10, WHITE);
    }
}

} // namespace gui
