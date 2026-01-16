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
    int syncBase = 0;  // 0 = fit to 1 bar, >0 = that many steps per bar (enables polyrhythms)
    std::vector<int> activeSteps;  // 1-indexed step positions
    std::vector<int> sliceMarkers; // Sample indices for slice points
    

    
    // Mixer Properties
    float volume = 1.0f;
    float pan = 0.5f; // 0.0=Left, 0.5=Center, 1.0=Right
    
    // Sample Fade Settings
    float fadeIn = 0.0f; // Global Fade In (0.0-0.5)
    float fadeOut = 0.0f; // Global Fade Out (0.0-0.5)
    bool fadeSlices = false; // If true, use per-slice settings (or slice mode logic)
    
    // Per-Slice Fade Overrides (Index -> Value)
    std::map<int, float> sliceFadeIns; 
    std::map<int, float> sliceFadeOuts;

    std::map<int, int> stepPitches; // Step (1-indexed) -> Semitone Offset (relative to C4/Root)
    std::map<int, float> stepVelocities; // Step (1-indexed) -> Velocity [0.0 - 1.0]
    std::map<int, std::vector<int>> stepFX; // Step (1-indexed) -> List of FX IDs
    std::map<int, std::map<int, float>> stepFXParams; // Step -> FX ID -> Value

    static constexpr int FX_NONE = 0;
    static constexpr int FX_CUTOFF = 1;
    static constexpr int FX_SLIDE = 2;
    static constexpr int FX_STUTTER = 3;
    static constexpr int FX_NUDGE = 4;
    static constexpr int FX_SLICE = 5;
    static constexpr int FX_REVERSE = 6;
    static constexpr int FX_ATTACK = 16;
    static constexpr int FX_DECAY = 17;
    static constexpr int FX_SUSTAIN = 18;
    static constexpr int FX_RELEASE = 19;
    static constexpr int FX_ADSR = 20;
    
    // Parameters
    static constexpr int PAR_STUTTER_RATE = 100;
    static constexpr int PAR_STUTTER_SPEED = 101;
    static constexpr int PAR_SLIDE_TIME = 200;
    static constexpr int PAR_SLIDE_SQUELCH = 201;
    static constexpr int PAR_NUDGE_OFFSET = 300;
    static constexpr int PAR_SLICE_INDEX = 400; // 0-indexed slice index
    static constexpr int PAR_SLICE_CUTOFF = 401; // 1.0 = Cut at next slice, 0.0 = Play through
    static constexpr int PAR_REVERSE_MODE = 500;
    
    static constexpr int PAR_ATTACK_TIME = 1300;
    static constexpr int PAR_DECAY_TIME = 1301;
    static constexpr int PAR_SUSTAIN_LEVEL = 1302;
    static constexpr int PAR_RELEASE_TIME = 1303;
    
    // EQ Parameters (1400-1499)
    static constexpr int FX_EQ = 21;
    static constexpr int PAR_EQ_BAND1 = 1400;  // 60Hz  (-1 to 1 = -12dB to +12dB)
    static constexpr int PAR_EQ_BAND2 = 1401;  // 250Hz
    static constexpr int PAR_EQ_BAND3 = 1402;  // 1kHz
    static constexpr int PAR_EQ_BAND4 = 1403;  // 4kHz
    static constexpr int PAR_EQ_BAND5 = 1404;  // 12kHz
    
    // Audio data (loaded at runtime)
    juce::AudioBuffer<float> sampleBuffer;
    double sampleRate = 44100.0;
    
    bool isValid() const {
        return !name.empty() && !samplePath.empty() && steps > 0;
    }
    
    // Get step duration in samples
    // Get step duration in samples
    double getStepDurationSamples(int currentBpm, double engineSampleRate) const {
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
        
        int baseSteps = (syncBase > 0) ? syncBase : steps;
        double beatsPerStep = 4.0 / baseSteps;
        double secondsPerBeat = 60.0 / currentBpm;
        return secondsPerBeat * beatsPerStep * engineSampleRate;
    }
    
    // Check if a step should trigger the sample (1-indexed input)
    bool shouldTriggerAt(int step) const {
        for (int s : activeSteps) {
            if (s == step) return true;
        }
        return false;
    }
};
