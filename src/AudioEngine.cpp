#include "AudioEngine.h"
#include <iostream>
#include <cmath>

AudioEngine::AudioEngine() {
    formatManager.registerBasicFormats();
}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::initialize() {
    auto result = deviceManager.initialiseWithDefaultDevices(0, 2);
    if (result.isNotEmpty()) {
        std::cerr << "Audio init error: " << result.toStdString() << std::endl;
        return false;
    }
    
    deviceManager.addAudioCallback(this);
    return true;
}

void AudioEngine::shutdown() {
    stop();
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
}

bool AudioEngine::loadSample(Pattern& pattern) {
    juce::File file(pattern.samplePath);
    if (!file.existsAsFile()) {
        std::cerr << "Sample not found: " << pattern.samplePath << std::endl;
        return false;
    }
    
    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(file));
    
    if (!reader) {
        std::cerr << "Cannot read sample: " << pattern.samplePath << std::endl;
        return false;
    }
    
    pattern.sampleBuffer.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
    reader->read(&pattern.sampleBuffer, 0, (int)reader->lengthInSamples, 0, true, true);
    pattern.sampleRate = reader->sampleRate;
    
    return true;
}

void AudioEngine::addPattern(const Pattern& pattern) {
    std::lock_guard<std::mutex> lock(patternMutex);
    patterns[pattern.name] = pattern;
}

Pattern* AudioEngine::getPattern(const std::string& name) {
    std::lock_guard<std::mutex> lock(patternMutex);
    auto it = patterns.find(name);
    return (it != patterns.end()) ? &it->second : nullptr;
}

void AudioEngine::playPattern(const std::string& name) {
    std::lock_guard<std::mutex> lock(patternMutex);
    if (patterns.find(name) == patterns.end()) {
        std::cerr << "Pattern not found: " << name << std::endl;
        return;
    }
    
    currentPatternName = name;
    currentChain.clear();
    chainIndex = 0;
    currentStep = 0;
    samplePosition = 0;
    stepStartSample = 0;
    samplePlaybackPosition = 0;
    sampleIsPlaying = false;
    playing.store(true);
}

void AudioEngine::playChain(const PatternChain& chain) {
    if (chain.isEmpty()) {
        std::cerr << "Chain is empty" << std::endl;
        return;
    }
    
    std::lock_guard<std::mutex> lock(patternMutex);
    currentChain = chain;
    chainIndex = 0;
    currentPatternName = chain.getPatterns()[0];
    currentStep = 0;
    samplePosition = 0;
    stepStartSample = 0;
    samplePlaybackPosition = 0;
    sampleIsPlaying = false;
    activePatternNames.clear();
    patternStates.clear();
    playing.store(true);
    paused.store(false);
}

void AudioEngine::playMultiplePatterns(const std::vector<std::string>& names) {
    if (names.empty()) {
        std::cerr << "No patterns to play" << std::endl;
        return;
    }
    
    std::lock_guard<std::mutex> lock(patternMutex);
    activePatternNames = names;
    patternStates.clear();
    
    for (const auto& name : names) {
        if (patterns.find(name) != patterns.end()) {
            PatternPlayState state;
            patternStates[name] = state;
        }
    }
    
    currentChain.clear();
    currentPatternName.clear();
    playing.store(true);
    paused.store(false);
}

void AudioEngine::stop() {
    playing.store(false);
    paused.store(false);
    sampleIsPlaying = false;
    activePatternNames.clear();
    patternStates.clear();
}

void AudioEngine::pause() {
    if (playing.load()) {
        paused.store(true);
    }
}

