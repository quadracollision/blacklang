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

// ==========================================
// EQ EFFECT (Multi-band Parametric/Graphic)
// ==========================================
class EQEffect : public TrackEffect {
public:
    struct Band {
        float frequency;
        float q;
        float gaindB;
        
        // Biquad state
        double z1_L = 0, z2_L = 0;
        double z1_R = 0, z2_R = 0;
        double b0, b1, b2, a1, a2;
    };

    EQEffect() {
        // Param 0: Band Count (0=3, 1=6, 2=12) - stored as float index
        params.push_back({"Bands", 0.0f, 0.0f, 2.0f, 0.0f, ""});
        
        // Initialize with default bands (we'll support max 12)
        // We'll create parameters for all max 12 bands, but only show/process active ones
        // Frequencies will be: 
        // 3-Band: 100, 1000, 10000
        // 6-Band: 60, 200, 600, 2k, 6k, 12k
        // 12-Band: 30, 60, 120, 250, 500, 1k, 2k, 4k, 8k, 12k, 16k, 20k
        
        for (int i=0; i<12; ++i) {
            std::string name = "Band " + std::to_string(i+1);
            params.push_back({name, 0.0f, -12.0f, 12.0f, 0.0f, "dB"});
        }
        
        updateInternalParams();
    }
    
    std::string getName() const override { return "EQ"; }
    FXType getType() const override { return FX_EQ; }
    
    // Override to dynamically update frequencies when band count changes
    void setParam(int index, float value) override {
        TrackEffect::setParam(index, value);
        if (index == 0) setupBands();
        updateInternalParams();
    }
    
    void process(juce::AudioBuffer<float>& buffer) override {
        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();
        
        // Simple series processing of biquads
        for (int c = 0; c < numChannels; ++c) {
            float* data = buffer.getWritePointer(c);
            for (int i = 0; i < numSamples; ++i) {
                float sample = data[i];
                
                for (auto& band : bands) {
                    double in = (double)sample;
                    double out;
                    
                    if (c == 0) { // Left / Mono
                         out = in * band.b0 + band.z1_L;
                         band.z1_L = in * band.b1 + band.z2_L - band.a1 * out;
                         band.z2_L = in * band.b2 - band.a2 * out;
                    } else { // Right
                         out = in * band.b0 + band.z1_R;
                         band.z1_R = in * band.b1 + band.z2_R - band.a1 * out;
                         band.z2_R = in * band.b2 - band.a2 * out;
                    }
                    
                    sample = (float)out;
                }
                data[i] = sample;
            }
        }
    }
    
private:
    std::vector<Band> bands;
    
    void setupBands() {
        int mode = (int)params[0].value;
        std::vector<float> freqs;
        
        if (mode == 0) { // 3 Band
            freqs = {100.0f, 1000.0f, 10000.0f};
        } else if (mode == 1) { // 6 Band
            freqs = {60.0f, 200.0f, 600.0f, 2000.0f, 6000.0f, 12000.0f};
        } else { // 12 Band
            freqs = {30.0f, 60.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 12000.0f, 16000.0f, 20000.0f};
        }
        
        // Resize bands vector if needed, preserving state where possible
        if (bands.size() != freqs.size()) {
            std::vector<Band> newBands(freqs.size());
            bands = newBands;
        }
        
        for (size_t i=0; i<bands.size(); ++i) {
            bands[i].frequency = freqs[i];
            bands[i].q = 1.0f; // Default Q
            bands[i].gaindB = params[i+1].value;
        }
    }

