#include "FXControls.h"
#include "Widgets.h"
#include "../GuiState.h"
#include "../AudioEngine.h"
#include "../Pattern.h"
#include "../fx/FXTypes.h"
#include <algorithm>
#include <cstdio>
#include <string>

namespace gui {


FXControls::FXControls(GuiState& s, AudioEngine& e) : state(s), engine(e) {}

void FXControls::DrawFXSlider(Rectangle rect, const char* label, float* value, float min, float max) {
    DrawTextApp(label, rect.x - 100, rect.y, 16, WHITE);
    
    DrawRectangleRec(rect, DARKGRAY);
    DrawRectangleLinesEx(rect, 1, WHITE);
    
    float norm = (*value - min) / (max - min);
    if (norm < 0) norm = 0;
    if (norm > 1) norm = 1;
    Rectangle handle = {rect.x + norm * (rect.width - 10), rect.y - 2, 10, 14};
    DrawRectangleRec(handle, LIGHTGRAY);
    
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        Vector2 mouse = state.getMousePosition();
        if (CheckCollisionPointRec(mouse, {rect.x - 5, rect.y - 5, rect.width + 10, rect.height + 10})) {
            float newVal = min + ((mouse.x - rect.x) / rect.width) * (max - min);
            if (newVal < min) newVal = min;
            if (newVal > max) newVal = max;
            *value = newVal;
        }
    }
    
    DrawTextApp(TextFormat("%.2f", *value), rect.x + rect.width + 10, rect.y, 10, WHITE);
}

