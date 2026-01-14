#include "BusManager.h"
#include <algorithm>
#include <cmath>

// ============================================================================
// AudioBus Implementation
// ============================================================================

void AudioBus::prepareBuffer(int numSamples, int numChannels) {
    if (buffer.getNumChannels() != numChannels || buffer.getNumSamples() != numSamples) {
        buffer.setSize(numChannels, numSamples, false, true, false);
    }
}

void AudioBus::clearBuffer() {
    buffer.clear();
}

void AudioBus::processEffects(int numSamples, int numChannels) {
    std::lock_guard<std::mutex> lock(effectsMutex);
    if (effects.empty()) return;
    
    // Process each effect in chain
    for (auto& effect : effects) {
        if (effect && effect->isActive()) {
            effect->process(buffer);
        }
    }
}

void AudioBus::applyVolumeAndPan(int numChannels) {
    if (numChannels < 2) {
        // Mono output: just apply volume
        buffer.applyGain(volume);
        return;
    }
    
    // Stereo output: apply volume and pan
    // Pan law: constant power panning
    // Left = sqrt(1 - pan) * volume
    // Right = sqrt(pan) * volume
    
    float leftGain = std::sqrt(1.0f - pan) * volume * 1.414f;  // sqrt(2) for constant power
    float rightGain = std::sqrt(pan) * volume * 1.414f;
    
    // Clamp gains to reasonable range
    leftGain = std::min(leftGain, 2.0f);
    rightGain = std::min(rightGain, 2.0f);
    
    // Apply to channels
    if (buffer.getNumChannels() >= 1) {
        buffer.applyGain(0, 0, buffer.getNumSamples(), leftGain);
    }
    if (buffer.getNumChannels() >= 2) {
        buffer.applyGain(1, 0, buffer.getNumSamples(), rightGain);
    }
    
    // Additional channels use mono volume
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) {
        buffer.applyGain(ch, 0, buffer.getNumSamples(), volume);
    }
}

// ============================================================================
// BusManager Implementation
// ============================================================================

BusManager::BusManager() {
    masterBus.name = "Master";
}

BusManager::~BusManager() {
}

void BusManager::initialize(double sr, int channels) {
    sampleRate = sr;
    numChannels = channels;
    
    // Initialize master bus
    masterBus.prepareBuffer(512, numChannels);  // Default size, will resize as needed
}

void BusManager::prepareBuffers(int numSamples) {
    currentBufferSize = numSamples;
    
    std::lock_guard<std::mutex> lock(busMutex);
    // Prepare all track buffers
    for (auto& pair : tracks) {
        pair.second->prepareBuffer(numSamples, numChannels);
    }
    
    // Prepare master bus
    masterBus.prepareBuffer(numSamples, numChannels);
}

void BusManager::clearAllBuffers() {
    std::lock_guard<std::mutex> lock(busMutex);
    // Clear all track buffers
    for (auto& pair : tracks) {
        pair.second->clearBuffer();
    }
    
    // Clear master bus
    masterBus.clearBuffer();
}

AudioBus* BusManager::getOrCreateTrack(const std::string& trackName) {
    std::lock_guard<std::mutex> lock(busMutex);
    // Check if track exists
    auto it = tracks.find(trackName);
    if (it != tracks.end()) {
        return it->second.get();
    }
    
    // Create new track
    auto newBus = std::make_unique<AudioBus>();
    newBus->name = trackName;
    newBus->prepareBuffer(currentBufferSize, numChannels);
    
    tracks[trackName] = std::move(newBus);
    return tracks[trackName].get();
}

AudioBus* BusManager::getTrack(const std::string& trackName) {
    std::lock_guard<std::mutex> lock(busMutex);
    auto it = tracks.find(trackName);
    if (it != tracks.end()) {
        return it->second.get();
    }
    return nullptr;
}

AudioBus* BusManager::getMasterBus() {
    return &masterBus;
}

void BusManager::mixTracksToMaster() {
    std::vector<AudioBus*> tracksToProcess;
    {
        std::lock_guard<std::mutex> lock(busMutex);
        for (auto& pair : tracks) {
            tracksToProcess.push_back(pair.second.get());
        }
    }

    // Sum all track outputs to master bus
    for (auto* trackBusPtr : tracksToProcess) {
        AudioBus& trackBus = *trackBusPtr;
        
        // Apply Insert FX
        trackBus.processEffects(currentBufferSize, numChannels);
        
        // First apply volume and pan to the track
        trackBus.applyVolumeAndPan(numChannels);
        
        // Then add to master
        for (int ch = 0; ch < numChannels; ++ch) {
            if (ch < trackBus.buffer.getNumChannels() && ch < masterBus.buffer.getNumChannels()) {
                masterBus.buffer.addFrom(
                    ch,  // destination channel
                    0,   // destination start sample
                    trackBus.buffer,  // source buffer
                    ch,  // source channel
                    0,   // source start sample
                    currentBufferSize  // number of samples
                );
            }
        }
    }
}

void BusManager::applyMasterProcessing() {
    // Apply master volume and pan (though pan on master is usually centered)
    masterBus.applyVolumeAndPan(numChannels);
    
    // Future: Apply master FX chain here
    // - Limiter
    // - Maximizer
    // - Dithering
    
    // For now, just apply soft clipping to prevent digital clipping
    for (int ch = 0; ch < masterBus.buffer.getNumChannels(); ++ch) {
        float* channelData = masterBus.buffer.getWritePointer(ch);
        for (int i = 0; i < masterBus.buffer.getNumSamples(); ++i) {
            float sample = channelData[i];
            
            // Proper soft clip using tanh - saturates at 1.0
            if (std::abs(sample) > 0.8f) {
                float sign = (sample > 0) ? 1.0f : -1.0f;
                float x = std::abs(sample);
                // Soft curve from 0.8 to 1.0
                sample = sign * (0.8f + 0.2f * std::tanh((x - 0.8f) / 0.2f));
            }
            
            // Hard limit at ±1.0 (safety)
            if (sample > 1.0f) sample = 1.0f;
            if (sample < -1.0f) sample = -1.0f;
            
            channelData[i] = sample;
        }
    }
}