    void updateInternalParams() override {
        if (bands.empty()) setupBands();
        
        // Update gains and coeffs
        const double pi = 3.14159265358979323846;
        
        for (size_t i=0; i<bands.size(); ++i) {
             bands[i].gaindB = params[i+1].value;
             
             // Peaking filter coeffs
             double A = std::pow(10.0, bands[i].gaindB / 40.0);
             double w0 = 2.0 * pi * bands[i].frequency / sampleRate;
             double alpha = std::sin(w0) / (2.0 * bands[i].q);
             
             double b0 = 1.0 + alpha * A;
             double b1 = -2.0 * std::cos(w0);
             double b2 = 1.0 - alpha * A;
             double a0 = 1.0 + alpha / A;
             double a1 = -2.0 * std::cos(w0);
             double a2 = 1.0 - alpha / A;
             
             bands[i].b0 = b0 / a0;
             bands[i].b1 = b1 / a0;
             bands[i].b2 = b2 / a0;
             bands[i].a1 = a1 / a0;
             bands[i].a2 = a2 / a0;
        }
    }
};

// ==========================================
// SATURATION EFFECT (Smooth Tanh)
// ==========================================
class SaturationEffect : public TrackEffect {
public:
    SaturationEffect() {
        params.push_back({"Drive", 3.0f, 1.0f, 20.0f, 3.0f, ""});
        params.push_back({"Output", 0.8f, 0.0f, 1.5f, 0.8f, ""});
    }
    
    std::string getName() const override { return "Saturation"; }
    FXType getType() const override { return FX_SATURATION; }
    
    void process(juce::AudioBuffer<float>& buffer) override {
        float drive = params[0].value;
        float outLevel = params[1].value;
        
        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();
        
        for (int c = 0; c < numChannels; ++c) {
            float* data = buffer.getWritePointer(c);
            for (int i = 0; i < numSamples; ++i) {
                // Tanh Soft Clip
                data[i] = std::tanh(data[i] * drive) * outLevel;
            }
        }
    }
};

// ==========================================
// OVERDRIVE EFFECT (Aggressive)
// ==========================================
class OverdriveEffect : public TrackEffect {
public:
    OverdriveEffect() {
        params.push_back({"Drive", 5.0f, 1.0f, 50.0f, 5.0f, ""});
        params.push_back({"Tone", 0.5f, 0.05f, 0.95f, 0.5f, "Hz"}); // Simple LP coeff
        params.push_back({"Level", 0.5f, 0.0f, 1.0f, 0.5f, ""});
    }
    
    std::string getName() const override { return "Overdrive"; }
    FXType getType() const override { return FX_OVERDRIVE; }
    
    void process(juce::AudioBuffer<float>& buffer) override {
        float drive = params[0].value;
        float tone = params[1].value; // Smoothing factor (1 = unfiltered, 0 = heavy)
        float level = params[2].value;
        
        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();
        
        for (int c = 0; c < numChannels; ++c) {
            float* data = buffer.getWritePointer(c);
            float lpState = 0.0f;
            
            for (int i = 0; i < numSamples; ++i) {
                float in = data[i] * drive;
                
                // Hard Clip / Foldback
                float out;
                if (in > 1.0f) out = 0.666f; // 2/3
                else if (in < -1.0f) out = -0.666f;
                else out = in - (in * in * in) / 3.0f;
                
                // Simple LP Tone
                lpState += tone * (out - lpState);
                
                data[i] = lpState * level;
            }
        }
    }
};

// ==========================================
// CHORUS EFFECT (Modulated Delay)
// ==========================================
class ChorusEffect : public TrackEffect {
public:
    ChorusEffect() {
        params.push_back({"Rate", 1.5f, 0.1f, 10.0f, 1.5f, "Hz"});
        params.push_back({"Depth", 2.0f, 0.0f, 10.0f, 2.0f, "ms"});
        params.push_back({"Mix", 0.5f, 0.0f, 1.0f, 0.5f, "%"});
        
        delayBufferSize = 192000;
        delayBuffer.setSize(2, delayBufferSize);
        delayBuffer.clear();
    }
    
    std::string getName() const override { return "Chorus"; }
    FXType getType() const override { return FX_CHORUS; }
    
