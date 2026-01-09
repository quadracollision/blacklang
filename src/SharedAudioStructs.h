#pragma once

#include <stdint.h>

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
        isReverse = false;
    }
    
    bool isReverse = false;
};
