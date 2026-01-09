#include "AudioEngine.h"
#include "fx/FXProcessor.h"
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

std::vector<std::string> AudioEngine::getAvailableOutputDevices() {
    std::vector<std::string> devices;
    
    auto* currentDevice = deviceManager.getCurrentAudioDevice();
    if (currentDevice) {
        auto deviceNames = currentDevice->getOutputChannelNames();
        // Get actual device names from device type
        auto* deviceType = deviceManager.getCurrentDeviceTypeObject();
        if (deviceType) {
            auto deviceNameArray = deviceType->getDeviceNames(false); // false = output devices
            for (const auto& name : deviceNameArray) {
                devices.push_back(name.toStdString());
            }
        }
    }
    
    // Fallback: try to get from available device types
    if (devices.empty()) {
        for (auto* deviceType : deviceManager.getAvailableDeviceTypes()) {
            auto deviceNameArray = deviceType->getDeviceNames(false);
            for (const auto& name : deviceNameArray) {
                devices.push_back(name.toStdString());
            }
        }
    }
    
    return devices;
}

std::string AudioEngine::getCurrentOutputDevice() {
    auto* device = deviceManager.getCurrentAudioDevice();
    if (device) {
        return device->getName().toStdString();
    }
    return "";
}

bool AudioEngine::setOutputDevice(const std::string& deviceName) {
    // Stop playback first
    bool wasPlaying = playing.load();
    if (wasPlaying) {
        stop();
    }
    
    // Save callback state
    deviceManager.removeAudioCallback(this);
    
    // Get current audio device setup
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.outputDeviceName = juce::String(deviceName);
    
    // Apply new setup
    auto result = deviceManager.setAudioDeviceSetup(setup, true);
    
    // Re-add callback (this will trigger audioDeviceAboutToStart which reinitializes bus manager)
    deviceManager.addAudioCallback(this);
    
    return result.isEmpty();
}

