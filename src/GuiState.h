#pragma once

#include "AudioEngine.h"
#include <map>
#include <vector>
#include <string>
#include <raylib.h>

struct PatternColumn {
    std::string title;
    std::vector<std::string> patternNames;
    Rectangle bounds;
    float scrollY = 0.0f;
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
    char bpmBuffer[8] = {0};
    char stepsBuffer[4] = {0};
    
    // File Browser
    bool showFileBrowser = false;
    std::vector<std::string> fileList;
    
    // Melodic Mode State
    bool showMelodicControls = false;
    int selectedOctave = 4;

    int selectedNote = 0; // 0=C, 1=C#, etc. (Offset within octave)
    float currentVelocity = 1.0f; // [0.0 - 1.0]
    bool isDraggingVelocity = false;
    
    // Scrolling
    float scrollOffsetY = 0.0f;
    float contentHeight = 600.0f; // Estimated content height
    

    
    // Track which field is being edited
    int focusedFieldId = -1; // 0: Name, 1: Sample, 2: BPM, 3: Steps

    // FX Mode State
    bool showFxControls = false;
    int selectedStep = -1; // 0-63
    int currentFxType = 0; // 0=None, 1=CutOff
    int selectedAvailableFxId = -1;
    int selectedAppliedFxId = -1;
};

struct GuiState {
    std::vector<PatternColumn> columns;
    std::map<int, std::string> activePatterns; // Column Index -> Pattern Name
    
    DragState drag;
    PatternEditorState editor;
    
    // Column Renaming
    int renamingColumnIndex = -1;
    char columnRenameBuffer[64] = {0};
    int focusedFieldId = -1; // Global focus tracker
    
    // Main View Scroll
    float mainScrollX = 0.0f;
    float mainContentWidth = 0.0f;
    
    // Transport state mirroring AudioEngine
    bool isPlaying = false;
    int bpm = 120;
    char globalBpmBuffer[8] = "120";
    
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
    
    int patternIdCounter = 1;
};
