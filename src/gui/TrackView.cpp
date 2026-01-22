#include "TrackView.h"
#include "TrackFX.h"
#include "ArrangementView.h" // NEW
#include <raymath.h>
#include "Widgets.h"
#include "StepGrid.h"
#include "../FilePicker.h"
#include "../GuiState.h"
#include "../AudioEngine.h"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath> // for abs
#if defined(__ANDROID__)
#include <android/log.h>
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "BlackLang_GUI", __VA_ARGS__)
#else
#include <cstdio>
#define LOGD(...) printf(__VA_ARGS__); printf("\n")
#endif

namespace gui {

TrackView::TrackView(GuiState& s, AudioEngine& e) : state(s), engine(e) {}

void TrackView::Draw() {
    static ArrangementView arrangementView(state, engine);

    // 1. Manage Drag State Promotion (Hold -> Drag)
    // Long Press Logic:
    // - If held for > 0.4s without moving much: Promote to DRAG
    // - If moved > 10px BEFORE 0.4s: Cancel HOLD, Enforce SCROLL
    if (state.drag.isHolding) {
        float dist = Vector2Distance(state.getMousePosition(), state.drag.initialClickPos);
        double holdDuration = GetTime() - state.drag.holdStartTime;
        
        // Thresholds
        float moveThreshold = 10.0f; // px
        double longPressTime = 0.4;  // seconds
        
        if (holdDuration > longPressTime && dist < moveThreshold) {
            // HELD LONG ENOUGH -> START DRAGGING
            state.drag.isDragging = true;
            state.drag.isHolding = false;
            state.drag.isScrolling = false; // Kill scroll
            state.drag.scrollDirection = 0;
            
            // Haptic Feedback (simulated or real if available)
            // TODO: Add haptic trigger if possible
        } else if (dist > moveThreshold) {
            // MOVED TOO SOON -> IS SCROLLING
            state.drag.isHolding = false; 
            // state.drag.isScrolling remains true (was set on press)
        } else if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
             // Released early without moving -> This is a CLICK (Handled in HandlePatternClick)
             state.drag.isHolding = false;
        }
    }

    // 1.5 Touch Scroll Logic with direction detection
    if (state.drag.isScrolling && !state.drag.isDragging) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            Vector2 mouse = state.getMousePosition();
            Vector2 delta = Vector2Subtract(mouse, state.drag.lastMousePos);
            
            // Determine direction if pending
            if (state.drag.scrollDirection == 0) {
                float totalDeltaX = std::abs(mouse.x - state.drag.scrollStartPos.x);
                float totalDeltaY = std::abs(mouse.y - state.drag.scrollStartPos.y);
                
                // Need at least 10px movement to lock direction
                if (totalDeltaX > 10 || totalDeltaY > 10) {
                    if (totalDeltaX > totalDeltaY) {
                        state.drag.scrollDirection = 2;  // Horizontal
                    } else {
                        state.drag.scrollDirection = 1;  // Vertical
                    }
                    // NOTE: Don't cancel isHolding here - let the distance check handle it
                }
            }
            
            // Apply scroll based on locked direction
            if (state.drag.scrollDirection == 1) {
                // Vertical scroll within column
                if (state.drag.scrollColumnIndex >= 0 && state.drag.scrollColumnIndex < (int)state.columns.size()) {
                    state.columns[(size_t)state.drag.scrollColumnIndex].scrollY -= delta.y;
                }
            } else if (state.drag.scrollDirection == 2) {
                // Horizontal scroll of track view
                state.mainScrollX -= delta.x;
                float maxScrollX = (state.columns.size() * (state.getColumnWidth() + 10)) - state.getScreenWidth() + 100;
                if (state.mainScrollX < 0) state.mainScrollX = 0;
                if (maxScrollX > 0 && state.mainScrollX > maxScrollX) state.mainScrollX = maxScrollX;
            }
            
