#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include <algorithm>
#include "FXTypes.h"
#include <raylib.h> // For Color

namespace fx {

struct TrackFXParam {
    std::string name;
    float value;
    float min;
    float max;
    float defaultValue;
    std::string suffix; // e.g. "ms", "%", "Hz"
};

class TrackEffect {
public:
    virtual ~TrackEffect() = default;

    virtual std::string getName() const = 0;
    virtual FXType getType() const = 0;
    
    // Audio Processing
    virtual void prepare(double sampleRate) { this->sampleRate = sampleRate; }
    virtual void process(juce::AudioBuffer<float>& buffer) = 0;
    
    // Parameter Handling
    virtual int getNumParams() const { return params.size(); }
    virtual TrackFXParam& getParam(int index) { return params[index]; }
    virtual void setParam(int index, float value) {
        if (index >= 0 && index < params.size()) {
            params[index].value = std::clamp(value, params[index].min, params[index].max);
            updateInternalParams();
        }
    }
    
    // State
    bool isActive() const { return active; }
    void setActive(bool state) { active = state; }

protected:
    bool active = true;
    double sampleRate = 44100.0;
    std::vector<TrackFXParam> params;
    
    // Called when a parameter changes so subclasses can recalculate coefficients
    virtual void updateInternalParams() {}
};

// ==========================================
// DELAY EFFECT (Simple Feed-Forward/Back)
// ==========================================
class DelayEffect : public TrackEffect {
public:
    DelayEffect() {
        // Param 0: Time (ms)
        params.push_back({"Time", 300.0f, 10.0f, 1000.0f, 300.0f, "ms"});
        // Param 1: Feedback
        params.push_back({"Feedback", 0.4f, 0.0f, 0.95f, 0.4f, ""});
        // Param 2: Mix
        params.push_back({"Mix", 0.3f, 0.0f, 1.0f, 0.3f, "%"});
        
        delayBufferSize = 192000; // Max 2 sec at 96k approx
        delayBuffer.setSize(2, delayBufferSize); // 2 channels
        delayBuffer.clear();
    }
    
    std::string getName() const override { return "Delay"; }
    FXType getType() const override { return FX_DELAY; }
    
    void process(juce::AudioBuffer<float>& buffer) override {
        float timeMs = params[0].value;
        float feedback = params[1].value;
        float mix = params[2].value;
        
        int delaySamples = (int)(timeMs * 0.001 * sampleRate);
        if (delaySamples < 1) delaySamples = 1;
        
        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();
        
        // Ensure delay buffer has enough channels
        if (delayBuffer.getNumChannels() < numChannels) {
            delayBuffer.setSize(numChannels, delayBufferSize, true, true, true);
        }
        
        for (int c = 0; c < numChannels; ++c) {
            const float* inputData = buffer.getReadPointer(c);
            float* outputData = buffer.getWritePointer(c);
            float* delayData = delayBuffer.getWritePointer(c);
            
            for (int i = 0; i < numSamples; ++i) {
                // Read from delay buffer
                int currentReadPos = (writePos + i - delaySamples + delayBufferSize) % delayBufferSize;
                float delayedSample = delayData[currentReadPos];
                
                float input = inputData[i];
                
                // Write to delay buffer
                float toWrite = input + (delayedSample * feedback);
                // Soft clip
                if (toWrite > 2.0f) toWrite = 2.0f;
                if (toWrite < -2.0f) toWrite = -2.0f;
                
                int currentWritePos = (writePos + i) % delayBufferSize;
                delayData[currentWritePos] = toWrite;
                
                // Output mix
                outputData[i] = (input * (1.0f - mix)) + (delayedSample * mix);
            }
        }
        
        writePos = (writePos + numSamples) % delayBufferSize;
    }

private:
    juce::AudioBuffer<float> delayBuffer;
    int delayBufferSize = 0;
    int writePos = 0;
};

// ==========================================
// REVERB EFFECT (JUCE Wrapper)
// ==========================================
class ReverbEffect : public TrackEffect {
public:
    ReverbEffect() {
        // Param 0: Room Size (0-1)
        params.push_back({"Room", 0.5f, 0.0f, 1.0f, 0.5f, ""});
        // Param 1: Damping (0-1)
        params.push_back({"Damp", 0.5f, 0.0f, 1.0f, 0.5f, ""});
        // Param 2: Wet (0-1)
        params.push_back({"Wet", 0.33f, 0.0f, 1.0f, 0.33f, ""});
        // Param 3: Dry (0-1)
        params.push_back({"Dry", 0.4f, 0.0f, 1.0f, 0.4f, ""});
        // Param 4: Width (0-1)
        params.push_back({"Width", 1.0f, 0.0f, 1.0f, 1.0f, ""});
        
        updateInternalParams();
    }
    
