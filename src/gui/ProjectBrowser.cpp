#include "ProjectBrowser.h"
#include "Widgets.h"
#include "../ProjectFile.h"
#include <filesystem>
#include <vector>
#include <algorithm>
#include <cstring>

#if defined(__ANDROID__)
#include <android/log.h>
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "ProjectBrowser", __VA_ARGS__)
#else
#define LOGD(...) 
#endif

namespace fs = std::filesystem;

namespace ProjectBrowser {

// Get the projects folder path (fixed on Android, cwd on desktop)
static std::string GetProjectsFolder() {
#if defined(__ANDROID__)
    // Use a fixed projects folder on Android
    std::string projectsPath = "/storage/emulated/0/BlackLang/projects";
    // Create directory if it doesn't exist
    try {
        if (!fs::exists(projectsPath)) {
            fs::create_directories(projectsPath);
        }
    } catch (...) {
        LOGD("Failed to create projects directory");
    }
    return projectsPath;
#else
    return fs::current_path().string();
#endif
}

void Refresh(GuiState& state) {
    state.projectBrowser.fileList.clear();
    state.projectBrowser.dirList.clear();
    state.projectBrowser.scrollY = 0;
    
    std::string path = state.projectBrowser.currentPath;
    if (path.empty()) return;
    
    try {
        if (!fs::exists(path) || !fs::is_directory(path)) {
            path = GetProjectsFolder();
            state.projectBrowser.currentPath = path;
        }
        
        for (const auto& entry : fs::directory_iterator(path)) {
            try {
#if defined(__ANDROID__)
                // On Android, only show files (no directory navigation)
                if (!entry.is_directory()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".json") {
                        state.projectBrowser.fileList.push_back(entry.path().filename().string());
                    }
                }
#else
                // On desktop, show directories and files
                if (entry.is_directory()) {
                    state.projectBrowser.dirList.push_back(entry.path().filename().string());
                } else {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".json") {
                        state.projectBrowser.fileList.push_back(entry.path().filename().string());
                    }
                }
#endif
            } catch (...) { continue; }
        }
        
        std::sort(state.projectBrowser.dirList.begin(), state.projectBrowser.dirList.end());
        std::sort(state.projectBrowser.fileList.begin(), state.projectBrowser.fileList.end());
        
    } catch (const fs::filesystem_error&) {
        // Permission denied or other error
        LOGD("Filesystem error accessing: %s", path.c_str());
    }
}

void Init(GuiState& state, bool saveMode) {
    state.projectBrowser.isSaveMode = saveMode;
    memset(state.projectBrowser.selectedFile, 0, sizeof(state.projectBrowser.selectedFile));
    state.projectBrowser.scrollY = 0;
    
    // Always use the projects folder
    state.projectBrowser.currentPath = GetProjectsFolder();
    
    Refresh(state);
}

void NavigateTo(GuiState& state, const std::string& path) {
#if defined(__ANDROID__)
    // On Android, don't allow navigation - stay in projects folder
    return;
#else
    state.projectBrowser.currentPath = path;
    Refresh(state);
#endif
}

void GoUp(GuiState& state) {
#if defined(__ANDROID__)
    // On Android, don't allow navigation - stay in projects folder
    return;
#else
    fs::path p(state.projectBrowser.currentPath);
    if (p.has_parent_path()) {
        state.projectBrowser.currentPath = p.parent_path().string();
        Refresh(state);
    }
#endif
}

