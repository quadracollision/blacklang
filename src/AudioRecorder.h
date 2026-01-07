#pragma once

#include <vector>
#include <atomic>
#include <thread>
#include <string>
#include <mutex>
#include <juce_audio_formats/juce_audio_formats.h>

class AudioRecorder {
public:
    AudioRecorder();
    ~AudioRecorder();
    
    // Start recording to the specified file
    void start(const std::string& filePath, double sampleRate, int numChannels);
    
    // Stop recording and close file
    void stop();
    
    bool isRecording() const;
    
    // Push audio data into the buffer (Interleaved if > 1 channel)
    // Audio Thread Safe
    void writeBlock(const float* const* channelData, int numSamples, int numIncomingChannels);

private:
    void backgroundTask(const std::string& path, double sr, int channels);
    
    std::atomic<bool> recording{false}; // UI state
    std::atomic<bool> workerRunning{false}; // Thread state
    std::thread workerThread;
    
    // Ring Buffer (Interleaved samples)
    // 10 seconds of stereo at 48k = 48000 * 2 * 10 = 960,000 floats (~4MB)
    static const size_t BUFFER_SAMPLES = 1048576; // 1M samples Power of 2
    std::vector<float> buffer;
    
    std::atomic<size_t> writeIndex{0};
    std::atomic<size_t> readIndex{0};
    
    int currentNumChannels = 2;
};