float FXControls::Draw(Rectangle area, Pattern& pattern, Rectangle parentScissor) {
    float startY = area.y;
    Pattern& p = pattern;
    
    DrawTextApp(TextFormat("Step: %d", state.editor.selectedStep + 1), area.x, startY + 10, 24, WHITE);
    
    if (state.editor.selectedStep != -1 && state.editor.stepStates[state.editor.selectedStep]) {
        DrawTextApp("FX Selection:", area.x, startY + 40, 22, WHITE);
        
        // Dynamic FX List from system
        struct FxOption { int id; std::string name; };
        std::vector<FxOption> availableFX;
        
        auto allFX = fx::GetAllFX();
        for (auto id : allFX) {
             if (fx::IsFXImplemented(id)) {
                 // Hide ADSR per user request (broken)
                 if (id == Pattern::FX_ADSR) continue;
                 
                 availableFX.push_back({id, std::string(fx::GetFXName(id))});
             }
        }
        
        auto& currentStepFX = p.stepFX[state.editor.selectedStep + 1];
        
        // Touch-friendly box sizes - centered in area
        float boxW = 200;
        float boxH = 140;
        float totalBoxWidth = boxW * 2 + 20; // 2 boxes + gap
        float boxStartX = area.x + (area.width - totalBoxWidth) / 2; // Center
        
        Rectangle availBox = {boxStartX, startY + 75, boxW, boxH};
        Rectangle appliedBox = {boxStartX + boxW + 20, startY + 75, boxW, boxH};
        
        // Draw Available Box
        DrawRectangleRec(availBox, BLACK);
        DrawRectangleLinesEx(availBox, 2, WHITE);
        DrawTextApp("Available", availBox.x, availBox.y - 18, 16, LIGHTGRAY);
        
        // Draw Applied Box
        DrawRectangleRec(appliedBox, BLACK);
        DrawRectangleLinesEx(appliedBox, 2, WHITE);
        DrawTextApp("Applied", appliedBox.x, appliedBox.y - 18, 16, LIGHTGRAY);

        // Filter lists
        std::vector<FxOption> listAvailable;
        std::vector<FxOption> listApplied;
        
        for (const auto& opt : availableFX) {
            bool isActive = std::find(currentStepFX.begin(), currentStepFX.end(), opt.id) != currentStepFX.end();
            if (isActive) {
                listApplied.push_back(opt);
            } else {
                listAvailable.push_back(opt);
            }
        }

        // --- Available List Logic ---
        BeginScissorMode((int)availBox.x, (int)availBox.y, (int)availBox.width, (int)availBox.height);
        float itemH = 35;  // Touch-friendly item height
        float contentH_Avail = listAvailable.size() * (itemH + 4);
        
        // Touch drag scrolling for Available list
        if (CheckCollisionPointRec(state.getMousePosition(), availBox)) {
            // Mouse wheel (desktop)
            state.editor.fxAvailableScrollY += GetMouseWheelMove() * 30.0f;
            state.editor.scrollConsumed = true;
            
            // Touch drag start
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                state.editor.fxAvailDragging = true;
                state.editor.fxDragStartY = state.getMousePosition().y;
                state.editor.fxDragStartScrollY = state.editor.fxAvailableScrollY;
            }
        }
        
        // Touch drag update
        if (state.editor.fxAvailDragging && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float deltaY = state.getMousePosition().y - state.editor.fxDragStartY;
            state.editor.fxAvailableScrollY = state.editor.fxDragStartScrollY + deltaY;
            state.editor.scrollConsumed = true;
        }
        
        // Touch drag end
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            state.editor.fxAvailDragging = false;
        }
        
        // Clamp scroll
        if (state.editor.fxAvailableScrollY > 0) state.editor.fxAvailableScrollY = 0;
        if (contentH_Avail > boxH) {
             if (state.editor.fxAvailableScrollY < -(contentH_Avail - boxH)) state.editor.fxAvailableScrollY = -(contentH_Avail - boxH);
        } else {
             state.editor.fxAvailableScrollY = 0;
        }
        
        float currY = availBox.y + 5 + state.editor.fxAvailableScrollY;
        for (const auto& opt : listAvailable) {
            Rectangle itemRect = {availBox.x + 5, currY, boxW - 12, itemH};
            bool isSelected = (state.editor.selectedAvailableFxId == opt.id);
            
            if (currY + itemH > availBox.y && currY < availBox.y + availBox.height) {
                if (isSelected) {
                    DrawRectangleRec(itemRect, ORANGE);
                } else if (CheckCollisionPointRec(state.getMousePosition(), itemRect) && CheckCollisionPointRec(state.getMousePosition(), availBox)) {
                    DrawRectangleRec(itemRect, {50, 50, 50, 255});
                }
                
                DrawTextApp(opt.name.c_str(), itemRect.x + 8, itemRect.y + 8, 16, isSelected ? BLACK : WHITE);
                
                if (CheckCollisionPointRec(state.getMousePosition(), itemRect) && CheckCollisionPointRec(state.getMousePosition(), availBox) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    state.editor.selectedAvailableFxId = opt.id;
                    state.editor.selectedAppliedFxId = -1;
                }
            }
            currY += itemH + 4;
        }
        EndScissorMode();
        
        // RESTORE PARENT SCISSOR
        BeginScissorMode((int)parentScissor.x, (int)parentScissor.y, (int)parentScissor.width, (int)parentScissor.height);
        
        // Scrollbar Available
        if (contentH_Avail > boxH) {
            float sbH = (boxH / contentH_Avail) * boxH;
            float sbY = availBox.y + (-state.editor.fxAvailableScrollY / contentH_Avail) * boxH;
            DrawRectangleRec({availBox.x + boxW - 6, sbY, 4, sbH}, DARKGRAY);
        }

        // --- Applied List Logic ---
        BeginScissorMode((int)appliedBox.x, (int)appliedBox.y, (int)appliedBox.width, (int)appliedBox.height);
        float contentH_Applied = listApplied.size() * (itemH + 4);
        
        // Touch drag scrolling for Applied list
        if (CheckCollisionPointRec(state.getMousePosition(), appliedBox)) {
            // Mouse wheel (desktop)
            state.editor.fxAppliedScrollY += GetMouseWheelMove() * 30.0f;
            state.editor.scrollConsumed = true;
            
            // Touch drag start
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                state.editor.fxAppliedDragging = true;
                state.editor.fxDragStartY = state.getMousePosition().y;
                state.editor.fxDragStartScrollY = state.editor.fxAppliedScrollY;
            }
        }
        
        // Touch drag update
        if (state.editor.fxAppliedDragging && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float deltaY = state.getMousePosition().y - state.editor.fxDragStartY;
            state.editor.fxAppliedScrollY = state.editor.fxDragStartScrollY + deltaY;
            state.editor.scrollConsumed = true;
        }
        
        // Touch drag end
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            state.editor.fxAppliedDragging = false;
        }
        
        // Clamp scroll
        if (state.editor.fxAppliedScrollY > 0) state.editor.fxAppliedScrollY = 0;
        if (contentH_Applied > boxH) {
             if (state.editor.fxAppliedScrollY < -(contentH_Applied - boxH)) state.editor.fxAppliedScrollY = -(contentH_Applied - boxH);
        } else {
             state.editor.fxAppliedScrollY = 0;
        }
        
        currY = appliedBox.y + 5 + state.editor.fxAppliedScrollY;
        for (const auto& opt : listApplied) {
            Rectangle itemRect = {appliedBox.x + 5, currY, boxW - 12, itemH};
            bool isSelected = (state.editor.selectedAppliedFxId == opt.id);
            
            if (currY + itemH > appliedBox.y && currY < appliedBox.y + appliedBox.height) {
                if (isSelected) {
                    DrawRectangleRec(itemRect, ORANGE);
                } else if (CheckCollisionPointRec(state.getMousePosition(), itemRect) && CheckCollisionPointRec(state.getMousePosition(), appliedBox)) {
                    DrawRectangleRec(itemRect, {50, 50, 50, 255});
                }
                
                DrawTextApp(opt.name.c_str(), itemRect.x + 8, itemRect.y + 8, 16, isSelected ? BLACK : WHITE);
                
                if (CheckCollisionPointRec(state.getMousePosition(), itemRect) && CheckCollisionPointRec(state.getMousePosition(), appliedBox) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    state.editor.selectedAppliedFxId = opt.id;
                    state.editor.selectedAvailableFxId = -1;
                }
            }
            currY += itemH + 4;
        }
        EndScissorMode();
        
        // RESTORE PARENT SCISSOR
        BeginScissorMode((int)parentScissor.x, (int)parentScissor.y, (int)parentScissor.width, (int)parentScissor.height);
        
        // Scrollbar Applied - thicker for touch
        if (contentH_Applied > boxH) {
            float sbH = (boxH / contentH_Applied) * boxH;
            float sbY = appliedBox.y + (-state.editor.fxAppliedScrollY / contentH_Applied) * boxH;
            DrawRectangleRec({appliedBox.x + boxW - 10, sbY, 8, sbH}, DARKGRAY);
        }
        
        // Add Button - touch friendly
        Rectangle addBtn = {availBox.x, availBox.y + boxH + 10, boxW, 45};
        DrawRectangleRec(addBtn, BLUE);
        const char* addTxt = "Add FX";
        int addW = MeasureTextApp(addTxt, 18);
        DrawTextApp(addTxt, addBtn.x + (addBtn.width - addW)/2, addBtn.y + 12, 18, WHITE);
        
        if (CheckCollisionPointRec(state.getMousePosition(), addBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (state.editor.selectedAvailableFxId != -1) {
                bool alreadyActive = std::find(currentStepFX.begin(), currentStepFX.end(), state.editor.selectedAvailableFxId) != currentStepFX.end();
                if (!alreadyActive) {
                    currentStepFX.push_back(state.editor.selectedAvailableFxId);
                    state.editor.selectedAvailableFxId = -1;
                    engine.addPattern(p);
                }
            }
        }
        
        // Remove Button - touch friendly
        Rectangle removeBtn = {appliedBox.x, appliedBox.y + boxH + 10, boxW, 45};
        DrawRectangleRec(removeBtn, RED);
        const char* remTxt = "Remove";
        int remW = MeasureTextApp(remTxt, 18);
        DrawTextApp(remTxt, removeBtn.x + (removeBtn.width - remW)/2, removeBtn.y + 12, 18, WHITE);
        
        if (CheckCollisionPointRec(state.getMousePosition(), removeBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (state.editor.selectedAppliedFxId != -1) {
                currentStepFX.erase(std::remove(currentStepFX.begin(), currentStepFX.end(), state.editor.selectedAppliedFxId), currentStepFX.end());
                state.editor.selectedAppliedFxId = -1;
                engine.addPattern(p);
            }
        }
        
        // Parameter Panel
        if (state.editor.selectedAppliedFxId != -1) {
            float paramPanelY = removeBtn.y + removeBtn.height + 15;
            // Use boxStartX to align params with the boxes (centered)
            DrawTextApp("FX Params:", boxStartX, paramPanelY, 22, WHITE);
            
            // Add vertical offset after header so sliders don't overlap
            float sliderStartY = paramPanelY + 35;
            
            int step = state.editor.selectedStep + 1;
            
            if (state.editor.selectedAppliedFxId == Pattern::FX_STUTTER) {
                DrawStutterParams(boxStartX, sliderStartY, p, step);
            } else if (state.editor.selectedAppliedFxId == Pattern::FX_SLIDE) {
                DrawSlideParams(boxStartX, sliderStartY, p, step);
            } else if (state.editor.selectedAppliedFxId == Pattern::FX_NUDGE) {
                DrawNudgeParams(boxStartX, sliderStartY, p, step);
            } else if (state.editor.selectedAppliedFxId == Pattern::FX_ADSR) {
                DrawADSRParams(boxStartX, sliderStartY, p, step);
            } else if (state.editor.selectedAppliedFxId == Pattern::FX_EQ) {
                DrawEQParams(boxStartX, sliderStartY, p, step);
            } else {
                DrawTextApp("No params", boxStartX + 120, sliderStartY, 20, GRAY);
            }
        }
        
        // Cleanup empty entries
        if (currentStepFX.empty()) {
            p.stepFX.erase(state.editor.selectedStep + 1);
            engine.addPattern(p);
        }
        
        startY += boxH + 140;  // Base UI height
        
        // Add extra height for parameter panel
        if (state.editor.selectedAppliedFxId != -1) {
             if (state.editor.selectedAppliedFxId == Pattern::FX_ADSR) {
                 startY += 400; // 4 sliders with 90px spacing
             } else if (state.editor.selectedAppliedFxId == Pattern::FX_EQ) {
                 startY += 230; // EQ graph panel
             } else {
                 startY += 250; // Standard params (2 sliders with 90px spacing + header)
             }
        }
    } else {
        DrawTextApp("Select an active step to edit FX", area.x, startY + 30, 22, GRAY);
        startY += 60;
    }
    
    startY += 60;
    return startY - area.y;
}

