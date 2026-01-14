#include "FontManager.h"
#include "EmbeddedFont.h"
#include "SFIntermosaicFont.h"
#include <raylib.h>
#include <raylib.h>
#include <iostream>

FontManager& FontManager::Get() {
    static FontManager instance;
    return instance;
}

void FontManager::Init() {
    if (currentFont.texture.id != 0) return; // Already initialized

    TraceLog(LOG_INFO, "FontManager: Initializing fonts...");

    // 1. Try to load Hachicro (User Preferred)
    // IMPORTANT: Raylib LoadFontFromMemory expects fileType (extension)
    // Using ".ttf" usually works.
    
    // Attempt SF Intermosaic Bold
    TraceLog(LOG_INFO, "FontManager: Attempting to load SF Intermosaic Bold...");
    Font mainFont = LoadFontFromMemory(".ttf", build_desktop_SF_Intermosaic_Bold_ttf, build_desktop_SF_Intermosaic_Bold_ttf_len, 32, 0, 0);
    
    if (mainFont.texture.id != 0) {
        currentFont = mainFont;
        // User requested no "unnatural scaling" issues. 
        // Bilinear is safer for vector fonts to avoid jaggy edges unless it's a pixel font.
        // Given "Bold" in name, it's likely vector.
        SetTextureFilter(currentFont.texture, TEXTURE_FILTER_BILINEAR); 
        TraceLog(LOG_INFO, "FontManager: SF Intermosaic Bold loaded successfully!");
    } else {
        TraceLog(LOG_WARNING, "FontManager: Failed to load Hachicro.");
        
        // 2. Fallback to Anonymous Pro (Known Good)
        TraceLog(LOG_INFO, "FontManager: Attempting to load Anonymous Pro(Fallback)...");
        // Using the variable names found in EmbeddedFont.h from previous steps
        // build_desktop__deps_raylib_src_examples_text_resources_anonymous_pro_bold_ttf
        Font anon = LoadFontFromMemory(".ttf", 
            build_desktop__deps_raylib_src_examples_text_resources_anonymous_pro_bold_ttf, 
            build_desktop__deps_raylib_src_examples_text_resources_anonymous_pro_bold_ttf_len, 
            32, 0, 0);
            
        if (anon.texture.id != 0) {
            currentFont = anon;
            // Anonymous Pro is vector, Bilinear is usually better, but Point is fine for retro feel
            SetTextureFilter(currentFont.texture, TEXTURE_FILTER_BILINEAR); 
            TraceLog(LOG_INFO, "FontManager: Anonymous Pro loaded as fallback.");
        } else {
            TraceLog(LOG_ERROR, "FontManager: Failed to load Fallback font. Using System Default.");
        }
    }
}

void FontManager::Draw(const char* text, int x, int y, int fontSize, Color color) {
    if (currentFont.texture.id != 0) {
        Vector2 pos = {(float)x, (float)y};
        // Use 1/10th of fontSize for spacing, typical for Raylib
        DrawTextEx(currentFont, text, pos, (float)fontSize, (float)fontSize/10.0f, color);
    } else {
        DrawText(text, x, y, fontSize, color);
    }
}

int FontManager::Measure(const char* text, int fontSize) {
    if (currentFont.texture.id != 0) {
        Vector2 size = MeasureTextEx(currentFont, text, (float)fontSize, (float)fontSize/10.0f);
        return (int)size.x;
    } else {
        return MeasureText(text, fontSize);
    }
}
