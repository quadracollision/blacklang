#pragma once

#include <vector>
#include <map>
#include <array>
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
    void handleAttack(PatternPlayState& state, const Pattern& pattern, int currentStep);
    void handleDecay(PatternPlayState& state, const Pattern& pattern, int currentStep);
    void handleSustain(PatternPlayState& state, const Pattern& pattern, int currentStep);
    void handleRelease(PatternPlayState& state, const Pattern& pattern, int currentStep);
    
    // EQ Processing
    struct BiquadState { float z1 = 0, z2 = 0; };
    std::array<BiquadState, 5> eqState;  // 5 band EQ filter state
    bool eqActive = false;
    std::array<float, 5> eqGains;        // Current band gains
    double eqSampleRate = 44100.0;       // Cached sample rate for EQ
    void handleEQ(PatternPlayState& state, const Pattern& pattern, int currentStep, double sampleRate);
    float applyEQ(float sample);
    void computeBiquadCoeffs(float freq, float gain, float q, double sampleRate, float& b0, float& b1, float& b2, float& a1, float& a2);
};

} // namespace fx
