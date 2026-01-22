#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>

struct TimelineEvent {
    std::string patternName;
    double startBeat = 0.0;
    double lengthBeats = 16.0; // Default 4 bars
    
    // UI selection state (transient)
    bool isSelected = false;
};

struct ArrangementTrack {
    std::string trackName;
    std::vector<TimelineEvent> clips;
};

class Arrangement {
public:
    std::vector<ArrangementTrack> tracks;
    
    // Helper to find track by name
    ArrangementTrack* getTrack(const std::string& name) {
        for (auto& t : tracks) {
            if (t.trackName == name) return &t;
        }
        return nullptr;
    }
    
    // Ensure track exists
    ArrangementTrack* getOrCreateTrack(const std::string& name) {
        ArrangementTrack* t = getTrack(name);
        if (t) return t;
        tracks.push_back({name, {}});
        return &tracks.back();
    }
    
    // Add clip
    void addClip(const std::string& trackName, const std::string& patternName, double startBeat, double lengthBeats) {
        std::lock_guard<std::mutex> lock(dataMutex);
        ArrangementTrack* t = getOrCreateTrack(trackName);
        t->clips.push_back({patternName, startBeat, lengthBeats});
    }
    
    // Remove clip by track and index
    void removeClip(const std::string& trackName, int clipIndex) {
        std::lock_guard<std::mutex> lock(dataMutex);
        ArrangementTrack* t = getTrack(trackName);
        if (t && clipIndex >= 0 && clipIndex < (int)t->clips.size()) {
            t->clips.erase(t->clips.begin() + clipIndex);
        }
    }
    
    // Clear
    void clear() {
        std::lock_guard<std::mutex> lock(dataMutex);
        tracks.clear();
    }
    
    std::mutex dataMutex;
};
