#pragma once

#include "Arrangement.h"
#include <string>
#include <map>
#include <functional>

class AudioEngine;
struct GuiState;

// Manages arrangement playback, driving pattern selection based on timeline clips
class ArrangementPlayer {
public:
    ArrangementPlayer(AudioEngine& engine, GuiState& state);
    
    // Playback control
    void start();
    void stop();
    void update(float deltaTime); // Call each frame
    
    // State queries
    bool isPlaying() const { return playing; }
    double getPlayheadBeat() const { return playheadBeat; }
    
    // Setters
    void setPlayheadBeat(double beat) { playheadBeat = beat; }
    void setBPM(int bpm) { this->bpm = bpm; }
    
private:
    AudioEngine& engine;
    GuiState& state;
    
    bool playing = false;
    double playheadBeat = 0.0;
    int bpm = 120;
    
    // Track which patterns are currently active per track (for change detection)
    std::map<std::string, std::string> activeClipsPerTrack;
    
    // Determine active clips at current beat and update AudioEngine
    void updateActiveClips();
};
