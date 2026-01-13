#pragma once

#include "AudioEngine.h"
#include <map>
#include <vector>
#include <string>
#include <raylib.h>

struct PatternColumn {
    std::string title;
    std::vector<std::string> patternNames;
    std::vector<bool> slotSyncEnabled;  // Per-slot sync flag - if true, queues instead of instant switch
    Rectangle bounds;
    float scrollY = 0.0f;
    
    // Mixer Mode State
    bool mixerMode = false;
    
    // Track/Bus Mapping
    std::string trackName;  // Name of the audio bus this column controls
    
    // These control the bus, not individual patterns
    float volume = 1.0f;
    float pan = 0.5f;
    
    // UI State
    float mixerScrollY = 0.0f;
    float mixerContentHeight = 0.0f;
    bool isDragging = false;
    float dragStartY = 0.0f;
    float dragStartScroll = 0.0f;
    
    // FX Selection (0 = Vol/Pan, 1+ = Insert Slots)
    int selectedFXSlot = 0;
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
    
    // Touch Scroll with direction detection
    bool isScrolling = false;
    int scrollColumnIndex = -1;  // For vertical scroll
    Vector2 lastMousePos = {0,0};
    Vector2 scrollStartPos = {0,0};  // Where scroll started
    int scrollDirection = 0;  // 0=pending, 1=vertical, 2=horizontal
    
    // Scrollbar Interaction
    int scrollbarDraggingColumn = -1;
    float scrollbarClickOffsetY = 0.0f;
    
    // Generic UI Control Locking
    std::string activeControlId = ""; 
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
    bool justOpened = false; // Set true when opened, cleared after first draw to prevent click-through
    Pattern currentPattern;
    
    StepClipboard clipboard; // Clipboard for step data
    
    bool stepStates[64] = {false};
    char nameBuffer[64] = {0};
    char originalName[64] = {0}; // Track original name for renaming
    char samplePathBuffer[256] = {0};
    char bpmBuffer[8] = {0};
    char stepsBuffer[4] = {0};
    char syncBaseBuffer[4] = {0};  // Sync base for polyrhythm timing
    
    // File Browser
    bool showFileBrowser = false;
    std::string currentPath;
    std::vector<std::string> fileList;
    std::vector<std::string> dirList; // Separate dirs vs files
    float browserScrollY = 0.0f;
    char selectedFileBuffer[256] = {0};
    
    // Melodic Mode State
    bool showMelodicControls = false;
    int selectedOctave = 4;

    int selectedNote = 0; // 0=C, 1=C#, etc. (Offset within octave)
    float currentVelocity = 1.0f; // [0.0 - 1.0]
    bool isDraggingVelocity = false;
    
    // Scrolling with touch drag support
    float scrollOffsetY = 0.0f;
    float contentHeight = 600.0f; // Estimated content height
    bool editorDragging = false;
    float editorDragStartY = 0.0f;
    float editorDragStartScrollY = 0.0f;
    

    
    // Track which field is being edited
    int focusedFieldId = -1; // 0: Name, 1: Sample, 2: BPM, 3: Steps
    
    // Preview playback state
    bool isPreviewing = false;

    // FX Mode State
    bool showFxControls = false;
    int selectedStep = -1; // 0-63
    int currentFxType = 0; // 0=None, 1=CutOff
    int selectedAppliedFxId = -1;
    int selectedAvailableFxId = -1;
    float fxAvailableScrollY = 0.0f;
    float fxAppliedScrollY = 0.0f;
    bool fxAvailDragging = false;
    bool fxAppliedDragging = false;
    float fxDragStartY = 0.0f;
    float fxDragStartScrollY = 0.0f;
    
    // Slicer Mode State
    bool showSlicerControls = false;
    int selectedSliceIndex = 0;
    bool slicerCutoffEnabled = false;
    float waveformZoom = 1.0f;    // 1.0 = fit to view, >1 = zoomed in
    float waveformScrollX = 0.0f; // 0.0-1.0 scroll position (normalized)
    bool slicerPlayModeEnabled = false; // Toggle for preview/record mode
    
    // Scroll Event Handling
    bool scrollConsumed = false; // Flag to indicate if a child widget consumed the scroll event
};

// Track-level pattern clipboard (for copy/paste in track view)
struct TrackClipboard {
    bool hasData = false;
    std::string patternName;
    
    // New Workflow State
    bool isSelectingSource = false; // Step 1: User clicked Copy, waiting for source click
    bool isPasting = false;         // Step 2: User selected source, allowing multi-paste
};

enum class PopupType { None, Main, Audio, Project };

