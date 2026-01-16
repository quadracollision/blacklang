#pragma once

#include "Pattern.h"
#include "PatternChain.h"
#include "SharedAudioStructs.h"
#include "BusManager.h"
#include "fx/FXProcessor.h"
#include "AudioRecorder.h"
#include "ResampleManager.h"
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

    void updateActivePatterns(const std::vector<std::pair<std::string, std::string>>& patternsWithTracks); // Live switching with explicit tracks
    void queuePatternSwitch(const std::string& trackName, const std::string& newPatternName); // Per-slot sync: queue pattern to switch at end of current
    int getPatternProgress(const std::string& name); // Visual feedback
    void stop();
    void clearAllPatterns();  // Clear all patterns for new project
    void pause();
    void resume();
    bool isPlaying() const { return playing.load(); }

    bool isPaused() const { return paused.load(); }

    void setBPM(int newBpm) { globalBpm.store(newBpm); }
    int getBPM() const { return globalBpm.load(); }
    
    // Sync
    void scheduleResync();
    void resyncAllPatterns(); // Immediate (legacy/force)
    
    // Preview
    void previewSlice(Pattern& pattern, int sliceIndex, bool playToEnd = false);
    
    // Bus/Track Management
    void assignPatternToTrack(const std::string& patternName, const std::string& trackName);
    std::string getTrackForPattern(const std::string& patternName) const;
    AudioBus* getTrackBus(const std::string& trackName);
    
    // Audio Device Management
    std::vector<std::string> getAvailableOutputDevices();
    std::string getCurrentOutputDevice();
    bool setOutputDevice(const std::string& deviceName);
    void setOutputDeviceAsync(const std::string& deviceName, std::function<void(bool)> callback);
    bool isDeviceSwitching() const { return deviceSwitching.load(); }

    // Recording (Legacy file-based)
    void startRecording(const std::string& filename, bool stems);
    void stopRecording();
    bool isRecording();
    
    // In-Memory Recording (New - for Android)
    void armRecording(bool stems);
    void disarmRecording();
    bool isRecordingArmed() const { return recordingArmed.load(); }
    void startInMemoryRecording();  // Called when Play pressed while armed
    void stopInMemoryRecording();   // Called when Stop pressed while recording
    
    // Access recorded buffers (after stopInMemoryRecording)
    const juce::AudioBuffer<float>& getRecordedMaster() const { return recordedMasterBuffer; }
    const std::map<std::string, juce::AudioBuffer<float>>& getRecordedStems() const { return recordedStemBuffers; }
    int getRecordedSampleCount() const { return recordedSampleCount; }
    double getRecordedSampleRate() const { return sampleRate; }
    void clearRecordedBuffers();
    
    // Preview playback of recorded audio
    void startRecordingPreview(const std::string& stemName = "");
    void stopRecordingPreview();
    void seekRecordingPreview(int64_t sample);
    bool isPreviewingRecording() const { return recordingPreviewActive.load(); }
    
    // Save recorded audio to file
    bool saveRecordedAudio(const std::string& filepath);
    bool saveRecordedStems(const std::string& directory, const std::string& baseName);
    bool saveRecordingWrapper(const std::string& filepath);
    
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
    
    // Resample Manager (public for PatternEditor access)
    ResampleManager resampleManager;

private:
    juce::AudioDeviceManager deviceManager;
    juce::AudioFormatManager formatManager;
    
    std::map<std::string, Pattern> patterns;
    mutable std::mutex patternMutex;
    
    // Playback state
    std::atomic<bool> playing{false};
    std::atomic<bool> paused{false};
    std::atomic<uint64_t> audioFrameCount{0}; // Heartbeat counter
    std::string currentPatternName;
    PatternChain currentChain;
    int chainIndex = 0;
    
    // Multi-pattern playback
    std::vector<std::string> activePatternNames;
    
    // Sequencer state
    double sampleRate = 44100.0;
    std::atomic<int> globalBpm{120};

    // Global/Legacy Sequencer state (for single-pattern mode)
    int64_t samplePosition = 0;
    int currentStep = 0;
    int64_t stepStartSample = 0;
    
    // Global Sample playback
    int64_t samplePlaybackPosition = 0;
    int64_t samplePlaybackEnd = 0;
    bool sampleIsPlaying = false;
    bool sampleIsReverse = false;
    
    std::map<std::string, PatternPlayState> patternStates;

    void advanceSequencer(int numSamples);
    // Refactored helper to trigger notes (pitch, velocity, FX)
    void triggerStep(PatternPlayState& state, Pattern& pattern);
    
    // Sync internals
    void resyncAllPatternsInternal();
    std::atomic<bool> pendingResync{false};
    // Preview Logic
    struct PreviewState {
        int64_t position = 0;
        int64_t endPosition = 0;
        bool active = false;
        Pattern* sourcePattern = nullptr;
        int fadeInSamplesRemaining = 0;
    } previewState;
    
    // FX
    fx::FXProcessor fxProcessor;
    
    // Bus Manager
    BusManager busManager;
    std::map<std::string, std::string> patternToTrack;  // Pattern name -> Track name mapping
    
    // Recorder
    AudioRecorder mainRecorder;  // Records master bus (whole mix)
    std::map<std::string, AudioRecorder> stemRecorders;  // Records individual track buses
    bool recordingStems = false;
    std::mutex recordingMutex; // Protects access to recorders
    
    // Device switching
    std::atomic<bool> deviceSwitching{false};

    // Per-Slot Sync Queueing
    std::map<std::string, std::string> pendingPatternQueues; // TrackName -> Queued PatternName
    std::mutex queueMutex;

    // In-Memory Recording (New - for Android)
    std::atomic<bool> recordingArmed{false};
    std::atomic<bool> inMemoryRecording{false};
    bool inMemoryRecordingStems = false;
    juce::AudioBuffer<float> recordedMasterBuffer;
    std::map<std::string, juce::AudioBuffer<float>> recordedStemBuffers;
    int recordedSampleCount = 0;
    int recordedBufferCapacity = 0;
    
    // Recording Preview
    std::atomic<bool> recordingPreviewActive{false};
    int64_t recordingPreviewPosition = 0;
    std::string recordingPreviewStem = "";
};
