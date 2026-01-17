#pragma once

#include <stdint.h>

struct PatternPlayState {
    int64_t samplePosition = 0;
    int currentStep = 0;
    int64_t stepStartSample = 0;
    double samplePlaybackPosition = 0.0;
    int64_t sampleEndPosition = 0; // For Nudge/Crop FX
    int64_t sliceEndPosition = -1; // -1 if not slicing or play through
    bool stopAtSliceEnd = false;    // Slice flags
    bool stopAtEnd = false;         // Per-slot sync: stop at pattern end and switch to queued pattern
    
    double playbackStartPosition = 0.0; // Start position of the current trigger (0 or slice start)
    int currentSliceIndex = -1; // -1 if not slicing, or index into sliceMarkers
    
    int64_t playbackDelaySamples = 0; // For Nudge timing offset (delay before starting playback)
    
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
        bool active = false;
        void reset() { low=0; band=0; high=0; active=false; }
        
        // Simple SVF (State Variable Filter)
        float process(float input, float cutoff, float res) {
            active = true;
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

    // Cache for per-sample processing
    float currentStepSquelch = 0.0f;
    bool hasStepSquelch = false;

    // ADSR State
    bool useADSR = false;
    int adsrPhase = 0; // 0=Off, 1=A, 2=D, 3=S, 4=R
    float adsrLevel = 0.0f;
    float adsrAttackRate = 0.001f;
    float adsrDecayRate = 0.001f;
    float adsrSustainLevel = 1.0f;
    float adsrReleaseRate = 0.001f;
    float adsrCurrentValue = 0.0f;

    void reset() {
        samplePosition = 0;
        currentStep = 0;
        stepStartSample = 0;
        samplePlaybackPosition = 0.0;
        sampleEndPosition = 0;
        sliceEndPosition = -1;
        sliceEndPosition = -1;
        stopAtSliceEnd = false;
        playbackDelaySamples = 0;
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
        
        // ADSR Reset
        useADSR = false;
        adsrPhase = 0;
        adsrCurrentValue = 0.0f;
        adsrAttackRate = 0.0f; // Reset to 0 so we know if it was set
        
        isReverse = false;
    }
    
    bool isReverse = false;

    // EQ State (Per-pattern isolation)
    struct EQState {
        bool active = false;
        float gains[5] = {0.0f}; // 5 bands
        double sampleRate = 44100.0;
        
        struct BiquadState {
            double z1 = 0.0, z2 = 0.0;
        } bandStates[5];
        
        void reset() {
            active = false;
            for(int i=0; i<5; ++i) {
                gains[i] = 0.0f;
                bandStates[i].z1 = 0.0;
                bandStates[i].z2 = 0.0;
            }
        }
    } eq;
};
