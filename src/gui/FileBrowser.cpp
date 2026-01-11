#include "FileBrowser.h"
#include "Widgets.h"
#include "../FilePicker.h"
#include <filesystem>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cstring>

#if defined(__ANDROID__)
#include <android/log.h>
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "FileBrowser", __VA_ARGS__)
#else
#define LOGD(...) 
#endif

namespace fs = std::filesystem;

namespace FileBrowser {

void Refresh(GuiState& state) {
    state.editor.fileList.clear();
    state.editor.dirList.clear();
    state.editor.browserScrollY = 0; // Reset scroll on refresh
    
    std::string path = state.editor.currentPath;
    if (path.empty()) return;
    
    try {
        // Verify path exists
        if (!fs::exists(path) || !fs::is_directory(path)) {
            #if defined(__ANDROID__)
            path = "/storage/emulated/0/";
            #else
            path = fs::current_path().string();
            #endif
            state.editor.currentPath = path;
        }
        
        for (const auto& entry : fs::directory_iterator(path)) {
            // Error handling for permission issues during iteration
            try {
                if (entry.is_directory()) {
                    state.editor.dirList.push_back(entry.path().filename().string());
                } else {
                    // Show all files - let user select any audio file
                    std::string filename = entry.path().filename().string();
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    // Filter to common audio formats
                    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || 
                        ext == ".flac" || ext == ".aiff" || ext == ".aif" ||
                        ext == ".m4a" || ext == ".wma") {
                        state.editor.fileList.push_back(filename);
                    }
                }
            } catch (...) { continue; }
        }
        
        // Sort
        std::sort(state.editor.dirList.begin(), state.editor.dirList.end());
        std::sort(state.editor.fileList.begin(), state.editor.fileList.end());
        
    } catch (const fs::filesystem_error& e) {
        // Permission denied or other error
        // If critical, fallback to root?
    }
}

void Init(GuiState& state) {
    // Clear previous selection - both buffers!
    memset(state.editor.selectedFileBuffer, 0, sizeof(state.editor.selectedFileBuffer));
    state.editor.browserScrollY = 0;
    
    #if defined(__ANDROID__)
    // Request permissions (async)
    FilePicker::requestPermissions();
    // Only set default path if not already set (remembers last folder)
    if (state.editor.currentPath.empty()) {
        state.editor.currentPath = "/storage/emulated/0";
    }
    #else
    if (state.editor.currentPath.empty()) {
        state.editor.currentPath = fs::current_path().string();
    }
    #endif
    
    Refresh(state);
}

void NavigateTo(GuiState& state, const std::string& path) {
    std::string safePath = path;
    
    // Safety restriction (mainly Android)
    #if defined(__ANDROID__)
    std::string root = "/storage/emulated/0";
    // Check if path is valid (starts with root)
    if (safePath.find(root) != 0) {
        safePath = root;
    }
    #endif
    
    state.editor.currentPath = safePath;
    Refresh(state);
}

void GoUp(GuiState& state) {
    fs::path p(state.editor.currentPath);
    if (p.has_parent_path()) {
        state.editor.currentPath = p.parent_path().string();
        Refresh(state);
    }
}