void FXControls::DrawStutterParams(float x, float paramPanelY, Pattern& p, int step) {
    // Rate Control
    DrawTextApp("RATE:", x, paramPanelY + 15, 22, WHITE);
    
    float currentRate = 4.0f;
    if (p.stepFXParams[step].count(Pattern::PAR_STUTTER_RATE)) {
        currentRate = p.stepFXParams[step][Pattern::PAR_STUTTER_RATE];
    }
    
    bool rateDragging = false;
    Rectangle rateSlider = {x + 120, paramPanelY, 250, 70};
    float newRate = DrawSlider(rateSlider, currentRate, 1.0f, 16.0f, DARKGRAY, LIGHTGRAY, state.getMousePosition(), &rateDragging);
    
    // Use activeControlId pattern like TrackFX mixer
    if (rateDragging && !state.editor.editorDragging) {
        state.drag.activeControlId = "STEPFX_STUTTER_RATE";
        state.consumeClick();
        p.stepFXParams[step][Pattern::PAR_STUTTER_RATE] = newRate;
        engine.addPattern(p);
    }
    DrawTextApp(TextFormat("%.1f", newRate), rateSlider.x + rateSlider.width + 15, paramPanelY + 18, 18, WHITE);

    // Speed Control
    paramPanelY += 90;
    DrawTextApp("SPEED:", x, paramPanelY + 15, 22, WHITE);
    
    float currentSpeed = 1.0f;
    if (p.stepFXParams[step].count(Pattern::PAR_STUTTER_SPEED)) {
        currentSpeed = p.stepFXParams[step][Pattern::PAR_STUTTER_SPEED];
    }
    
    bool speedDragging = false;
    Rectangle speedSlider = {x + 120, paramPanelY, 250, 70};
    float newSpeed = DrawSlider(speedSlider, currentSpeed, 0.5f, 4.0f, DARKGRAY, LIGHTGRAY, state.getMousePosition(), &speedDragging);
    
    if (speedDragging && !state.editor.editorDragging) {
        state.drag.activeControlId = "STEPFX_STUTTER_SPEED";
        state.consumeClick();
        p.stepFXParams[step][Pattern::PAR_STUTTER_SPEED] = newSpeed;
        engine.addPattern(p);
    }
    DrawTextApp(TextFormat("%.2f", newSpeed), speedSlider.x + speedSlider.width + 15, paramPanelY + 18, 18, WHITE);
}

