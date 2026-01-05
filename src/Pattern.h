#pragma once

#include <string>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>

struct Pattern {
    std::string name;
    std::string samplePath;
    int bpm = 120;
    int steps = 16;
    std::vector<int> activeSteps;  // 1-indexed step positions
    
    // Audio data (loaded at runtime)
    juce::AudioBuffer<float> sampleBuffer;
    double sampleRate = 44100.0;
    
    bool isValid() const {
        return !name.empty() && !samplePath.empty() && steps > 0;
    }
    
    // Get step duration in samples
    double getStepDurationSamples() const {
        double beatsPerStep = 4.0 / steps;  // Assumes 4/4 time
        double secondsPerBeat = 60.0 / bpm;
        return secondsPerBeat * beatsPerStep * sampleRate;
    }
    
    // Check if a step should trigger the sample (1-indexed input)
    bool shouldTriggerAt(int step) const {
        for (int s : activeSteps) {
            if (s == step) return true;
        }
        return false;
    }
};
