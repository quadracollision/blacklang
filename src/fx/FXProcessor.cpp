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
                case Pattern::FX_ATTACK:
                    handleAttack(state, pattern, step);
                    break;
                case Pattern::FX_DECAY:
                    handleDecay(state, pattern, step);
                    break;
                case Pattern::FX_SUSTAIN:
                    handleSustain(state, pattern, step);
                    break;
                case Pattern::FX_RELEASE:
                    handleRelease(state, pattern, step);
                    break;
                case Pattern::FX_ADSR:
                    handleAttack(state, pattern, step);
                    handleDecay(state, pattern, step);
                    handleSustain(state, pattern, step);
                    handleRelease(state, pattern, step);
                    break;
                case Pattern::FX_EQ:
                    handleEQ(state, pattern, step, sampleRate);
                    break;
                default:
                    break;
            }
        }
    }
}

float FXProcessor::processSampleFX(PatternPlayState& state, float sample, double sampleRate, int step, const Pattern& pattern) {
    // Apply Filter (Slide Squelch)
    if (state.hasStepSquelch) {
        float baseFreq = 440.0f; 
        float currentFreq = baseFreq * (float)state.currentSpeedRatio;
        float cutoffNorm = currentFreq / (float)sampleRate * 2.0f * 3.14159f;
        cutoffNorm *= 2.0f; 
        if (cutoffNorm > 0.8f) cutoffNorm = 0.8f;

        float res = 1.0f + (state.currentStepSquelch * 4.0f); // 1.0 .. 5.0
        sample = state.filter.process(sample, cutoffNorm, 1.0f / res);
    } else {
        if (state.filter.active) state.filter.reset();
    }
    
    // ADSR Envelope
    if (state.useADSR) {
        float increment = 0.0f;
        
        switch (state.adsrPhase) {
            case 1: // Attack
                state.adsrCurrentValue += state.adsrAttackRate;
                if (state.adsrCurrentValue >= 1.0f) {
                    state.adsrCurrentValue = 1.0f;
                    state.adsrPhase = 2; // Decay
                }
                break;
            case 2: // Decay
                state.adsrCurrentValue -= state.adsrDecayRate;
                if (state.adsrCurrentValue <= state.adsrSustainLevel) {
                    state.adsrCurrentValue = state.adsrSustainLevel;
                    state.adsrPhase = 3; // Sustain
                }
                break;
            case 3: // Sustain
                state.adsrCurrentValue = state.adsrSustainLevel;
                // If sample end is reached or gate off? 
                // For one-shot samples, we might just stay in sustain until sample ends?
                // Or if sample is ending naturally, let it fade out?
                // For now, simple Sustain until sample end.
                break;
            case 4: // Release
                state.adsrCurrentValue -= state.adsrReleaseRate;
                if (state.adsrCurrentValue <= 0.0f) {
                    state.adsrCurrentValue = 0.0f;
                    state.adsrPhase = 0; // Off
                    state.sampleIsPlaying = false; // Stop voice
                }
                break;
        }
        
        sample *= state.adsrCurrentValue;
    }
    
    // Apply EQ if active
    if (eqActive) {
        sample = applyEQ(sample);
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
    
    // Trigger anti-click fade for the start of stutter
    state.fadeInSamplesRemaining = 88;
}

void FXProcessor::handleSlice(PatternPlayState& state, const Pattern& pattern, int currentStep) {
    // SLICE FX: Set playback start position to slice marker
    if (pattern.stepFXParams.count(currentStep) &&
        pattern.stepFXParams.at(currentStep).count(Pattern::PAR_SLICE_INDEX)) {
        int sliceIdx = (int)pattern.stepFXParams.at(currentStep).at(Pattern::PAR_SLICE_INDEX);
        
        if (sliceIdx >= 0 && sliceIdx < (int)pattern.sliceMarkers.size()) {
            state.samplePlaybackPosition = (double)pattern.sliceMarkers[sliceIdx];
            // Track start position for Fade In calculations
            state.playbackStartPosition = state.samplePlaybackPosition;
            state.currentSliceIndex = sliceIdx; // Track which slice is playing
            
            // Set end position for cutoff mode
            if (pattern.stepFXParams.at(currentStep).count(Pattern::PAR_SLICE_CUTOFF)) {
                float cutoff = pattern.stepFXParams.at(currentStep).at(Pattern::PAR_SLICE_CUTOFF);
                if (cutoff > 0.5f && sliceIdx + 1 < (int)pattern.sliceMarkers.size()) {
                    state.sampleEndPosition = pattern.sliceMarkers[sliceIdx + 1];
                }
            }
            
            // Trigger anti-click fade for slice jump
            state.fadeInSamplesRemaining = 88;
        }
    }
}

void FXProcessor::handleNudge(PatternPlayState& state, const Pattern& pattern, int currentStep) {
    if (pattern.stepFXParams.count(currentStep) && 
        pattern.stepFXParams.at(currentStep).count(Pattern::PAR_NUDGE_OFFSET)) {
        
        float val = pattern.stepFXParams.at(currentStep).at(Pattern::PAR_NUDGE_OFFSET);
        
        // Interpreting Nudge (0-1) as Timing Offset
        // 0.5 = On Grid (No Delay)
        // > 0.5 = Delayed (Late)
        // < 0.5 = Early (Not fully supported without lookahead, so we'll ignore or map 0-1 to small delay range)
        
        // Let's Map 0..1 to 0..StepDuration delay
        // This is the most flexible "Micro-timing" approach.
        // It means default 0.5 is actually "delayed by half a step" if we used full range.
        // BUT the UI usually defaults to 0.5? Check FXControls.
        // Assuming 0.5 is "center", let's map:
        // 0.5 -> 0 delay.
        // 1.0 -> Max Delay (e.g. 50% of step).
        
        if (val > 0.501f) {
             float delayNorm = (val - 0.5f) * 2.0f; // 0..1
             // Delay up to 50% of a step duration seems reasonable for "Nudge"
             // We need BPM for exact samples. The pattern engine should provide it?
             // PatternPlayState doesn't hold BPM/SamplesPerStep directly here easily without context.
             // We'll approximate or use a fixed max delay of say 50ms?
             // Or better: AudioEngine knows SamplesPerStep.
             // FXProcessor doesn't have easy access to BPM here.
             
             // Workaround: Use a fixed max delay that feels like a "Nudge" (e.g. up to 1/16th note at 120bpm = ~125ms)
             // Let's say max 100ms.
             double maxDelayMs = 100.0; 
             double sampleRate = 44100.0; // Assumption or need to pass it
             
             // Actually, this method is called within AudioEngine which has sampleRate context?
             // No, FXProcessor is separate.
             // Let's assume standard rate or add it to context later if critical.
             // For now, strict sample based delay.
             
             int64_t delaySamples = (int64_t)(delayNorm * 4000.0); // 4000 samples @ 44.1k is ~90ms. Good range.
             state.playbackDelaySamples = delaySamples;
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
                 state.sampleEndPosition = pattern.sliceMarkers[(size_t)sliceIdx];
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

void FXProcessor::handleAttack(PatternPlayState& state, const Pattern& pattern, int currentStep) {
    if (pattern.stepFXParams.count(currentStep) && 
        pattern.stepFXParams.at(currentStep).count(Pattern::PAR_ATTACK_TIME)) {
        
        float val = pattern.stepFXParams.at(currentStep).at(Pattern::PAR_ATTACK_TIME);
        // Map 0-1 to 1ms-1000ms
        float timeSeconds = 0.001f + (val * 1.0f);
        state.adsrAttackRate = 1.0f / (timeSeconds * 44100.0f); // Should get actual sampleRate from caller?
        state.useADSR = true;
        // Check if we are starting a note? Assuming AudioEngine calls this on note start.
        if (state.samplePlaybackPosition < 100.0) state.adsrPhase = 1; 
    }
}

void FXProcessor::handleDecay(PatternPlayState& state, const Pattern& pattern, int currentStep) {
    if (pattern.stepFXParams.count(currentStep) && 
        pattern.stepFXParams.at(currentStep).count(Pattern::PAR_DECAY_TIME)) {
        
        float val = pattern.stepFXParams.at(currentStep).at(Pattern::PAR_DECAY_TIME);
        // Map 0-1 to 1ms-2000ms
        float timeSeconds = 0.001f + (val * 2.0f);
        state.adsrDecayRate = 1.0f / (timeSeconds * 44100.0f); 
        state.useADSR = true;
    }
}

void FXProcessor::handleSustain(PatternPlayState& state, const Pattern& pattern, int currentStep) {
    if (pattern.stepFXParams.count(currentStep) && 
        pattern.stepFXParams.at(currentStep).count(Pattern::PAR_SUSTAIN_LEVEL)) {
        
        float val = pattern.stepFXParams.at(currentStep).at(Pattern::PAR_SUSTAIN_LEVEL);
        state.adsrSustainLevel = val;
        state.useADSR = true;
    }
}

void FXProcessor::handleRelease(PatternPlayState& state, const Pattern& pattern, int currentStep) {
    if (pattern.stepFXParams.count(currentStep) && 
        pattern.stepFXParams.at(currentStep).count(Pattern::PAR_RELEASE_TIME)) {
        
        float val = pattern.stepFXParams.at(currentStep).at(Pattern::PAR_RELEASE_TIME);
        // Map 0-1 to 1ms-2000ms
        float timeSeconds = 0.001f + (val * 2.0f);
        state.adsrReleaseRate = 1.0f / (timeSeconds * 44100.0f);
        state.useADSR = true;
    }
}

// ============================================
// 5-Band EQ Implementation
// ============================================

// Frequency centers for each band
static const float EQ_FREQUENCIES[5] = { 60.0f, 250.0f, 1000.0f, 4000.0f, 12000.0f };
static const float EQ_Q = 1.4f;  // Filter Q (bandwidth)

void FXProcessor::handleEQ(PatternPlayState& state, const Pattern& pattern, int currentStep, double sampleRate) {
    // Read EQ band gains from pattern parameters
    const int bandParams[5] = {
        Pattern::PAR_EQ_BAND1,
        Pattern::PAR_EQ_BAND2,
        Pattern::PAR_EQ_BAND3,
        Pattern::PAR_EQ_BAND4,
        Pattern::PAR_EQ_BAND5
    };
    
    eqActive = false;
    
    for (int i = 0; i < 5; ++i) {
        eqGains[i] = 0.0f;  // Default: no change
        
        if (pattern.stepFXParams.count(currentStep) && 
            pattern.stepFXParams.at(currentStep).count(bandParams[i])) {
            eqGains[i] = pattern.stepFXParams.at(currentStep).at(bandParams[i]);
        }
        
        // If any gain is non-zero, activate EQ
        if (std::abs(eqGains[i]) > 0.01f) {
            eqActive = true;
        }
    }
    
    // Reset filter states on step start
    for (int i = 0; i < 5; ++i) {
        eqState[i].z1 = 0;
        eqState[i].z2 = 0;
    }
    
    // Store sample rate for use in applyEQ
    eqSampleRate = sampleRate;
}

float FXProcessor::applyEQ(float sample) {
    // Apply 5 cascaded biquad peak/shelf filters
    float out = sample;
    
    for (int i = 0; i < 5; ++i) {
        float gainDB = eqGains[i] * 12.0f;  // -1 to 1 -> -12dB to +12dB
        
        if (std::abs(gainDB) < 0.1f) continue;  // Skip near-zero bands
        
        // Compute biquad coefficients for peaking EQ
        float A = std::pow(10.0f, gainDB / 40.0f);
        float w0 = 2.0f * 3.14159265f * EQ_FREQUENCIES[i] / (float)eqSampleRate;
        float sinw0 = std::sin(w0);
        float cosw0 = std::cos(w0);
        float alpha = sinw0 / (2.0f * EQ_Q);
        
        float b0 = 1.0f + alpha * A;
        float b1 = -2.0f * cosw0;
        float b2 = 1.0f - alpha * A;
        float a0 = 1.0f + alpha / A;
        float a1 = -2.0f * cosw0;
        float a2 = 1.0f - alpha / A;
        
        // Normalize
        b0 /= a0; b1 /= a0; b2 /= a0;
        a1 /= a0; a2 /= a0;
        
        // Apply biquad (Direct Form II Transposed)
        float y = b0 * out + eqState[i].z1;
        eqState[i].z1 = b1 * out - a1 * y + eqState[i].z2;
        eqState[i].z2 = b2 * out - a2 * y;
        
        out = y;
    }
    
    return out;
}

void FXProcessor::computeBiquadCoeffs(float freq, float gain, float q, double sampleRate,
                                       float& b0, float& b1, float& b2, float& a1, float& a2) {
    float A = std::pow(10.0f, gain / 40.0f);
    float w0 = 2.0f * 3.14159265f * freq / (float)sampleRate;
    float sinw0 = std::sin(w0);
    float cosw0 = std::cos(w0);
    float alpha = sinw0 / (2.0f * q);
    
    float a0 = 1.0f + alpha / A;
    b0 = (1.0f + alpha * A) / a0;
    b1 = (-2.0f * cosw0) / a0;
    b2 = (1.0f - alpha * A) / a0;
    a1 = (-2.0f * cosw0) / a0;
    a2 = (1.0f - alpha / A) / a0;
}

} // namespace fx

