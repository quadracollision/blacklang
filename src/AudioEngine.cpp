#include "AudioEngine.h"
#include <iostream>

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
                
                if (pattern.sampleBuffer.getNumSamples() == 0) continue;
                
                // Check if we need to advance to next step
                double stepDuration = pattern.getStepDurationSamples();
                if (state.samplePosition >= state.stepStartSample + (int64_t)stepDuration) {
                    state.currentStep++;
                    state.stepStartSample = state.samplePosition;
                    
                    if (state.currentStep > pattern.steps) {
                        state.currentStep = 1;
                        state.stepStartSample = 0;
                        state.samplePosition = 0;
                    }
                    
                    if (pattern.shouldTriggerAt(state.currentStep)) {
                        state.samplePlaybackPosition = 0;
                        state.sampleIsPlaying = true;
                    }
                }
                
                // Mix sample if playing
                if (state.sampleIsPlaying && state.samplePlaybackPosition < pattern.sampleBuffer.getNumSamples()) {
                    for (int ch = 0; ch < numOutputChannels; ++ch) {
                        int srcCh = std::min(ch, pattern.sampleBuffer.getNumChannels() - 1);
                        outputChannelData[ch][i] += pattern.sampleBuffer.getSample(srcCh, (int)state.samplePlaybackPosition);
                    }
                    state.samplePlaybackPosition++;
                }
                
                state.samplePosition++;
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
        double stepDuration = pattern->getStepDurationSamples();
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