void FXControls::DrawSlideParams(float x, float paramPanelY, Pattern& p, int step) {
    // Time Control
    DrawTextApp("TIME:", x, paramPanelY + 15, 22, WHITE);
    
    float currentTime = 1.0f;
    if (p.stepFXParams[step].count(Pattern::PAR_SLIDE_TIME)) {
        currentTime = p.stepFXParams[step][Pattern::PAR_SLIDE_TIME];
    }
    
    bool timeDragging = false;
    Rectangle timeSlider = {x + 120, paramPanelY, 250, 70};
    float newTime = DrawSlider(timeSlider, currentTime, 0.1f, 1.0f, DARKGRAY, LIGHTGRAY, state.getMousePosition(), &timeDragging);
    
    if (timeDragging && !state.editor.editorDragging) {
        state.drag.activeControlId = "STEPFX_SLIDE_TIME";
        state.consumeClick();
        p.stepFXParams[step][Pattern::PAR_SLIDE_TIME] = newTime;
        engine.addPattern(p);
    }
    DrawTextApp(TextFormat("%.2f", newTime), timeSlider.x + timeSlider.width + 15, paramPanelY + 18, 18, WHITE);

    // Squelch Control
    paramPanelY += 90;
    DrawTextApp("SQUELCH:", x, paramPanelY + 15, 22, WHITE);
    
    float currentSquelch = 0.0f;
    if (p.stepFXParams[step].count(Pattern::PAR_SLIDE_SQUELCH)) {
        currentSquelch = p.stepFXParams[step][Pattern::PAR_SLIDE_SQUELCH];
    }
    
    bool squelchDragging = false;
    Rectangle squelchSlider = {x + 120, paramPanelY, 250, 70};
    float newSquelch = DrawSlider(squelchSlider, currentSquelch, 0.0f, 1.0f, DARKGRAY, LIGHTGRAY, state.getMousePosition(), &squelchDragging);
    
    if (squelchDragging && !state.editor.editorDragging) {
        state.drag.activeControlId = "STEPFX_SLIDE_SQUELCH";
        state.consumeClick();
        p.stepFXParams[step][Pattern::PAR_SLIDE_SQUELCH] = newSquelch;
        engine.addPattern(p);
    }
    DrawTextApp(TextFormat("%.2f", newSquelch), squelchSlider.x + squelchSlider.width + 15, paramPanelY + 18, 18, WHITE);
}