    void process(juce::AudioBuffer<float>& buffer) override {
        float rate = params[0].value;
        float depthMs = params[1].value;
        float mix = params[2].value;
        
        float depthSamples = depthMs * 0.001f * sampleRate;
        float lfoInc = (2.0f * 3.14159f * rate) / sampleRate;
        
        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();
        
        if (delayBuffer.getNumChannels() < numChannels) {
            delayBuffer.setSize(numChannels, delayBufferSize, true, true, true);
        }
        
        for (int c = 0; c < numChannels; ++c) {
            const float* inputData = buffer.getReadPointer(c);
            float* outputData = buffer.getWritePointer(c);
            float* delayData = delayBuffer.getWritePointer(c);
            
            // Stereo Phase offset
            float currentPhase = phase + (c * 3.14159f / 2.0f);
            
            for (int i = 0; i < numSamples; ++i) {
                // Determine read pos with LFO
                float lfoVal = (std::sin(currentPhase) + 1.0f) * 0.5f; // 0..1
                float currentDelay = 0.005f * sampleRate + (lfoVal * depthSamples); // Base 5ms delay + mod
                
                // Circular Read with Interpolation
                float readIndex = (writePos + i) - currentDelay;
                while (readIndex < 0) readIndex += delayBufferSize;
                while (readIndex >= delayBufferSize) readIndex -= delayBufferSize;
                
                int i1 = (int)readIndex;
                int i2 = (i1 + 1) % delayBufferSize;
                float frac = readIndex - i1;
                
                float delayedSample = delayData[i1] * (1.0f - frac) + delayData[i2] * frac;
                
                // Write Input
                int wPos = (writePos + i) % delayBufferSize;
                delayData[wPos] = inputData[i];
                
                // Output
                outputData[i] = inputData[i] * (1.0f - mix) + delayedSample * mix;
                
                currentPhase += lfoInc;
            }
        }
        
        phase += lfoInc * numSamples;
        while (phase > 2.0f * 3.14159f) phase -= 2.0f * 3.14159f;
        
        writePos = (writePos + numSamples) % delayBufferSize;
    }
    
private:
    juce::AudioBuffer<float> delayBuffer;
    int delayBufferSize;
    int writePos = 0;
    float phase = 0.0f;
};

// ==========================================
// FLANGER EFFECT (Feed-back Modulated Delay)
// ==========================================
class FlangerEffect : public TrackEffect {
public:
    FlangerEffect() {
        params.push_back({"Rate", 0.5f, 0.01f, 5.0f, 0.5f, "Hz"});
        params.push_back({"Depth", 1.0f, 0.0f, 5.0f, 1.0f, "ms"});
        params.push_back({"Feed", 0.6f, 0.0f, 0.95f, 0.6f, ""});
        params.push_back({"Mix", 0.5f, 0.0f, 1.0f, 0.5f, "%"});
        
        delayBufferSize = 19200; // Small buffer fine for Flanger
        delayBuffer.setSize(2, delayBufferSize);
        delayBuffer.clear();
    }
    
    std::string getName() const override { return "Flanger"; }
    FXType getType() const override { return FX_FLANGER; }
    
