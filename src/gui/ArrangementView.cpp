#include "ArrangementView.h"
#include <cmath>
#include <cstdio>
#include "Widgets.h"
#include <algorithm>
#include <raymath.h>

namespace gui {

// Snap options labels
static const char* SNAP_LABELS[] = {"Free", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64"};
static const double SNAP_VALUES[] = {0.0, 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25};
static const int SNAP_COUNT = 8;

ArrangementView::ArrangementView(GuiState& s, AudioEngine& e) 
    : state(s), engine(e), player(e, s) {}

float ArrangementView::beatToPixel(double beat, float startX) {
    return startX + (float)(beat * pixelsPerBeat) - scrollX;
}

double ArrangementView::pixelToBeat(float px, float startX) {
    return (double)(px + scrollX - startX) / pixelsPerBeat;
}

double ArrangementView::snapBeat(double beat) {
    if (snapIndex == 0) return beat; // Free
    double snapVal = SNAP_VALUES[snapIndex];
    return std::round(beat / snapVal) * snapVal;
}

void ArrangementView::Draw(Rectangle bounds) {
    // Update arrangement player (advances playhead, triggers patterns)
    player.update(GetFrameTime());
    
    // Reserve space for transport bar at bottom
    float transportH = 60.0f;
    Rectangle mainArea = {bounds.x, bounds.y, bounds.width, bounds.height - transportH};
    Rectangle transportArea = {bounds.x, bounds.y + bounds.height - transportH, bounds.width, transportH};
    
    DrawRectangleRec(mainArea, Color{20, 20, 20, 255});
    DrawRectangleLinesEx(mainArea, 1, DARKGRAY);
    
    BeginScissorMode((int)mainArea.x, (int)mainArea.y, (int)mainArea.width, (int)mainArea.height);
    DrawGrid(mainArea);
    DrawTracks(mainArea);
    DrawPlayhead(mainArea);
    EndScissorMode();
    
    DrawTransportBar(transportArea);
    HandleInput(bounds);
    
    // Drag Ghost
    if (state.drag.isDragging && CheckCollisionPointRec(state.getMousePosition(), mainArea)) {
        DrawTextApp("Drop Here", state.getMousePosition().x + 20, state.getMousePosition().y, 14, WHITE);
    }
}

void ArrangementView::DrawGrid(Rectangle bounds) {
    float headerH = 24.0f;
    float startX = 100.0f;
    float drawX = bounds.x + startX;
    
    double startBeat = pixelToBeat(drawX, drawX);
    double endBeat = pixelToBeat(drawX + bounds.width, drawX);
    
    int startBar = (int)(startBeat / 4.0);
    int endBar = (int)(endBeat / 4.0) + 1;
    
    for (int b = startBar; b <= endBar; ++b) {
        if (b < 0) continue;
        float x = beatToPixel(b * 4.0, drawX);
        if (x < drawX) continue;
        if (x > bounds.x + bounds.width) break;
        
        DrawLine(x, bounds.y + headerH, x, bounds.y + bounds.height, Color{50, 50, 50, 255});
        char buf[8];
        sprintf(buf, "%d", b + 1);
        DrawTextApp(buf, x + 2, bounds.y + 4, 14, GRAY);
    }
}

void ArrangementView::DrawTracks(Rectangle bounds) {
    float headerH = 24.0f;
    
    // Dynamic Track Height (Fit to view if possible)
    float availH = bounds.height - headerH;
    float rowH = (state.columns.size() > 0) ? (availH / state.columns.size()) : 50.0f;
    if (rowH < 50.0f) rowH = 50.0f; // Minimum height
    
    float startX = 100.0f;
    
    // Calculate the drawable area (below the header)
    float clipAreaTop = bounds.y + headerH;
    float clipAreaBottom = bounds.y + bounds.height;
    
    float currentY = bounds.y + headerH - scrollY;
    
    for (size_t i = 0; i < state.columns.size(); ++i) {
        PatternColumn& col = state.columns[i];
        
        // Skip if row is completely above or below visible area
        if (currentY + rowH < clipAreaTop || currentY > clipAreaBottom) {
            currentY += rowH;
            continue;
        }
        
        // Track Header (clipped to visible area)
        float visibleTop = std::max(currentY, clipAreaTop);
        float visibleBottom = std::min(currentY + rowH, clipAreaBottom);
        float visibleHeight = visibleBottom - visibleTop;
        
        if (visibleHeight > 0) {
            Rectangle nameRect = {bounds.x, visibleTop, startX, visibleHeight};
            DrawRectangleRec(nameRect, Color{35, 35, 35, 255});
            DrawRectangleLinesEx(nameRect, 1, DARKGRAY);
            
            // Only draw text if enough space
            if (visibleTop <= currentY + 16 && visibleBottom >= currentY + 30) {
                DrawTextApp(col.title.c_str(), nameRect.x + 6, currentY + 16, 14, WHITE);
            }
        }
        
        // Timeline Area
        Rectangle timelineRect = {bounds.x + startX, currentY, bounds.width - startX, rowH};
        
        // Highlight if dragging over
        if (state.drag.isDragging && CheckCollisionPointRec(state.getMousePosition(), timelineRect)) {
             DrawRectangleRec(timelineRect, Color{40, 40, 50, 100});
        }

        // Draw Clips (with scissor clamped to visible timeline area)
        int scissorY = (int)std::max(currentY, clipAreaTop);
        int scissorH = (int)std::min(currentY + rowH, clipAreaBottom) - scissorY;
        if (scissorH > 0) {
            BeginScissorMode((int)(bounds.x + startX), scissorY, (int)(bounds.width - startX), scissorH);
            if (auto* arrangement = engine.getArrangement()) {
                 if (auto* track = arrangement->getTrack(col.trackName)) {
                     for (size_t c = 0; c < track->clips.size(); ++c) {
                         const auto& clip = track->clips[c];
                         
                         // Calculate actual clip length from pattern data
                         // syncBase = 0 means pattern fits in 1 bar (4 beats)
                         // syncBase > 0 means that many steps per bar
                         //   e.g., 16 steps with syncBase=4 = 16/4 = 4 bars = 16 beats
                         float clipLengthBeats = 4.0f; // Default 1 bar
                         if (Pattern* pattern = engine.getPattern(clip.patternName)) {
                             if (pattern->syncBase > 0) {
                                 // bars = totalSteps / stepsPerBar
                                 float bars = (float)pattern->steps / (float)pattern->syncBase;
                                 clipLengthBeats = bars * 4.0f; // 4 beats per bar
                             } else {
                                 // syncBase = 0: pattern fits in 1 bar = 4 beats
                                 clipLengthBeats = 4.0f;
                             }
                         }
                         
                         float gx = beatToPixel(clip.startBeat, bounds.x + startX);
                         float gw = clipLengthBeats * pixelsPerBeat;
                         Rectangle clipRect = {gx, currentY + 3, gw, rowH - 6};
                         
                         // Culling
                         if (clipRect.x + clipRect.width < bounds.x + startX) continue;
                         if (clipRect.x > bounds.x + bounds.width) continue;
                         
                         // Selection highlight
                         bool isSelected = (selectedTrackIdx == (int)i && selectedClipIdx == (int)c);
                         Color clipColor = isSelected ? ORANGE : SKYBLUE;
                         
                         DrawRectangleRec(clipRect, clipColor);
                         DrawRectangleLinesEx(clipRect, isSelected ? 2 : 1, isSelected ? WHITE : DARKGRAY);
                         
                         // Only draw text if clip is wide enough
                         if (clipRect.width > 30) {
                             // Clip text to clip bounds
                             BeginScissorMode((int)clipRect.x, (int)clipRect.y, (int)clipRect.width, (int)clipRect.height);
                             DrawTextApp(clip.patternName.c_str(), clipRect.x + 4, clipRect.y + 12, 14, BLACK);
                             EndScissorMode();
                             
                             // Restore main scissor
                             BeginScissorMode((int)(bounds.x + startX), scissorY, (int)(bounds.width - startX), scissorH);
                         }
                     }
                 }
            }
            EndScissorMode();
        }

        currentY += rowH;
    }
}

void ArrangementView::DrawPlayhead(Rectangle bounds) {
    float headerH = 24.0f;
    float startX = 100.0f;
    
    double beat = player.getPlayheadBeat();
    float x = beatToPixel(beat, bounds.x + startX);
    
    // Always draw playhead position (even when stopped)
    if (x >= bounds.x + startX && x <= bounds.x + bounds.width) {
        Color playheadColor = player.isPlaying() ? RED : Color{200, 80, 80, 255};
        DrawLine((int)x, (int)(bounds.y + headerH), (int)x, (int)(bounds.y + bounds.height), playheadColor);
        
        // Draw playhead triangle at top
        Vector2 tri[3] = {
            {x - 6, bounds.y + headerH},
            {x + 6, bounds.y + headerH},
            {x, bounds.y + headerH + 10}
        };
        DrawTriangle(tri[0], tri[1], tri[2], playheadColor);
    }
    
    // Auto-scroll to follow playhead (only when playing)
    if (player.isPlaying()) {
        float viewRight = bounds.x + bounds.width - 50;
        if (x > viewRight) {
            scrollX += (x - viewRight);
        }
    }
}

void ArrangementView::DrawTransportBar(Rectangle bounds) {
    DrawRectangleRec(bounds, Color{30, 30, 30, 255});
    DrawRectangleLinesEx(bounds, 1, DARKGRAY);
    
    float btnW = 80.0f;
    float btnH = 44.0f;
    float gap = 12.0f;
    float btnY = bounds.y + (bounds.height - btnH) / 2;
    float btnX = bounds.x + 12.0f;
    
    // Snap Button
    Rectangle snapBtn = {btnX, btnY, btnW, btnH};
    DrawRectangleRec(snapBtn, Color{50, 50, 60, 255});
    DrawRectangleLinesEx(snapBtn, 1, GRAY);
    DrawTextApp(SNAP_LABELS[snapIndex], snapBtn.x + 8, snapBtn.y + 14, 14, WHITE);
    
    if (state.isClickAvailable() && CheckCollisionPointRec(state.getMousePosition(), snapBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick();
        snapIndex = (snapIndex + 1) % SNAP_COUNT;
    }
    btnX += btnW + gap;
    
    // Delete Button
    Rectangle delBtn = {btnX, btnY, btnW, btnH};
    DrawRectangleRec(delBtn, Color{80, 30, 30, 255});
    DrawRectangleLinesEx(delBtn, 1, GRAY);
    DrawTextApp("Del", delBtn.x + 24, delBtn.y + 14, 14, WHITE);
    
    if (state.isClickAvailable() && CheckCollisionPointRec(state.getMousePosition(), delBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick();
        // Delete selected clip
        if (selectedTrackIdx >= 0 && selectedClipIdx >= 0 && selectedTrackIdx < (int)state.columns.size()) {
            std::string trackName = state.columns[selectedTrackIdx].trackName;
            if (auto* arrangement = engine.getArrangement()) {
                arrangement->removeClip(trackName, selectedClipIdx);
            }
            selectedTrackIdx = -1;
            selectedClipIdx = -1;
        }
    }
    btnX += btnW + gap;
    
    // Go to Start Button (<)
    float navBtnW = 44.0f;
    Rectangle startBtn = {btnX, btnY, navBtnW, btnH};
    DrawRectangleRec(startBtn, Color{50, 50, 60, 255});
    DrawRectangleLinesEx(startBtn, 1, GRAY);
    DrawTextApp("<", startBtn.x + 16, startBtn.y + 14, 14, WHITE);
    
    if (state.isClickAvailable() && CheckCollisionPointRec(state.getMousePosition(), startBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick();
        player.setPlayheadBeat(0.0);
    }
    btnX += navBtnW + gap;
    
    // Go to End Button (>)
    Rectangle endBtn = {btnX, btnY, navBtnW, btnH};
    DrawRectangleRec(endBtn, Color{50, 50, 60, 255});
    DrawRectangleLinesEx(endBtn, 1, GRAY);
    DrawTextApp(">", endBtn.x + 16, endBtn.y + 14, 14, WHITE);
    
    if (state.isClickAvailable() && CheckCollisionPointRec(state.getMousePosition(), endBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick();
        // Find the end of the last pattern
        double maxEndBeat = 0.0;
        if (auto* arrangement = engine.getArrangement()) {
            for (auto& track : arrangement->tracks) {
                for (auto& clip : track.clips) {
                    maxEndBeat = std::max(maxEndBeat, clip.startBeat + clip.lengthBeats);
                }
            }
        }
        player.setPlayheadBeat(maxEndBeat);
    }
    btnX += navBtnW + gap;
    
    // Play Button
    Rectangle playBtn = {btnX, btnY, btnW, btnH};
    bool isArrangementPlaying = player.isPlaying();
    DrawRectangleRec(playBtn, isArrangementPlaying ? Color{30, 80, 30, 255} : Color{50, 50, 60, 255});
    DrawRectangleLinesEx(playBtn, 1, GRAY);
    DrawTextApp(isArrangementPlaying ? "Stop" : "Play", playBtn.x + 18, playBtn.y + 14, 14, WHITE);
    
    if (state.isClickAvailable() && CheckCollisionPointRec(state.getMousePosition(), playBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick();
        if (isArrangementPlaying) {
            player.stop();
            if (state.recorder.isRecording) {
                // Stop engine recording first
                engine.stopInMemoryRecording();
                state.recorder.isRecording = false;
                
                // Show Review UI
                state.recorder.showReview = true;
                state.recorder.isArmed = false; // Auto-disarm
            }
        } else {
            if (state.recorder.isArmed) {
                // Initialize engine recording
                engine.startInMemoryRecording();
                state.recorder.isRecording = true;
            }
            player.start();
        }
    }
}

void ArrangementView::HandleInput(Rectangle bounds) {
    // Block input if modal overlays are active
    if (state.editor.isOpen || state.recorder.showModeSelection || state.recorder.showReview) {
        return;
    }

    float headerH = 24.0f;
    float transportH = 60.0f;
    
    // Dynamic Height
    float availH = bounds.height - transportH - headerH;
    float rowH = (state.columns.size() > 0) ? (availH / state.columns.size()) : 50.0f;
    if (rowH < 50.0f) rowH = 50.0f;
    
    float startX = 100.0f;
    Rectangle mainArea = {bounds.x, bounds.y, bounds.width, bounds.height - transportH};
    Rectangle timelineArea = {bounds.x + startX, bounds.y + headerH, bounds.width - startX, bounds.height - headerH - transportH};
    
    // Early exit if mouse is outside arrangement area
    if (!CheckCollisionPointRec(state.getMousePosition(), bounds)) {
        return;
    }
    
    // ========== PINCH ZOOM ==========
    int touchCount = GetTouchPointCount();
    if (touchCount == 2) {
        Vector2 touch0 = GetTouchPosition(0);
        Vector2 touch1 = GetTouchPosition(1);
        float currentDist = sqrtf((touch1.x - touch0.x) * (touch1.x - touch0.x) + 
                                   (touch1.y - touch0.y) * (touch1.y - touch0.y));
        if (pinchActive) {
            float delta = currentDist - lastPinchDist;
            pixelsPerBeat = std::clamp(pixelsPerBeat + delta * 0.1f, 5.0f, 100.0f);
        }
        pinchActive = true;
        lastPinchDist = currentDist;
        return;
    }
    pinchActive = false;
    
    // ========== SCROLL WHEEL (zoom/scroll) ==========
    if (CheckCollisionPointRec(state.getMousePosition(), mainArea)) {
        float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.01f) {
            if (IsKeyDown(KEY_LEFT_SHIFT)) {
                scrollX = std::max(0.0f, scrollX - wheel * 50.0f);
            } else if (IsKeyDown(KEY_LEFT_CONTROL)) {
                scrollY = std::max(0.0f, scrollY - wheel * rowH * 0.5f);
            } else {
                // Zoom centered on mouse
                double beatUnderMouse = pixelToBeat(state.getMousePosition().x, bounds.x + startX);
                pixelsPerBeat = std::clamp(pixelsPerBeat * (wheel > 0 ? 1.1f : 0.9f), 5.0f, 100.0f);
                scrollX = std::max(0.0f, (float)(beatUnderMouse * pixelsPerBeat) - 
                                   (state.getMousePosition().x - bounds.x - startX));
            }
        }
    }
    
    // ========== DELETE KEY ==========
    if ((IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) && 
        selectedTrackIdx >= 0 && selectedClipIdx >= 0 && 
        selectedTrackIdx < (int)state.columns.size()) {
        std::string trackName = state.columns[selectedTrackIdx].trackName;
        if (auto* arrangement = engine.getArrangement()) {
            arrangement->removeClip(trackName, selectedClipIdx);
        }
        selectedTrackIdx = -1;
        selectedClipIdx = -1;
    }
    
    // ========== CLIP DRAG (active) ==========
    if (clipDragActive) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            Vector2 mouse = state.getMousePosition();
            if (!clipDragThresholdMet && Vector2Distance(clipDragStartPos, mouse) > 5.0f) {
                clipDragThresholdMet = true;
            }
            if (clipDragThresholdMet && selectedTrackIdx >= 0 && selectedClipIdx >= 0) {
                double newBeat = snapBeat(clipOriginalBeat + 
                    pixelToBeat(mouse.x, bounds.x + startX) - clipDragStartBeat);
                if (newBeat < 0) newBeat = 0;
                
                std::string trackName = state.columns[selectedTrackIdx].trackName;
                if (auto* arr = engine.getArrangement()) {
                    if (auto* track = arr->getTrack(trackName)) {
                        if (selectedClipIdx < (int)track->clips.size()) {
                            track->clips[selectedClipIdx].startBeat = newBeat;
                        }
                    }
                }
            }
        } else {
            clipDragActive = false;
            clipDragThresholdMet = false;
        }
        return;
    }
    
    // ========== TIMELINE PAN (update scroll while dragging) ==========
    // Skip on pressed frame - let initialization set correct coordinates first
    if (timelineDragActive && 
        (IsMouseButtonDown(MOUSE_LEFT_BUTTON) || IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) &&
        !IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) {
        Vector2 delta = {timelineDragStart.x - state.getMousePosition().x,
                        timelineDragStart.y - state.getMousePosition().y};
        scrollX = std::max(0.0f, timelineScrollStartX + delta.x);
        scrollY = std::max(0.0f, timelineScrollStartY + delta.y);
    }
    
    // ========== LABEL AREA DRAG (vertical scroll only) ==========
    Rectangle labelArea = {bounds.x, bounds.y + headerH, startX, bounds.height - headerH - transportH};
    if (labelDragActive && IsMouseButtonDown(MOUSE_LEFT_BUTTON) &&
        !IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        float deltaY = labelDragStart.y - state.getMousePosition().y;
        scrollY = std::max(0.0f, labelScrollStartY + deltaY);
    }
    
    // ========== HEADER SEEK DRAG (set playhead position) ==========
    Rectangle headerArea = {bounds.x + startX, bounds.y, bounds.width - startX, headerH};
    if (headerSeekActive && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        double clickBeat = std::max(0.0, pixelToBeat(state.getMousePosition().x, bounds.x + startX));
        player.setPlayheadBeat(clickBeat);
    }
    
    // ========== DRAG-AND-DROP FROM PATTERN LIST ==========
    if (state.drag.isDragging && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(state.getMousePosition(), mainArea)) {
            float mouseY = state.getMousePosition().y - (bounds.y + headerH) + scrollY;
            int trackIdx = (int)(mouseY / rowH);
            
            if (trackIdx >= 0 && trackIdx < (int)state.columns.size()) {
                std::string trackName = state.columns[trackIdx].trackName;
                double beat = std::max(0.0, snapBeat(pixelToBeat(state.getMousePosition().x, bounds.x + startX)));
                
                float length = 4.0f;
                if (Pattern* p = engine.getPattern(state.drag.patternName)) {
                    length = (p->syncBase > 0) ? (float)p->steps / p->syncBase * 4.0f : 4.0f;
                }
                
                if (auto* arr = engine.getArrangement()) {
                    arr->addClip(trackName, state.drag.patternName, beat, length);
                    engine.assignPatternToTrack(state.drag.patternName, state.columns[trackIdx].trackName);
                }
                state.drag.isDragging = false;
                state.drag.isHolding = false;
            }
        }
        return;
    }
    
    // ========== MOUSE PRESS IN TIMELINE AREA ==========
    if (CheckCollisionPointRec(state.getMousePosition(), timelineArea) && 
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !state.drag.isDragging) {
        
        float mouseY = state.getMousePosition().y - (bounds.y + headerH) + scrollY;
        int trackIdx = (int)(mouseY / rowH);
        
        // Check if clicking on an existing clip
        if (trackIdx >= 0 && trackIdx < (int)state.columns.size()) {
            std::string trackName = state.columns[trackIdx].trackName;
            if (auto* arr = engine.getArrangement()) {
                if (auto* track = arr->getTrack(trackName)) {
                    for (size_t c = 0; c < track->clips.size(); ++c) {
                        const auto& clip = track->clips[c];
                        float clipX = beatToPixel(clip.startBeat, bounds.x + startX);
                        float clipW = (float)(clip.lengthBeats * pixelsPerBeat);
                        Rectangle clipRect = {clipX, bounds.y + headerH + trackIdx * rowH - scrollY + 2, 
                                              clipW, rowH - 4};
                        
                        if (CheckCollisionPointRec(state.getMousePosition(), clipRect)) {
                            // Clicked on clip - select and start drag
                            state.consumeClick();
                            selectedTrackIdx = trackIdx;
                            selectedClipIdx = (int)c;
                            clipDragActive = true;
                            clipDragThresholdMet = false;
                            clipDragStartPos = state.getMousePosition();
                            clipOriginalBeat = clip.startBeat;
                            clipDragStartBeat = pixelToBeat(state.getMousePosition().x, bounds.x + startX);
                            return;
                        }
                    }
                }
            }
        }
        
        // Clicked on empty space - always start pan tracking (we decide on release if it was a tap)
        selectedTrackIdx = -1;
        selectedClipIdx = -1;
        
        // Start pan tracking regardless of pattern selection
        timelineDragActive = true;
        timelineDragStart = state.getMousePosition();
        timelineScrollStartX = scrollX;
        timelineScrollStartY = scrollY;
        
        // Store info for potential tap-to-place
        pendingPlaceTrackIdx = trackIdx;
        pendingPlaceBeat = std::max(0.0, snapBeat(pixelToBeat(state.getMousePosition().x, bounds.x + startX)));
    }
    
    // ========== TIMELINE PAN RELEASE (check for tap-to-place) ==========
    if (timelineDragActive && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        timelineDragActive = false;
        
        // Check if this was a tap (minimal movement) vs a scroll
        float dragDist = Vector2Distance(timelineDragStart, state.getMousePosition());
        if (dragDist < 10.0f && pendingPlaceTrackIdx >= 0 && pendingPlaceTrackIdx < (int)state.columns.size()) {
            // It was a tap - try to place pattern
            std::string selectedPattern = "";
            for (auto& pair : state.activePatternSlots) {
                if (pair.first >= 0 && pair.first < (int)state.columns.size()) {
                    PatternColumn& col = state.columns[pair.first];
                    if (pair.second >= 0 && pair.second < (int)col.patternNames.size() && 
                        !col.patternNames[pair.second].empty()) {
                        selectedPattern = col.patternNames[pair.second];
                        break;
                    }
                }
            }
            
            if (!selectedPattern.empty()) {
                std::string trackName = state.columns[pendingPlaceTrackIdx].trackName;
                
                float length = 4.0f;
                if (Pattern* p = engine.getPattern(selectedPattern)) {
                    length = (p->syncBase > 0) ? (float)p->steps / p->syncBase * 4.0f : 4.0f;
                }
                
                if (auto* arr = engine.getArrangement()) {
                    arr->addClip(trackName, selectedPattern, pendingPlaceBeat, length);
                    engine.assignPatternToTrack(selectedPattern, state.columns[pendingPlaceTrackIdx].trackName);
                }
            }
        }
        pendingPlaceTrackIdx = -1;
    }
    
    // ========== MIDDLE CLICK PAN ==========
    if (CheckCollisionPointRec(state.getMousePosition(), mainArea) && 
        IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) {
        timelineDragActive = true;
        timelineDragStart = state.getMousePosition();
        timelineScrollStartX = scrollX;
        timelineScrollStartY = scrollY;
    }
    
    // ========== LABEL AREA PRESS (initiate vertical scroll) ==========
    if (CheckCollisionPointRec(state.getMousePosition(), labelArea) && 
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !state.drag.isDragging) {
        state.consumeClick();
        labelDragActive = true;
        labelDragStart = state.getMousePosition();
        labelScrollStartY = scrollY;
    }
    
    // ========== LABEL AREA RELEASE ==========
    if (labelDragActive && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        labelDragActive = false;
    }
    
    // ========== HEADER PRESS (initiate seek) ==========
    if (CheckCollisionPointRec(state.getMousePosition(), headerArea) && 
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !state.drag.isDragging) {
        state.consumeClick();
        headerSeekActive = true;
        double clickBeat = std::max(0.0, pixelToBeat(state.getMousePosition().x, bounds.x + startX));
        player.setPlayheadBeat(clickBeat);
    }
    
    // ========== HEADER RELEASE ==========
    if (headerSeekActive && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        headerSeekActive = false;
    }
}

} // namespace gui
