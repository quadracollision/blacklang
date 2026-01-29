#include "AudioEngine.h"
#include "fx/FXProcessor.h"
#include <iostream>
#include <cmath>

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>

#if defined(__ANDROID__)
#include <android/log.h>
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "AudioEngine", __VA_ARGS__)
#else
#define LOGD(...) 
#endif
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
    std::lock_guard<std::mutex> lock(patternMutex);
    playing.store(false);
    paused.store(false);
    sampleIsPlaying = false;
    activePatternNames.clear();
    patternStates.clear();
}

void AudioEngine::clearAllPatterns() {
    std::lock_guard<std::mutex> lock(patternMutex);
    playing.store(false);
    paused.store(false);
    sampleIsPlaying = false;
    activePatternNames.clear();
    patternStates.clear();
    patterns.clear();
    currentPatternName.clear();
    pendingPatternQueues.clear();
}

void AudioEngine::pause() {
    if (playing.load()) {
        paused.store(true);
    }
}

// Update active patterns with specific track assignments
void AudioEngine::updateActivePatterns(const std::vector<std::pair<std::string, std::string>>& patternsWithTracks) {
    if (patternsWithTracks.empty()) {
        stop();
        return;
    }
    
    std::lock_guard<std::mutex> lock(patternMutex);

    // Build list of composite keys (instances)
    std::vector<std::string> newActiveInstanceKeys;
    newActiveInstanceKeys.reserve(patternsWithTracks.size());
    
    for (const auto& pair : patternsWithTracks) {
        // key = "PatternName@TrackName" to allow same pattern on multiple tracks
        std::string key = pair.first + "@" + pair.second;
        newActiveInstanceKeys.push_back(key);
        
        // Store track mapping for this specific instance
        patternToTrack[key] = pair.second;
    }
    
    // Build set for fast lookup
    std::set<std::string> newActiveSet(newActiveInstanceKeys.begin(), newActiveInstanceKeys.end());
    
    // Initialize new active instances, keep existing ones running
    for (size_t i = 0; i < newActiveInstanceKeys.size(); ++i) {
        const std::string& key = newActiveInstanceKeys[i];
        const std::string& baseName = patternsWithTracks[i].first;
        
        if (patternStates.find(key) == patternStates.end()) {
             // New instance starting now
             if (patterns.count(baseName)) {
                PatternPlayState newState;
                newState.currentStep = 0;
                double dur = patterns[baseName].getStepDurationSamples(globalBpm.load(), sampleRate);
                newState.stepStartSample = -(int64_t)dur;
                newState.samplePosition = 0;
                patternStates[key] = newState;
             }
        }
    }
    
    // Update active list
    activePatternNames = newActiveInstanceKeys;
    
    // Cleanup old states
    std::vector<std::string> toRemove;
    for (auto& pair : patternStates) {
        if (newActiveSet.find(pair.first) == newActiveSet.end()) {
            toRemove.push_back(pair.first);
        }
    }
    for (const auto& r : toRemove) {
        patternStates.erase(r);
        patternToTrack.erase(r); // Also clean up track map
    }
    
    if (!playing.load()) {
        playing.store(true);
        paused.store(false);
    }
}

