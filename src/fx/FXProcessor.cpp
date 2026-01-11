#include "FXProcessor.h"
#include "../SharedAudioStructs.h"
#include <cmath>

namespace fx {

FXProcessor::FXProcessor() {}

void FXProcessor::reset() {
    // Reset any internal state if needed
}

void FXProcessor::processStepFX(PatternPlayState& state, const Pattern& pattern, int step, double bpm, double sampleRate) {
    // Check for FX on this step
    if (pattern.stepFX.count(step)) {
        const auto& fxList = pattern.stepFX.at(step);
        for (int fxId : fxList) {
            switch (fxId) {
                case Pattern::FX_CUTOFF:
                    handleCutoff(state);
                    break;
                case Pattern::FX_SLIDE:
                    handleSlide(state, pattern, step, bpm, sampleRate);
                    break;
                case Pattern::FX_STUTTER:
                    handleStutter(state, pattern, step, bpm, sampleRate);
                    break;
                case Pattern::FX_SLICE:
                    handleSlice(state, pattern, step);
                    break;
                case Pattern::FX_NUDGE:
                    handleNudge(state, pattern, step);
                    break;
                case Pattern::FX_REVERSE:
                    handleReverse(state, pattern, step);
                    break;
                // Add new FX handlers here
                default:
                    break;
            }
        }
    }
}

float FXProcessor::processSampleFX(PatternPlayState& state, float sample, double sampleRate, int step, const Pattern& pattern) {
    // Apply Filter (Slide Squelch)
    if (pattern.stepFXParams.count(step) && 
        pattern.stepFXParams.at(step).count(Pattern::PAR_SLIDE_SQUELCH)) {
        
        float squelch = pattern.stepFXParams.at(step).at(Pattern::PAR_SLIDE_SQUELCH);
        if (squelch > 0.01f) {
            float baseFreq = 440.0f; 
            float currentFreq = baseFreq * (float)state.currentSpeedRatio;
            float cutoffNorm = currentFreq / (float)sampleRate * 2.0f * 3.14159f;
            cutoffNorm *= 2.0f; 
            if (cutoffNorm > 0.8f) cutoffNorm = 0.8f;

            float res = 1.0f + (squelch * 4.0f); // 1.0 .. 5.0
            sample = state.filter.process(sample, cutoffNorm, 1.0f / res);
        } else {
            state.filter.reset();
        }
    } else {
        state.filter.reset();
    }
    
    return sample;
}

void FXProcessor::handleCutoff(PatternPlayState& state) {
    state.sampleIsPlaying = false; // Stop playback
}

void FXProcessor::handleSlide(PatternPlayState& state, const Pattern& pattern, int currentStep, double bpm, double sampleRate) {
    // Look ahead for next active melodic step
    int nextStep = -1;
    for (int s = currentStep + 1; s <= pattern.steps; ++s) {
        if (pattern.stepPitches.count(s)) {
            nextStep = s;
            break;
        }
    }
    
    if (nextStep != -1) {
        state.isSliding = true;
        int nextSemitones = pattern.stepPitches.at(nextStep);
        state.slideTargetRatio = std::pow(2.0, nextSemitones / 12.0);
        
        // Calculate duration to next step in samples
        double stepDur = pattern.getStepDurationSamples(bpm, sampleRate);
        double samplesDist = (nextStep - currentStep) * stepDur;
        
        // Apply Slide Time Parameter
        float timeParam = 1.0f;
        if (pattern.stepFXParams.count(currentStep) && 
            pattern.stepFXParams.at(currentStep).count(Pattern::PAR_SLIDE_TIME)) {
            timeParam = pattern.stepFXParams.at(currentStep).at(Pattern::PAR_SLIDE_TIME);
        }
        if (timeParam < 0.01f) timeParam = 0.01f;
        
        samplesDist *= (double)timeParam;

        if (samplesDist > 0) {
            state.slideStepIncrement = (state.slideTargetRatio - state.currentSpeedRatio) / samplesDist;
        }
    }
}

void FXProcessor::handleStutter(PatternPlayState& state, const Pattern& pattern, int currentStep, double bpm, double sampleRate) {
    state.isStuttering = true;
    double stepDur = pattern.getStepDurationSamples(bpm, sampleRate);
    
    float rate = 4.0f; // Default
    if (pattern.stepFXParams.count(currentStep) && 
        pattern.stepFXParams.at(currentStep).count(Pattern::PAR_STUTTER_RATE)) {
        rate = pattern.stepFXParams.at(currentStep).at(Pattern::PAR_STUTTER_RATE);
    }
    if (rate < 1.0f) rate = 1.0f;

    state.stutterIntervalSamples = (int)(stepDur / rate);
    if (state.stutterIntervalSamples < 100) state.stutterIntervalSamples = 100;
}

void FXProcessor::handleSlice(PatternPlayState& state, const Pattern& pattern, int currentStep) {
    // SLICE FX: Set playback start position to slice marker
    if (pattern.stepFXParams.count(currentStep) &&
        pattern.stepFXParams.at(currentStep).count(Pattern::PAR_SLICE_INDEX)) {
        int sliceIdx = (int)pattern.stepFXParams.at(currentStep).at(Pattern::PAR_SLICE_INDEX);
        
        if (sliceIdx >= 0 && sliceIdx < (int)pattern.sliceMarkers.size()) {
            state.samplePlaybackPosition = (double)pattern.sliceMarkers[sliceIdx];
            
            // Set end position for cutoff mode
            if (pattern.stepFXParams.at(currentStep).count(Pattern::PAR_SLICE_CUTOFF)) {
                float cutoff = pattern.stepFXParams.at(currentStep).at(Pattern::PAR_SLICE_CUTOFF);
                if (cutoff > 0.5f && sliceIdx + 1 < (int)pattern.sliceMarkers.size()) {
                    state.sampleEndPosition = pattern.sliceMarkers[sliceIdx + 1];
                }
            }
        }
    }
}

void FXProcessor::handleNudge(PatternPlayState& state, const Pattern& pattern, int currentStep) {
    if (pattern.stepFXParams.count(currentStep) && 
        pattern.stepFXParams.at(currentStep).count(Pattern::PAR_NUDGE_OFFSET)) {
        
        float val = pattern.stepFXParams.at(currentStep).at(Pattern::PAR_NUDGE_OFFSET);
        int64_t totalSamples = pattern.sampleBuffer.getNumSamples();
        
        if (val > 0.5f) {
            // Right Side: Adjust Start (offset from 0)
            float norm = (val - 0.5f) * 2.0f; // 0.0 to 1.0
            state.samplePlaybackPosition = (double)(norm * totalSamples);
        } else if (val < 0.5f) {
            // Left Side: Adjust End (Shorten duration)
            float norm = val * 2.0f; // 0.0 to 1.0
            state.sampleEndPosition = (int64_t)(norm * totalSamples);
        }
    }
}

void FXProcessor::handleReverse(PatternPlayState& state, const Pattern& pattern, int currentStep) {
    state.isReverse = true;
    
    // If not slicing, start from end
    // If slicing, logic handled in handleSlice? No, slice sets playback position
    // We need to override playback position to end of slice or end of sample
    
    // Check if we are slicing in this step
    bool isSlicing = false;
    if (pattern.stepFXParams.count(currentStep) && 
        pattern.stepFXParams.at(currentStep).count(Pattern::PAR_SLICE_INDEX)) {
        isSlicing = true;
        
        int sliceIdx = (int)pattern.stepFXParams.at(currentStep).at(Pattern::PAR_SLICE_INDEX);
        if (sliceIdx >= 0 && sliceIdx + 1 < (int)pattern.sliceMarkers.size()) {
             // Start at end of slice
             state.samplePlaybackPosition = (double)pattern.sliceMarkers[sliceIdx + 1];
             state.sampleEndPosition = pattern.sliceMarkers[sliceIdx]; // It becomes the 'end' (play backwards to this)
             
             // Actually, the main loop checks: if (reverse) pos--; if (pos <= endPos) stop;
             // But AudioEngine uses 'sampleEndPosition' as a forward stop marker usually.
             // We need to clarify usage in AudioEngine.
             // For now, let's set play position to end of slice.
        } else {
             // Last slice or invalid
             state.samplePlaybackPosition = (double)pattern.sampleBuffer.getNumSamples();
             if (sliceIdx >= 0 && sliceIdx < (int)pattern.sliceMarkers.size()) {
                 state.sampleEndPosition = pattern.sliceMarkers[sliceIdx];
             } else {
                 state.sampleEndPosition = 0; 
             }
        }
    } 
    
    if (!isSlicing) {
        // Full reverse
        state.samplePlaybackPosition = (double)pattern.sampleBuffer.getNumSamples();
        state.sampleEndPosition = 0;
    }
}

} // namespace fx