void FXControls::DrawNudgeParams(float x, float paramPanelY, Pattern& p, int step) {
    DrawTextApp("NUDGE:", x, paramPanelY + 15, 22, WHITE);
    
    float currentOffset = 0.5f;
    if (p.stepFXParams[step].count(Pattern::PAR_NUDGE_OFFSET)) {
        currentOffset = p.stepFXParams[step][Pattern::PAR_NUDGE_OFFSET];
    }
    
    bool offsetDragging = false;
    Rectangle offsetSlider = {x + 120, paramPanelY, 250, 70};
    float newOffset = DrawSlider(offsetSlider, currentOffset, 0.0f, 1.0f, DARKGRAY, ORANGE, state.getMousePosition(), &offsetDragging);
    
    if (offsetDragging && !state.editor.editorDragging) {
        state.drag.activeControlId = "STEPFX_NUDGE_OFFSET";
        state.consumeClick();
        p.stepFXParams[step][Pattern::PAR_NUDGE_OFFSET] = newOffset;
        engine.addPattern(p);
    }
    
    // Text Description
    if (newOffset > 0.55f) {
        DrawTextApp(TextFormat("Start +%.0f%%", (newOffset-0.5f)*200), offsetSlider.x + offsetSlider.width + 15, paramPanelY + 18, 18, WHITE);
    } else if (newOffset < 0.45f) {
        DrawTextApp(TextFormat("Len %.0f%%", newOffset*200), offsetSlider.x + offsetSlider.width + 15, paramPanelY + 18, 18, WHITE);
    } else {
        DrawTextApp("Full", offsetSlider.x + offsetSlider.width + 15, paramPanelY + 18, 18, WHITE);
    }
}

