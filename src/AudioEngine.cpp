#include "AudioEngine.h"
#include "fx/FXProcessor.h"
#include <iostream>
#include <cmath>
#include <set>

// Debug logging disabled - sync feature removed for rewrite
#define SYNC_LOG(...) ((void)0)

AudioEngine::AudioEngine() {
    formatManager.registerBasicFormats();
    formatManager.registerFormat(new juce::OggVorbisAudioFormat(), false);
#if JUCE_USE_MP3AUDIOFORMAT
    formatManager.registerFormat(new juce::MP3AudioFormat(), false);
#endif
    formatManager.registerFormat(new juce::FlacAudioFormat(), false);
}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::initialize() {
#if defined(JUCE_ANDROID)
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.bufferSize = 1024; // Larger buffer for stability on Android
    setup.sampleRate = 48000.0;
    
    auto result = deviceManager.initialise(0, 2, nullptr, true, "", &setup);
#else
    auto result = deviceManager.initialiseWithDefaultDevices(0, 2);
#endif

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
    if (patterns.find(name) == patterns.end()) {
        std::cerr << "Pattern not found: " << name << std::endl;
        return;
    }
    
    currentPatternName = name;
    // Unified path: use multi-pattern logic even for single pattern
    // This ensures consistent timing/sync behavior and uses the optimized loop
    playMultiplePatterns({name});
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
        dur = patterns[currentPatternName].getStepDurationSamples(globalBpm.load(), sampleRate);
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
            double dur = patterns[name].getStepDurationSamples(globalBpm.load(), sampleRate);
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

void AudioEngine::updateActivePatterns(const std::vector<std::pair<std::string, std::string>>& patternsWithTracks) {
    if (patternsWithTracks.empty()) {
        stop();
        return;
    }
    
    std::lock_guard<std::mutex> lock(patternMutex);

    // Update track assignments
    std::vector<std::string> names;
    names.reserve(patternsWithTracks.size());
    for (const auto& pair : patternsWithTracks) {
        names.push_back(pair.first);
        if (!pair.second.empty()) {
            patternToTrack[pair.first] = pair.second;
        }
    }
    
    // Build set of requested patterns
    std::set<std::string> newActiveSet(names.begin(), names.end());
    
    // Initialize new patterns, keep existing ones running
    for (const auto& name : names) {
        if (patternStates.find(name) == patternStates.end()) {
            PatternPlayState newState;
            newState.currentStep = 1;
            newState.stepStartSample = 0;
            newState.samplePosition = 0;
            patternStates[name] = newState;
        }
    }
    
    // Rebuild activePatternNames
    activePatternNames.clear();
    for (const auto& name : newActiveSet) {
        activePatternNames.push_back(name);
    }
    
    // Cleanup old states
    std::vector<std::string> toRemove;
    for (auto& pair : patternStates) {
        if (newActiveSet.find(pair.first) == newActiveSet.end()) {
            toRemove.push_back(pair.first);
        }
    }
    for (const auto& r : toRemove) patternStates.erase(r);
    
    if (!playing.load()) {
        playing.store(true);
        paused.store(false);
    }
}

void AudioEngine::queuePatternSwitch(const std::string& trackName, const std::string& newPatternName) {
    std::lock_guard<std::mutex> lock(patternMutex);
    
    // Find currently playing pattern on this track
    std::string currentPattern = "";
    for (const auto& pair : patternStates) {
        if (patternToTrack.count(pair.first) && patternToTrack[pair.first] == trackName) {
            currentPattern = pair.first;
            break;
        }
    }
    
    if (currentPattern.empty()) {
        // No pattern playing on this track - shouldn't happen but handle gracefully
        return;
    }
    
    // Queue the new pattern
    {
        std::lock_guard<std::mutex> qLock(queueMutex);
        pendingPatternQueues[trackName] = newPatternName;
    }
    
    // Mark current pattern to stop at end
    patternStates[currentPattern].stopAtEnd = true;
    
    // Make sure the new pattern has track assignment
    patternToTrack[newPatternName] = trackName;
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
         // Lock mutex for preview mixing (accesses pattern data)
         {
             std::lock_guard<std::mutex> lock(patternMutex);
             mixPreview(outputChannelData, numOutputChannels); 
         }
         
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
        
        // ---------------------------------------------------------
        // OPTIMIZATION: Hoist map lookups out of the hot loop
        // ---------------------------------------------------------
        struct PatternContext {
            Pattern* pattern;
            PatternPlayState* state;
            AudioBus* trackBus;
            std::string patternName;
        };
        
        // Stack allocation for contexts (limit to reasonable max, e.g., 64 patterns)
        // Or use a reusable vector member to avoid allocations, but stack is fast enough for small N
        PatternContext contexts[64]; 
        int activeContextCount = 0;
        
        for (const auto& patName : activePatternNames) {
            auto patIt = patterns.find(patName);
            if (patIt != patterns.end()) {
                // Determine track and bus ONCE
                std::string trackName = patternToTrack.count(patName) ? patternToTrack.at(patName) : "";
                AudioBus* bus = busManager.getOrCreateTrack(trackName);
                
                if (bus && activeContextCount < 64) {
                    contexts[activeContextCount].pattern = &patIt->second;
                    contexts[activeContextCount].state = &patternStates[patName];
                    contexts[activeContextCount].trackBus = bus;
                    contexts[activeContextCount].patternName = patName;
                    activeContextCount++;
                }
            }
        }

        // ---------------------------------------------------------
        // Sample Processing Loop
        // ---------------------------------------------------------
        for (int i = 0; i < numSamples; ++i) {
            for (int k = 0; k < activeContextCount; ++k) {
                PatternContext& ctx = contexts[k];
                Pattern& pattern = *ctx.pattern;
                PatternPlayState& state = *ctx.state;
                AudioBus* trackBus = ctx.trackBus; // Direct pointer access
                

                
                // Check if we need to advance to next step
                double stepDuration = pattern.getStepDurationSamples(globalBpm.load(), sampleRate);
                if (state.samplePosition >= state.stepStartSample + (int64_t)stepDuration) {
                    state.currentStep++;
                    state.stepStartSample = state.samplePosition;
                    
                    // SYNC_LOG("Pattern %s advanced to step %d (total %d)", ctx.patternName.c_str(), state.currentStep, pattern.steps);
                    
                    if (state.currentStep > pattern.steps) {
                        // Check for queued pattern switch (per-slot sync)
                        if (state.stopAtEnd) {
                            std::string trackName = patternToTrack.count(ctx.patternName) ? patternToTrack[ctx.patternName] : "";
                            std::string nextPat = "";
                            
                            {
                                std::lock_guard<std::mutex> qLock(queueMutex);
                                if (!trackName.empty() && pendingPatternQueues.count(trackName)) {
                                    nextPat = pendingPatternQueues[trackName];
                                    pendingPatternQueues.erase(trackName);
                                }
                            }
                            
                            if (!nextPat.empty() && patterns.count(nextPat)) {
                                // Execute the swap
                                std::string oldPatternName = ctx.patternName; // Save for cleanup
                                state.sampleIsPlaying = false;
                                state.stopAtEnd = false;
                                
                                // Initialize state for new pattern
                                PatternPlayState newState;
                                newState.currentStep = 1;
                                newState.stepStartSample = 0;
                                newState.samplePosition = 0;
                                patternStates[nextPat] = newState;
                                patternToTrack[nextPat] = trackName;
                                
                                // Update active pattern names
                                for (size_t idx = 0; idx < activePatternNames.size(); ++idx) {
                                    if (activePatternNames[idx] == oldPatternName) {
                                        activePatternNames[idx] = nextPat;
                                        break;
                                    }
                                }
                                
                                // Remove old pattern from patternStates so next swap finds correct current pattern
                                patternStates.erase(oldPatternName);
                                
                                // Update context for remainder of this audio block
                                ctx.patternName = nextPat;
                                ctx.pattern = &patterns[nextPat];
                                ctx.state = &patternStates[nextPat];
                                
                                // Trigger step 1 of new pattern
                                Pattern& newPattern = *ctx.pattern;
                                PatternPlayState& newStateRef = *ctx.state;
                                if (newPattern.shouldTriggerAt(1)) {
                                    triggerStep(newStateRef, newPattern);
                                }
                                
                                k--;
                                continue;
                            }
                            state.stopAtEnd = false;
                        }
                        
                        // Pattern cycle - reset to step 1
                        state.currentStep = 1;
                        state.stepStartSample = state.samplePosition;

                        // QUEUED SYNC CHECK (Legacy)
                        if (pendingResync.load()) {
                            resyncAllPatternsInternal();
                        }
                    }
                    
                    if (pattern.shouldTriggerAt(state.currentStep)) {
                        triggerStep(state, pattern);
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
                                state.samplePlaybackPosition = (double)pattern.sampleBuffer.getNumSamples(); // Default
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
            } // Close for loop over contexts (patterns)
        } // Close for loop over samples

    
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
    
}
}
    


void AudioEngine::triggerStep(PatternPlayState& state, Pattern& pattern) {
    state.samplePlaybackPosition = 0.0;
    state.sampleIsPlaying = true;
    state.fadeInSamplesRemaining = 88; // 2ms fade-in
    state.isStuttering = false; // Reset start of step
    state.isStuttering = false; // Reset start of step
    state.stutterIntervalSamples = 0;
    
    // Reset ADSR
    state.useADSR = false;
    state.adsrPhase = 0;
    state.adsrCurrentValue = 0.0f;
    state.adsrAttackRate = 0.0f;
    
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
    fxProcessor.processStepFX(state, pattern, state.currentStep, globalBpm.load(), sampleRate);
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



void AudioEngine::startRecording(const std::string& filename, bool stems) {
    std::lock_guard<std::mutex> lock(recordingMutex);
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
    std::lock_guard<std::mutex> lock(patternMutex);
    patternToTrack[patternName] = trackName;
}

std::string AudioEngine::getTrackForPattern(const std::string& patternName) const {
    std::lock_guard<std::mutex> lock(patternMutex);
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

void AudioEngine::scheduleResync() {
    pendingResync.store(true);
}

void AudioEngine::resyncAllPatternsInternal() {
    // Mutex is already locked by caller (audioDeviceIOCallbackWithContext or resyncAllPatterns)
    
    // Clear flag
    pendingResync.store(false);
    
    // Calculate global time
    double currentBpm = globalBpm.load();
    
    for (auto& pair : patternStates) {
        std::string name = pair.first;
        PatternPlayState& state = pair.second;
        
        if (patterns.find(name) == patterns.end()) continue;
        Pattern& pattern = patterns[name];
        
        // Calculate step duration for this pattern
        double stepDuration = pattern.getStepDurationSamples(currentBpm, sampleRate);
        
        if (stepDuration <= 0.001) continue;
        
        // Calculate exact step including fractional part
        double totalSteps = (double)audioFrameCount.load() / stepDuration;
        int64_t fullSteps = (int64_t)totalSteps;
        double phase = totalSteps - (double)fullSteps;
        
        // Wrap to pattern length
        int stepIndex = (fullSteps % pattern.steps); 
        
        int oldStep = state.currentStep;
        
        state.currentStep = stepIndex + 1; // 1-based
        
        // Reset sample position to be aligned with the step phase
        state.samplePosition = 0;
        state.stepStartSample = -(int64_t)(phase * stepDuration);
        
        // Reset sub-step state (force fresh start if we jumped)
        state.sampleIsPlaying = false; 
        state.isStuttering = false;
        state.isSliding = false;
        
        // CRITICAL FIX: If we landed on a note, TRIGGER IT!
        // But only if we are at the very start (phase is small) or we jumped to a new step.
        // Actually, trigger it regardless, but we need to respect the offset.
        // However, `triggerStep` assumes samplePosition=0.
        // We set samplePosition=0 above, but stepStartSample is negative to account for delay.
        // So effectively we are at `(phase * stepDuration)` into the step.
        
        if (pattern.shouldTriggerAt(state.currentStep)) {
             // We manually trigger it
             triggerStep(state, pattern);
             
             // Adjust playback position to account for the phase
             if (state.sampleIsPlaying) {
                 double samplesIntoStep = phase * stepDuration;
                 state.samplePlaybackPosition += samplesIntoStep * state.currentSpeedRatio;
                 
                 // Fade-in adjustment? Maybe skip it if we are far in.
                 if (samplesIntoStep > 100) state.fadeInSamplesRemaining = 0;
             }
        }
    }
}

// Legacy/Immediate (calls internal directly)
void AudioEngine::resyncAllPatterns() {
    std::lock_guard<std::mutex> lock(patternMutex);
    resyncAllPatternsInternal();
}

