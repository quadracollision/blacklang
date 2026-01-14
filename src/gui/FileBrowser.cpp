#include "FileBrowser.h"
#include "Widgets.h"
#include "../FilePicker.h"
#include "../ProjectFile.h"
#include <filesystem>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cstring>

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#if defined(__ANDROID__)
#include <jni.h>
#include <android/log.h>
#include <android_native_app_glue.h>

extern "C" struct android_app* GetAndroidApp();

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "FileBrowser", __VA_ARGS__)
#else
#define LOGD(...)
#endif

namespace fs = std::filesystem;

namespace gui {
    // Old implementation removed
}

namespace FileBrowser {

// Helper to get consistent root on Android
static std::string GetRootPath() {
#if defined(__ANDROID__)
    // Use App-Specific Entry for guaranteed write access (Bypassing Scoped Storage)
    std::string root = "/storage/emulated/0/Android/data/com.quadracollision.blacklang/files/Projects";
    
    std::error_code ec;
    if (!fs::exists(root)) {
        fs::create_directories(root, ec);
    }
    
    if (ec) {
        LOGD("Failed to create project root: %s", ec.message().c_str());
        // Fallback to internal data dir if external fails (extremely rare)
        root = "/data/data/com.quadracollision.blacklang/files/Projects"; 
    }
    return root;
#else
    return fs::current_path().string();
#endif
}

void Refresh(GuiState& state) {
    state.editor.fileList.clear();
    state.editor.dirList.clear();
    state.editor.browserScrollY = 0; // Reset scroll on refresh
    
    std::string path = state.editor.currentPath;
    LOGD("FileBrowser::Refresh called. Path: '%s'", path.c_str());
    
    if (path.empty()) {
        LOGD("FileBrowser::Refresh - Path is empty!");
        return;
    }
    
    try {
        // Verify path exists
        if (!fs::exists(path) || !fs::is_directory(path)) {
            LOGD("FileBrowser::Refresh - Path does not exist or is not dir: %s", path.c_str());
            path = GetRootPath();
            state.editor.currentPath = path;
            LOGD("FileBrowser::Refresh - Reset to root: %s", path.c_str());
        }
        
        for (const auto& entry : fs::directory_iterator(path)) {
            // Error handling for permission issues during iteration
            try {
                if (entry.is_directory()) {
                    state.editor.dirList.push_back(entry.path().filename().string());
                } else {
                    std::string filename = entry.path().filename().string();
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    
                    bool include = false;
                    
                    if (state.editor.browserMode == PatternEditorState::BrowserMode::Samples) {
                         // Audio filtering
                         if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || 
                             ext == ".flac" || ext == ".aiff" || ext == ".aif" ||
                             ext == ".m4a" || ext == ".wma") {
                             include = true;
                         }
                    } else {
                         // Project filtering (Load/Save)
                         if (ext == ".json") {
                             include = true;
                         }
                    }
                    
                    if (include) {
                        state.editor.fileList.push_back(filename);
                    }
                }
            } catch (const std::exception& ex) { 
                LOGD("FileBrowser::Refresh - Iteration error: %s", ex.what());
                continue; 
            }
        }
        
        LOGD("FileBrowser::Refresh - Found %zu dirs, %zu files", state.editor.dirList.size(), state.editor.fileList.size());
        
        // Sort
        std::sort(state.editor.dirList.begin(), state.editor.dirList.end());
        std::sort(state.editor.fileList.begin(), state.editor.fileList.end());
        
    } catch (const fs::filesystem_error& e) {
        // Permission denied or other error
        LOGD("Filesystem error: %s", e.what());
    }
}

