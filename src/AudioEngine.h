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
    
    // Audio Device Management
    std::vector<std::string> getAvailableOutputDevices();
    std::string getCurrentOutputDevice();
    bool setOutputDevice(const std::string& deviceName);
    
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
    std::atomic<uint64_t> audioFrameCount{0}; // Heartbeat counter
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
        int64_t sampleEndPosition = 0; // For Nudge/Crop FX
        int64_t sliceEndPosition = -1; // -1 if not slicing or play through
        bool stopAtSliceEnd = false; 
        int64_t fadeInSamplesRemaining = 0; // For 2ms fade-in (88 samples at 44100Hz)
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
        int stutterCounter = 0;
    
        // Filter State
        struct SimpleFilter {
            float low = 0.0f, band = 0.0f, high = 0.0f;
            void reset() { low=0; band=0; high=0; }
            
            // Simple SVF (State Variable Filter)
            float process(float input, float cutoff, float res) {
                cutoff = (cutoff > 0.99f) ? 0.99f : cutoff;
                // f = 2 * sin(pi * cutoff / sampleRate) -> approx for low cutoffs: 2*pi*fc/fs
                // We'll treat 'cutoff' as the generic f coefficient directly for simplicity here
                // or do quick calc: 
                float f = cutoff; 
                
                low = low + f * band;
                high = input - low - res * band;
                band = band + f * high;
                return low;
            }
        } filter;

        void reset() {
            samplePosition = 0;
            currentStep = 0;
            stepStartSample = 0;
            samplePlaybackPosition = 0.0;
            sampleEndPosition = 0;
            sliceEndPosition = -1;
            stopAtSliceEnd = false;
            sampleIsPlaying = false;
            currentSpeedRatio = 1.0;
            currentVelocity = 1.0f;
            isSliding = false;
            slideTargetRatio = 1.0;
            slideStepIncrement = 0.0;
            isStuttering = false;
            stutterIntervalSamples = 0;
            stutterCounter = 0;
            filter.reset();
        }
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
    int64_t samplePlaybackEnd = 0;
    bool sampleIsPlaying = false;
    
    void advanceSequencer(int numSamples);
    void triggerSample(Pattern& pattern, int step);
};