    std::string getName() const override { return "Reverb"; }
    FXType getType() const override { return FX_REVERB; }
    
    void prepare(double sampleRate) override {
        TrackEffect::prepare(sampleRate);
        reverb.setSampleRate(sampleRate);
        updateInternalParams();
    }
    
    void process(juce::AudioBuffer<float>& buffer) override {
        if (buffer.getNumChannels() == 1) {
            // Mono
            reverb.processMono(buffer.getWritePointer(0), buffer.getNumSamples());
        } else if (buffer.getNumChannels() == 2) {
            // Stereo
            reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples());
        }
    }
    
protected:
    void updateInternalParams() override {
        juce::Reverb::Parameters p;
        p.roomSize   = params[0].value;
        p.damping    = params[1].value;
        p.wetLevel   = params[2].value;
        p.dryLevel   = params[3].value;
        p.width      = params[4].value;
        p.freezeMode = 0.0f;
        reverb.setParameters(p);
    }

private:
    juce::Reverb reverb;
};

// ==========================================
// COMPRESSOR EFFECT
// ==========================================
class CompressorEffect : public TrackEffect {
public:
    CompressorEffect() {
        // Param 0: Threshold (dB)
        params.push_back({"Thresh", -10.0f, -60.0f, 0.0f, -10.0f, "dB"});
        // Param 1: Ratio
        params.push_back({"Ratio", 4.0f, 1.0f, 20.0f, 4.0f, ":1"});
        // Param 2: Attack (ms)
        params.push_back({"Att", 10.0f, 0.1f, 100.0f, 10.0f, "ms"});
        // Param 3: Release (ms)
        params.push_back({"Rel", 100.0f, 10.0f, 1000.0f, 100.0f, "ms"});
        // Param 4: Gain (dB)
        params.push_back({"Gain", 0.0f, 0.0f, 24.0f, 0.0f, "dB"});
        
        updateInternalParams();
    }
    
    std::string getName() const override { return "Compressor"; }
    FXType getType() const override { return FX_COMPRESSOR; }
    
    void process(juce::AudioBuffer<float>& buffer) override {
        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();
        
        // Coeff extraction
        float threshdB = params[0].value;
        float ratio = params[1].value;
        float gaindB = params[4].value;
        float makeup = std::pow(10.0f, gaindB / 20.0f);
        
        float attTime = params[2].value * 0.001f;
        float relTime = params[3].value * 0.001f;
        float attCoef = std::exp(-1.0f / (attTime * sampleRate));
        float relCoef = std::exp(-1.0f / (relTime * sampleRate));

        for (int i = 0; i < numSamples; ++i) {
            // Mono detection (max of all channels)
            float inputAbs = 0.0f;
            for (int c = 0; c < numChannels; ++c) {
                float v = std::abs(buffer.getReadPointer(c)[i]);
                if (v > inputAbs) inputAbs = v;
            }
            
            // Envelope
            if (inputAbs > envelope)
                envelope = attCoef * envelope + (1.0f - attCoef) * inputAbs;
            else
                envelope = relCoef * envelope + (1.0f - relCoef) * inputAbs;
            
            // Gain Compute
            float envdB = 20.0f * std::log10(envelope + 1e-6f);
            float gainReductiondB = 0.0f;
            
            if (envdB > threshdB) {
                gainReductiondB = (envdB - threshdB) * (1.0f - 1.0f/ratio);
            }
            
            float gain = std::pow(10.0f, -gainReductiondB / 20.0f);
            
            // Apply
            for (int c = 0; c < numChannels; ++c) {
                buffer.getWritePointer(c)[i] *= (gain * makeup);
            }
        }
    }
    
private:
    float envelope = 0.0f;
};

// Factory
inline std::shared_ptr<TrackEffect> CreateTrackEffect(FXType type) {
    switch (type) {
        case FX_DELAY: return std::make_shared<DelayEffect>();
        case FX_REVERB: return std::make_shared<ReverbEffect>();
        case FX_COMPRESSOR: return std::make_shared<CompressorEffect>();
        default: return nullptr;
    }
}

} // namespace fx
