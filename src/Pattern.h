#pragma once

#include <string>
#include <vector>
#include <map>
#include <juce_audio_basics/juce_audio_basics.h>

struct Pattern {
    std::string name;
    std::string samplePath;
    int bpm = 120;
    int steps = 16;
    std::vector<int> activeSteps;  // 1-indexed step positions

    std::map<int, int> stepPitches; // Step (1-indexed) -> Semitone Offset (relative to C4/Root)
    std::map<int, float> stepVelocities; // Step (1-indexed) -> Velocity [0.0 - 1.0]
    std::map<int, std::vector<int>> stepFX; // Step (1-indexed) -> List of FX IDs
    std::map<int, std::map<int, float>> stepFXParams; // Step -> FX ID -> Value

    static constexpr int FX_NONE = 0;
    static constexpr int FX_CUTOFF = 1;
    static constexpr int FX_SLIDE = 2;
    static constexpr int FX_STUTTER = 3;
    
    // Parameters
    static constexpr int PAR_STUTTER_RATE = 100;
    static constexpr int PAR_STUTTER_SPEED = 101;
    
    // Audio data (loaded at runtime)
    juce::AudioBuffer<float> sampleBuffer;
    double sampleRate = 44100.0;
    
    bool isValid() const {
        return !name.empty() && !samplePath.empty() && steps > 0;
    }
    
    // Get step duration in samples
    double getStepDurationSamples(int currentBpm) const {
        // Use standard 1/16th note steps mechanism usually?
        // Or "4.0 / steps" implies 1 bar?
        // If 16 steps = 1 bar.
        // 4 beats per bar.
        // 4 steps per beat.
        // step = 1/4 beat.
        // secondsPerBeat = 60 / bpm.
        // secondsPerStep = (60/bpm) / (steps/4.0) ?
        
        // Existing logic: double beatsPerStep = 4.0 / steps;
        // If steps=16 -> beatsPerStep = 0.25.
        // duration = (60/bpm) * 0.25. Correct for 16th notes.
        
        double beatsPerStep = 4.0 / steps;  
        double secondsPerBeat = 60.0 / currentBpm;
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