bool Draw(GuiState& state, AudioEngine& engine) {
    if (!state.editor.showFileBrowser) return false;
    
    // Virtual mouse for input
    Vector2 mousePos = state.getMousePosition();
    
    // Overlay - Consume all clicks!
    DrawRectangle(0, 0, state.getScreenWidth(), state.getScreenHeight(), Color{0, 0, 0, 220});
    
    // Window dimensions
    float winW = 600;
    float winH = 450;
    
    // Responsive sizing for mobile
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
    float headerH = 60; // Taller header
    DrawRectangle(winX, winY, winW, headerH, Color{50, 50, 50, 255});
    DrawText("File Browser", winX + 15, winY + 20, 24, WHITE);
    
    // Close button (Top-Right)
    float btnSize = 44;
    float padding = 8;
    if (DrawButton({winX + winW - btnSize - padding, winY + padding, btnSize, btnSize}, "X", RED, WHITE, mousePos)) {
        state.editor.showFileBrowser = false;
        return true; 
    }
    
    // "Up" button (Right of text, Left of Close?)
    // Or maybe just right next to Close for easy thumb access?
    // Let's put Close Far Right, Up to the Left of it.
    if (DrawButton({winX + winW - (btnSize*2) - (padding*2), winY + padding, btnSize, btnSize}, "^", DARKGRAY, WHITE, mousePos)) {
        GoUp(state);
    }
    
    // Home Button?
    if (DrawButton({winX + winW - (btnSize*3) - (padding*3), winY + padding, btnSize, btnSize}, "H", DARKGRAY, WHITE, mousePos)) {
        #if defined(__ANDROID__)
        NavigateTo(state, "/storage/emulated/0");
        #else
        NavigateTo(state, fs::current_path().string());
        #endif
    }
    
    // Current Path Display
    // Below header
    std::string pathDisplay = state.editor.currentPath;
    if (pathDisplay.length() > 50) pathDisplay = "..." + pathDisplay.substr(pathDisplay.length() - 47);
    DrawText(pathDisplay.c_str(), winX + 15, winY + headerH + 10, 14, LIGHTGRAY);
    
    // Scrollbar dimensions
    float scrollbarW = 20;
    // Adjust list rect for taller header and path text
    float listY = winY + headerH + 35;
    Rectangle listRect = {winX + 10, listY, winW - 30 - scrollbarW, winH - (listY - winY) - 50};
    Rectangle scrollbarTrack = {listRect.x + listRect.width + 5, listRect.y, scrollbarW, listRect.height};
    
    DrawRectangleRec(listRect, BLACK);
    
    float itemH = 60; // Taller for touch
    float totalItems = (float)(state.editor.dirList.size() + state.editor.fileList.size());
    float contentHeight = totalItems * itemH;
    float maxScroll = std::max(0.0f, contentHeight - listRect.height);
    
    // Scrollbar Logic
    float thumbRatio = (contentHeight > 0) ? (listRect.height / contentHeight) : 1.0f;
    if (thumbRatio > 1.0f) thumbRatio = 1.0f;
    float thumbH = std::max(30.0f, listRect.height * thumbRatio);
    // Clamp scroll
    if (state.editor.browserScrollY < 0) state.editor.browserScrollY = 0;
    if (state.editor.browserScrollY > maxScroll) state.editor.browserScrollY = maxScroll;
    
    float scrollRatio = (maxScroll > 0) ? (state.editor.browserScrollY / maxScroll) : 0.0f;
    float thumbY = scrollbarTrack.y + scrollRatio * (scrollbarTrack.height - thumbH);
    Rectangle thumbRect = {scrollbarTrack.x, thumbY, scrollbarW, thumbH};
    
    // Draw scrollbar
    DrawRectangleRec(scrollbarTrack, Color{40, 40, 40, 255});
    DrawRectangleRec(thumbRect, Color{100, 100, 100, 255});
    
    // ----------------------------------------------------------------------------------
    // DRAG SCROLLING
    // ----------------------------------------------------------------------------------
    static bool isDraggingContent = false;
    static float dragStartY = 0;
    static float dragStartScroll = 0;
    
    if (CheckCollisionPointRec(mousePos, listRect)) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            isDraggingContent = true;
            dragStartY = mousePos.y;
            dragStartScroll = state.editor.browserScrollY;
        }
    }
    
    if (isDraggingContent) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float deltaY = mousePos.y - dragStartY;
            state.editor.browserScrollY = dragStartScroll - deltaY; // Invert delta for "natural" scroll
            
            // Clamp during drag
            if (state.editor.browserScrollY < 0) state.editor.browserScrollY = 0;
            if (state.editor.browserScrollY > maxScroll) state.editor.browserScrollY = maxScroll;
        } else {
            isDraggingContent = false;
        }
    }
    // ----------------------------------------------------------------------------------

    // Scrollbar drag handling (simultaneous)
    static bool isDraggingScrollbar = false;
    static float sbDragStartY = 0;
    static float sbDragStartScroll = 0;
    
    if (CheckCollisionPointRec(mousePos, scrollbarTrack)) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(mousePos, thumbRect)) {
                isDraggingScrollbar = true;
                sbDragStartY = mousePos.y;
                sbDragStartScroll = state.editor.browserScrollY;
            } else {
                float clickRatio = (mousePos.y - scrollbarTrack.y) / scrollbarTrack.height;
                state.editor.browserScrollY = clickRatio * maxScroll;
            }
        }
    }
    
    if (isDraggingScrollbar) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float deltaY = mousePos.y - sbDragStartY;
            float scrollDelta = (deltaY / (scrollbarTrack.height - thumbH)) * maxScroll;
            state.editor.browserScrollY = sbDragStartScroll + scrollDelta;
        } else {
            isDraggingScrollbar = false;
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
            
            DrawRectangleRec({itemRect.x + 5, itemRect.y + 5, itemRect.height - 10, itemRect.height - 10}, ORANGE); // Icon placeholder
            DrawText("/", itemRect.x + 15, itemRect.y + 15, 20, BLACK);
            DrawText(dir.c_str(), itemRect.x + itemRect.height + 5, itemRect.y + 15, 20, WHITE);
            
            // Interaction (Only if NOT dragging significantly)
            if (isHover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && !isDraggingContent && !isDraggingScrollbar) {
                 // Cheap way to avoid click-on-drag: check if we moved much. 
                 // Better: check distance from dragStart. 
                 // For now, checking isDraggingContent==false is handled by Input system (released clears flag).
                 // Actually, IsMouseButtonReleased is true on the frame release happens. 
                 // If we were dragging, we don't want to click.
                 float dist = fabs(mousePos.y - dragStartY);
                 if (dist < 10.0f) {
                    std::string nextPath = state.editor.currentPath;
                    if (nextPath.back() != '/') nextPath += "/";
                    nextPath += dir;
                    NavigateTo(state, nextPath);
                 }
            }
        }
        yOffset += itemH;
    }
    
    // Files
    for (const auto& file : state.editor.fileList) {
        if (yOffset + itemH > 0 && yOffset < listRect.height) {
            Rectangle itemRect = {listRect.x, listRect.y + yOffset, listRect.width, itemH};
            
            // Highlight selected
            bool isSelected = (file == state.editor.selectedFileBuffer);
            if (isSelected) DrawRectangleRec(itemRect, Color{0, 100, 0, 100});
            
            bool isHover = CheckCollisionPointRec(mousePos, itemRect);
            if (isHover) DrawRectangleRec(itemRect, Color{60, 60, 60, 50});

            DrawText(file.c_str(), itemRect.x + 10, itemRect.y + 15, 20, isSelected ? GREEN : LIGHTGRAY);
            
             if (isHover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && !isDraggingContent && !isDraggingScrollbar) {
                 float dist = fabs(mousePos.y - dragStartY);
                 if (dist < 10.0f) {
                    // Select file and build full path
                    strcpy(state.editor.selectedFileBuffer, file.c_str());
                    
                    fs::path fullPath = fs::path(state.editor.currentPath) / file;
                    std::string fullPathStr = fullPath.string();
                    
                    // Update path buffer and pattern
                    strcpy(state.editor.samplePathBuffer, fullPathStr.c_str());
                    state.editor.currentPattern.samplePath = fullPathStr;
                    
                    // Reset slices and load audio
                    state.editor.currentPattern.sliceMarkers.clear();
                    engine.loadSample(state.editor.currentPattern);
                    
                    // Auto-close after loading
                    state.editor.showFileBrowser = false;
                 }
            }
        }
        yOffset += itemH;
    }
        
    EndScissorMode();
    
    // Footer Buttons
    float footerY = winY + winH - 45;
    
    // Permissions Debug (Optional)
    #if defined(__ANDROID__)
    // DrawText("Checking perms...", winX + 10, footerY + 10, 10, GRAY);
    #endif

    // LOAD Button
    if (DrawButton({winX + winW - 100, footerY, 90, 35}, "LOAD", Color{0, 100, 200, 255}, WHITE, mousePos)) {
         if (strlen(state.editor.selectedFileBuffer) > 0) {
             fs::path p(state.editor.currentPath);
             p /= state.editor.selectedFileBuffer;
             
             std::string fullPath = p.string();
             LOGD("LOAD pressed - selectedFileBuffer: %s", state.editor.selectedFileBuffer);
             LOGD("LOAD pressed - fullPath: %s", fullPath.c_str());
             
             // Update Editor State
             strcpy(state.editor.samplePathBuffer, fullPath.c_str());
             state.editor.currentPattern.samplePath = fullPath;
             
             // Reset Slices
             state.editor.currentPattern.sliceMarkers.clear();
             
             // Load Audio
             engine.loadSample(state.editor.currentPattern);
             
             // Close Browser
             state.editor.showFileBrowser = false;
         } else {
             LOGD("LOAD pressed but selectedFileBuffer is EMPTY!");
         }
    }
    
    return true; // Input Consumed
}

} // namespace FileBrowser
