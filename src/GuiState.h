#pragma once

#include "AudioEngine.h"
#include <vector>
#include <string>
#include <raylib.h>

struct PatternColumn {
    std::string title;
    std::vector<std::string> patternNames;
    Rectangle bounds;
};

struct DragState {
    bool isDragging = false;
    std::string patternName;
    Vector2 startPos;
    Vector2 currentPos;
    int sourceColumnIndex = -1;
    
    // Hold to drag logic
    double holdStartTime = 0.0;
    bool isHolding = false;
    Vector2 initialClickPos;
};

struct PatternEditorState {
    bool isOpen = false;
    Pattern currentPattern;
    bool stepStates[64] = {false};
    char nameBuffer[64] = {0};
    char originalName[64] = {0}; // Track original name for renaming
    char samplePathBuffer[256] = {0};
    char samplePathBuffer[256] = {0};
    char bpmBuffer[8] = {0};
    char stepsBuffer[4] = {0};
    
    // Track which field is being edited
    int focusedFieldId = -1; // 0: Name, 1: Sample, 2: BPM, 3: Steps
};

struct GuiState {
    std::vector<PatternColumn> columns;
    std::vector<std::string> selectedPatterns;
    
    DragState drag;
    PatternEditorState editor;
    
    // Transport state mirroring AudioEngine
    bool isPlaying = false;
    int bpm = 120;
    
    // UI Layout
    const int COLUMN_WIDTH = 160;
    const int PATTERN_HEIGHT = 90;
    const int HEADER_HEIGHT = 60;
    const int FOOTER_HEIGHT = 60;
    
    void initDemo() {
        // Add some default columns
        columns.push_back({"Drums", {}, {0, 0, 0, 0}});
        columns.push_back({"Bass", {}, {0, 0, 0, 0}});
        columns.push_back({"Leads", {}, {0, 0, 0, 0}});
    }
};