void AudioEngine::setOutputDeviceAsync(const std::string& deviceName, std::function<void(bool)> callback) {
    // Prevent multiple simultaneous switches
    if (deviceSwitching.load()) {
        if (callback) callback(false);
        return;
    }
    
    deviceSwitching.store(true);
    
    // Launch in a separate thread
    std::thread([this, deviceName, callback]() {
        bool success = setOutputDevice(deviceName);
        deviceSwitching.store(false);
        if (callback) callback(success);
    }).detach();
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
    activePatternNames.clear(); // CRITICAL FIX: Ensure we exit multi-pattern mode
    currentChain.clear();
    chainIndex = 0;
    currentStep = 0;
    
    // FIX: Initialize to negative duration so first step triggers immediately
    double dur = 0.0;
    if (patterns.find(name) != patterns.end()) {
        dur = patterns[name].getStepDurationSamples(globalBpm.load());
    }
    stepStartSample = -(int64_t)dur;
    samplePosition = 0;

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
    
    // FIX: Initialize to negative duration so first step triggers immediately
    double dur = 0.0;
    if (patterns.find(currentPatternName) != patterns.end()) {
        dur = patterns[currentPatternName].getStepDurationSamples(globalBpm.load());
    }
    stepStartSample = -(int64_t)dur;
    samplePosition = 0;

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
            // FIX: Initialize to ensure Step 1 triggers immediately
            state.currentStep = 0; 
            double dur = patterns[name].getStepDurationSamples(globalBpm.load());
            state.stepStartSample = -(int64_t)dur;
            state.samplePosition = 0;
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
    
    // FIX: Fallback for Single Pattern / Chain Mode
    if (activePatternNames.empty() && name == currentPatternName) {
        return currentStep;
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
    int numChannels = device->getActiveOutputChannels().countNumberOfSetBits();
    busManager.initialize(sampleRate, numChannels);
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
    
    // Helper Lambda for Preview Mixing
    auto mixPreview = [&](float* const* targetBuffer, int numTargetChannels) {
        if (previewState.active && previewState.sourcePattern && previewState.sourcePattern->sampleBuffer.getNumSamples() > 0) {
            int totalSamples = previewState.sourcePattern->sampleBuffer.getNumSamples();
            int64_t end = previewState.endPosition > 0 ? previewState.endPosition : totalSamples;
            
            for (int i = 0; i < numSamples; ++i) {
                if (previewState.active && previewState.position < end && previewState.position < totalSamples) {
                     float vol = 1.0f; 
                     for (int ch = 0; ch < numTargetChannels; ++ch) {
                         int srcCh = std::min(ch, previewState.sourcePattern->sampleBuffer.getNumChannels() - 1);
                         targetBuffer[ch][i] += previewState.sourcePattern->sampleBuffer.getSample(srcCh, (int)previewState.position) * vol;
                     }
                     previewState.position++;
                     if (previewState.position >= end || previewState.position >= totalSamples) {
                        previewState.active = false;
                     }
                }
            }
        }
    };
    
    if (!playing.load() || paused.load()) {
         mixPreview(outputChannelData, numOutputChannels); // Allow preview even if paused/stopped!
         
         // Fix: Ensure we record even if the sequencer is stopped
         {
             std::unique_lock<std::mutex> lock(recordingMutex, std::try_to_lock);
             if (lock.owns_lock()) {
                 if (mainRecorder.isRecording()) {
                     mainRecorder.writeBlock(outputChannelData, numSamples, numOutputChannels);
                 }
             }
         }
         
         return;
    }
    
    std::lock_guard<std::mutex> lock(patternMutex);
    
    // Multi-pattern playback mode
    if (!activePatternNames.empty()) {
        // Prepare bus buffers
        busManager.prepareBuffers(numSamples);
        busManager.clearAllBuffers();
        
        // Process each sample
        for (int i = 0; i < numSamples; ++i) {
            for (const auto& patName : activePatternNames) {
                auto patIt = patterns.find(patName);
                if (patIt == patterns.end()) continue;
                
                Pattern& pattern = patIt->second;
                auto& state = patternStates[patName];
                
                // Get the track bus this pattern belongs to
                std::string trackName = getTrackForPattern(patName);
                AudioBus* trackBus = busManager.getOrCreateTrack(trackName);
                if (!trackBus) continue;
                
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
                        state.fadeInSamplesRemaining = 88; // 2ms fade-in
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

                        // FX Processing (Modular)
                        fxProcessor.processStepFX(state, pattern, state.currentStep, globalBpm.load());
                    }
                }
                
                // Process and mix sample if playing
                if (pattern.sampleBuffer.getNumSamples() > 0 && state.sampleIsPlaying) {
                    if (state.isReverse) {
                        state.samplePlaybackPosition -= state.currentSpeedRatio;
                    } else {
                        state.samplePlaybackPosition += state.currentSpeedRatio;
                    }
                    
                    // Handle Stutter Re-trigger
                    if (state.isStuttering && state.stutterIntervalSamples > 0) {
                        int64_t samplesInStep = state.samplePosition - state.stepStartSample;
                        if (samplesInStep > 0 && samplesInStep % state.stutterIntervalSamples == 0) {
                            // Re-trigger
                            if (state.isReverse) {
                                // For reverse, re-trigger from 'start' (which is the high end) ??
                                // Or simpler: just reset to initial position for this step
                                // Implementation complexity: we need to store initial trigger pos.
                                // Fallback: just standard retrigger logic might look weird in reverse.
                                // Let's just reset to whatever 'start' means.
                                // Current implementation of stutter overrides playback pos.
                                state.samplePlaybackPosition = (double)pattern.sampleBuffer.getNumSamples(); // Default
                                // If specific slice/reverse logic was processed, this might break it.
                                // Ideally we re-run processStepFX? OR just ignore stutter on reverse for now to be safe.
                            } else {
                                state.samplePlaybackPosition = 0.0; 
                            }
                            state.fadeInSamplesRemaining = 88; // Fade-in on stutter
                        }
                    }
                    
                    // Apply Slide
                    if (state.isSliding) {
                        state.currentSpeedRatio += state.slideStepIncrement;
                         if ((state.slideStepIncrement > 0 && state.currentSpeedRatio >= state.slideTargetRatio) ||
                             (state.slideStepIncrement < 0 && state.currentSpeedRatio <= state.slideTargetRatio)) {
                             state.currentSpeedRatio = state.slideTargetRatio;
                             state.isSliding = false;
                         }
                    }
                    
                    // Stop conditions
                     if (!state.isReverse) {
                         if (state.samplePlaybackPosition >= pattern.sampleBuffer.getNumSamples() || 
                            (state.sampleEndPosition > 0 && state.samplePlaybackPosition >= state.sampleEndPosition)) {
                             state.sampleIsPlaying = false;
                         }
                     } else {
                         // Reverse stop
                         if (state.samplePlaybackPosition < 0 || 
                            (state.sampleEndPosition > 0 && state.samplePlaybackPosition <= state.sampleEndPosition)) {
                             state.sampleIsPlaying = false;
                         }
                     }

                     float currentSample = 0.0f;
                     int sampleIdx = (int)state.samplePlaybackPosition;
                     
                     if (state.sampleIsPlaying && sampleIdx >= 0 && sampleIdx < pattern.sampleBuffer.getNumSamples()) {
                        float sample = pattern.sampleBuffer.getSample(0, sampleIdx);
                        
                        // Per-Sample FX (Filter, etc.)
                        sample = fxProcessor.processSampleFX(state, sample, sampleRate, state.currentStep, pattern);
                        
                        // --- ANTI-CLICK ENVELOPE (1-2ms fade-in/out) ---
                        const int fadeLen = 88;
                        float envelope = 1.0f;
                        
                        // Dynamic Fade-In
                        if (state.fadeInSamplesRemaining > 0) {
                            envelope *= (1.0f - (float)state.fadeInSamplesRemaining / (float)fadeLen);
                            state.fadeInSamplesRemaining--;
                        }
                        
                        // Fade-out at end (either Buffer End or Custom End)
                        int64_t effectiveEnd = pattern.sampleBuffer.getNumSamples();
                        if (state.sampleEndPosition > 0 && state.sampleEndPosition < effectiveEnd) {
                            effectiveEnd = state.sampleEndPosition;
                        }
                        
                        if (sampleIdx >= effectiveEnd - fadeLen) {
                            int samplesRemaining = effectiveEnd - sampleIdx;
                            if (samplesRemaining < 0) samplesRemaining = 0;
                            envelope *= ((float)samplesRemaining / (float)fadeLen);
                        }
                        
                        currentSample = sample * state.currentVelocity * envelope;
                     }

                     // Write to track bus instead of output
                     for (int ch = 0; ch < std::min(numOutputChannels, trackBus->buffer.getNumChannels()); ++ch) {
                         trackBus->buffer.addSample(ch, i, currentSample);
                     }
                } // Close if sampleIsPlaying
                
                state.samplePosition++;
            } // Close for patName
        } // Close for i
    
    // Mix all tracks to master and apply master processing
    busManager.mixTracksToMaster();
    
    // Inject Preview into Master Bus (so it gets recorded)
    AudioBus* masterBusForPreview = busManager.getMasterBus();
    float* masterWritePointers[2] = {
        masterBusForPreview->buffer.getWritePointer(0),
        masterBusForPreview->buffer.getNumChannels() > 1 ? masterBusForPreview->buffer.getWritePointer(1) : masterBusForPreview->buffer.getWritePointer(0)
    };
    mixPreview(masterWritePointers, masterBusForPreview->buffer.getNumChannels());

    busManager.applyMasterProcessing();
    
    // Copy master bus to output
    AudioBus* masterBus = busManager.getMasterBus();
    for (int ch = 0; ch < numOutputChannels; ++ch) {
        if (ch < masterBus->buffer.getNumChannels()) {
            for (int i = 0; i < numSamples; ++i) {
                outputChannelData[ch][i] = masterBus->buffer.getSample(ch, i);
            }
        }
    }
    
    // Send to Recorder - now recording from buses!
    {
        std::unique_lock<std::mutex> lock(recordingMutex, std::try_to_lock);
        if (lock.owns_lock()) {
            if (mainRecorder.isRecording()) {
                // Record master bus
                const float* masterChannels[2] = {
                    masterBus->buffer.getReadPointer(0),
                    masterBus->buffer.getNumChannels() > 1 ? masterBus->buffer.getReadPointer(1) : masterBus->buffer.getReadPointer(0)
                };
                mainRecorder.writeBlock(masterChannels, numSamples, 2);
            }
            
            // Record stems if enabled
            if (recordingStems) {
                for (auto& pair : stemRecorders) {
                    const std::string& trackName = pair.first;
                    AudioBus* trackBus = busManager.getTrack(trackName);
                    
                    if (trackBus && trackBus->buffer.getNumSamples() > 0) {
                        const float* trackChannels[2] = {
                            trackBus->buffer.getReadPointer(0),
                            trackBus->buffer.getNumChannels() > 1 ? trackBus->buffer.getReadPointer(1) : trackBus->buffer.getReadPointer(0)
                        };
                        pair.second.writeBlock(trackChannels, numSamples, 2);
                    }
                }
            }
        }
    }
    
    return;
}
    
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
                triggerSample(*pattern, currentStep);
            }
        }
        
        // Mix sample if playing
        if (sampleIsPlaying && samplePlaybackPosition < pattern->sampleBuffer.getNumSamples() && samplePlaybackPosition < samplePlaybackEnd) {
            // --- ANTI-CLICK ENVELOPE (1-2ms fade-in/out) ---
            const int fadeInSamples = 88;
            const int fadeOutSamples = 88;
            int totalSamples = pattern->sampleBuffer.getNumSamples();
            int sampleIdx = (int)samplePlaybackPosition;
            float envelope = 1.0f;
            
            if (sampleIdx < fadeInSamples) {
                envelope = (float)sampleIdx / (float)fadeInSamples;
            }
            else if (sampleIdx >= totalSamples - fadeOutSamples) {
                int samplesRemaining = totalSamples - sampleIdx;
                envelope = (float)samplesRemaining / (float)fadeOutSamples;
            }
            
            // Calculate Gain based on Volume and Pan
            float vol = pattern->volume;
            float pan = pattern->pan;
            
            // Stereo Panning (Simple Linear)
            float leftGain = vol * (1.0f - pan) * 2.0f; // *2 to maintain power at center roughly
            float rightGain = vol * pan * 2.0f;
            
            // Allow over 1.0 slightly if needed, but clamp pan
            if (pan < 0.0f) pan = 0.0f;
            if (pan > 1.0f) pan = 1.0f;
            
            for (int ch = 0; ch < numOutputChannels; ++ch) {
                // Determine gain for this channel
                float gain = 1.0f;
                if (numOutputChannels >= 2) {
                    if (ch == 0) gain = leftGain;
                    else if (ch == 1) gain = rightGain;
                    else gain = vol; // Aux channels get mono mix
                } else {
                    gain = vol; // Mixing down
                }
                
                // Mix
                int srcCh = std::min(ch, pattern->sampleBuffer.getNumChannels() - 1);
                outputChannelData[ch][i] += pattern->sampleBuffer.getSample(srcCh, sampleIdx) * envelope * gain;
            }
            
            if (sampleIsReverse) {
                samplePlaybackPosition--;
                if (samplePlaybackPosition < 0 || (samplePlaybackEnd > 0 && samplePlaybackPosition <= samplePlaybackEnd)) {
                    sampleIsPlaying = false; // Stop
                }
            } else {
                samplePlaybackPosition++;
                // Check stop
                if (samplePlaybackPosition >= pattern->sampleBuffer.getNumSamples() || samplePlaybackPosition >= samplePlaybackEnd) {
                    sampleIsPlaying = false;
                }
            }
        }
        
        samplePosition++;
    }
    


}