            state.drag.lastMousePos = mouse;
        }
    }

    // 2. Draw Columns
    // 2. Draw Columns
    float startX = 20.0f;
    if (state.isPortrait) {
        // Center columns (should be ~0 if width fits exactly)
        startX = (float)(state.getScreenWidth() - (3 * state.getColumnWidth())) / 2.0f;
    }
    float colX = startX - state.mainScrollX;
    
    // Scissor for main view area
    float headerH = (float)state.getHeaderHeight();
    float footerH = (float)state.getFooterHeight();
    float screenW = (float)state.getScreenWidth();
    float screenH = (float)state.getScreenHeight();
    
    // PORTRAIT: 3 Visible Cells + Button Area (Stacked)
    // Landscape: Full height
    float patternH = (float)state.getPatternHeight();
    float buttonsH = state.isPortrait ? 160.0f : 40.0f; // Large Button Area (reduced to 160)
    // Fix: Include Track Header (30px) in visible content height calculation
    float visibleContentH = (state.isPortrait ? 30.0f : 0.0f) + (3.0f * patternH) + buttonsH;
    
    float viewY = headerH;
    float viewH = screenH - headerH - footerH;
    
    if (state.isPortrait) {
        // Bottom Align: Gap at Top
        // If content fits with gap, use gap. Else full height.
        if (visibleContentH < viewH) {
            float topGap = viewH - visibleContentH;
            
            // Draw Arrangement View in the gap!
            if (topGap > 50.0f) {
                // Timeline starts right under header bar (use actual header size, not padding)
                float actualHeaderTop = 60.0f; // Adjusted to avoid overlapping text
                Rectangle timelineRect = { 0, actualHeaderTop, screenW, headerH + topGap - actualHeaderTop };
                arrangementView.Draw(timelineRect);
            }
            
            viewY = screenH - footerH - visibleContentH;
            viewH = visibleContentH;
        }
    }
    
    Rectangle viewRect = {0, viewY, screenW, viewH};
    BeginScissorMode((int)viewRect.x, (int)viewRect.y, (int)viewRect.width, (int)viewRect.height);

    for (size_t i = 0; i < state.columns.size(); ++i) {
        state.columns[i].bounds = {
            colX,
            viewY, // Top of view area
            (float)state.getColumnWidth(),
            viewH - buttonsH // Content area for patterns
        };
        DrawColumn((int)i, state.columns[i]);
        // No gap in portrait mode to ensure 3 tracks fit perfectly (Screen/3)
        float colGap = state.isPortrait ? 0.0f : 10.0f;
        colX += state.getColumnWidth() + colGap;
    }
    
    // Add Column Button (Only in Landscape - Portrait has it in TransportBar)
    if (!state.isPortrait) {
        Rectangle addColBtn = {colX, headerH + 20, 40, (float)state.getPatternHeight()};
        DrawRectangleRec(addColBtn, Color{40, 40, 40, 255});
        DrawTextApp("+", addColBtn.x + 13, addColBtn.y + 30, 30, GRAY);
        
        if (state.isClickAvailable() && !state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), addColBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
             state.consumeClick();
             if (CheckCollisionPointRec(state.getMousePosition(), viewRect)) {
                 // FIX: Correctly initialize PatternColumn with all fields
                 std::string newTrackName = "Track_" + std::to_string(state.columns.size());
                 state.columns.push_back({
                     "Track " + std::to_string(state.columns.size() + 1), // Title
                     std::vector<std::string>(16, ""),                    // PatternNames
                     std::vector<bool>(16, false),                        // SlotSyncEnabled
                     {0, 0, 0, 0},                                        // Bounds
                     0.0f,                                                // ScrollY
                     false,                                               // MixerMode
                     newTrackName,                                        // TrackName
                     1.0f,                                                // Volume
                     0.5f                                                 // Pan
                 });
             }
        }
    }
    
    EndScissorMode();

    // ... (Drag Logic Omitted for brevity, assumed unchanged) ...
    // Note: I am replacing a chunk that ends before DrawColumn implementation, so I need to check where to insert the button drawing logic.
    // Ah, the button drawing is INSIDE DrawColumn which is below.
    // I need to use another replace call for DrawColumn button logic.
    
    // END of main Layout Logic replacement.


    // 3. Global Drop Logic (if Dragging)
    if (state.drag.isDragging) {
        // Draw Ghost
        Vector2 mouse = state.getMousePosition();
        Rectangle ghostRect = { mouse.x + 10, mouse.y + 10, 140, 45 };
        DrawRectangleRec(ghostRect, Color{60, 60, 60, 200});
        DrawRectangleLinesEx(ghostRect, 1, WHITE);
        DrawTextApp(state.drag.patternName.c_str(), ghostRect.x + 10, ghostRect.y + 15, 10, WHITE);

        // Handle Release
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            bool dropped = false;
            // Find Target
            for (size_t c = 0; c < state.columns.size(); ++c) {
                PatternColumn& col = state.columns[c];
                
                // Check if mouse is within column Content Area (approximate)
                if (CheckCollisionPointRec(mouse, col.bounds)) {
                    // Calculate Slot Index based on Scroll
                    float topMargin = 40.0f;
                    float relativeY = mouse.y - (col.bounds.y + topMargin) + col.scrollY;
                    int slotIdx = (int)(relativeY / (state.getPatternHeight() + 5));
                    
                    if (slotIdx >= 0 && slotIdx < (int)col.patternNames.size()) {
                        // VALID DROP TARGET
                        std::string draggedName = state.drag.patternName;
                        int srcCol = state.drag.sourceColumnIndex;
                        int srcSlot = state.drag.sourceSlotIndex;

                        if (!draggedName.empty() && srcCol != -1 && srcSlot != -1) {
                            bool isSelfDrop = ((int)c == srcCol && slotIdx == srcSlot);
                            
                            if (!isSelfDrop) {
                                // Execute Move
                                if (col.patternNames[(size_t)slotIdx].empty()) {
                                    col.patternNames[(size_t)slotIdx] = draggedName;
                                    // FIX: Update track assignment
                                    engine.assignPatternToTrack(draggedName, col.title);
                                    
                                    // Safety check for source (it might have changed?)
                                    if (srcCol >= 0 && srcCol < (int)state.columns.size()) {
                                        if (srcSlot >= 0 && srcSlot < (int)state.columns[(size_t)srcCol].patternNames.size()) {
                                            state.columns[(size_t)srcCol].patternNames[(size_t)srcSlot] = "";
                                        }
                                    }
                                } else {
                                    // Swap
                                    std::string targetName = col.patternNames[(size_t)slotIdx];
                                    col.patternNames[(size_t)slotIdx] = draggedName;
                                    // FIX: Update track for dragged pattern
                                    engine.assignPatternToTrack(draggedName, col.title);
                                    
                                     if (srcCol >= 0 && srcCol < (int)state.columns.size()) {
                                        if (srcSlot >= 0 && srcSlot < (int)state.columns[(size_t)srcCol].patternNames.size()) {
                                            state.columns[(size_t)srcCol].patternNames[(size_t)srcSlot] = targetName;
                                            // FIX: Update track for swapped target
                                            engine.assignPatternToTrack(targetName, state.columns[(size_t)srcCol].title);
                                        }
                                    }
                                }
                                
                                // Fix Active Selection
                                if (state.activePatternSlots.count(srcCol) && state.activePatternSlots[srcCol] == srcSlot) {
                                    state.activePatternSlots.erase(srcCol);
                                }

                                dropped = true;
                            }
                        }
                    } 
                    break; // Found column
                }
            }
            // End Drag
            state.drag.isDragging = false;
            state.drag.isHolding = false;
        }
    }
    // 4. Cleanup Scroll State (Post-processing to ensure click events see drag state)
    if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON) && state.drag.isScrolling) {
        state.drag.isScrolling = false;
        state.drag.scrollColumnIndex = -1;
        state.drag.scrollDirection = 0;
    }
}

