#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Pattern.h"
#include <atomic>

/**
 * ResampleManager - Self-contained class for recording one pattern cycle
 * and transferring the audio to a pattern's sample buffer.
 * 
 * Usage:
 *   1. Call start(patternSteps, bpm, sampleRate) when user clicks Resample
 *   2. Call feedSamples() from audio callback while recording
 *   3. Call update(currentStep) each frame to detect cycle completion
 *   4. When isComplete() returns true, call transferToPattern()
 */
class ResampleManager {
public:
    ResampleManager() = default;
    
    /**
     * Start a new resample recording session.
     * @param patternSteps Number of steps in the pattern
     * @param bpm Current tempo
     * @param sampleRate Audio sample rate
     */
    void start(int patternSteps, int bpm, double sampleRate) {
        // Calculate expected duration
        double beatsPerStep = 4.0 / patternSteps;  // Assuming 4 beats per bar
        double secondsPerBeat = 60.0 / bpm;
        double totalSeconds = secondsPerBeat * beatsPerStep * patternSteps;
        
        // Add 10% buffer for safety
        int expectedSamples = (int)(totalSeconds * sampleRate * 1.1);
        
        // Cap at 30 seconds max (reasonable limit)
        int maxSamples = (int)(sampleRate * 30);
        if (expectedSamples > maxSamples) expectedSamples = maxSamples;
        
        // Allocate buffer
        resampleBuffer.setSize(2, expectedSamples, false, true, false);
        resampleBuffer.clear();
        
        recordedSamples = 0;
        bufferCapacity = expectedSamples;
        totalSteps = patternSteps;      // Store total steps for cycle detection
        startStep = -1;                 // Will be set on first update()
        lastStep = -1;
        hasPassedStart = false;
        completed.store(false);
        recording.store(true);
    }
    
    /**
     * Stop recording immediately.
     */
    void stop() {
        recording.store(false);
    }
    
    /**
     * Check if currently recording.
     */
    bool isRecording() const {
        return recording.load();
    }
    
    /**
     * Check if recording completed a full cycle.
     */
    bool isComplete() const {
        return completed.load();
    }
    
    /**
     * Feed audio samples from the audio callback.
     * Only call this when isRecording() is true.
     */
    void feedSamples(float* const* outputChannelData, int numSamples, int numChannels) {
        if (!recording.load()) return;
        
        int remain = bufferCapacity - recordedSamples;
        if (remain <= 0) {
            recording.store(false);
            completed.store(true);
            return;
        }
        
        int toRecord = numSamples < remain ? numSamples : remain;
        
        for (int ch = 0; ch < 2 && ch < numChannels; ++ch) {
            resampleBuffer.copyFrom(ch, recordedSamples, outputChannelData[ch], toRecord);
        }
        
        recordedSamples += toRecord;
    }
    
    /**
     * Update cycle detection. Call this from the UI thread each frame.
     * @param currentStep Current playback step (1-indexed)
     * @return true if cycle just completed
     */
    bool update(int currentStep) {
        if (!recording.load()) return false;
        
        // First update - record starting step
        if (startStep < 0) {
            startStep = currentStep;
            lastStep = currentStep;
            return false;
        }
        
        // Detect cycle completion: step wrapped from high to low
        // This handles any pattern length correctly
        // e.g., for 32 steps: going from step 32 to step 1 indicates wrap
        if (lastStep > currentStep && currentStep <= startStep) {
            // Only complete if we've actually moved past start
            if (hasPassedStart) {
                recording.store(false);
                completed.store(true);
                return true;
            }
        }
        
        // Track when we've advanced past start step
        if (!hasPassedStart && currentStep > startStep) {
            hasPassedStart = true;
        }
        
        lastStep = currentStep;
        return false;
    }
    
    /**
     * Transfer the recorded audio to a pattern's sample buffer.
     * Clears existing slice markers since the audio is new.
     */
    void transferToPattern(Pattern& pattern) {
        if (recordedSamples > 0) {
            // Copy the actually recorded portion
            pattern.sampleBuffer.setSize(2, recordedSamples, false, true, false);
            for (int ch = 0; ch < 2; ++ch) {
                pattern.sampleBuffer.copyFrom(ch, 0, resampleBuffer, ch, 0, recordedSamples);
            }
            
            // Clear slice markers since audio changed
            pattern.sliceMarkers.clear();
            
            // Update sample path to indicate it's resampled
            pattern.samplePath = "[resampled]";
        }
        
        // Reset state for next use
        reset();
    }
    
    /**
     * Get recorded sample count (for debug/display).
     */
    int getRecordedSamples() const {
        return recordedSamples;
    }
    
    /**
     * Reset all state.
     */
    void reset() {
        recording.store(false);
        completed.store(false);
        startStep = -1;
        lastStep = -1;
        hasPassedStart = false;
        recordedSamples = 0;
    }

private:
    juce::AudioBuffer<float> resampleBuffer;
    int recordedSamples = 0;
    int bufferCapacity = 0;
    
    int totalSteps = 16;        // Pattern step count
    int startStep = -1;         // Step when recording started
    int lastStep = -1;          // Previous step (for wrap detection)
    bool hasPassedStart = false;
    
    std::atomic<bool> recording{false};
    std::atomic<bool> completed{false};
};