void AudioEngine::updateActivePatterns(const std::vector<std::string>& names) {
    if (names.empty()) {
        stop();
        return;
    }
    
    std::lock_guard<std::mutex> lock(patternMutex);
    
    // Find a reference state from currently playing patterns for sync
    PatternPlayState refState;
    bool hasRef = false;
    if (!activePatternNames.empty()) {
        for (const auto& existing : activePatternNames) {
            if (patternStates.count(existing)) {
                refState = patternStates[existing];
                hasRef = true;
                break;
            }
        }
    }
    
    activePatternNames = names;
    
    // Sync new patterns
    for (const auto& name : names) {
        if (patternStates.find(name) == patternStates.end()) {
            PatternPlayState newState;
            newState.currentStep = 1; // Start at step 1
            if (hasRef) {
                // Approximate sync: copy sample position
                newState.samplePosition = refState.samplePosition;
                // We let currentStep be calculated/corrected in the next callback loop 
                // or we can calculate it here if needed.
                // But since 'patterns' map might be needed to calculate step, 
                // and we are in lock, we can do it.
                if (patterns.count(name)) {
                     Pattern& p = patterns[name];
                     double stepSamples = p.getStepDurationSamples(globalBpm.load());
                     if (stepSamples > 0) {
                         newState.currentStep = (int)((newState.samplePosition / (int64_t)stepSamples) % p.steps) + 1;
                         newState.stepStartSample = newState.samplePosition - (newState.samplePosition % (int64_t)stepSamples);
                     }
                }
            }
            patternStates[name] = newState;
        }
    }
    
    // Cleanup old states
    std::vector<std::string> toRemove;
    for (auto& pair : patternStates) {
        bool found = false;
        for (const auto& n : names) if (n == pair.first) found = true;
        if (!found) toRemove.push_back(pair.first);
    }
    // Actually we don't strictly need to remove them from map, but it keeps it clean
    for (const auto& r : toRemove) patternStates.erase(r);
    
    if (!playing.load()) {
        playing.store(true);
        paused.store(false);
    }
}

int AudioEngine::getPatternProgress(const std::string& name) {
    if (!playing.load()) return -1;
    // Use blocking lock to ensure we get the state
    // try_lock can cause flickering cursor if audio thread is busy
    std::lock_guard<std::mutex> lock(patternMutex);
    
    if (patternStates.count(name)) {
        return patternStates[name].currentStep;
    }
    return -1;
}

void AudioEngine::resume() {
    if (playing.load() && paused.load()) {
        paused.store(false);
    }
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    sampleRate = device->getCurrentSampleRate();
}

void AudioEngine::audioDeviceStopped() {
    playing.store(false);
}