void TrackView::DrawColumn(int index, PatternColumn& col) {
    DrawRectangleRec(col.bounds, Color{35, 35, 35, 255});
    
    // 0. Mixer Mode Check
    if (col.mixerMode) {
        state.columns[index].bounds = col.bounds; // Ensure bounds synced
        DrawTrackMixer(col.bounds, col, state, engine);
        return;
    }
    
    // Old column highlighting removed. Cell highlighting handled in cell loop below.
    
    // Content area
    // Content area
    float topMargin = state.isPortrait ? 30.0f : 40.0f;
    float bottomMargin = state.isPortrait ? 0.0f : 40.0f;
    Rectangle contentArea = {
        col.bounds.x,
        col.bounds.y + topMargin,
        col.bounds.width,
        col.bounds.height - topMargin - bottomMargin
    };
    
    // Initial Calc
    float totalContentHeight = (float)col.patternNames.size() * (float)(state.getPatternHeight() + 5);
    
    // Scroll Logic
    if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), col.bounds)) {
        float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.001f) {
            col.scrollY -= wheel * 20.0f; // Smoother scrolling (reduced from 30)
        }
    }
    
    // Header (Title Bar)
    Rectangle headerRect = {col.bounds.x, col.bounds.y, col.bounds.width, 30};
    
    // Draw Window Title Bar
    DrawRectangleRec(headerRect, Color{20, 60, 100, 255}); // Darker Blue Title Bar

    if (state.renamingColumnIndex == index) {
        if (DrawTextInput(headerRect, state.columnRenameBuffer, 63, 1000 + index, state.focusedFieldId) || IsKeyPressed(KEY_ENTER)) {
            col.title = state.columnRenameBuffer;
            state.renamingColumnIndex = -1;
            state.focusedFieldId = -1;
        }
    } else {
        DrawTextApp(col.title.c_str(), headerRect.x + 5, headerRect.y + 4, 18, WHITE);
        
        // Double-click to rename
        if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), headerRect)) {
            static double lastHeaderClickTime = 0;
            static int lastHeaderClickIndex = -1;
            
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                double now = GetTime();
                if (now - lastHeaderClickTime < 0.3 && lastHeaderClickIndex == index) {
                    state.renamingColumnIndex = index;
                    strcpy(state.columnRenameBuffer, col.title.c_str());
                    state.focusedFieldId = 1000 + index;
#if defined(__ANDROID__)
                    FilePicker::showKeyboard();
#endif
                }
                lastHeaderClickTime = now;
                lastHeaderClickIndex = index;
            }
        }
    }
    
    // Button rects defined later in DrawColumn
    
    BeginScissorMode((int)contentArea.x, (int)contentArea.y, (int)contentArea.width, (int)contentArea.height);
    
    float y = contentArea.y - col.scrollY;
    
    // Auto-fill grid to visible height to prevent "popping into existence" or empty space
    int visibleCells = (int)(contentArea.height / (state.getPatternHeight() + 5)) + 2; 
    int minCells = std::max(16, visibleCells);
    if (col.patternNames.size() < (size_t)minCells) {
        col.patternNames.resize((size_t)minCells, "");
        col.slotSyncEnabled.resize((size_t)minCells, true); // Default Sync ON
    }

    // Clamp Scroll (Recalculate after resize)
    totalContentHeight = (float)col.patternNames.size() * (float)(state.getPatternHeight() + 5);
    float maxScroll = std::max(0.0f, totalContentHeight - contentArea.height);
    if (col.scrollY < 0) col.scrollY = 0;
    if (col.scrollY > maxScroll) col.scrollY = maxScroll;
    
    for (size_t i = 0; i < col.patternNames.size(); ++i) {
        float cellPadding = state.isPortrait ? 2.0f : 10.0f;  // Equal padding on both sides
        float cellWidth = col.bounds.width - (cellPadding * 2);
        Rectangle cellRect = {
            col.bounds.x + cellPadding,
            y,
            cellWidth,
            (float)state.getPatternHeight()
        };
        y += state.getPatternHeight() + 1;
        
        bool isSelected = (state.activePatternSlots.count(index) && state.activePatternSlots[index] == (int)i);
        // overlapsButton check removed - buttons are outside content area now
        
        if (state.drag.isDragging && 
            state.drag.sourceColumnIndex == index && 
            state.drag.sourceSlotIndex == (int)i) {
            // Beind dragged source
            DrawRectangleLinesEx(cellRect, 2.0f, DARKGRAY);
        } else if (!state.editor.isOpen) { // BLOCKED IF EDITOR IS OPEN
            if (col.patternNames[i].empty()) {
                // Empty Cell
                DrawRectangleRec(cellRect, Color{30, 30, 30, 255});
                DrawRectangleLinesEx(cellRect, 1, Color{50, 50, 50, 255});
                DrawTextApp("-", cellRect.x + cellRect.width/2 - 5, cellRect.y + cellRect.height/2 - 10, 20, DARKGRAY);
                
                // Allow interaction
                HandlePatternClick(index, (int)i, "", cellRect);
            } else {
                // Occupied Cell
                // Occupied Cell
                
                // Check for instance-specific progress first (Arrangement)
                std::string instanceKey = col.patternNames[i] + "@" + col.trackName;
                int currentStep = engine.getPatternProgress(instanceKey);
                
                // Fallback to generic pattern progress if not found (or error)
                if (currentStep <= 0) {
                    currentStep = engine.getPatternProgress(col.patternNames[i]);
                }
                
                DrawPatternBox(col.patternNames[i], cellRect, isSelected, currentStep);
                
                // Draw sync indicator badge if slot has sync enabled
                if (i < col.slotSyncEnabled.size() && col.slotSyncEnabled[i]) {
                    float badgeSize = 16;
                    Rectangle syncBadge = {cellRect.x + cellRect.width - badgeSize - 2, cellRect.y + 2, badgeSize, badgeSize};
                    DrawRectangleRec(syncBadge, Color{0, 180, 0, 255});
                    DrawTextApp("S", (int)(syncBadge.x + 4), (int)(syncBadge.y + 1), 12, WHITE);
                }

                // Draw arrangement-playing indicator
                if (state.arrangementPlayState.isPlaying) {
                    auto it = state.arrangementPlayState.activeClipsPerTrack.find(col.trackName);
                    if (it != state.arrangementPlayState.activeClipsPerTrack.end() && it->second == col.patternNames[i]) {
                        // This pattern is being played by arrangement - green border
                        DrawRectangleLinesEx(cellRect, 3, GREEN);
                    }
                }                
                // Draw mini step grid inside the pattern box
                Pattern* pat = engine.getPattern(col.patternNames[i]);
                if (pat) {
                    // Fill the area below the title bar (approx 30px height)
                    float titleH = 30.0f;
                    Rectangle gridRect = {
                        cellRect.x, 
                        cellRect.y + titleH, 
                        cellRect.width, 
                        cellRect.height - titleH
                    };
                    DrawStepGrid(gridRect, *pat, currentStep, state);
                }

                // Drag hold animation removed per request
                /*
                if (isHoldingThis) {
                    float progress = (float)(GetTime() - state.drag.holdStartTime) / 0.225f;
                    if (progress > 1.0f) progress = 1.0f;
                    DrawRectangleLinesEx(cellRect, 1, YELLOW);
                    DrawRectangle(cellRect.x, cellRect.y + cellRect.height - 5, cellRect.width * progress, 5, YELLOW);
                }
                */
                
                if (state.isShiftMode && !state.shiftEditingPatternName.empty() && 
                    col.patternNames[i] == state.shiftEditingPatternName && !isSelected) {
                    DrawRectangleLinesEx(cellRect, 1, YELLOW);
                }
                
                HandlePatternClick(index, (int)i, col.patternNames[i], cellRect);
            }
        }
        
        // NEW WORKFLOW HIGHLIGHTING: Source Selection
        if (state.trackClipboard.isSelectingSource && !state.editor.isOpen) {
             // Highlight pattern to be copied (must be occupied)
             if (!col.patternNames[i].empty() && CheckCollisionPointRec(state.getMousePosition(), cellRect)) {
                 DrawRectangleLinesEx(cellRect, 2, ORANGE);
                 DrawRectangle(cellRect.x, cellRect.y, cellRect.width, cellRect.height, Color{255, 161, 0, 50}); // translucent orange
             }
        }

        // PASTE MODE HIGHLIGHTING & INTERACTION
        if (state.trackClipboard.isPasting && !state.editor.isOpen) {
            // Check if slot is empty
            bool isEmpty = col.patternNames[i].empty();
            
            // Highlight specific cell only if empty
           if (isEmpty && CheckCollisionPointRec(state.getMousePosition(), cellRect)) {
                DrawRectangleLinesEx(cellRect, 2, MAGENTA);
                DrawRectangle(cellRect.x, cellRect.y, cellRect.width, cellRect.height, Color{255, 0, 255, 50});
                
                // Handle Paste Click directly here or via HandlePatternClick?
                // HandlePatternClick handles mechanics, let's delegate or do it here.
                // Doing it here is cleaner for "empty" slots since HandlePatternClick is called for both.
                // Actually HandlePatternClick IS called for empty slots too (line 266).
                // So let's rely on HandlePatternClick to trigger the paste action to avoid double-handling.
           }
        }
        
        // Draw selection for EMPTY cells too if selected
        if (state.activePatternSlots.count(index) && state.activePatternSlots[index] == (int)i) {
             DrawRectangleLinesEx(cellRect, 1.0f, YELLOW); 
        }

        // Drag Destination Logic - VISUAL OVERLAY (Draw LAST to be visible over content)
        if (state.drag.isDragging && CheckCollisionPointRec(state.getMousePosition(), cellRect)) {
            // Visible Indicator
            DrawRectangle(cellRect.x, cellRect.y, cellRect.width, cellRect.height, Color{0, 255, 0, 60}); // Brighter Green Highlight
            DrawRectangleLinesEx(cellRect, 3.0f, GREEN); // Thicker Border
        }
    }
    
    EndScissorMode();
    
    // Note: Vertical scrollbar removed - scrolling still works via touch drag and mouse wheel

    // --- Column Buttons ---
    float btnStart_y = col.bounds.y + col.bounds.height;
    
    // Define Rects based on mode
    Rectangle addRect, delRect, mixRect;
    
    if (state.isPortrait) {
        // Stacked: + - Row, Mixer Row
        // Dynamic Height Calculation to fill available space
        float footerY = (float)state.getScreenHeight() - (float)state.getFooterHeight();
        float availableH = footerY - btnStart_y;
        if (availableH < 80) availableH = 80; // Safety min
        
        float gap = 5.0f;
        float rowH = (availableH - gap) / 2.0f;
        
        // Row 1: + -
        float halfW = (col.bounds.width - gap) / 2.0f;
        addRect = {col.bounds.x, btnStart_y, halfW, rowH};
        delRect = {col.bounds.x + halfW + gap, btnStart_y, halfW, rowH};
        
        // Row 2: Mixer
        mixRect = {col.bounds.x, btnStart_y + rowH + gap, col.bounds.width, rowH};
    } else {
        // Landscape: Inline
        float btnH = 35.0f;
        addRect = {col.bounds.x + 5, btnStart_y, 40, btnH};
        delRect = {addRect.x + 45, btnStart_y, 40, btnH};
        mixRect = {delRect.x + 45, btnStart_y, col.bounds.width - 95, btnH};
    }
    
    // 1. Add Button (+)
    DrawRectangleRec(addRect, Color{50, 50, 50, 255});
    DrawRectangleLinesEx(addRect, 1.0f, Color{100, 100, 100, 255}); // Border
    DrawTextApp("+", addRect.x + (addRect.width - 14)/2, addRect.y + (addRect.height - 24)/2, 24, WHITE);
    
    if (state.isClickAvailable() && !state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), addRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick();
        
        // Add Logic (Inlined from HandleAddPattern)
        int targetSlot = -1;
        if (state.activePatternSlots.count(index)) {
            int selectedSlot = state.activePatternSlots[index];
            if (selectedSlot >= 0 && selectedSlot < (int)col.patternNames.size()) {
                if (col.patternNames[(size_t)selectedSlot].empty()) {
                    targetSlot = selectedSlot;
                }
            }
        }
        
        if (targetSlot != -1) {
            Pattern p;
            std::string candidateName;
            do {
                candidateName = "Pat" + std::to_string(state.patternIdCounter++);
            } while (engine.getPattern(candidateName) != nullptr);
            
            p.name = candidateName;
            p.bpm = state.bpm;
            engine.addPattern(p);
            engine.assignPatternToTrack(p.name, col.title);
            
            col.patternNames[(size_t)targetSlot] = p.name;
            if (targetSlot >= (int)col.slotSyncEnabled.size()) {
                col.slotSyncEnabled.resize((size_t)(targetSlot + 1), false);
            }
            col.slotSyncEnabled[(size_t)targetSlot] = true;
        }
    }
    
    // 2. Delete Button (-)
    DrawRectangleRec(delRect, Color{80, 20, 20, 255});
    DrawRectangleLinesEx(delRect, 1.0f, Color{100, 100, 100, 255}); // Border
    DrawTextApp("-", delRect.x + (delRect.width - 14)/2, delRect.y + (delRect.height - 24)/2, 24, WHITE);
    
    if (state.isClickAvailable() && !state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), delRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick();
        // Delete Logic
        if (state.activePatternSlots.count(index)) {
            int selectedSlot = state.activePatternSlots[index];
            if (selectedSlot >= 0 && selectedSlot < (int)col.patternNames.size()) {
                col.patternNames[(size_t)selectedSlot] = "";
                state.activePatternSlots.erase(index);
            }
        }
    }
    
    // 3. Mixer Button
    DrawRectangleRec(mixRect, Color{30, 30, 40, 255});
    DrawRectangleLinesEx(mixRect, 1.0f, Color{100, 100, 100, 255}); // Border
    const char* mixText = "MIXER";
    int fontSize = 20;
    int textW = MeasureText(mixText, fontSize);
    DrawTextApp(mixText, mixRect.x + (mixRect.width - textW)/2, mixRect.y + (mixRect.height - fontSize)/2, fontSize, GRAY);
    
    if (state.isClickAvailable() && !state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), mixRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state.consumeClick();
        col.mixerMode = !col.mixerMode; // Toggle
    }
    
    // Note: External HandleAddPattern call removed
}