// Replaces Init()
void Open(GuiState& state, PatternEditorState::BrowserMode mode) {
    state.editor.showFileBrowser = true;
    state.editor.browserWaitForMouseUp = true; // Prevent click-through
    state.editor.browserMode = mode;
    state.editor.browserScrollY = 0;
    
    // Clear selection
    memset(state.editor.selectedFileBuffer, 0, sizeof(state.editor.selectedFileBuffer));
    // Clear save filename if opening save mode
    if (mode == PatternEditorState::BrowserMode::ProjectSave) {
        memset(state.editor.projectSaveFilename, 0, sizeof(state.editor.projectSaveFilename));
    }
    
    #if defined(__ANDROID__)
    // Request permissions (async)
    FilePicker::requestPermissions();
    
    // Logic: 
    // - Samples: Browse from System Root (/storage/emulated/0) to find user files.
    // - Projects: Browse from App Directory (/storage/emulated/0/BlackLang) ensuring write access.
    
    if (mode == PatternEditorState::BrowserMode::Samples) {
         if (!state.editor.lastSamplePath.empty()) {
             state.editor.currentPath = state.editor.lastSamplePath;
         } else {
             state.editor.currentPath = "/storage/emulated/0";
         }
    } else if (mode == PatternEditorState::BrowserMode::RecordingSave) {
         // Recordings: Fixed path in App-Specific/files/Recordings
         state.editor.currentPath = "/storage/emulated/0/Android/data/com.quadracollision.blacklang/files/Recordings";
         std::error_code ec;
         if (!fs::exists(state.editor.currentPath)) {
             fs::create_directories(state.editor.currentPath, ec);
             LOGD("FileBrowser::Open - Created Recordings Dir: %s", state.editor.currentPath.c_str());
         }
         LOGD("FileBrowser::Open - Recordings Path: %s", state.editor.currentPath.c_str());
    } else {
         // Project Load/Save: ALWAYS use guaranteed writable path
         // Ignore previous path to prevent saving in sample folders
         std::string newPath = GetRootPath(); 
         std::error_code ec;
         if (!fs::exists(newPath)) {
             fs::create_directories(newPath, ec);
             LOGD("FileBrowser::Open - Created Project Root: %s", newPath.c_str());
         }
         LOGD("FileBrowser::Open - Enforcing Project Root: %s", newPath.c_str());
         state.editor.currentPath = newPath;
    }
    LOGD("FileBrowser::Open - Mode: %d, Path: %s", (int)mode, state.editor.currentPath.c_str());
    #else
    if (state.editor.currentPath.empty()) {
        if (mode == PatternEditorState::BrowserMode::Samples && !state.editor.lastSamplePath.empty()) {
             state.editor.currentPath = state.editor.lastSamplePath;
        } else {
             state.editor.currentPath = fs::current_path().string();
        }
    }
    #endif
    
    Refresh(state);
}

void NavigateTo(GuiState& state, const std::string& path) {
    std::string safePath = path;
    
    #if defined(__ANDROID__)
    // Ensure we don't go above /storage/emulated/0 to avoid crashes
    std::string root = "/storage/emulated/0";
    if (safePath.find(root) != 0) {
        // If it looks like a system path, fallback to safe root
        safePath = GetRootPath();
    }
    #endif
    
    state.editor.currentPath = safePath;
    
    // Persist path if in Samples mode
    if (state.editor.browserMode == PatternEditorState::BrowserMode::Samples) {
        state.editor.lastSamplePath = state.editor.currentPath;
    }
    
    Refresh(state);
}

void GoUp(GuiState& state) {
    LOGD("FileBrowser::GoUp from: %s", state.editor.currentPath.c_str());
    fs::path p(state.editor.currentPath);
    
    #if defined(__ANDROID__)
    // Prevent going up past emulated/0 OR the App-Specific Root in strict modes
    std::string current = state.editor.currentPath;
    
    // Define strict roots
    std::string projectRoot = "/storage/emulated/0/Android/data/com.quadracollision.blacklang/files/Projects";
    std::string recordingRoot = "/storage/emulated/0/Android/data/com.quadracollision.blacklang/files/Recordings";
    
    // Block escaping strict roots in relevant modes
    if (state.editor.browserMode == PatternEditorState::BrowserMode::ProjectSave || 
        state.editor.browserMode == PatternEditorState::BrowserMode::ProjectLoad) {
        if (current == projectRoot) {
            LOGD("FileBrowser::GoUp - Blocked at Project Root: %s", current.c_str());
            return;
        }
    }
    else if (state.editor.browserMode == PatternEditorState::BrowserMode::RecordingSave) {
        if (current == recordingRoot) {
            LOGD("FileBrowser::GoUp - Blocked at Recording Root: %s", current.c_str());
            return;
        }
    }

    std::string pStr = p.string();
    if (pStr == "/storage/emulated/0" || pStr == "/storage/emulated/0/" || pStr == "/") {
         LOGD("FileBrowser::GoUp - Blocked at root: %s", pStr.c_str());
         return;
    }
    #endif
    
    if (p.has_parent_path()) {
        state.editor.currentPath = p.parent_path().string();
        LOGD("FileBrowser::GoUp - New path: %s", state.editor.currentPath.c_str());
        Refresh(state);
    } else {
        LOGD("FileBrowser::GoUp - No parent path");
    }
}