void AudioEngine::audioDeviceIOCallbackWithContext(
    const float* const* /*inputChannelData*/,
    int /*numInputChannels*/,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext& /*context*/)
{
    // Clear output
    for (int ch = 0; ch < numOutputChannels; ++ch) {
        std::fill(outputChannelData[ch], outputChannelData[ch] + numSamples, 0.0f);
    }
    
    if (!playing.load() || paused.load()) return;
    
    std::lock_guard<std::mutex> lock(patternMutex);
    
    // Multi-pattern playback mode
    if (!activePatternNames.empty()) {
        for (int i = 0; i < numSamples; ++i) {
            for (const auto& patName : activePatternNames) {
                auto patIt = patterns.find(patName);
                if (patIt == patterns.end()) continue;
                
                Pattern& pattern = patIt->second;
                auto& state = patternStates[patName];
                
                // Check if we need to advance to next step
                double stepDuration = pattern.getStepDurationSamples(globalBpm.load());
                if (state.samplePosition >= state.stepStartSample + (int64_t)stepDuration) {
                    state.currentStep++;
                    state.stepStartSample = state.samplePosition;
                    
                    if (state.currentStep > pattern.steps) {
                        state.currentStep = 1;
                        state.stepStartSample = 0;
                        state.samplePosition = 0;
                    }
                    
                    if (pattern.shouldTriggerAt(state.currentStep)) {
                        state.samplePlaybackPosition = 0.0;
                        state.sampleIsPlaying = true;
                        state.isStuttering = false; // Reset start of step
                        state.stutterIntervalSamples = 0;
                        
                        // Calculate Pitch
                        int semitones = 0;
                        if (pattern.stepPitches.count(state.currentStep)) {
                            semitones = pattern.stepPitches[state.currentStep];
                        }
                        state.currentSpeedRatio = std::pow(2.0, semitones / 12.0);
                        
                        // Calculate Velocity
                        state.currentVelocity = 1.0f;
                        if (pattern.stepVelocities.count(state.currentStep)) {
                            state.currentVelocity = pattern.stepVelocities[state.currentStep];
                        }
                        
                        // Apply Stutter Speed if present
                        if (pattern.stepFXParams.count(state.currentStep) &&
                            pattern.stepFXParams.at(state.currentStep).count(Pattern::PAR_STUTTER_SPEED)) {
                             float speedMult = pattern.stepFXParams.at(state.currentStep).at(Pattern::PAR_STUTTER_SPEED);
                             if (speedMult > 0.0f) state.currentSpeedRatio *= speedMult;
                        }

                        if (pattern.stepFX.count(state.currentStep)) {
                             const auto& fxList = pattern.stepFX[state.currentStep];
                             for (int fx : fxList) {
                                 if (fx == Pattern::FX_CUTOFF) {
                                    state.samplePlaybackPosition = 0.0;
                                    state.sampleIsPlaying = true;
                                 }
                                 else if (fx == Pattern::FX_SLIDE) {
                                     // Look ahead for next active melodic step
                                     int nextStep = -1;
                                     for (int s = state.currentStep + 1; s <= pattern.steps; ++s) {
                                         if (pattern.stepPitches.count(s)) {
                                             nextStep = s;
                                             break;
                                         }
                                     }
                                     // Wrap around check? Maybe not for now, just within pattern
                                     
                                     if (nextStep != -1) {
                                         state.isSliding = true;
                                         int nextSemitones = pattern.stepPitches[nextStep];
                                         state.slideTargetRatio = std::pow(2.0, nextSemitones / 12.0);
                                         
                                         // Calculate duration to next step in samples
                                         double stepDur = pattern.getStepDurationSamples(globalBpm.load());
                                         double samplesDist = (nextStep - state.currentStep) * stepDur;
                                         
                                         // Apply Slide Time Parameter
                                         float timeParam = 1.0f;
                                         if (pattern.stepFXParams.count(state.currentStep) && 
                                             pattern.stepFXParams.at(state.currentStep).count(Pattern::PAR_SLIDE_TIME)) {
                                             timeParam = pattern.stepFXParams.at(state.currentStep).at(Pattern::PAR_SLIDE_TIME);
                                         }
                                         if (timeParam < 0.01f) timeParam = 0.01f;
                                         
                                         // Shorten distance based on time param (faster slide = less distance to cover target)
                                         // Actually "Time" usually means duration. 
                                         // If Time = 1.0 -> Full gap duration.
                                         // If Time = 0.5 -> Half gap duration (faster slide).
                                         samplesDist *= (double)timeParam;

                                         if (samplesDist > 0) {
                                             state.slideStepIncrement = (state.slideTargetRatio - state.currentSpeedRatio) / samplesDist;
                                         }
                                     }
                                 }
                                 else if (fx == Pattern::FX_STUTTER) {
                                      state.isStuttering = true;
                                      double stepDur = pattern.getStepDurationSamples(globalBpm.load());
                                      
                                      float rate = 4.0f; // Default
                                      if (pattern.stepFXParams.count(state.currentStep) && 
                                          pattern.stepFXParams.at(state.currentStep).count(Pattern::PAR_STUTTER_RATE)) {
                                          rate = pattern.stepFXParams.at(state.currentStep).at(Pattern::PAR_STUTTER_RATE);
                                      }
                                      if (rate < 1.0f) rate = 1.0f;

                                      state.stutterIntervalSamples = (int)(stepDur / rate);
                                      if (state.stutterIntervalSamples < 100) state.stutterIntervalSamples = 100;
                                 }
                             }
                        }
                    }
                }
                
                // Mix sample if playing and exists
                if (pattern.sampleBuffer.getNumSamples() > 0 && state.sampleIsPlaying) {
                    for (int ch = 0; ch < numOutputChannels; ++ch) {
                        int srcCh = std::min(ch, pattern.sampleBuffer.getNumChannels() - 1);
                        const float* inData = pattern.sampleBuffer.getReadPointer(srcCh);
                        float* outData = outputChannelData[ch];
                        
                        // Linear Interpolation
                        double pos = state.samplePlaybackPosition;
                        int idx = (int)pos;
                        float frac = (float)(pos - idx);
                        
                        // Safety check for indices
                        if (idx >= 0 && idx < pattern.sampleBuffer.getNumSamples()) {
                             float s1 = inData[idx];
                             float s2 = (idx + 1 < pattern.sampleBuffer.getNumSamples()) ? inData[idx + 1] : 0.0f;
                             outData[i] += (s1 + frac * (s2 - s1)) * state.currentVelocity;
                        }
                    }
                    
                    state.samplePlaybackPosition += state.currentSpeedRatio;
                    
                    // Handle Stutter Re-trigger
                    if (state.isStuttering && state.stutterIntervalSamples > 0) {
                        int64_t samplesInStep = state.samplePosition - state.stepStartSample;
                        if (samplesInStep > 0 && samplesInStep % state.stutterIntervalSamples == 0) {
                            state.samplePlaybackPosition = 0.0; // Re-trigger
                        }
                    }
                    
                    // Apply Slide
                    if (state.isSliding) {
                        state.currentSpeedRatio += state.slideStepIncrement;
                        // Clamp? Or let it overshoot slightly/stop at target?
                        // Simple approach: Check if we passed target
                         if ((state.slideStepIncrement > 0 && state.currentSpeedRatio >= state.slideTargetRatio) ||
                             (state.slideStepIncrement < 0 && state.currentSpeedRatio <= state.slideTargetRatio)) {
                             state.currentSpeedRatio = state.slideTargetRatio;
                             state.isSliding = false;
                         }
                    }
                    // Stop if we passed the end
                     if (state.samplePlaybackPosition >= pattern.sampleBuffer.getNumSamples()) {
                         state.sampleIsPlaying = false;
                     }

                     float currentSample = 0.0f;
                     int sampleIdx = (int)state.samplePlaybackPosition;
                     
                     if (state.sampleIsPlaying && sampleIdx >= 0 && sampleIdx < pattern.sampleBuffer.getNumSamples()) {
                        float sample = pattern.sampleBuffer.getSample(0, sampleIdx);
                        
                        // Apply Filter if Squelch is active (Slide FX mainly)
                        if (pattern.stepFXParams.count(state.currentStep) && 
                            pattern.stepFXParams.at(state.currentStep).count(Pattern::PAR_SLIDE_SQUELCH)) {
                            
                            float squelch = pattern.stepFXParams.at(state.currentStep).at(Pattern::PAR_SLIDE_SQUELCH);
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
                        
                        currentSample = sample * state.currentVelocity;
                     }

                     for (int ch = 0; ch < numOutputChannels; ++ch) {
                         outputChannelData[ch][i] += currentSample;
                     }
                } // Close if sampleIsPlaying
                
                state.samplePosition++;
            } // Close for patName
        } // Close for i
    
    // If we processed multi-patterns, we are done.
    return;
} // Close if activePatternNames not empty (Wait, if I return, I don't need else. But I need to close the IF)

// Actually, I want to return from FUNCTION if multi-pattern was handled?
// If I close 'if', then execution continues to legacy.
// So:
// } // Close Loop
// return;
// } // Close partial logic?

// Let's look at the START again.
// if (!activePatternNames.empty()) {
//    for (...) ...
//    return;
// }

// So I need:
// } (Close 360 if)
// } (Close 249 for patName)
// } (Close 248 for i)
// return;
// } (Close 247 if)
    
    // Single pattern / chain mode (legacy)
    Pattern* pattern = nullptr;
    auto it = patterns.find(currentPatternName);
    if (it != patterns.end()) {
        pattern = &it->second;
    }
    
    if (!pattern || pattern->sampleBuffer.getNumSamples() == 0) return;
    
    // Process audio
    for (int i = 0; i < numSamples; ++i) {
        // Check if we need to advance to next step
        double stepDuration = pattern->getStepDurationSamples(globalBpm.load());
        if (samplePosition >= stepStartSample + (int64_t)stepDuration) {
            currentStep++;
            stepStartSample = samplePosition;
            
            if (currentStep > pattern->steps) {
                currentStep = 1;
                stepStartSample = 0;
                samplePosition = 0;
                
                // Advance chain if playing chain
                if (!currentChain.isEmpty()) {
                    chainIndex++;
                    if (chainIndex >= (int)currentChain.size()) {
                        chainIndex = 0;
                    }
                    currentPatternName = currentChain.getPatterns()[chainIndex];
                    it = patterns.find(currentPatternName);
                    if (it != patterns.end()) {
                        pattern = &it->second;
                    }
                }
            }
            
            // Check if this step triggers the sample
            if (pattern->shouldTriggerAt(currentStep)) {
                triggerSample();
            }
        }
        
        // Mix sample if playing
        if (sampleIsPlaying && samplePlaybackPosition < pattern->sampleBuffer.getNumSamples()) {
            for (int ch = 0; ch < numOutputChannels; ++ch) {
                int srcCh = std::min(ch, pattern->sampleBuffer.getNumChannels() - 1);
                outputChannelData[ch][i] += pattern->sampleBuffer.getSample(srcCh, (int)samplePlaybackPosition);
            }
            samplePlaybackPosition++;
        }
        
        samplePosition++;
    }
}

void AudioEngine::triggerSample() {
    samplePlaybackPosition = 0;
    sampleIsPlaying = true;
}

