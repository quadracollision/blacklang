#pragma once

#include "Pattern.h"
#include "PatternChain.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <atomic>
#include <mutex>

class AudioEngine : public juce::AudioIODeviceCallback {
public:
    AudioEngine();
    ~AudioEngine() override;
    
    // Initialize audio
    bool initialize();
    void shutdown();
    
    // Sample loading
    bool loadSample(Pattern& pattern);
    
    // Pattern management
    void addPattern(const Pattern& pattern);
    Pattern* getPattern(const std::string& name);
    const std::map<std::string, Pattern>& getPatterns() const { return patterns; }
    
    // Playback control
    void playPattern(const std::string& name);
    void playChain(const PatternChain& chain);
    void playMultiplePatterns(const std::vector<std::string>& names);
    void updateActivePatterns(const std::vector<std::string>& names); // Live switching
    int getPatternProgress(const std::string& name); // Visual feedback
    void stop();
    void pause();
    void resume();
    bool isPlaying() const { return playing.load(); }

    bool isPaused() const { return paused.load(); }

    void setBPM(int newBpm) { globalBpm.store(newBpm); }
    int getBPM() const { return globalBpm.load(); }
    
    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData,
        int numInputChannels,
        float* const* outputChannelData,
        int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext& context) override;
    
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    juce::AudioDeviceManager deviceManager;
    juce::AudioFormatManager formatManager;
    
    std::map<std::string, Pattern> patterns;
    std::mutex patternMutex;
    
    // Playback state
    std::atomic<bool> playing{false};
    std::atomic<bool> paused{false};
    std::string currentPatternName;
    PatternChain currentChain;
    int chainIndex = 0;
    
    // Multi-pattern playback
    std::vector<std::string> activePatternNames;
    struct PatternPlayState {
        int64_t samplePosition = 0;
        int currentStep = 0;
        int64_t stepStartSample = 0;
        double samplePlaybackPosition = 0.0;
        bool sampleIsPlaying = false;
        double currentSpeedRatio = 1.0;
        float currentVelocity = 1.0f;
        
        // Slide / Portamento
        bool isSliding = false;
        double slideTargetRatio = 1.0;
        double slideStepIncrement = 0.0;
        
        // Stutter State
        bool isStuttering = false;
        int stutterIntervalSamples = 0;
    };
    std::map<std::string, PatternPlayState> patternStates;
    
    // Sequencer state
    double sampleRate = 44100.0;
    int64_t samplePosition = 0;
    int currentStep = 0;
    int64_t stepStartSample = 0;
    std::atomic<int> globalBpm{120};
    
    // Sample playback
    int64_t samplePlaybackPosition = 0;
    bool sampleIsPlaying = false;
    
    void advanceSequencer(int numSamples);
    void triggerSample();
};