void TrackView::HandlePatternClick(int colIndex, int slotIndex, const std::string& patternName, Rectangle cellRect) {
    // patternName can be empty now
    
    // PatternColumn& col = state.columns[colIndex]; // Unused variable warning? Used in logic
    PatternColumn& col = state.columns[(size_t)colIndex];
    Rectangle contentArea = {
        col.bounds.x,
        col.bounds.y + 40.0f,
        col.bounds.width,
        col.bounds.height - 80.0f
    };
    
    // Check Collision using PASSED cellRect (Matches Drawing)
    if (!state.editor.isOpen && CheckCollisionPointRec(state.getMousePosition(), contentArea) && CheckCollisionPointRec(state.getMousePosition(), cellRect)) {
        // LOGD("HandlePatternClick HIT: Col %d Slot %d. ActiveID: %s", colIndex, slotIndex, state.drag.activeControlId.c_str());
        static double lastPatternClickTime = 0;
        static std::string lastPatternClickName = "";

        // 1. Handle Release (Selection, Edit, Paste) - Only if NOT dragging/scrolling
        // 1. Handle Release (Selection, Edit, Paste) - Only if NOT dragging/scrolling
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            // STRICT CLICK OWNERSHIP: Only act if WE started the interaction
            std::string cellId = "GridCell_" + std::to_string(colIndex) + "_" + std::to_string(slotIndex);
            
            // Allow if we are the active control OR if no control is active (legacy fallback, but locking is better)
            // Actually, if "Add Track" was clicked, it consumed the click but probably didn't set a lock ID?
            // "Add Track" consumes click. So we shouldn't have set lock ID.
            // But here we are in Release. "Add Track" happened frames ago on Press.
            // If "Add Track" didn't set lock, activeControlId is empty.
            // If we rely on activeControlId match, we strictly require Press to have happened here.
            
            bool isMyInteraction = (state.drag.activeControlId == cellId);
            
            bool wasDragging = state.drag.isDragging || state.drag.scrollDirection != 0;

            if (isMyInteraction && !wasDragging) {
                LOGD("Pattern Clicked: Col %d Slot %d. Processing Select/Edit.", colIndex, slotIndex);
                // ... (Logic remains identical, just wrapped in isMyInteraction)
                // NEW WORKFLOW: Source Selection (Priority over everything else)
                if (state.trackClipboard.isSelectingSource) {
                    state.trackClipboard.patternName = patternName;
                    state.trackClipboard.hasData = true;
                    // FIX: Capture Sync Status from Source
                    if (state.columns[(size_t)colIndex].slotSyncEnabled.size() > (size_t)slotIndex) {
                        state.trackClipboard.syncEnabled = state.columns[(size_t)colIndex].slotSyncEnabled[(size_t)slotIndex];
                    } else {
                        state.trackClipboard.syncEnabled = false; // Default
                    }
                    
                    state.trackClipboard.isSelectingSource = false;
                    state.trackClipboard.isPasting = true; // Enter paste mode immediately
                    // Global Unlock done by Draw()
                    return;
                }
                
                // NEW WORKFLOW: Pasting (Priority over selection/edit)
                if (state.trackClipboard.isPasting && state.trackClipboard.hasData) {
                    // Check if target slot is empty
                    if (state.columns[(size_t)colIndex].patternNames[(size_t)slotIndex].empty()) {
                        // Perform Paste
                             Pattern* origPat = engine.getPattern(state.trackClipboard.patternName);
                             if (origPat) {
                                Pattern copy = *origPat;
                                copy.name = origPat->name + "_" + std::to_string(state.patternIdCounter++);
                                if (copy.samplePath != "") engine.loadSample(copy);
                                engine.addPattern(copy);
                                
                                // FIX: Register track assignment
                                engine.assignPatternToTrack(copy.name, state.columns[(size_t)colIndex].title);
                                
                                // FIX: Apply Copied Sync Mode status
                                if (state.columns[(size_t)colIndex].slotSyncEnabled.size() > (size_t)slotIndex) {
                                     state.columns[(size_t)colIndex].slotSyncEnabled[(size_t)slotIndex] = state.trackClipboard.syncEnabled;
                                }
                                
                                // Assign to slot
                                state.columns[(size_t)colIndex].patternNames[(size_t)slotIndex] = copy.name;
                             }
                    }
                    return;
                }
                
                // Header Click Logic (Single Click to Edit)
                // Replaces double-click logic. Only applies to occupied patterns.
                if (!patternName.empty()) {
                    Rectangle headerRect = {cellRect.x, cellRect.y, cellRect.width, 22};
                    if (CheckCollisionPointRec(state.getMousePosition(), headerRect)) {
                            Pattern* p = engine.getPattern(patternName);
                            if (p) {
                                state.editor.currentPattern = *p;
                                state.editor.isOpen = true;
                                state.editor.justOpened = true;
                                state.editor.showFileBrowser = false;
                                strcpy(state.editor.nameBuffer, p->name.c_str());
                                strcpy(state.editor.originalName, p->name.c_str());
                                strcpy(state.editor.samplePathBuffer, p->samplePath.c_str());
                                sprintf(state.editor.bpmBuffer, "%d", p->bpm);
                                sprintf(state.editor.stepsBuffer, "%d", p->steps);
                                sprintf(state.editor.syncBaseBuffer, "%d", p->syncBase);
                                for (int s = 0; s < 64; ++s) state.editor.stepStates[s] = p->shouldTriggerAt(s+1);
                            }
                            return; // Create separation: Header = Edit, Body = Select
                    }
                }
                
                // Double Click check removed (superseded by header click)
                double now = GetTime();
                lastPatternClickTime = now;
                lastPatternClickName = patternName;

                // Shift mode - edit without changing selection
                if (state.isShiftMode && state.isLiveEditMode) {
                    Pattern* p = engine.getPattern(patternName);
                    if (p) {
                        state.shiftEditingPatternName = p->name;
                        state.editor.currentPattern = *p;
                        state.editor.isOpen = true;
                        state.editor.justOpened = true;
                        state.editor.sourceColumnIndex = colIndex; // Set Source
                        state.editor.sourceSlotIndex = -1; // Pattern click doesn't map to single slot easily here? 
                        // Actually HandlePatternClick has slotIndex.
                        // Wait, shift edit uses patternName lookup. But we are in HandlePatternClick which HAS slotIndex.
                        // So we should use it.
                        state.editor.sourceSlotIndex = slotIndex;
                        strcpy(state.editor.nameBuffer, p->name.c_str());
                        strcpy(state.editor.originalName, p->name.c_str());
                        strcpy(state.editor.samplePathBuffer, p->samplePath.c_str());
                        sprintf(state.editor.bpmBuffer, "%d", p->bpm);
                        sprintf(state.editor.stepsBuffer, "%d", p->steps);
                        sprintf(state.editor.syncBaseBuffer, "%d", p->syncBase);
                        for (int s = 0; s < 64; ++s) state.editor.stepStates[s] = p->shouldTriggerAt(s+1);
                    }
                    return;
                }
                
                // Selection Logic - Allow one pattern per column by default
                bool isMulti = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) || IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
                
                // Check if this pattern is already selected in this column
                bool wasSelected = (state.activePatternSlots.count(colIndex) && state.activePatternSlots[colIndex] == slotIndex);
                
                if (!isMulti) {
                    // Normal click: toggle selection in this column
                    if (wasSelected) {
                        // Deselect if clicking the same pattern again
                        state.activePatternSlots.erase(colIndex);
                    } else {
                        // Select this pattern (keeps selections in other columns)
                        state.activePatternSlots[colIndex] = slotIndex;
                    }
                } else {
                    // Shift/Ctrl: Toggle behavior for multi-select within same column
                    if (wasSelected) {
                        state.activePatternSlots.erase(colIndex);
                    } else {
                        state.activePatternSlots[colIndex] = slotIndex;
                    }
                }
                
                // Sync with audio engine
                // Sync with audio engine
                // Always update track assignment (critical for Always Sync)
                if (colIndex >= 0 && colIndex < (int)state.columns.size()) {
                    PatternColumn& pc = state.columns[(size_t)colIndex];
                    if (slotIndex >= 0 && slotIndex < (int)pc.patternNames.size()) {
                        std::string pname = pc.patternNames[(size_t)slotIndex];
                        if (!pname.empty()) {
                                engine.assignPatternToTrack(pname, pc.trackName);
                        }
                    }
                }

                if (engine.isPlaying()) {
                    // Check if clicked slot has sync enabled
                    bool shouldQueue = false;
                    std::string queueTrackName = "";
                    std::string queuePatternName = "";
                    
                    if (colIndex >= 0 && colIndex < (int)state.columns.size()) {
                        PatternColumn& pc = state.columns[(size_t)colIndex];
                        if (slotIndex >= 0 && slotIndex < (int)pc.patternNames.size()) {
                            // Check if this slot has sync enabled
                            if (slotIndex < (int)pc.slotSyncEnabled.size() && pc.slotSyncEnabled[(size_t)slotIndex]) {
                                std::string pname = pc.patternNames[(size_t)slotIndex];
                                if (!pname.empty()) {
                                    shouldQueue = true;
                                    queueTrackName = pc.trackName;
                                    queuePatternName = pname;
                                }
                            }
                        }
                    }
                    
                    if (shouldQueue) {
                        // Per-slot sync: queue pattern to switch at end of current
                        engine.queuePatternSwitch(queueTrackName, queuePatternName);
                    } else {
                        // Immediate switch
                        std::vector<std::pair<std::string, std::string>> allActive;
                        for (auto& pair : state.activePatternSlots) {
                            int cIdx = pair.first;
                            int sIdx = pair.second;
                            if (cIdx >= 0 && cIdx < (int)state.columns.size()) {
                                PatternColumn& pc = state.columns[(size_t)cIdx];
                                if (sIdx >= 0 && sIdx < (int)pc.patternNames.size()) {
                                    std::string pname = pc.patternNames[(size_t)sIdx];
                                    if (!pname.empty()) {
                                        allActive.push_back({pname, pc.trackName});
                                    }
                                }
                            }
                        }
                        engine.updateActivePatterns(allActive);
                    }
                }
            }
        }

        // 2. Initialize Drag/Scroll on Press
        if (state.isClickAvailable() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            state.consumeClick();
            
            // STRICT CLICK OWNERSHIP: Claim the lock
            std::string cellId = "GridCell_" + std::to_string(colIndex) + "_" + std::to_string(slotIndex);
            state.drag.activeControlId = cellId;
            
            // Start hold for drag OR scroll
            if (!patternName.empty()) {
                state.drag.isDragging = false;
                state.drag.isHolding = true;
                state.drag.holdStartTime = GetTime();
                state.drag.initialClickPos = state.getMousePosition();
                state.drag.sourceColumnIndex = colIndex;
                state.drag.sourceSlotIndex = slotIndex;
                state.drag.patternName = patternName;
                // Also start scroll mode - will switch to drag if hold long enough
                state.drag.isScrolling = true;
                state.drag.scrollColumnIndex = colIndex;
                state.drag.lastMousePos = state.getMousePosition();
                state.drag.scrollStartPos = state.getMousePosition();
                state.drag.scrollDirection = 0;  // Pending
            } else {
                // Empty cell -> Start Scroll
                state.drag.isScrolling = true;
                state.drag.scrollColumnIndex = colIndex;
                state.drag.lastMousePos = state.getMousePosition();
                state.drag.scrollStartPos = state.getMousePosition();
                state.drag.scrollDirection = 0;  // Pending
                state.drag.isHolding = false;
                state.drag.isDragging = false;
            }
        }
    }
}

// HandleAddPattern definition removed (inlined)
} // namespace gui
