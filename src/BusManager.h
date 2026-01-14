#pragma once

#include <string>
#include <map>
#include <juce_audio_basics/juce_audio_basics.h>

#include <memory>
#include <mutex>
#include <vector>
#include "fx/TrackEffect.h"

namespace fx {
    struct FXSlot; // Forward declaration for future FX integration
}

struct AudioBus {
    std::string name;
    float volume = 1.0f;
    float pan = 0.5f;  // 0.0=Left, 0.5=Center, 1.0=Right
    juce::AudioBuffer<float> buffer;
    
    // Track-level FX chain
    std::vector<std::shared_ptr<fx::TrackEffect>> effects;
    mutable std::mutex effectsMutex;
    
    // AudioBus is not copyable due to mutex
    AudioBus() = default;
    AudioBus(const AudioBus&) = delete;
    AudioBus& operator=(const AudioBus&) = delete;
    
    void prepareBuffer(int numSamples, int numChannels);
    void clearBuffer();
    // Process chain of effects
    void processEffects(int numSamples, int numChannels);
    
    void applyVolumeAndPan(int numChannels);
};

class BusManager {
public:
    BusManager();
    ~BusManager();
    
    void initialize(double sampleRate, int numChannels);
    void prepareBuffers(int numSamples);
    
    // Track/Bus Management
    AudioBus* getOrCreateTrack(const std::string& trackName);
    AudioBus* getTrack(const std::string& trackName);
    AudioBus* getMasterBus();
    
    // Audio Processing
    void mixTracksToMaster();
    void applyMasterProcessing();
    
    // Clear all track buffers (called at start of each audio callback)
    void clearAllBuffers();
    
private:
    std::map<std::string, std::unique_ptr<AudioBus>> tracks;
    mutable std::mutex busMutex;
    AudioBus masterBus;
    double sampleRate = 44100.0;
    int numChannels = 2;
    int currentBufferSize = 0;
};
