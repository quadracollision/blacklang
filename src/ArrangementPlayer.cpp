#include "ArrangementPlayer.h"
#include "AudioEngine.h"
#include "GuiState.h"
#include <algorithm>

ArrangementPlayer::ArrangementPlayer(AudioEngine& engine, GuiState& state)
    : engine(engine), state(state) {}

void ArrangementPlayer::start() {
    if (!playing) {
        playing = true;
        playheadBeat = 0.0;
        activeClipsPerTrack.clear();
        bpm = engine.getBPM();
        
        // Trigger patterns immediately for beat 0
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
    std::vector<std::pair<std::string, std::string>> patternsToPlay; // patternName, trackName
    
    // For each track, find clip that contains current playhead
    for (auto& track : arrangement->tracks) {
        std::string activePattern = "";
        
        for (auto& clip : track.clips) {
            double clipEnd = clip.startBeat + clip.lengthBeats;
            if (playheadBeat >= clip.startBeat && playheadBeat < clipEnd) {
                activePattern = clip.patternName;
                break; // First matching clip wins
            }
        }
        
        if (!activePattern.empty()) {
            newActiveClips[track.trackName] = activePattern;
            patternsToPlay.push_back({activePattern, track.trackName});
        }
    }
    
    // Detect changes and update AudioEngine
    bool changed = (newActiveClips != activeClipsPerTrack);
    
    if (changed) {
        // Build list of patterns with track assignments
        std::vector<std::pair<std::string, std::string>> patternTrackPairs;
        for (auto& pair : newActiveClips) {
            patternTrackPairs.push_back({pair.second, pair.first}); // patternName, trackName
        }
        
        if (!patternTrackPairs.empty()) {
            engine.updateActivePatterns(patternTrackPairs);
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
        // Reached end - stop playback
        stop();
    }
}
