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
    state.editor.currentPath = path;
    Refresh(state);
}

void GoUp(GuiState& state) {
    fs::path p(state.editor.currentPath);
    if (p.has_parent_path()) {
        state.editor.currentPath = p.parent_path().string();
        Refresh(state);
    }
}

void Draw(GuiState& state, AudioEngine& engine) {
    if (!state.editor.showFileBrowser) return;
    
    // Virtual mouse for input
    Vector2 mousePos = state.getMousePosition();
    
    // Overlay
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
    DrawRectangle(winX, winY, winW, 40, Color{50, 50, 50, 255});
    DrawText("File Browser", winX + 10, winY + 12, 20, WHITE);
    
    // Close button
    if (DrawButton({winX + winW - 35, winY + 5, 30, 30}, "X", RED, WHITE, mousePos)) {
        state.editor.showFileBrowser = false;
    }
    
    // Current Path Display (with basic truncation if too long)
    std::string pathDisplay = state.editor.currentPath;
    if (pathDisplay.length() > 50) pathDisplay = "..." + pathDisplay.substr(pathDisplay.length() - 47);
    DrawText(pathDisplay.c_str(), winX + 10, winY + 50, 10, LIGHTGRAY);
    
    // "Up" button
    if (DrawButton({winX + 10, winY + 70, 40, 30}, "..", DARKGRAY, WHITE, mousePos)) {
        GoUp(state);
    }
    
    // Scrollbar dimensions
    float scrollbarW = 20;
    Rectangle listRect = {winX + 10, winY + 110, winW - 30 - scrollbarW, winH - 160};
    Rectangle scrollbarTrack = {listRect.x + listRect.width + 5, listRect.y, scrollbarW, listRect.height};
    
    DrawRectangleRec(listRect, BLACK);
    
    float itemH = 40; // Taller for touch
    float totalItems = (float)(state.editor.dirList.size() + state.editor.fileList.size());
    float contentHeight = totalItems * itemH;
    float maxScroll = std::max(0.0f, contentHeight - listRect.height);
    
    // Calculate scrollbar thumb
    float thumbRatio = (contentHeight > 0) ? (listRect.height / contentHeight) : 1.0f;
    if (thumbRatio > 1.0f) thumbRatio = 1.0f;
    float thumbH = std::max(30.0f, listRect.height * thumbRatio);
    float scrollRatio = (maxScroll > 0) ? (state.editor.browserScrollY / maxScroll) : 0.0f;
    float thumbY = scrollbarTrack.y + scrollRatio * (scrollbarTrack.height - thumbH);
    Rectangle thumbRect = {scrollbarTrack.x, thumbY, scrollbarW, thumbH};
    
    // Draw scrollbar
    DrawRectangleRec(scrollbarTrack, Color{40, 40, 40, 255});
    DrawRectangleRec(thumbRect, Color{100, 100, 100, 255});
    
    // Scrollbar drag handling
    static bool isDraggingScrollbar = false;
    static float dragStartY = 0;
    static float dragStartScroll = 0;
    
    if (CheckCollisionPointRec(mousePos, scrollbarTrack)) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (CheckCollisionPointRec(mousePos, thumbRect)) {
                // Start dragging thumb
                isDraggingScrollbar = true;
                dragStartY = mousePos.y;
                dragStartScroll = state.editor.browserScrollY;
            } else {
                // Click on track - jump to position
                float clickRatio = (mousePos.y - scrollbarTrack.y) / scrollbarTrack.height;
                state.editor.browserScrollY = clickRatio * maxScroll;
            }
        }
    }
    
    if (isDraggingScrollbar) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float deltaY = mousePos.y - dragStartY;
            float scrollDelta = (deltaY / (scrollbarTrack.height - thumbH)) * maxScroll;
            state.editor.browserScrollY = dragStartScroll + scrollDelta;
        } else {
            isDraggingScrollbar = false;
        }
    }
    
    // Mouse wheel scrolling
    state.editor.browserScrollY -= GetMouseWheelMove() * 30.0f;
    
    // Clamp scroll
    if (state.editor.browserScrollY < 0) state.editor.browserScrollY = 0;
    if (state.editor.browserScrollY > maxScroll) state.editor.browserScrollY = maxScroll;
    
    // Clip handling for list content
    BeginScissorMode((int)listRect.x, (int)listRect.y, (int)listRect.width, (int)listRect.height);
    
    float startY = listRect.y - state.editor.browserScrollY;
    float y = startY;
    
    // Only allow item clicks if NOT dragging scrollbar
    bool canClickItems = !isDraggingScrollbar;
    
    // Draw Directories
    for (const auto& dir : state.editor.dirList) {
        if (y + itemH > listRect.y && y < listRect.y + listRect.height) {
            Rectangle itemRect = {listRect.x, y, listRect.width, itemH};
            // Only check hover if mouse is within the list area
            bool inListArea = CheckCollisionPointRec(mousePos, listRect);
            bool hover = inListArea && CheckCollisionPointRec(mousePos, itemRect);
            
            if (hover) DrawRectangleRec(itemRect, Color{60, 60, 60, 255});
            
            DrawText(("/ " + dir).c_str(), listRect.x + 5, y + 10, 20, YELLOW);
            
            if (canClickItems && hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                fs::path p(state.editor.currentPath);
                p /= dir;
                NavigateTo(state, p.string());
            }
        }
        y += itemH;
    }
    
    // Draw Files
    for (const auto& file : state.editor.fileList) {
        if (y + itemH > listRect.y && y < listRect.y + listRect.height) {
            Rectangle itemRect = {listRect.x, y, listRect.width, itemH};
            bool isSelected = (file == state.editor.selectedFileBuffer);
            // Only check hover if mouse is within the list area (not in footer/header)
            bool inListArea = CheckCollisionPointRec(mousePos, listRect);
            bool hover = inListArea && CheckCollisionPointRec(mousePos, itemRect);
            
            if (isSelected) DrawRectangleRec(itemRect, Color{0, 100, 200, 255});
            else if (hover) DrawRectangleRec(itemRect, Color{60, 60, 60, 255});
            
            DrawText(file.c_str(), listRect.x + 5, y + 10, 20, WHITE);
            
            if (canClickItems && hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                strcpy(state.editor.selectedFileBuffer, file.c_str());
                LOGD("Selected file: %s", file.c_str());
            }
        }
        y += itemH;
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
}

} // namespace FileBrowser