bool Draw(GuiState& state, AudioEngine& engine) {
    if (!state.editor.showFileBrowser) return false;
    
    // LOGD_ONCE("FileBrowser::Draw - Drawing overlay"); // Too spammy, but useful if we suspect it's not drawing
    
    Vector2 mousePos = state.getMousePosition();
    
    // Input Blocking Logic
    bool inputBlocked = state.editor.browserWaitForMouseUp;
    if (inputBlocked) {
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            state.editor.browserWaitForMouseUp = false;
        }
    }
    
    // Overlay
    DrawRectangle(0, 0, state.getScreenWidth(), state.getScreenHeight(), Color{0, 0, 0, 220});
    
    // Window dimensions
    float winW = 600;
    float winH = 450;
    if (state.getScreenWidth() < winW) {
        winW = (float)state.getScreenWidth() - 20;
        winH = (float)state.getScreenHeight() - 40;
    }
    float winX = ((float)state.getScreenWidth() - winW) / 2;
    float winY = ((float)state.getScreenHeight() - winH) / 2;
    
    Rectangle winRect = {winX, winY, winW, winH};
    DrawRectangleRec(winRect, Color{30, 30, 30, 255});
    DrawRectangleLinesEx(winRect, 1, DARKGRAY);
    
    // Header
    float headerH = 60;
    DrawRectangle(winX, winY, winW, headerH, Color{50, 50, 50, 255});
    
    const char* title = "File Browser";
    if (state.editor.browserMode == PatternEditorState::BrowserMode::Samples) title = "Load Sample";
    else if (state.editor.browserMode == PatternEditorState::BrowserMode::ProjectLoad) title = "Load Project";
    else if (state.editor.browserMode == PatternEditorState::BrowserMode::ProjectSave) title = "Save Project";
    else if (state.editor.browserMode == PatternEditorState::BrowserMode::RecordingSave) title = "Save Recording";
    
    DrawTextApp(title, winX + 15, winY + 20, 24, WHITE);
    
    // Header Buttons
    float btnSize = 44;
    float padding = 8;
    
    // Close (X)
    if (!inputBlocked && DrawButton({winX + winW - btnSize - padding, winY + padding, btnSize, btnSize}, "X", RED, WHITE, mousePos, 20)) {
        state.editor.showFileBrowser = false;
        return true; 
    }
    
    // Up (^)
    if (!inputBlocked && DrawButton({winX + winW - (btnSize*2) - (padding*2), winY + padding, btnSize, btnSize}, "^", DARKGRAY, WHITE, mousePos, 20)) {
        GoUp(state);
    }
    
    // Home (H)
    if (!inputBlocked && DrawButton({winX + winW - (btnSize*3) - (padding*3), winY + padding, btnSize, btnSize}, "H", DARKGRAY, WHITE, mousePos, 20)) {
        NavigateTo(state, GetRootPath());
    }
    
    // Path Display
    std::string pathDisplay = state.editor.currentPath;
    if (pathDisplay.length() > 50) pathDisplay = "..." + pathDisplay.substr(pathDisplay.length() - 47);
    DrawTextApp(pathDisplay.c_str(), winX + 15, winY + headerH + 10, 14, LIGHTGRAY);
    
    // Save Filename Input (Only in Save Mode)
    float listY = winY + headerH + 35;
    if (state.editor.browserMode == PatternEditorState::BrowserMode::ProjectSave || 
        state.editor.browserMode == PatternEditorState::BrowserMode::RecordingSave) {
        
        DrawTextApp("Filename:", winX + 15, listY, 14, WHITE);
        Rectangle nameBox = {winX + 110, listY - 5, 200, 28};
        
        // Use proper buffer based on mode
        char* buffer = state.editor.projectSaveFilename;
        if (state.editor.browserMode == PatternEditorState::BrowserMode::RecordingSave) buffer = state.recorder.filenameBuffer;
         
        DrawTextInput(nameBox, buffer, 63, 600, state.focusedFieldId, mousePos);
        
        const char* ext = ".json";
        if (state.editor.browserMode == PatternEditorState::BrowserMode::RecordingSave) ext = ".wav";
        DrawTextApp(ext, nameBox.x + nameBox.width + 5, listY, 14, GRAY);
        
        listY += 35;
    }
    
    // File List Area
    float scrollbarW = 20;
    float footerH = 50;
    Rectangle listRect = {winX + 10, listY, winW - 30 - scrollbarW, winH - (listY - winY) - footerH};
    Rectangle scrollbarTrack = {listRect.x + listRect.width + 5, listRect.y, scrollbarW, listRect.height};
    
    DrawRectangleRec(listRect, BLACK);
    
    // Scroll Calculation
    float itemH = 55; 
    float totalItems = (float)(state.editor.dirList.size() + state.editor.fileList.size());
    float contentHeight = totalItems * itemH;
    float maxScroll = std::max(0.0f, contentHeight - listRect.height);
    
    // Clamp Scroll
    if (state.editor.browserScrollY < 0) state.editor.browserScrollY = 0;
    if (state.editor.browserScrollY > maxScroll) state.editor.browserScrollY = maxScroll;
    
    // Draw Scrollbar
    float thumbRatio = (contentHeight > 0) ? (listRect.height / contentHeight) : 1.0f;
    if (thumbRatio > 1.0f) thumbRatio = 1.0f;
    float thumbH = std::max(30.0f, listRect.height * thumbRatio);
    float scrollRatio = (maxScroll > 0) ? (state.editor.browserScrollY / maxScroll) : 0.0f;
    float thumbY = scrollbarTrack.y + scrollRatio * (scrollbarTrack.height - thumbH);
    
    DrawRectangleRec(scrollbarTrack, Color{40, 40, 40, 255});
    DrawRectangleRec({scrollbarTrack.x, thumbY, scrollbarW, thumbH}, Color{100, 100, 100, 255});
    
    // Scroll Drag Logic (Simplified)
    static bool isDraggingScroll = false;
    static bool isDraggingContent = false; 
    static float dragStartY = 0;
    static float dragStartScroll = 0;
    
    if (!inputBlocked && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (CheckCollisionPointRec(mousePos, scrollbarTrack)) {
             state.consumeClick();
             isDraggingScroll = true;
             dragStartY = mousePos.y;
             dragStartScroll = state.editor.browserScrollY;
        } else if (CheckCollisionPointRec(mousePos, listRect)) {
             state.consumeClick(); // Prevent background scroll
             isDraggingContent = true;
             dragStartY = mousePos.y;
             dragStartScroll = state.editor.browserScrollY;
        }
    }
    
    if (isDraggingScroll) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float delta = mousePos.y - dragStartY;
            float trackH = scrollbarTrack.height - thumbH;
            if (trackH > 0) {
                float scrollDelta = (delta / trackH) * maxScroll;
                state.editor.browserScrollY = dragStartScroll + scrollDelta;
            }
        }
    } else if (isDraggingContent) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float delta = dragStartY - mousePos.y; // Drag up -> Scroll increase
            state.editor.browserScrollY = dragStartScroll + delta;
        }
    }
    
    
    // Draw Items
    BeginScissorMode((int)listRect.x, (int)listRect.y, (int)listRect.width, (int)listRect.height);
    float yOffset = -state.editor.browserScrollY;
    
    // Directories
    for (const auto& dir : state.editor.dirList) {
        if (yOffset + itemH > 0 && yOffset < listRect.height) {
            Rectangle itemRect = {listRect.x, listRect.y + yOffset, listRect.width, itemH};
            bool isHover = CheckCollisionPointRec(mousePos, itemRect);
            
            if (isHover) DrawRectangleRec(itemRect, Color{50, 50, 60, 255});
            
            DrawRectangleRec({itemRect.x+5, itemRect.y+10, 30, 30}, ORANGE); // Icon
            DrawTextApp("/", itemRect.x+15, itemRect.y+15, 20, BLACK);
            DrawTextApp(dir.c_str(), itemRect.x+45, itemRect.y+15, 20, WHITE);
            
            bool isDragClick = isDraggingContent && fabs(mousePos.y - dragStartY) > 5.0f;
            if (!inputBlocked && isHover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && !isDraggingScroll && !isDragClick) {
                std::string nextPath = state.editor.currentPath;
                if (nextPath.back() != '/' && nextPath.back() != '\\') nextPath += "/";
                nextPath += dir;
                NavigateTo(state, nextPath);
            }
        }
        yOffset += itemH;
    }
    
    // Files
    for (const auto& file : state.editor.fileList) {
        if (yOffset + itemH > 0 && yOffset < listRect.height) {
            Rectangle itemRect = {listRect.x, listRect.y + yOffset, listRect.width, itemH};
            bool isSelected = (file == state.editor.selectedFileBuffer);
            bool isHover = CheckCollisionPointRec(mousePos, itemRect);
            
            if (isSelected) DrawRectangleRec(itemRect, Color{0, 100, 0, 100});
            else if (isHover) DrawRectangleRec(itemRect, Color{60, 60, 60, 50});
            
            DrawTextApp(file.c_str(), itemRect.x + 10, itemRect.y + 15, 20, isSelected ? GREEN : LIGHTGRAY);
            
            bool isDragClick = isDraggingContent && fabs(mousePos.y - dragStartY) > 5.0f;
            if (!inputBlocked && isHover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && !isDraggingScroll && !isDragClick) {
                strcpy(state.editor.selectedFileBuffer, file.c_str());
            }
        }
        yOffset += itemH;
    }
    EndScissorMode();
    
    // Footer Action Button
    float footerY = winY + winH - 45;
    const char* actionLabel = "SELECT";
    if (state.editor.browserMode == PatternEditorState::BrowserMode::Samples) actionLabel = "LOAD";
    else if (state.editor.browserMode == PatternEditorState::BrowserMode::ProjectLoad) actionLabel = "LOAD PROJECT";
    else if (state.editor.browserMode == PatternEditorState::BrowserMode::ProjectSave) actionLabel = "SAVE PROJECT";
    else if (state.editor.browserMode == PatternEditorState::BrowserMode::RecordingSave) actionLabel = "SAVE RECORDING";
    
    if (!inputBlocked && DrawButton({winX + winW - 220, footerY, 210, 35}, actionLabel, BLUE, WHITE, mousePos, 20)) {
        if (state.editor.browserMode == PatternEditorState::BrowserMode::ProjectSave) {
            // Save Project
            std::string filename = state.editor.projectSaveFilename;
            if (filename.empty()) filename = "project";
            if (filename.find(".json") == std::string::npos) filename += ".json";
            
            fs::path fullPath = fs::path(state.editor.currentPath) / filename;
            LOGD("Saving project to: %s", fullPath.string().c_str());
            
            // Collect GUI data for save
            std::vector<SerializedColumn> cols; 
            for (const auto& col : state.columns) {
                SerializedColumn c;
                c.title = col.title;
                c.trackName = col.trackName;
                c.volume = col.volume;
                c.pan = col.pan;
                c.patternNames = col.patternNames;
                c.slotSyncEnabled = col.slotSyncEnabled;
                
                // Get FX Chain from Engine
                AudioBus* bus = engine.getTrackBus(col.trackName);
                if (bus) {
                    for (const auto& effect : bus->effects) {
                         SerializedFX sfx;
                         sfx.type = (int)effect->getType();
                         sfx.enabled = effect->isActive(); 
                         for (int pIdx = 0; pIdx < effect->getNumParams(); ++pIdx) {
                             sfx.params.push_back(effect->getParam(pIdx).value);
                         }
                         c.fxChain.push_back(sfx);
                    }
                }
                
                cols.push_back(c);
            }
            
            if (ProjectFile::save(fullPath.string(), engine.getPatterns(), state.activeChain, cols)) {
                LOGD("Project saved successfully");
                state.editor.showFileBrowser = false;
            } else {
                 LOGD("Project save failed!");
            }
            
        } else if (state.editor.browserMode == PatternEditorState::BrowserMode::ProjectLoad) {
             // ... (Existing Load Logic) ...
             if (strlen(state.editor.selectedFileBuffer) > 0) {
                fs::path fullPath = fs::path(state.editor.currentPath) / state.editor.selectedFileBuffer;
                
                std::map<std::string, Pattern> patterns;
                PatternChain chain;
                std::vector<SerializedColumn> cols;
                
                if (ProjectFile::load(fullPath.string(), patterns, chain, cols)) {
                    // Update engine patterns
                    // Update engine patterns
                    for (const auto& kv : patterns) {
                        Pattern p = kv.second;
                        // Auto-load sample to commit it to the sequence
                        if (!p.samplePath.empty()) {
                            engine.loadSample(p);
                        }
                        engine.addPattern(p);
                    }
                    
                    state.activeChain = chain;
                    
                    // Rebuild columns
                    state.columns.clear();
                    state.activePatternSlots.clear(); // Clear selections
                    
                    float width = (float)state.getScreenWidth() / std::max(1, (int)cols.size());
                    float x = 0;
                    
                    int colIndex = 0;
                    for (const auto& c : cols) {
                        PatternColumn pc;
                        pc.title = c.title;
                        pc.trackName = c.trackName;
                        pc.volume = c.volume;
                        pc.pan = c.pan;
                        pc.patternNames = c.patternNames;
                        pc.slotSyncEnabled = c.slotSyncEnabled;
                        
                        // Default size check
                        if (pc.slotSyncEnabled.size() < 8) pc.slotSyncEnabled.resize(8, false);
                        
                        pc.bounds = {x, 0, width, (float)state.getScreenHeight() - 120}; 
                        pc.bounds = {x, 0, width, (float)state.getScreenHeight() - 120}; 
                        
                        // Apply Mixer State
                        AudioBus* bus = engine.getTrackBus(c.trackName);
                        if (bus) {
                             bus->volume = c.volume;
                             bus->pan = c.pan;
                             bus->effects.clear(); // Clear default/existing
                             for (const auto& sfx : c.fxChain) {
                                 auto effect = fx::CreateTrackEffect((fx::FXType)sfx.type);
                                 if (effect) {
                                     effect->setActive(sfx.enabled);
                                     for (size_t pIdx = 0; pIdx < sfx.params.size(); ++pIdx) {
                                          effect->setParam(pIdx, sfx.params[pIdx]);
                                     }
                                     bus->effects.push_back(effect);
                                 }
                             }
                        }
                        
                        state.columns.push_back(pc);
                        
                        x += width;
                        colIndex++;
                    }
                    
                    state.mainContentWidth = x;
                    state.editor.showFileBrowser = false;
                }
            }
        } else if (state.editor.browserMode == PatternEditorState::BrowserMode::RecordingSave) {
             // Save Recording
             std::string filename = state.recorder.filenameBuffer;
             if (filename.empty()) filename = "recording";
             if (filename.find(".wav") == std::string::npos) filename += ".wav";
             
             // Force safe path for Android Recording Save to ensure write permission
             #if defined(__ANDROID__)
             state.editor.currentPath = "/storage/emulated/0/Android/data/com.quadracollision.blacklang/files/Recordings";
             // Ensure it exists
             std::error_code ec;
             if (!fs::exists(state.editor.currentPath)) fs::create_directories(state.editor.currentPath, ec);
             #endif

             fs::path fullPath = fs::path(state.editor.currentPath) / filename;
             LOGD("Saving recording to: %s", fullPath.string().c_str());
             
             int sampleCount = engine.getRecordedSampleCount();
             LOGD("FileBrowser - Recorded Sample Count: %d", sampleCount);
             
             if (sampleCount <= 0) {
                 LOGD("FileBrowser - Error: No audio recorded!");
             }

             // Trigger save via Engine (which accesses the recording buffer)
             bool success = engine.saveRecordingWrapper(fullPath.string());
             
             if (success && fs::exists(fullPath)) {
                 LOGD("FileBrowser - File created successfully: %s", fullPath.string().c_str());
                 state.editor.showFileBrowser = false;
                 
                 // Clear buffers now that we are done (and only on success)
                 engine.clearRecordedBuffers();
             } else {
                 LOGD("FileBrowser - Error: File save failed. Engine result: %d", success);
             }
        } else if (state.editor.browserMode == PatternEditorState::BrowserMode::Samples) {
             // LOAD SAMPLE
             if (strlen(state.editor.selectedFileBuffer) > 0) {
                 fs::path fullPath = fs::path(state.editor.currentPath) / state.editor.selectedFileBuffer;
                 state.editor.currentPattern.samplePath = fullPath.string();
                 strcpy(state.editor.samplePathBuffer, fullPath.string().c_str());
                 state.editor.currentPattern.sliceMarkers.clear();
                 engine.loadSample(state.editor.currentPattern);
                 state.editor.showFileBrowser = false;
             }
        }
    }
    
    // Clear Drag State on Release
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        isDraggingScroll = false;
        isDraggingContent = false;
    }
    
    return true;
}

} // namespace FileBrowser