void AudioEngine::previewSlice(Pattern& pattern, int sliceIndex, bool playToEnd) {
    // Thread Safety: Ensure we use the pattern instance managed by AudioEngine
    // Note: 'pattern' passed here is likely the GUI copy. We should check if we have it in our map.
    // However, if the user just edited markers, the GUI copy has the latest markers, but AudioEngine copy might not if 'addPattern' wasn't called.
    // But 'addPattern' IS called on every marker edit (SYNC comment in PatternEditor.cpp).
    // So looking up by name is safest.
    
    std::lock_guard<std::mutex> lock(patternMutex);
    auto it = patterns.find(pattern.name);
    if (it != patterns.end()) {
        Pattern& enginePattern = it->second;
        if (enginePattern.sampleBuffer.getNumSamples() == 0) return;
        
        if (sliceIndex >= 0 && sliceIndex < (int)enginePattern.sliceMarkers.size()) {
            int64_t start = enginePattern.sliceMarkers[sliceIndex];
            int64_t end = enginePattern.sampleBuffer.getNumSamples();
            
            // If NOT playing to end (default cutoff behavior), stop at next marker
            if (!playToEnd) {
                if (sliceIndex + 1 < (int)enginePattern.sliceMarkers.size()) {
                    end = enginePattern.sliceMarkers[sliceIndex + 1];
                }
            }
            
            previewState.sourcePattern = &enginePattern;
            previewState.position = start;
            previewState.endPosition = end;
            previewState.active = true;
        }
    }
}