bool Draw(GuiState& state, AudioEngine& engine) {
    if (!state.projectBrowser.isOpen) return false;
    
    Vector2 mousePos = state.getMousePosition();
    
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
    DrawRectangle((int)winX, (int)winY, (int)winW, (int)headerH, Color{50, 50, 50, 255});
    
    const char* title = state.projectBrowser.isSaveMode ? "Save Project" : "Load Project";
    DrawText(title, (int)(winX + 15), (int)(winY + 20), 24, WHITE);
    
    // Close button
    float btnSize = 44;
    float padding = 8;
    if (DrawButton({winX + winW - btnSize - padding, winY + padding, btnSize, btnSize}, "X", RED, WHITE, mousePos)) {
        state.projectBrowser.isOpen = false;
        return true;
    }
    
#if !defined(__ANDROID__)
    // Up button (desktop only)
    if (DrawButton({winX + winW - (btnSize*2) - (padding*2), winY + padding, btnSize, btnSize}, "^", DARKGRAY, WHITE, mousePos)) {
        GoUp(state);
    }
    
    // Home button (desktop only)
    if (DrawButton({winX + winW - (btnSize*3) - (padding*3), winY + padding, btnSize, btnSize}, "H", DARKGRAY, WHITE, mousePos)) {
        NavigateTo(state, fs::current_path().string());
    }
#endif
    
    // Current path display
#if defined(__ANDROID__)
    DrawText("Projects", (int)(winX + 15), (int)(winY + headerH + 10), 14, LIGHTGRAY);
#else
    std::string pathDisplay = state.projectBrowser.currentPath;
    if (pathDisplay.length() > 50) pathDisplay = "..." + pathDisplay.substr(pathDisplay.length() - 47);
    DrawText(pathDisplay.c_str(), (int)(winX + 15), (int)(winY + headerH + 10), 14, LIGHTGRAY);
#endif
    
    // Filename input for save mode
    float listY = winY + headerH + 35;
    if (state.projectBrowser.isSaveMode) {
        DrawText("Filename:", (int)(winX + 15), (int)listY, 14, WHITE);
        Rectangle nameBox = {winX + 100, listY - 5, 200, 28};
        DrawTextInput(nameBox, state.projectBrowser.filenameBuffer, 63, 600, state.focusedFieldId, mousePos);
        DrawText(".json", (int)(nameBox.x + nameBox.width + 5), (int)listY, 14, GRAY);
        listY += 35;
    }
    
    // Scrollbar dimensions
    float scrollbarW = 20;
    Rectangle listRect = {winX + 10, listY, winW - 30 - scrollbarW, winH - (listY - winY) - 55};
    
    DrawRectangleRec(listRect, BLACK);
    
    float itemH = 50;
    float totalItems = (float)(state.projectBrowser.dirList.size() + state.projectBrowser.fileList.size());
    float contentHeight = totalItems * itemH;
    float maxScroll = std::max(0.0f, contentHeight - listRect.height);
    
    // Clamp scroll
    if (state.projectBrowser.scrollY < 0) state.projectBrowser.scrollY = 0;
    if (state.projectBrowser.scrollY > maxScroll) state.projectBrowser.scrollY = maxScroll;
    
    // Draw scrollbar
    Rectangle scrollbarTrack = {listRect.x + listRect.width + 5, listRect.y, scrollbarW, listRect.height};
    float thumbRatio = (contentHeight > 0) ? (listRect.height / contentHeight) : 1.0f;
    if (thumbRatio > 1.0f) thumbRatio = 1.0f;
    float thumbH = std::max(30.0f, listRect.height * thumbRatio);
    float scrollRatio = (maxScroll > 0) ? (state.projectBrowser.scrollY / maxScroll) : 0.0f;
    float thumbY = scrollbarTrack.y + scrollRatio * (scrollbarTrack.height - thumbH);
    
    DrawRectangleRec(scrollbarTrack, Color{40, 40, 40, 255});
    DrawRectangleRec({scrollbarTrack.x, thumbY, scrollbarW, thumbH}, Color{100, 100, 100, 255});
    
    // Drag scrolling
    static bool isDragging = false;
    static float dragStartY = 0;
    static float dragStartScroll = 0;
    
    if (CheckCollisionPointRec(mousePos, listRect)) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            isDragging = true;
            dragStartY = mousePos.y;
            dragStartScroll = state.projectBrowser.scrollY;
        }
    }
    
    if (isDragging) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float deltaY = mousePos.y - dragStartY;
            state.projectBrowser.scrollY = dragStartScroll - deltaY;
            if (state.projectBrowser.scrollY < 0) state.projectBrowser.scrollY = 0;
            if (state.projectBrowser.scrollY > maxScroll) state.projectBrowser.scrollY = maxScroll;
        } else {
            isDragging = false;
        }
    }
    
    // Draw items
    BeginScissorMode((int)listRect.x, (int)listRect.y, (int)listRect.width, (int)listRect.height);
    
    float yOffset = -state.projectBrowser.scrollY;
    
    // Directories
    for (const auto& dir : state.projectBrowser.dirList) {
        if (yOffset + itemH > 0 && yOffset < listRect.height) {
            Rectangle itemRect = {listRect.x, listRect.y + yOffset, listRect.width, itemH};
            
            bool isHover = CheckCollisionPointRec(mousePos, itemRect);
            if (isHover) DrawRectangleRec(itemRect, Color{50, 50, 60, 255});
            
            DrawRectangleRec({itemRect.x + 5, itemRect.y + 5, itemRect.height - 10, itemRect.height - 10}, ORANGE);
            DrawText("/", (int)(itemRect.x + 15), (int)(itemRect.y + 12), 20, BLACK);
            DrawText(dir.c_str(), (int)(itemRect.x + itemRect.height + 5), (int)(itemRect.y + 12), 18, WHITE);
            
            if (isHover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && !isDragging) {
                float dist = fabsf(mousePos.y - dragStartY);
                if (dist < 10.0f) {
                    std::string nextPath = state.projectBrowser.currentPath;
                    if (nextPath.back() != '/') nextPath += "/";
                    nextPath += dir;
                    NavigateTo(state, nextPath);
                }
            }
        }
        yOffset += itemH;
    }
    
    // Files
    for (const auto& file : state.projectBrowser.fileList) {
        if (yOffset + itemH > 0 && yOffset < listRect.height) {
            Rectangle itemRect = {listRect.x, listRect.y + yOffset, listRect.width, itemH};
            
            bool isSelected = (file == state.projectBrowser.selectedFile);
            if (isSelected) DrawRectangleRec(itemRect, Color{0, 100, 0, 100});
            
            bool isHover = CheckCollisionPointRec(mousePos, itemRect);
            if (isHover) DrawRectangleRec(itemRect, Color{60, 60, 60, 50});
            
            DrawText(file.c_str(), (int)(itemRect.x + 10), (int)(itemRect.y + 12), 18, isSelected ? GREEN : LIGHTGRAY);
            
            if (isHover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && !isDragging) {
                float dist = fabsf(mousePos.y - dragStartY);
                if (dist < 10.0f) {
                    strcpy(state.projectBrowser.selectedFile, file.c_str());
                    // For save mode, also update filename buffer
                    if (state.projectBrowser.isSaveMode) {
                        std::string nameWithoutExt = file;
                        if (nameWithoutExt.length() > 5 && nameWithoutExt.substr(nameWithoutExt.length() - 5) == ".json") {
                            nameWithoutExt = nameWithoutExt.substr(0, nameWithoutExt.length() - 5);
                        }
                        strncpy(state.projectBrowser.filenameBuffer, nameWithoutExt.c_str(), 63);
                    }
                }
            }
        }
        yOffset += itemH;
    }
    
    EndScissorMode();
    
    // Footer buttons
    float footerY = winY + winH - 50;
    
    if (state.projectBrowser.isSaveMode) {
        // Save button
        if (DrawButton({winX + winW - 100, footerY, 90, 40}, "SAVE", Color{0, 150, 0, 255}, WHITE, mousePos)) {
            if (strlen(state.projectBrowser.filenameBuffer) > 0) {
                std::string filename = std::string(state.projectBrowser.filenameBuffer) + ".json";
                fs::path fullPath = fs::path(state.projectBrowser.currentPath) / filename;
                
                // Convert layout for serialization
                std::vector<SerializedColumn> cols;
                for (const auto& col : state.columns) {
                    cols.push_back({col.title, col.trackName, col.patternNames, col.slotSyncEnabled});
                }
                
                ProjectFile::save(fullPath.string(), engine.getPatterns(), state.activeChain, cols);
                state.projectBrowser.isOpen = false;
            }
        }
    } else {
        // Load button
        if (DrawButton({winX + winW - 100, footerY, 90, 40}, "LOAD", Color{0, 100, 200, 255}, WHITE, mousePos)) {
            if (strlen(state.projectBrowser.selectedFile) > 0) {
                fs::path fullPath = fs::path(state.projectBrowser.currentPath) / state.projectBrowser.selectedFile;
                
                std::map<std::string, Pattern> patterns;
                std::vector<SerializedColumn> loadedCols;
                
                if (ProjectFile::load(fullPath.string(), patterns, state.activeChain, loadedCols)) {
                    engine.stop();
                    
                    // Get project directory for resolving relative sample paths
                    fs::path projectDir = fullPath.parent_path();
                    
                    for (auto& [name, pat] : patterns) {
                        if (!pat.samplePath.empty()) {
                            // Try multiple path resolutions
                            std::string resolvedPath = pat.samplePath;
                            
                            // If path doesn't exist as-is, try relative to project directory
                            if (!fs::exists(resolvedPath)) {
                                fs::path relativeToProject = projectDir / pat.samplePath;
                                if (fs::exists(relativeToProject)) {
                                    resolvedPath = relativeToProject.string();
                                } else {
                                    // Try just the filename in project directory
                                    fs::path justFilename = projectDir / fs::path(pat.samplePath).filename();
                                    if (fs::exists(justFilename)) {
                                        resolvedPath = justFilename.string();
                                    }
                                }
                            }
                            
                            pat.samplePath = resolvedPath;
                            engine.loadSample(pat);
                        }
                        engine.addPattern(pat);
                    }
                    state.columns.clear();
                    state.activePatternSlots.clear();
                    
                    if (!loadedCols.empty()) {
                        for (const auto& sCol : loadedCols) {
                            state.columns.push_back({sCol.title, sCol.patternNames, sCol.slotSyncEnabled, {0,0,0,0}, 0.0f, false, sCol.trackName, 1.0f, 0.5f});
                        }
                    } else {
                        state.columns.resize(4);
                        for (const auto& [name, pat] : patterns) {
                            state.columns[0].patternNames.push_back(name);
                        }
                    }
                    state.projectBrowser.isOpen = false;
                }
            }
        }
    }
    
    return true;
}

} // namespace ProjectBrowser