void FXControls::DrawADSRParams(float x, float y, Pattern& p, int step) {
    auto drawParamSlider = [&](const char* label, int paramId, float defaultVal, float min, float max, float yOffset) {
        DrawTextApp(label, x, y + yOffset + 15, 22, WHITE);
        
        float currentVal = defaultVal;
        if (p.stepFXParams[step].count(paramId)) {
            currentVal = p.stepFXParams[step][paramId];
        }
        
        bool isDragging = false;
        Rectangle slider = {x + 120, y + yOffset, 250, 70};
        float newVal = DrawSlider(slider, currentVal, min, max, DARKGRAY, LIGHTGRAY, state.getMousePosition(), &isDragging);
        
        if (isDragging && !state.editor.editorDragging) {
            state.drag.activeControlId = std::string("STEPFX_ADSR_") + label;
            state.consumeClick();
            p.stepFXParams[step][paramId] = newVal;
            engine.addPattern(p);
        }
        DrawTextApp(TextFormat("%.2f", newVal), slider.x + slider.width + 15, y + yOffset + 18, 18, WHITE);
    };
    
    drawParamSlider("ATTACK:", Pattern::PAR_ATTACK_TIME, 0.05f, 0.0f, 1.0f, 0);
    drawParamSlider("DECAY:", Pattern::PAR_DECAY_TIME, 0.1f, 0.0f, 1.0f, 90);
    drawParamSlider("SUSTAIN:", Pattern::PAR_SUSTAIN_LEVEL, 0.8f, 0.0f, 1.0f, 180);
    drawParamSlider("RELEASE:", Pattern::PAR_RELEASE_TIME, 0.2f, 0.0f, 1.0f, 270);
}