void AudioEngine::triggerSample(Pattern& pattern, int step) {
    samplePlaybackPosition = 0;
    
    // Default to full sample
    int64_t totalSamples = pattern.sampleBuffer.getNumSamples();
    int64_t endPos = totalSamples;
    int64_t startPos = 0;
    
    // Check for Nudge FX (Bipolar)
    // 0.5 = Center (Full Length)
    // > 0.5 = Offset Start (Cut Attack)
    // < 0.5 = Offset End (Cut Tail / Gate)
    

    if (pattern.stepFX.count(step)) {
        for (int fxId : pattern.stepFX[step]) {
            if (fxId == Pattern::FX_NUDGE) {
                 if (pattern.stepFXParams[step].count(Pattern::PAR_NUDGE_OFFSET)) {
                     float val = pattern.stepFXParams[step][Pattern::PAR_NUDGE_OFFSET];
                     
                     if (val > 0.5f) {
                         // Right Side: Adjust Start
                         float norm = (val - 0.5f) * 2.0f; // 0.0 to 1.0
                         startPos = (int64_t)(norm * totalSamples);
                     } else if (val < 0.5f) {
                         // Left Side: Adjust End
                         float norm = val * 2.0f; // 0.0 to 1.0
                         endPos = (int64_t)(norm * totalSamples);
                     }
                 }
            }

            else if (fxId == Pattern::FX_REVERSE) {
                sampleIsReverse = true;
                // Start from end (default)
                startPos = totalSamples;
                endPos = 0; // Play backwards to 0 
                
                // Note: Slice logic will override this below if both are present
            }
            else if (fxId == Pattern::FX_SLICE) {
                 if (pattern.stepFXParams[step].count(Pattern::PAR_SLICE_INDEX)) {
                     int sliceIdx = (int)pattern.stepFXParams[step][Pattern::PAR_SLICE_INDEX];
                     
                     // Safety Bounds Check
                     if (sliceIdx >= 0 && sliceIdx < (int)pattern.sliceMarkers.size()) {
                         // Normal Slice
                         int64_t sStart = pattern.sliceMarkers[sliceIdx];
                         int64_t sEnd = totalSamples;
                         if (sliceIdx + 1 < (int)pattern.sliceMarkers.size()) {
                             sEnd = pattern.sliceMarkers[sliceIdx + 1];
                         }
                         
                         // Determine end pos (next marker or end)
                         // Handle cutoff
                         bool cutoff = false;
                         if (pattern.stepFXParams[step].count(Pattern::PAR_SLICE_CUTOFF)) {
                             cutoff = (pattern.stepFXParams[step][Pattern::PAR_SLICE_CUTOFF] > 0.5f);
                         }
                         if (!cutoff) {
                             sEnd = totalSamples; // Only if not reverse?
                         }
                         
                         if (sampleIsReverse) {
                             startPos = sEnd;
                             endPos = sStart;
                         } else {
                             startPos = sStart;
                             endPos = sEnd;
                         }
                     }
                 }
            }
        }
    }
    
    // Set Playback State - Add robust checks
    if (startPos >= 0 && startPos < totalSamples && endPos > startPos && endPos <= totalSamples) {
        samplePlaybackPosition = startPos;
        samplePlaybackEnd = endPos;
        sampleIsPlaying = true;
    } else {
        sampleIsPlaying = false; // Fallback to silence if invalid
    }
}