    void process(juce::AudioBuffer<float>& buffer) override {
        float rate = params[0].value;
        float depthMs = params[1].value;
        float feedback = params[2].value;
        float mix = params[3].value;
        
        float depthSamples = depthMs * 0.001f * sampleRate;
        float lfoInc = (2.0f * 3.14159f * rate) / sampleRate;
        
        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();
        
        if (delayBuffer.getNumChannels() < numChannels) {
            delayBuffer.setSize(numChannels, delayBufferSize, true, true, true);
        }
        
        for (int c = 0; c < numChannels; ++c) {
            const float* inputData = buffer.getReadPointer(c);
            float* outputData = buffer.getWritePointer(c);
            float* delayData = delayBuffer.getWritePointer(c);
            
            float currentPhase = phase; 
            
            for (int i = 0; i < numSamples; ++i) {
                // Short modulation delay (1-5ms)
                float lfoVal = (std::sin(currentPhase) + 1.0f) * 0.5f; 
                float currentDelay = 0.001f * sampleRate + (lfoVal * depthSamples); // Base 1ms
                
                // Read
                float readIndex = (writePos + i) - currentDelay;
                while (readIndex < 0) readIndex += delayBufferSize;
                while (readIndex >= delayBufferSize) readIndex -= delayBufferSize;
                
                int i1 = (int)readIndex;
                int i2 = (i1 + 1) % delayBufferSize;
                float frac = readIndex - i1;
                float delayedSample = delayData[i1] * (1.0f - frac) + delayData[i2] * frac;
                
                // Feedback write
                int wPos = (writePos + i) % delayBufferSize;
                delayData[wPos] = inputData[i] + (delayedSample * feedback);
                
                // Output
                outputData[i] = inputData[i] * (1.0f - mix) + delayedSample * mix;
                
                currentPhase += lfoInc;
            }
        }
        
        phase += lfoInc * numSamples;
        while (phase > 2.0f * 3.14159f) phase -= 2.0f * 3.14159f;
        
        writePos = (writePos + numSamples) % delayBufferSize;
    }

private:
    juce::AudioBuffer<float> delayBuffer;
    int delayBufferSize;
    int writePos = 0;
    float phase = 0.0f;
};
// ==========================================
// BITCRUSHER EFFECT (Lo-Fi)
// ==========================================
class BitCrushEffect : public TrackEffect {
public:
    BitCrushEffect() {
        // Param 0: Bits (24 down to 1)
        params.push_back({"Bits", 8.0f, 1.0f, 24.0f, 8.0f, "bit"});
        // Param 1: Rate Div (1 to 50) - Sample Hold Factor
        params.push_back({"Rate", 4.0f, 1.0f, 50.0f, 4.0f, "x"});
        // Param 2: Mix
        params.push_back({"Mix", 1.0f, 0.0f, 1.0f, 1.0f, "%"});
    }
    
    std::string getName() const override { return "Bitcrush"; }
    FXType getType() const override { return FX_BITCRUSH; }
    
    void process(juce::AudioBuffer<float>& buffer) override {
        float bits = params[0].value;
        float rateDiv = params[1].value;
        float mix = params[2].value;
        
        // Quantization step size
        // If bits = 8, steps = 256. Step size = 1/256.
        float steps = std::pow(2.0f, bits);
        float stepSize = 1.0f / steps;
        
        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();
        
        // Ensure state vector size
        if (channelPhasors.size() < numChannels) {
            channelPhasors.resize(numChannels, 0.0f);
            heldSamples.resize(numChannels, 0.0f);
        }
        
        for (int c = 0; c < numChannels; ++c) {
            float* data = buffer.getWritePointer(c);
            float phasor = channelPhasors[c];
            float currentHold = heldSamples[c];
            
            for (int i = 0; i < numSamples; ++i) {
                float in = data[i];
                
                // Downsampling Logic
                phasor += 1.0f;
                if (phasor >= rateDiv) {
                    phasor -= rateDiv;
                    currentHold = in;
                }
                
                // Bit Reduction (Quantize the held sample)
                // Range -1 to 1.
                // Scale to 0..1? No, audio is bipolar -1..1
                // Quantize: floor(x / step + 0.5) * step
                
                float crushed = std::floor(currentHold / stepSize + 0.5f) * stepSize;
                
                // Mix
                data[i] = in * (1.0f - mix) + crushed * mix;
            }
            channelPhasors[c] = phasor;
            heldSamples[c] = currentHold;
        }
    }

private:
    std::vector<float> channelPhasors;
    std::vector<float> heldSamples;
};