void FXControls::DrawEQParams(float x, float y, Pattern& p, int step) {
    // Constants
    const char* bandLabels[5] = { "60", "250", "1K", "4K", "12K" };
    const int bandParams[5] = {
        Pattern::PAR_EQ_BAND1, Pattern::PAR_EQ_BAND2, Pattern::PAR_EQ_BAND3,
        Pattern::PAR_EQ_BAND4, Pattern::PAR_EQ_BAND5
    };
    
    // Graph dimensions - centered to match FX panel width (420px = 200 + 20 + 200)
    float panelWidth = 420;
    float graphW = 360;
    float graphH = 130;
    float graphX = x + (panelWidth - graphW) / 2;  // Center horizontally
    float graphY = y + 20;
    float barW = 40;
    float barGap = (graphW - barW * 5) / 6;
    float centerY = graphY + graphH / 2;
    
    // Draw dB labels on left side (outside graph)
    DrawTextApp("+12", graphX - 28, graphY, 10, GRAY);
    DrawTextApp("0", graphX - 12, centerY - 5, 10, GRAY);
    DrawTextApp("-12", graphX - 28, graphY + graphH - 10, 10, GRAY);
    
    // Draw background grid
    DrawRectangle(graphX, graphY, graphW, graphH, {20, 20, 30, 255});
    DrawRectangleLinesEx({graphX, graphY, graphW, graphH}, 2, GRAY);
    
    // Draw horizontal reference lines (0dB, +6dB, -6dB)
    DrawLine(graphX, centerY, graphX + graphW, centerY, {80, 80, 80, 255});  // 0dB
    DrawLine(graphX, graphY + graphH * 0.25f, graphX + graphW, graphY + graphH * 0.25f, {50, 50, 50, 255});  // +6dB
    DrawLine(graphX, graphY + graphH * 0.75f, graphX + graphW, graphY + graphH * 0.75f, {50, 50, 50, 255});  // -6dB
    
    // Use scissor mode to clip bars within graph bounds
    BeginScissorMode((int)graphX, (int)graphY, (int)graphW, (int)graphH);
    
    // Draw 5 bars (inside scissor)
    for (int i = 0; i < 5; ++i) {
        float barX = graphX + barGap + i * (barW + barGap);
        
        // Get current value
        float gain = 0.0f;  // -1 to 1
        if (p.stepFXParams[step].count(bandParams[i])) {
            gain = p.stepFXParams[step][bandParams[i]];
        }
        
        // Calculate bar position (gain maps to Y position)
        float gainNorm = (1.0f - gain) / 2.0f;  // 0 (top) to 1 (bottom)
        float handleY = graphY + gainNorm * graphH;
        
        // Draw bar from center
        Color barColor = gain >= 0 ? Color{60, 180, 100, 255} : Color{180, 80, 80, 255};
        if (gain >= 0) {
            DrawRectangle(barX, handleY, barW, centerY - handleY, barColor);
        } else {
            DrawRectangle(barX, centerY, barW, handleY - centerY, barColor);
        }
        
        // Draw handle (draggable) - now clipped by scissor
        Rectangle handleRect = {barX, handleY - 5, barW, 10};
        DrawRectangleRec(handleRect, WHITE);
    }
    
    EndScissorMode();
    
    // Draw frequency labels below graph (outside scissor)
    for (int i = 0; i < 5; ++i) {
        float barX = graphX + barGap + i * (barW + barGap);
        int labelW = MeasureTextApp(bandLabels[i], 12);
        DrawTextApp(bandLabels[i], barX + (barW - labelW) / 2, graphY + graphH + 5, 12, WHITE);
    }
    
    // Handle drag interaction (for all bars)
    for (int i = 0; i < 5; ++i) {
        float barX = graphX + barGap + i * (barW + barGap);
        Rectangle hitArea = {barX - 5, graphY, barW + 10, graphH};
        
        if (CheckCollisionPointRec(state.getMousePosition(), hitArea) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            state.editor.scrollConsumed = true;
            
            float mouseY = state.getMousePosition().y;
            float newGainNorm = (mouseY - graphY) / graphH;  // 0 to 1
            if (newGainNorm < 0) newGainNorm = 0;
            if (newGainNorm > 1) newGainNorm = 1;
            float newGain = 1.0f - newGainNorm * 2.0f;  // -1 to 1
            p.stepFXParams[step][bandParams[i]] = newGain;
        }
        
        if (CheckCollisionPointRec(state.getMousePosition(), hitArea) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            engine.addPattern(p);
        }
    }
}

} // namespace gui

