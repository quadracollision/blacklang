#pragma once

#include <vector>
#include <map>
#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>
#include "FXTypes.h"
#include "../Pattern.h"

// Forward declaration of internal state structures if needed
// Or we pass the state structure from AudioEngine

struct PatternPlayState; // Forward declare from AudioEngine.h (we might need to move this struct to shared header)

namespace fx {

class FXProcessor {
public:
    FXProcessor();
    
    // reset filter state etc
    void reset();

    // Process all FX for a given step
    // state: Current playback state for the pattern (sample pos, speed, etc)
    // pattern: The pattern data
    // step: 1-based step index
    // bpm: Current global BPM
    // Process all FX for a given step
    // state: Current playback state for the pattern (sample pos, speed, etc)
    // pattern: The pattern data
    // step: 1-based step index
    // bpm: Current global BPM
    void processStepFX(PatternPlayState& state, const Pattern& pattern, int step, double bpm, double sampleRate);

    // Process per-sample FX (like filter)
    // Returns processed sample
    float processSampleFX(PatternPlayState& state, float sample, double sampleRate, int step, const Pattern& pattern);
    
private:
    // Helpers
    void handleCutoff(PatternPlayState& state);
    void handleSlide(PatternPlayState& state, const Pattern& pattern, int currentStep, double bpm, double sampleRate);
    void handleStutter(PatternPlayState& state, const Pattern& pattern, int currentStep, double bpm, double sampleRate);
    void handleSlice(PatternPlayState& state, const Pattern& pattern, int currentStep);
    void handleNudge(PatternPlayState& state, const Pattern& pattern, int currentStep);
    void handleReverse(PatternPlayState& state, const Pattern& pattern, int currentStep);
};

} // namespace fx