struct RecordingState {
    // Mode Selection
    bool showModeSelection = false;
    bool recordStems = false;
    
    // Armed/Recording State
    bool isArmed = false;      // Waiting for Play to start recording
    bool isRecording = false;  // Actively recording
    
    // Review UI
    bool showReview = false;
    bool justOpenedReview = false;  // Prevent click-through on first frame
    char filenameBuffer[64] = "recording";
    
    // Preview Playback in Review
    bool isPreviewing = false;
    int64_t previewPosition = 0;
    std::string previewingStem = ""; // Empty = master
    
    // Legacy (for transport bar compatibility)
    bool showControls = false;
};

struct SettingsState {
    PopupType activePopup = PopupType::None;
    bool showSettingsMenu = false; // Legacy/Global toggle used by main loop if any
    std::vector<std::string> availableOutputDevices;
    std::string currentDevice;
    int selectedDeviceIndex = 0;
    bool isSwitchingDevice = false;  // Indicates device switch in progress
};

struct ProjectBrowserState {
    bool isOpen = false;
    bool isSaveMode = false; // true = save, false = load
    std::string currentPath;
    std::vector<std::string> fileList; // .json files
    std::vector<std::string> dirList;
    float scrollY = 0.0f;
    char filenameBuffer[64] = "project"; // For save mode
    char selectedFile[256] = {0}; // For load mode
};

struct GuiState {
    std::vector<PatternColumn> columns;
    std::map<int, int> activePatternSlots; // Column Index -> Slot Index (which pattern in that column is selected)
    
    DragState drag;
    PatternEditorState editor;
    TrackClipboard trackClipboard; // For copy/paste patterns in track view
    SettingsState settings; // Audio device settings
    RecordingState recorder;
    ProjectBrowserState projectBrowser; // For project save/load file browser
    
    // Column Renaming
    int renamingColumnIndex = -1;
    char columnRenameBuffer[64] = {0};
    int focusedFieldId = -1; // Global focus tracker
    
    // Main View Scroll with touch drag support
    float mainScrollX = 0.0f;
    float mainContentWidth = 0.0f;
    bool mainDraggingX = false;
    float mainDragStartX = 0.0f;
    float mainDragStartScrollX = 0.0f;
    
    // Transport state mirroring AudioEngine
    bool isPlaying = false;
    bool isLiveEditMode = false; // Persistent Edit mode (stays on until Edit button is clicked off)
    bool isShiftMode = false; // When ON, clicking patterns opens them for editing without changing selection
    std::string shiftEditingPatternName; // The pattern currently being edited via Shift mode
    int bpm = 120;
    char globalBpmBuffer[8] = "120";
    PatternChain activeChain; // Current Chain or Song Sequence
    
    // UI Layout
    const int COLUMN_WIDTH = 250; // Increased to 250
    const int PATTERN_HEIGHT = 90;
    const int HEADER_HEIGHT = 60;
    const int FOOTER_HEIGHT = 60;
    
    // Virtual screen dimensions (for render-texture scaling on mobile)
    int virtualWidth = 960;
    int virtualHeight = 540;
    Vector2 virtualMouse = {0, 0};  // Transformed mouse position
    float uiScale = 1.0f;           // Current UI scale factor
    
    // Global Click Consumption (prevents click-through)
    bool clickConsumed = false;
    
    // Helper methods for consistent virtual screen/mouse access
    int getScreenWidth() const { return virtualWidth; }
    int getScreenHeight() const { return virtualHeight; }
    Vector2 getMousePosition() const { return virtualMouse; }
    
    // Click consumption helpers - call consumeClick() when handling a click
    // in overlay/modal UI. Other components check isClickAvailable() before processing.
    void consumeClick() { clickConsumed = true; }
    bool isClickAvailable() const { return !clickConsumed; }
    void resetClickState() { clickConsumed = false; }
    
    void initDemo() {
     // Add some default columns with track names
        columns.push_back({"Drums", std::vector<std::string>(16, ""), std::vector<bool>(16, false), {0, 0, 0, 0}, 0.0f, false, "Track_0", 1.0f, 0.5f});
        columns.push_back({"Bass", std::vector<std::string>(16, ""), std::vector<bool>(16, false), {0, 0, 0, 0}, 0.0f, false, "Track_1", 1.0f, 0.5f});
        columns.push_back({"Leads", std::vector<std::string>(16, ""), std::vector<bool>(16, false), {0, 0, 0, 0}, 0.0f, false, "Track_2", 1.0f, 0.5f});
    }
    
    int patternIdCounter = 1;
};
