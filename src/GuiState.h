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
    
    // Mixer Mode State
    bool mixerMode = false;
    
    // Track/Bus Mapping
    std::string trackName;  // Name of the audio bus this column controls
    
    // These control the bus, not individual patterns
    float volume = 1.0f;
    float pan = 0.5f;
};

struct DragState {
    bool isDragging = false;
    std::string patternName;
    Vector2 startPos;
    Vector2 currentPos;
    int sourceColumnIndex = -1;
    int sourceSlotIndex = -1; // Which slot in the column is being dragged
    
    // Hold to drag logic
    double holdStartTime = 0.0;
    bool isHolding = false;
    Vector2 initialClickPos;
};

struct StepClipboard {
    bool hasData = false;
    bool active = false;
    
    bool hasPitch = false;
    int pitch = 0;
    
    bool hasVelocity = false;
    float velocity = 1.0f;
    
    std::vector<int> fxList;
    std::map<int, float> fxParams;
    
    // Modal Interaction
    bool isCopyMode = false;
    bool isPasteMode = false;
    bool isEditMode = false;
};

struct PatternEditorState {
    bool isOpen = false;
    Pattern currentPattern;
    
    StepClipboard clipboard; // Clipboard for step data
    
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
    int selectedAppliedFxId = -1;
    int selectedAvailableFxId = -1;
    
    // Slicer Mode State
    bool showSlicerControls = false;
    int selectedSliceIndex = 0;
    bool slicerCutoffEnabled = false;
    float waveformZoom = 1.0f;    // 1.0 = fit to view, >1 = zoomed in
    float waveformScrollX = 0.0f; // 0.0-1.0 scroll position (normalized)
};

// Track-level pattern clipboard (for copy/paste in track view)
struct TrackClipboard {
    bool hasData = false;
    std::string patternName;
    bool isCopyMode = false;
    bool isPasteMode = false;
};

enum class PopupType { None, Main, Audio, Project };

struct RecordingState {
    bool showControls = false;
    bool isRecording = false;
    bool recordStems = false; // false = Whole, true = Stems
    char filenameBuffer[64] = "recording";
};

struct SettingsState {
    PopupType activePopup = PopupType::None;
    bool showSettingsMenu = false; // Legacy/Global toggle used by main loop if any
    std::vector<std::string> availableOutputDevices;
    std::string currentDevice;
    int selectedDeviceIndex = 0;
    bool isSwitchingDevice = false;  // Indicates device switch in progress
};

struct GuiState {
    std::vector<PatternColumn> columns;
    std::map<int, int> activePatternSlots; // Column Index -> Slot Index (which pattern in that column is selected)
    
    DragState drag;
    PatternEditorState editor;
    TrackClipboard trackClipboard; // For copy/paste patterns in track view
    SettingsState settings; // Audio device settings
    RecordingState recorder;
    
    // Column Renaming
    int renamingColumnIndex = -1;
    char columnRenameBuffer[64] = {0};
    int focusedFieldId = -1; // Global focus tracker
    
    // Main View Scroll
    float mainScrollX = 0.0f;
    float mainContentWidth = 0.0f;
    
    // Transport state mirroring AudioEngine
    bool isPlaying = false;
    bool isLiveEditMode = false; // Persistent Edit mode (stays on until Edit button is clicked off)
    bool isShiftMode = false; // When ON, clicking patterns opens them for editing without changing selection
    std::string shiftEditingPatternName; // The pattern currently being edited via Shift mode
    int bpm = 120;
    char globalBpmBuffer[8] = "120";
    PatternChain activeChain; // Current Chain or Song Sequence
    
    // UI Layout
    const int COLUMN_WIDTH = 220; // Increased from 160
    const int PATTERN_HEIGHT = 90;
    const int HEADER_HEIGHT = 60;
    const int FOOTER_HEIGHT = 60;
    
    void initDemo() {
     // Add some default columns with track names
        columns.push_back({"Drums", std::vector<std::string>(16, ""), {0, 0, 0, 0}, 0.0f, false, "Track_0", 1.0f, 0.5f});
        columns.push_back({"Bass", std::vector<std::string>(16, ""), {0, 0, 0, 0}, 0.0f, false, "Track_1", 1.0f, 0.5f});
        columns.push_back({"Leads", std::vector<std::string>(16, ""), {0, 0, 0, 0}, 0.0f, false, "Track_2", 1.0f, 0.5f});
    }
    
    int patternIdCounter = 1;
};