void AudioEngine::startRecording(const std::string& filename, bool stems) {
    recordingStems = stems;
    
    if (stems) {
        // Record individual track buses as stems
        // Get all active track buses
        std::vector<std::string> trackNames;
        
        // Collect all track names from the bus manager
        // We'll record all non-empty tracks
        for (const auto& pair : patternToTrack) {
            const std::string& trackName = pair.second;
            // Check if this track name is already in our list
            bool found = false;
            for (const auto& name : trackNames) {
                if (name == trackName) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                trackNames.push_back(trackName);
            }
        }
        
        // Start a recorder for each track
        for (const auto& trackName : trackNames) {
            std::string stemPath = "recordings/" + filename + "_" + trackName + ".wav";
            stemRecorders[trackName].start(stemPath, sampleRate, 2);
        }
        
        // Also record master mix
        std::string masterPath = "recordings/" + filename + "_master.wav";
        mainRecorder.start(masterPath, sampleRate, 2);
        
    } else {
        // Record only master bus (whole mix)
        std::string path = "recordings/" + filename + ".wav";
        mainRecorder.start(path, sampleRate, 2);
    }
}

void AudioEngine::stopRecording() {
    std::lock_guard<std::mutex> lock(recordingMutex);
    mainRecorder.stop();
    
    // Stop all stem recorders
    for (auto& pair : stemRecorders) {
        pair.second.stop();
    }
    stemRecorders.clear();
    recordingStems = false;
}

bool AudioEngine::isRecording() {
    return mainRecorder.isRecording();
}

// ============================================================================
// Bus/Track Management
// ============================================================================

void AudioEngine::assignPatternToTrack(const std::string& patternName, const std::string& trackName) {
    patternToTrack[patternName] = trackName;
}

std::string AudioEngine::getTrackForPattern(const std::string& patternName) const {
    auto it = patternToTrack.find(patternName);
    if (it != patternToTrack.end()) {
        return it->second;
    }
    // Default fallback: use Track_0
    return "Track_0";
}

AudioBus* AudioEngine::getTrackBus(const std::string& trackName) {
    return busManager.getOrCreateTrack(trackName);
}