// ==========================================
// FILTER EFFECT (Resonant LP/HP/BP)
// ==========================================
class FilterEffect : public TrackEffect {
public:
    FilterEffect() {
        // Param 0: Cutoff (Hz)
        params.push_back({"Cutoff", 1000.0f, 20.0f, 20000.0f, 1000.0f, "Hz"});
        // Param 1: Resonance (Q)
        params.push_back({"Reso", 1.0f, 0.1f, 10.0f, 1.0f, ""});
        // Param 2: Type (0=LP, 1=HP, 2=BP)
        params.push_back({"Type", 0.0f, 0.0f, 2.0f, 0.0f, ""});
        
        updateInternalParams();
    }
    
    std::string getName() const override { return "Filter"; }
    FXType getType() const override { return FX_FILTER; }
    
    void process(juce::AudioBuffer<float>& buffer) override {
        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();
        
        // Ensure state vectors
        if (z1.size() < numChannels) {
            z1.resize(numChannels, 0.0);
            z2.resize(numChannels, 0.0);
        }
        
        for (int c = 0; c < numChannels; ++c) {
            float* data = buffer.getWritePointer(c);
            
            for (int i = 0; i < numSamples; ++i) {
                double in = (double)data[i];
                
                // Direct Form I Biquad
                // y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
                // (normalized a0)
                
                double out = in * b0 + z1[c];
                z1[c] = in * b1 + z2[c] - a1 * out;
                z2[c] = in * b2 - a2 * out;
                
                data[i] = (float)out;
            }
        }
    }
    
protected:
    void updateInternalParams() override {
        float cutoff = params[0].value;
        float reso = params[1].value; // Q
        int type = (int)params[2].value;
        
        const double pi = 3.14159265358979323846;
        double w0 = 2.0 * pi * cutoff / sampleRate;
        double alpha = std::sin(w0) / (2.0 * reso);
        double cosW0 = std::cos(w0);
        
        double A0, A1, A2, B0, B1, B2;
        
        if (type == 0) { // Low Pass
            B0 = (1.0 - cosW0) / 2.0;
            B1 = 1.0 - cosW0;
            B2 = (1.0 - cosW0) / 2.0;
            A0 = 1.0 + alpha;
            A1 = -2.0 * cosW0;
            A2 = 1.0 - alpha;
        } else if (type == 1) { // High Pass
            B0 = (1.0 + cosW0) / 2.0;
            B1 = -(1.0 + cosW0);
            B2 = (1.0 + cosW0) / 2.0;
            A0 = 1.0 + alpha;
            A1 = -2.0 * cosW0;
            A2 = 1.0 - alpha;
        } else { // Band Pass (Constant Skirt Gain, peak gain = Q)
            B0 = alpha;
            B1 = 0.0;
            B2 = -alpha;
            A0 = 1.0 + alpha;
            A1 = -2.0 * cosW0;
            A2 = 1.0 - alpha;
        }
        
        // Normalize
        b0 = B0 / A0;
        b1 = B1 / A0;
        b2 = B2 / A0;
        a1 = A1 / A0;
        a2 = A2 / A0;
    }

private:
    double b0, b1, b2, a1, a2;
    std::vector<double> z1, z2;
};

// Factory
inline std::shared_ptr<TrackEffect> CreateTrackEffect(FXType type) {
    switch (type) {
        case FX_DELAY: return std::make_shared<DelayEffect>();
        case FX_REVERB: return std::make_shared<ReverbEffect>();
        case FX_COMPRESSOR: return std::make_shared<CompressorEffect>();
        case FX_EQ: return std::make_shared<EQEffect>();
        case FX_SATURATION: return std::make_shared<SaturationEffect>();
        case FX_OVERDRIVE: return std::make_shared<OverdriveEffect>();
        case FX_CHORUS: return std::make_shared<ChorusEffect>();
        case FX_FLANGER: return std::make_shared<FlangerEffect>();
        case FX_BITCRUSH: return std::make_shared<BitCrushEffect>();
        case FX_FILTER: return std::make_shared<FilterEffect>();
        default: return nullptr;
    }
}

} // namespace fx
