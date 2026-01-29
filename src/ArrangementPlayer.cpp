#include "ArrangementPlayer.h"
#include "AudioEngine.h"
#include "GuiState.h"
#include <algorithm>
#include <tuple>

ArrangementPlayer::ArrangementPlayer(AudioEngine& engine, GuiState& state)
    : engine(engine), state(state) {}

void ArrangementPlayer::start() {
    if (!playing) {
        playing = true;
        // Note: playheadBeat is NOT reset here - start from current cursor position
        activeClipsPerTrack.clear();
        bpm = engine.getBPM();
        
        // Trigger patterns immediately for current beat position
        updateActiveClips();
    }
}

void ArrangementPlayer::stop() {
    if (playing) {
        playing = false;
        
        // Stop all patterns that were being played by arrangement
        engine.stop();
        
        // Clear arrangement play state
        state.arrangementPlayState.isPlaying = false;
        state.arrangementPlayState.activeClipsPerTrack.clear();
        activeClipsPerTrack.clear();
    }
    
    // Always reset cursor to beginning when stopped
    playheadBeat = 0.0;
}

void ArrangementPlayer::update(float deltaTime) {
    if (!playing) return;
    
    // Advance playhead
    bpm = engine.getBPM();
    double beatsPerSecond = bpm / 60.0;
    playheadBeat += beatsPerSecond * deltaTime;
    
    // Update active clips
    updateActiveClips();
    
    // Update GUI state for visual feedback
    state.arrangementPlayState.isPlaying = true;
    state.arrangementPlayState.activeClipsPerTrack = activeClipsPerTrack;
}

void ArrangementPlayer::updateActiveClips() {
    Arrangement* arrangement = engine.getArrangement();
    if (!arrangement) return;
    
    std::map<std::string, std::string> newActiveClips;
    // Now track beat offset within each clip: patternName, trackName, beatOffset
    std::vector<std::tuple<std::string, std::string, double>> patternsToPlay;
    
    // For each track, find clip that contains current playhead
    for (auto& track : arrangement->tracks) {
        std::string activePattern = "";
        double beatOffset = 0.0;
        
        for (auto& clip : track.clips) {
            double clipEnd = clip.startBeat + clip.lengthBeats;
            if (playheadBeat >= clip.startBeat && playheadBeat < clipEnd) {
                activePattern = clip.patternName;
                // Calculate how far into the clip (and thus pattern) we are
                beatOffset = playheadBeat - clip.startBeat;
                break; // First matching clip wins
            }
        }
        
        if (!activePattern.empty()) {
            newActiveClips[track.trackName] = activePattern;
            patternsToPlay.push_back({activePattern, track.trackName, beatOffset});
        }
    }
    
    // Detect changes and update AudioEngine
    bool changed = (newActiveClips != activeClipsPerTrack);
    
    if (changed) {
        if (!patternsToPlay.empty()) {
            // Use the offset-aware version for proper seeking
            engine.updateActivePatternsWithOffset(patternsToPlay);
        } else {
            // No clips active - stop playback
            engine.stop();
        }
        
        activeClipsPerTrack = newActiveClips;
    }
    
    // Check if we've passed the end of all clips (auto-stop or loop)
    double maxEndBeat = 0.0;
    for (auto& track : arrangement->tracks) {
        for (auto& clip : track.clips) {
            maxEndBeat = std::max(maxEndBeat, clip.startBeat + clip.lengthBeats);
        }
    }
    
    if (maxEndBeat > 0 && playheadBeat >= maxEndBeat) {
        // Reached end - stop playback and reset to beginning
        stop();
        playheadBeat = 0.0;
    }
}