// Update active patterns with specific track assignments and beat offsets (for seeking)
void AudioEngine::updateActivePatternsWithOffset(const std::vector<std::tuple<std::string, std::string, double>>& patternsWithTracksAndOffsets) {
    if (patternsWithTracksAndOffsets.empty()) {
        stop();
        return;
    }
    
    std::lock_guard<std::mutex> lock(patternMutex);

    // Build list of composite keys (instances)
    std::vector<std::string> newActiveInstanceKeys;
    newActiveInstanceKeys.reserve(patternsWithTracksAndOffsets.size());
    
    for (const auto& tuple : patternsWithTracksAndOffsets) {
        const std::string& patternName = std::get<0>(tuple);
        const std::string& trackName = std::get<1>(tuple);
        // key = "PatternName@TrackName" to allow same pattern on multiple tracks
        std::string key = patternName + "@" + trackName;
        newActiveInstanceKeys.push_back(key);
        
        // Store track mapping for this specific instance
        patternToTrack[key] = trackName;
    }
    
    // Build set for fast lookup
    std::set<std::string> newActiveSet(newActiveInstanceKeys.begin(), newActiveInstanceKeys.end());
    
    // Initialize new active instances with offset, keep existing ones running
    for (size_t i = 0; i < newActiveInstanceKeys.size(); ++i) {
        const std::string& key = newActiveInstanceKeys[i];
        const std::string& baseName = std::get<0>(patternsWithTracksAndOffsets[i]);
        double beatOffset = std::get<2>(patternsWithTracksAndOffsets[i]);
        
        // Always reinitialize with the new offset (for seek functionality)
        if (patterns.count(baseName)) {
            Pattern& pattern = patterns[baseName];
            PatternPlayState newState;
            
            // Calculate starting step from beat offset
            // beatOffset is beats into the pattern
            // steps per bar depends on syncBase
            int stepsPerBar = (pattern.syncBase > 0) ? pattern.syncBase : pattern.steps;
            double stepsPerBeat = stepsPerBar / 4.0;  // 4 beats per bar
            double stepOffset = beatOffset * stepsPerBeat;
            
            // Wrap if offset exceeds pattern length
            int startStep = ((int)stepOffset % pattern.steps) + 1; // 1-indexed
            if (startStep < 1) startStep = 1;
            if (startStep > pattern.steps) startStep = 1;
            
            newState.currentStep = startStep;
            double stepDur = pattern.getStepDurationSamples(globalBpm.load(), sampleRate);
            
            // Calculate how far into the current step we should be
            double fractionalStep = stepOffset - (int)stepOffset;
            int64_t samplesIntoStep = (int64_t)(fractionalStep * stepDur);
            
            newState.stepStartSample = -samplesIntoStep;
            newState.samplePosition = 0;
            patternStates[key] = newState;
            
            // Trigger the current step if it should play
            if (pattern.shouldTriggerAt(startStep)) {
                triggerStep(patternStates[key], pattern);
            }
        }
    }
    
    // Update active list
    activePatternNames = newActiveInstanceKeys;
    
    // Cleanup old states
    std::vector<std::string> toRemove;
    for (auto& pair : patternStates) {
        if (newActiveSet.find(pair.first) == newActiveSet.end()) {
            toRemove.push_back(pair.first);
        }
    }
    for (const auto& r : toRemove) {
        patternStates.erase(r);
        patternToTrack.erase(r);
    }
    
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
    
    // Fuzzy search for "PatternName@AnyTrack"
    // This allows TrackView to see progress even if the pattern is playing on a different track
    size_t nameLen = name.length();
    for (const auto& pair : patternStates) {
        // Check if key starts with "name@"
        // But first, handle if 'name' itself is an instance key (contains @), don't double search?
        // Actually, if 'name' was an instance key, the direct lookup above would have found it.
        // So we assume 'name' *might* be just the base name, OR a specific instance key that didn't match.
        // If 'name' is "Pat1@Track1" and we want to find "Pat1@Track2" ?? 
        // No, current logic in TrackView tries specific then base.
        // So if TrackView called with "Pat1", we want to find "Pat1@TrackX".
        
        // Check prefix
        if (pair.first.length() > nameLen && 
            pair.first.rfind(name + "@", 0) == 0) {
            return pair.second.currentStep;
        }
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
            
            const int fadeLen = 88;
            for (int i = 0; i < numSamples; ++i) {
                if (previewState.active && previewState.position < end && previewState.position < totalSamples) {
                     float vol = 1.0f; 
                     
                     // 1. Fade-in
                     if (previewState.fadeInSamplesRemaining > 0) {
                         vol *= (1.0f - (float)previewState.fadeInSamplesRemaining / (float)fadeLen);
                         previewState.fadeInSamplesRemaining--;
                     }
                     
                     // 2. Fade-out near end
                     if (previewState.position >= end - fadeLen) {
                         int64_t remaining = end - previewState.position;
                         if (remaining < 0) remaining = 0;
                         vol *= ((float)remaining / (float)fadeLen);
                     }

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
    
    // If NOT playing, handle Preview and Skip Playback Logic
    if (!playing.load() || paused.load()) {
         // Lock mutex for preview mixing (accesses pattern data)
         {
             std::lock_guard<std::mutex> lock(patternMutex);
             mixPreview(outputChannelData, numOutputChannels); 
         }
         
         // Recording Preview Playback (from in-memory buffer)
         if (recordingPreviewActive.load() && recordedSampleCount > 0) {
             for (int i = 0; i < numSamples; ++i) {
                 if (recordingPreviewPosition >= recordedSampleCount) {
                     recordingPreviewActive.store(false);
                     break;
                 }
                 
                 for (int ch = 0; ch < numOutputChannels; ++ch) {
                     int srcCh = std::min(ch, recordedMasterBuffer.getNumChannels() - 1);
                     outputChannelData[ch][i] += recordedMasterBuffer.getSample(srcCh, (int)recordingPreviewPosition);
                 }
                 recordingPreviewPosition++;
             }
         }
         
         // SKIP playback logic, fall through to Recording Logic at bottom
         goto recording_block;
    }
    
    // PLAYBACK LOGIC STARTS HERE
    {
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
        PatternContext contexts[64]; 
        int activeContextCount = 0;
        
        for (const auto& instanceKey : activePatternNames) {
            // Extract base pattern name from instance key "Pattern@Track"
            // If separation not found, treat whole string as pattern name (legacy/single-mode)
            std::string baseName = instanceKey;
            size_t sep = instanceKey.find('@');
            if (sep != std::string::npos) {
                baseName = instanceKey.substr(0, sep);
            }

            auto patIt = patterns.find(baseName);
            if (patIt != patterns.end()) {
                // Determine track and bus
                // Search using full instance key first
                std::string trackName = "";
                if (patternToTrack.count(instanceKey)) {
                    trackName = patternToTrack.at(instanceKey);
                } else if (patternToTrack.count(baseName)) {
                     // Fallback for non-instance keys
                    trackName = patternToTrack.at(baseName);
                }
                
                AudioBus* bus = busManager.getOrCreateTrack(trackName);
                
                if (bus && activeContextCount < 64) {
                    contexts[activeContextCount].pattern = &patIt->second;
                    contexts[activeContextCount].state = &patternStates[instanceKey]; // Use instance key for state
                    contexts[activeContextCount].trackBus = bus;
                    contexts[activeContextCount].patternName = instanceKey; // Store key for context (important for queue updates)
                    activeContextCount++;
                }
            }
        }

        // ---------------------------------------------------------
        // Sample Processing Loop
        // ---------------------------------------------------------
        double currentSR = sampleRate;
        int currentBPM = globalBpm.load();
        
        for (int i = 0; i < numSamples; ++i) {
            for (int k = 0; k < activeContextCount; ++k) {
                PatternContext& ctx = contexts[k];
                Pattern& pattern = *ctx.pattern;
                PatternPlayState& state = *ctx.state;
                AudioBus* trackBus = ctx.trackBus; 
                
                // Check if we need to advance to next step
                double stepDuration = pattern.getStepDurationSamples(currentBPM, currentSR);
                if (state.samplePosition >= state.stepStartSample + (int64_t)stepDuration) {
                    state.currentStep++;
                    state.stepStartSample = state.samplePosition;
                    
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
                                std::string oldPatternName = ctx.patternName; 
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
                                
                                // Remove old pattern from patternStates
                                patternStates.erase(oldPatternName);
                                
                                // Update context
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
                
                if (state.playbackDelaySamples > 0) {
                     state.playbackDelaySamples--;
                } 
                // Process and mix sample if playing (and not delaying)
                else if (pattern.sampleBuffer.getNumSamples() > 0 && state.sampleIsPlaying) {
                    if (state.isReverse) {
                        state.samplePlaybackPosition -= state.currentSpeedRatio;
                    } else {
                        state.samplePlaybackPosition += state.currentSpeedRatio;
                    }
                    
                    // Handle Stutter Re-trigger
                    if (state.isStuttering && state.stutterIntervalSamples > 0) {
                        int64_t samplesInStep = state.samplePosition - state.stepStartSample;
                        if (samplesInStep > 0 && samplesInStep % state.stutterIntervalSamples == 0) {
                            if (state.isReverse) {
                                state.samplePlaybackPosition = (double)pattern.sampleBuffer.getNumSamples(); 
                            } else {
                                state.samplePlaybackPosition = 0.0; 
                            }
                            state.fadeInSamplesRemaining = 88; 
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
                    
                    // Stop conditions - simplified (actual stop handled in envelope logic below)
                    bool forceStop = false;
                    if (!state.isReverse) {
                        if (state.samplePlaybackPosition >= (double)pattern.sampleBuffer.getNumSamples() + 10.0) forceStop = true; // Safety margin
                    } else {
                        if (state.samplePlaybackPosition < -10.0) forceStop = true;
                    }
                    if (forceStop) state.sampleIsPlaying = false;

                     float currentSample = 0.0f;
                     int sampleIdx = (int)state.samplePlaybackPosition;
                     
                     if (state.sampleIsPlaying && sampleIdx >= 0 && sampleIdx < pattern.sampleBuffer.getNumSamples()) {
                        float sample = pattern.sampleBuffer.getSample(0, sampleIdx);
                        
                        // Per-Sample FX (Filter, etc.)
                        sample = fxProcessor.processSampleFX(state, sample, currentSR, state.currentStep, pattern);
                        
                        // --- USER FADE SETTINGS ---
                        float fadeEnv = 1.0f;
                        
                        // Calculate fade duration in samples (based on percentage)
                        // If Slice Mode is active (detected by playback starting > 0 or slice params), use slice duration?
                        // User Request: "fade in either the entire sample or just the slices"
                        // Since we track playbackStartPosition, we can calculate relatively.
                        // "Duration" for fade calc:
                        double totalDur = (double)pattern.sampleBuffer.getNumSamples();
                        double effectiveStart = 0.0;
                        double effectiveEnd = totalDur;
                        
                        if (pattern.fadeSlices && state.playbackStartPosition > 0) {
                            effectiveStart = state.playbackStartPosition;
                            // Estimate end? If sliceEndPosition is set (Cutoff or Reverse), use it.
                            // If just playing through, maybe use next marker?
                            // For simplicity/robustness: Use a reasonable duration or just relative to start.
                            // If Cutoff is active, we have state.sampleEndPosition.
                            if (state.sampleEndPosition > 0) effectiveEnd = (double)state.sampleEndPosition;
                            // Else we don't strictly knwo when "slice" ends if playing through.
                            // But usually "Fade Slice" implies fading the slice itself.
                        }
                        
                        double duration = effectiveEnd - effectiveStart;
                        if (duration < 100.0) duration = 100.0; // Safety floor
                        
                        // Determine active Fade Params
                        float activeFadeIn = 0.0f;
                        float activeFadeOut = 0.0f;
                        
                        if (pattern.fadeSlices) {
                            // Slice Mode: Default 0, check for overrides
                            if (state.currentSliceIndex >= 0) {
                                if (pattern.sliceFadeIns.count(state.currentSliceIndex)) {
                                    activeFadeIn = pattern.sliceFadeIns.at(state.currentSliceIndex);
                                }
                                if (pattern.sliceFadeOuts.count(state.currentSliceIndex)) {
                                    activeFadeOut = pattern.sliceFadeOuts.at(state.currentSliceIndex);
                                }
                            }
                        } else {
                            // Global Mode
                            activeFadeIn = pattern.fadeIn;
                            activeFadeOut = pattern.fadeOut;
                        }
                        
                        // Apply Fade In
                        if (activeFadeIn > 0.001f) {
                            double fadeInLen = duration * activeFadeIn;
                            double posInFade = state.samplePlaybackPosition - effectiveStart;
                            if (state.isReverse) posInFade = effectiveEnd - state.samplePlaybackPosition; // In reverse, start is end
                            
                            if (posInFade < fadeInLen && posInFade >= 0) {
                                fadeEnv *= (float)(posInFade / fadeInLen);
                            }
                        }
                        
                        // Apply Fade Out
                        if (activeFadeOut > 0.001f) {
                            double fadeOutLen = duration * activeFadeOut;
                            double distFromEnd = effectiveEnd - state.samplePlaybackPosition;
                            if (state.isReverse) distFromEnd = state.samplePlaybackPosition - effectiveStart;
                            
                            if (distFromEnd < fadeOutLen && distFromEnd >= 0) {
                                fadeEnv *= (float)(distFromEnd / fadeOutLen);
                            }
                        }
                        
                        sample *= fadeEnv;

                        // --- ANTI-CLICK ENVELOPE (2ms fade-in/out) ---
                        const double fadeLen = 88.0;
                        float envelope = 1.0f;
                        
                        // 1. Fade-In (Start of note/stutter)
                        if (state.fadeInSamplesRemaining > 0) {
                            envelope *= (1.0f - (float)state.fadeInSamplesRemaining / (float)fadeLen);
                            state.fadeInSamplesRemaining--;
                        }
                        
                        // 2. Fade-Out (End of buffer or Slice Cutoff)
                        if (state.isReverse) {
                            double target = (state.sampleEndPosition > 0) ? (double)state.sampleEndPosition : 0.0;
                            if (state.samplePlaybackPosition <= target) {
                                envelope = 0.0f;
                                state.sampleIsPlaying = false;
                            } else if (state.samplePlaybackPosition <= target + fadeLen) {
                                float dist = (float)(state.samplePlaybackPosition - target);
                                envelope *= (dist / (float)fadeLen);
                            }
                        } else {
                            double target = (state.sampleEndPosition > 0 && state.sampleEndPosition < pattern.sampleBuffer.getNumSamples()) 
                                           ? (double)state.sampleEndPosition : (double)pattern.sampleBuffer.getNumSamples();
                            
                            if (state.samplePlaybackPosition >= target) {
                                envelope = 0.0f;
                                state.sampleIsPlaying = false;
                            } else if (state.samplePlaybackPosition >= target - fadeLen) {
                                float dist = (float)(target - state.samplePlaybackPosition);
                                envelope *= (dist / (float)fadeLen);
                            }
                        }
                        
                        // 3. Stutter Border Fade-Out (Ensure repeats don't click)
                        if (state.sampleIsPlaying && state.isStuttering && state.stutterIntervalSamples > 0) {
                            int64_t samplesInStutter = (state.samplePosition - state.stepStartSample) % state.stutterIntervalSamples;
                            int64_t remainingInStutter = state.stutterIntervalSamples - samplesInStutter;
                            if (remainingInStutter < (int64_t)fadeLen) {
                                envelope *= ((float)remainingInStutter / (float)fadeLen);
                            }
                        }
                        
                        if (envelope < 0.0f) envelope = 0.0f;
                        if (envelope > 1.0f) envelope = 1.0f;
                        
                        currentSample = sample * state.currentVelocity * envelope;
                     }

                     // Write to track bus instead of output
                     for (int ch = 0; ch < std::min(numOutputChannels, trackBus->buffer.getNumChannels()); ++ch) {
                         trackBus->buffer.addSample(ch, i, currentSample);
                     }
                } 
                
                state.samplePosition++;
            } 
        } 

    
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
    }
    
    // Copy master bus to output
    AudioBus* masterBus = busManager.getMasterBus();
    for (int ch = 0; ch < numOutputChannels; ++ch) {
        if (ch < masterBus->buffer.getNumChannels()) {
            const float* src = masterBus->buffer.getReadPointer(ch);
            if (outputChannelData[ch]) {
                juce::FloatVectorOperations::copy(outputChannelData[ch], src, numSamples);
            }
        } else {
            if (outputChannelData[ch]) {
                juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
            }
        }
    }
    } // End Playback Scope
    
    // Feed resample manager if active (isolated from other recording)
    if (resampleManager.isRecording()) {
        resampleManager.feedSamples(outputChannelData, numSamples, numOutputChannels);
    }
    
    // Send to Recorder - now recording from buses!
    recording_block:
    {
        std::unique_lock<std::mutex> lock(recordingMutex, std::try_to_lock);
        if (lock.owns_lock()) {
            
            // 1. File-based recording (legacy)
            if (mainRecorder.isRecording()) {
                // Always record what is going to output
                mainRecorder.writeBlock(outputChannelData, numSamples, numOutputChannels);
            }
            
            // Record stems if enabled (file-based)
            if (recordingStems) {
                for (auto& pair : stemRecorders) {
                    const std::string& trackName = pair.first;
                    AudioBus* trackBus = busManager.getTrack(trackName);
                    
                    if (playing.load() && !paused.load() && trackBus && trackBus->buffer.getNumSamples() > 0) {
                        const float* trackChannels[2] = {
                            trackBus->buffer.getReadPointer(0),
                            trackBus->buffer.getNumChannels() > 1 ? trackBus->buffer.getReadPointer(1) : trackBus->buffer.getReadPointer(0)
                        };
                        pair.second.writeBlock(trackChannels, numSamples, 2);
                    } else {
                         // Write silence for stems if stopped or track empty
                         float* silence[2];
                         static float silentBuffer[4096]; // Safe fixed size, static to avoid stack overhead
                         std::fill(silentBuffer, silentBuffer + std::min(4096, numSamples), 0.0f);
                         silence[0] = silentBuffer;
                         silence[1] = silentBuffer;
                         pair.second.writeBlock(silence, numSamples, 2);
                    }
                }
            }
            
            // 2. In-Memory Recording (New - for Android)
            if (inMemoryRecording.load() && recordedSampleCount + numSamples <= recordedBufferCapacity) {
                // Copy master to buffer (Always use outputChannelData - works for Playing AND Stopped/Preview)
                for (int ch = 0; ch < 2; ++ch) {
                    // Safety check: map to available output channels
                    float* src = outputChannelData[ch < numOutputChannels ? ch : 0];
                    for (int i = 0; i < numSamples; ++i) {
                        recordedMasterBuffer.setSample(ch, recordedSampleCount + i, src[i]);
                    }
                }
                
                // Copy stems if enabled
                if (inMemoryRecordingStems) {
                    bool canReadBuses = playing.load() && !paused.load();
                    
                    for (auto& pair : recordedStemBuffers) {
                        const std::string& trackName = pair.first;
                        AudioBus* trackBus = busManager.getTrack(trackName);
                        
                        if (canReadBuses && trackBus && trackBus->buffer.getNumSamples() > 0) {
                            for (int ch = 0; ch < 2; ++ch) {
                                int srcCh = std::min(ch, trackBus->buffer.getNumChannels() - 1);
                                pair.second.copyFrom(ch, recordedSampleCount, trackBus->buffer, srcCh, 0, numSamples);
                            }
                        } else {
                            // Write silence
                            pair.second.clear(recordedSampleCount, numSamples);
                        }
                    }
                }
                
                recordedSampleCount += numSamples;
            }
        }
    }
    
}

    


void AudioEngine::triggerStep(PatternPlayState& state, Pattern& pattern) {
    state.samplePlaybackPosition = 0.0;
    state.sampleIsPlaying = true;
    state.fadeInSamplesRemaining = 88; // 2ms fade-in
    state.sampleEndPosition = 0;       // Reset boundaries for logic check
    state.sliceEndPosition = -1;
    state.sampleEndPosition = 0;       // Reset boundaries for logic check
    state.sliceEndPosition = -1;
    state.sampleEndPosition = 0;       // Reset boundaries for logic check
    state.sliceEndPosition = -1;
    state.stopAtSliceEnd = false;
    state.playbackStartPosition = 0.0; // Reset start pos (default 0 or updated by Slice FX)
    state.currentSliceIndex = -1;      // Reset slice tracker
    state.isStuttering = false; // Reset start of step
    state.stutterIntervalSamples = 0;
    state.isReverse = false;   // Reset reverse mode - only applies if FX_REVERSE is on current step
    
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
            previewState.fadeInSamplesRemaining = 88; // 2ms fade-in
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
        pair.second.fadeInSamplesRemaining = 88; 
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

// NEW: Sync only patterns with beatsync enabled (syncBase > 0) to their next measure
void AudioEngine::syncPatternsWithBeatsync(const std::vector<std::string>& patternNames) {
    std::lock_guard<std::mutex> lock(patternMutex);
    
    for (const std::string& name : patternNames) {
        auto stateIt = patternStates.find(name);
        if (stateIt == patternStates.end()) continue;
        
        auto patIt = patterns.find(name);
        if (patIt == patterns.end()) continue;
        
        Pattern& pattern = patIt->second;
        
        // ONLY sync patterns with syncBase > 0 (beatsync enabled)
        if (pattern.syncBase <= 0) continue;
        
        PatternPlayState& state = stateIt->second;
        int currentStep = state.currentStep;
        
        // Calculate next measure boundary based on syncBase
        int stepsIntoCycle = (currentStep - 1) % pattern.syncBase;
        int nextBoundary;
        
        if (stepsIntoCycle == 0) {
            // Already at boundary, jump to next one
            nextBoundary = currentStep + pattern.syncBase;
        } else {
            // Jump forward to next boundary
            nextBoundary = currentStep + (pattern.syncBase - stepsIntoCycle);
        }
        
        // Wrap around if needed
        if (nextBoundary > pattern.steps) {
            nextBoundary = 1 + ((nextBoundary - 1) % pattern.steps);
        }
        
        // Jump to the boundary
        state.currentStep = nextBoundary;
        state.stepStartSample = audioFrameCount.load();
        
        // Reset playback state
        state.samplePosition = 0;
        state.sampleIsPlaying = false;
        state.isStuttering = false;
        state.isSliding = false;
        state.fadeInSamplesRemaining = 88;
        
        // Trigger note if present
        if (pattern.shouldTriggerAt(nextBoundary)) {
            triggerStep(state, pattern);
        }
    }
}


// ============================================================================
// In-Memory Recording (New - for Android)
// ============================================================================

void AudioEngine::armRecording(bool stems) {
    std::lock_guard<std::mutex> lock(recordingMutex);
    inMemoryRecordingStems = stems;
    recordingArmed.store(true);
    
    // Pre-allocate buffers (10 minutes at 44.1kHz stereo)
    double safeRate = sampleRate > 0 ? sampleRate : 44100.0;
    int maxSamples = (int)(safeRate * 60 * 10); // 10 minutes
    recordedBufferCapacity = maxSamples;
    recordedSampleCount = 0;
    
    recordedMasterBuffer.setSize(2, maxSamples, false, true, false);
    recordedMasterBuffer.clear();
    
    if (stems) {
        // Will allocate stem buffers when recording starts (after we know track names)
        recordedStemBuffers.clear();
    }
}

void AudioEngine::disarmRecording() {
    recordingArmed.store(false);
    inMemoryRecording.store(false);
}

void AudioEngine::startInMemoryRecording() {
    if (!recordingArmed.load()) return;
    
    std::lock_guard<std::mutex> lock(recordingMutex);
    recordedSampleCount = 0;
    
    // SAFETY CHECK: Ensure buffer has capacity. 
    // If sampleRate was 0 during Arm(), capacity might be 0. Fix it now.
    if (recordedBufferCapacity < 44100 * 60) { // Less than 1 minute? Suspicious.
        double safeRate = sampleRate > 0 ? sampleRate : 44100.0;
        int maxSamples = (int)(safeRate * 60 * 10); // 10 minutes
        recordedBufferCapacity = maxSamples;
        recordedMasterBuffer.setSize(2, maxSamples, false, true, false);
        recordedMasterBuffer.clear();
        
        if (inMemoryRecordingStems) {
            recordedStemBuffers.clear();
        }
    }
    
    if (inMemoryRecordingStems) {
        // Allocate buffers for each active track
        recordedStemBuffers.clear();
        for (const auto& pair : patternToTrack) {
            const std::string& trackName = pair.second;
            if (recordedStemBuffers.find(trackName) == recordedStemBuffers.end()) {
                recordedStemBuffers[trackName].setSize(2, recordedBufferCapacity, false, true, false);
                recordedStemBuffers[trackName].clear();
            }
        }
    }
    
    inMemoryRecording.store(true);
    recordingArmed.store(false); // No longer armed, now recording
}

void AudioEngine::stopInMemoryRecording() {
    inMemoryRecording.store(false);
}

bool AudioEngine::saveRecordingWrapper(const std::string& filepath) {
    return saveRecordedAudio(filepath);
}    
void AudioEngine::clearRecordedBuffers() {
    std::lock_guard<std::mutex> lock(recordingMutex);
    recordedMasterBuffer.clear();
    recordedStemBuffers.clear();
    recordedSampleCount = 0;
}

void AudioEngine::startRecordingPreview(const std::string& stemName) {
    recordingPreviewStem = stemName;
    recordingPreviewPosition = 0;
    recordingPreviewActive.store(true);
}

void AudioEngine::stopRecordingPreview() {
    recordingPreviewActive.store(false);
}

void AudioEngine::seekRecordingPreview(int64_t sample) {
    if (sample < 0) sample = 0;
    if (sample >= recordedSampleCount) sample = recordedSampleCount - 1;
    recordingPreviewPosition = sample;
}

bool AudioEngine::saveRecordedAudio(const std::string& filepath) {
    if (recordedSampleCount <= 0) return false;
    
    std::lock_guard<std::mutex> lock(recordingMutex);
    
    // Create output file
    juce::File outputFile(filepath);
    outputFile.deleteFile(); // Remove if exists
    
    // Create WAV format writer
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(
            new juce::FileOutputStream(outputFile),
            sampleRate,
            2,  // Stereo
            16, // 16-bit
            {},
            0
        )
    );
    

    
    if (!writer) {
        LOGD("AudioEngine::saveRecordedAudio - Failed to create WAV writer for: %s", filepath.c_str());
        return false;
    }
    
    LOGD("AudioEngine::saveRecordedAudio - Writing %d samples to %s", recordedSampleCount, filepath.c_str());
    
    // Write samples
    writer->writeFromAudioSampleBuffer(recordedMasterBuffer, 0, recordedSampleCount);
    writer->flush();
    
    return true;
}

bool AudioEngine::saveRecordedStems(const std::string& directory, const std::string& baseName) {
    if (recordedSampleCount <= 0) return false;
    if (recordedStemBuffers.empty()) {
        // No stems, just save master
        return saveRecordedAudio(directory + "/" + baseName + ".wav");
    }
    
    std::lock_guard<std::mutex> lock(recordingMutex);
    
    bool success = true;
    
    // Save master
    {
        juce::File masterFile(directory + "/" + baseName + "_master.wav");
        masterFile.deleteFile();
        
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(
                new juce::FileOutputStream(masterFile),
                sampleRate, 2, 16, {}, 0
            )
        );
        
        if (writer) {
            writer->writeFromAudioSampleBuffer(recordedMasterBuffer, 0, recordedSampleCount);
            writer->flush();
        } else {
            success = false;
        }
    }
    
    // Save each stem
    for (const auto& pair : recordedStemBuffers) {
        const std::string& stemName = pair.first;
        const juce::AudioBuffer<float>& buffer = pair.second;
        
        juce::File stemFile(directory + "/" + baseName + "_" + stemName + ".wav");
        stemFile.deleteFile();
        
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(
                new juce::FileOutputStream(stemFile),
                sampleRate, 2, 16, {}, 0
            )
        );
        
        if (writer) {
            writer->writeFromAudioSampleBuffer(buffer, 0, recordedSampleCount);
            writer->flush();
        } else {
            success = false;
        }
    }
    
    return success;
}
