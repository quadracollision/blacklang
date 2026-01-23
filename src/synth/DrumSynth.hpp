#pragma once

#include <cmath>
#include <vector>
#include <algorithm>
#include <random>
#include <array>

namespace SoundFriend {

//=============================================================================
// PATCH - All synthesis parameters
//=============================================================================
struct Patch {
    // ===== OSCILLATOR =====
    float freq = 60.0f;           // Base frequency in Hz (20-500)
    int waveform = 0;             // 0=sine, 1=triangle, 2=saw, 3=square, 4=custom
    float sub = 0.0f;             // Sub oscillator mix (0-1)
    int subOctave = 1;            // Sub octave: 1 = -1 octave, 2 = -2 octaves
    
    // Oscillator Enhancements
    float pulseWidth = 0.5f;      // Pulse width for square wave (0.1-0.9)
    float detune = 0.0f;          // Oscillator detune in cents (-100 to 100)
    float oscPhase = 0.0f;        // Starting phase (0-1)
    int unisonVoices = 1;         // Number of unison voices (1-8)
    float unisonDetune = 10.0f;   // Detune spread in cents (0-50)
    float unisonSpread = 0.5f;    // Stereo spread (0-1)
    
    // Polyphony / Chords
    std::vector<int> extraNotes; // Semitone offsets relative to root
    
    // Custom waveform table (64 points, normalized -1 to 1)
    static constexpr int CUSTOM_WAVEFORM_SIZE = 64;
    std::array<float, 64> customWaveform = []() {
        std::array<float, 64> arr;
        for (int i = 0; i < 64; ++i) {
            arr[i] = std::sin(static_cast<float>(i) / 64.0f * 6.283185307179586f);
        }
        return arr;
    }();
    
    // Custom LFO waveform table
    std::array<float, 64> customLfoWaveform = []() {
        std::array<float, 64> arr;
        for (int i = 0; i < 64; ++i) {
            arr[i] = std::sin(static_cast<float>(i) / 64.0f * 6.283185307179586f);
        }
        return arr;
    }();
    
    // ===== FM SYNTHESIS =====
    float fmRatio = 1.0f;         // FM modulator frequency ratio (0.25-8)
    float fmDepth = 0.0f;         // FM modulation depth (0-1)
    float fmEnvDepth = 0.0f;      // FM envelope depth (-1 to 1)
    
    // ===== RING MODULATION =====
    float ringModFreq = 440.0f;   // Ring modulator frequency (20-2000)
    float ringModMix = 0.0f;      // Ring modulation wet mix (0-1)
    
    // ===== PITCH ENVELOPE =====
    float punch = 0.5f;           // Pitch sweep amount (0-1) - legacy
    float pitchDecay = 0.05f;     // Pitch envelope decay time (0.01-0.5) - legacy
    // Extended pitch envelope
    float pitchEnvAttack = 0.01f; // Pitch envelope attack (0-1)
    float pitchEnvDecay = 0.1f;   // Pitch envelope decay (0.01-2)
    float pitchEnvSustain = 0.0f; // Pitch envelope sustain (0-1)
    float pitchEnvRelease = 0.1f; // Pitch envelope release (0-2)
    float pitchEnvDepth = 0.0f;   // Pitch envelope depth in semitones (-24 to 24)

    // ===== AMPLITUDE ENVELOPE =====
    float attack = 0.001f;        // Attack time (0.001-0.1s)
    float decay = 0.3f;           // Decay time (0.01-2.0s)
    float sustain = 0.0f;         // Sustain level (0-1)
    float release = 0.1f;         // Release time (0.01-1.0s)
    
    // ===== NOISE =====
    float noise = 0.0f;           // Noise mix (0-1)
    int noiseType = 0;            // 0=white, 1=pink, 2=filtered, 3=brown, 4=blue, 5=velvet, 6=crackle
    
    // ===== CLICK/TRANSIENT =====
    float click = 0.0f;           // Click amount (0-1)
    float clickFreq = 2000.0f;    // Click frequency (500-5000)
    
    // ===== FILTER =====
    float cutoff = 1.0f;          // Filter cutoff (0-1, 1=open)
    float resonance = 0.0f;       // Filter resonance (0-1)
    float filterEnv = 0.0f;       // Filter envelope amount (-1 to 1)
    float filterDecay = 0.1f;     // Filter envelope decay
    // Filter Enhancements
    int filterType = 0;           // 0=LP, 1=HP, 2=BP, 3=Notch
    float filterDrive = 0.0f;     // Pre-filter saturation (0-1)
    float filterKeyTrack = 0.0f;  // Filter key tracking (0-1)
    
    // ===== SATURATION/TONE =====
    float grit = 0.0f;            // Saturation (0-1)
    float tone = 0.5f;            // Tone balance (0=dark, 1=bright)
    
    // ===== DISTORTION =====
    float drive = 0.0f;           // Additional drive/gain (0-1)
    int distType = 0;             // 0=soft, 1=hard, 2=foldback, 3=bitcrush
    
    // ===== BITCRUSHER =====
    float bitDepth = 16.0f;       // Bit depth (1-16)
    float sampleReduce = 1.0f;    // Sample rate reduction factor (1-64)
    
    // ===== LFO 1 =====
    float lfoRate = 0.0f;         // LFO rate in Hz (0-20)
    float lfoDepth = 0.0f;        // LFO depth (0-1)
    int lfoTarget = 0;            // Legacy: 0=pitch, 1=filter, 2=amplitude
    int lfoWaveform = 0;          // 0=sine, 1=triangle, 2=square, 3=custom
    float lfoPhase = 0.0f;        // LFO starting phase (0-1)
    float lfoDelay = 0.0f;        // LFO fade-in time (0-2)
    
    // Flexible LFO modulation targets (bitfield for custom group routing)
    enum LfoTargetBit {
        LFO_TARGET_FREQ = 1 << 0,
        LFO_TARGET_CUTOFF = 1 << 1,
        LFO_TARGET_RESONANCE = 1 << 2,
        LFO_TARGET_AMP = 1 << 3,
        LFO_TARGET_DRIVE = 1 << 4,
        LFO_TARGET_NOISE = 1 << 5,
        LFO_TARGET_SUB = 1 << 6,
        LFO_TARGET_GRIT = 1 << 7,
        LFO_TARGET_FM = 1 << 8,
        LFO_TARGET_PAN = 1 << 9
    };
    uint32_t lfoTargets = 0;      // Bitfield of params to modulate (0 = use legacy lfoTarget)
    
    // ===== LFO 2 =====
    float lfo2Rate = 0.0f;        // LFO2 rate in Hz (0-20)
    float lfo2Depth = 0.0f;       // LFO2 depth (0-1)
    int lfo2Target = 0;           // 0=pitch, 1=filter, 2=amplitude
    int lfo2Waveform = 0;         // 0=sine, 1=triangle, 2=square
    float lfo2Phase = 0.0f;       // LFO2 starting phase (0-1)
    
    // ===== SAMPLE & HOLD =====
    float sampleHoldRate = 0.0f;  // S&H rate in Hz (0-20)
    float sampleHoldAmount = 0.0f; // S&H modulation amount (0-1)
    
    // ===== DELAY =====
    float delayTime = 0.25f;      // Delay time in seconds (0-1)
    float delayFeedback = 0.3f;   // Delay feedback (0-0.95)
    float delayMix = 0.0f;        // Delay wet mix (0-1)
    float delayFilter = 0.5f;     // Delay filter (0=dark, 1=bright)
    bool delaySync = false;       // Sync to tempo
    bool delayPingPong = false;   // Ping-pong stereo delay
    
    // ===== REVERB =====
    float reverbSize = 0.5f;      // Reverb room size (0-1)
    float reverbDamping = 0.5f;   // High frequency damping (0-1)
    float reverbMix = 0.0f;       // Reverb wet mix (0-1)
    float reverbPreDelay = 0.02f; // Pre-delay time (0-0.1)
    float reverbWidth = 1.0f;     // Stereo width (0-1)
    
    // ===== CHORUS =====
    float chorusRate = 1.0f;      // Chorus rate (0.1-5)
    float chorusDepth = 0.5f;     // Chorus depth (0-1)
    float chorusMix = 0.0f;       // Chorus wet mix (0-1)
    float chorusFeedback = 0.0f;  // Chorus feedback (0-0.5)
    
    // ===== PHASER =====
    float phaserRate = 1.0f;      // Phaser rate (0.1-5)
    float phaserDepth = 0.5f;     // Phaser depth (0-1)
    float phaserFeedback = 0.5f;  // Phaser feedback (0-0.95)
    float phaserMix = 0.0f;       // Phaser wet mix (0-1)
    int phaserStages = 4;         // Number of allpass stages (2-12)
    
    // ===== FLANGER =====
    float flangerRate = 0.5f;     // Flanger rate (0.1-5)
    float flangerDepth = 0.5f;    // Flanger depth (0-1)
    float flangerFeedback = 0.5f; // Flanger feedback (-0.95 to 0.95)
    float flangerMix = 0.0f;      // Flanger wet mix (0-1)
    
    // ===== DYNAMICS/COMPRESSION =====
    float compression = 0.0f;     // Simple compression amount (0-1) - legacy
    // Extended compression
    float compThreshold = 0.5f;   // Compression threshold (0-1)
    float compRatio = 4.0f;       // Compression ratio (1-20)
    float compAttack = 0.01f;     // Compressor attack (0.001-0.1)
    float compRelease = 0.1f;     // Compressor release (0.01-1)
    float compMakeup = 0.0f;      // Makeup gain (0-1)
    
    // ===== TEXTURE =====
    float granular = 0.0f;        // Granular texture amount (0-1)
    float vinyl = 0.0f;           // Vinyl/lo-fi texture (0-1)
    float wobble = 0.0f;          // Pitch wobble/flutter (0-1)
    
    // ===== STEREO =====
    float pan = 0.5f;             // Pan position (0=left, 0.5=center, 1=right)
    float stereoWidth = 1.0f;     // Stereo width (0-2, 1=normal)
    
    // ===== GLOBAL =====
    float duration = 0.0f;        // Sound length in seconds (0=auto, 0.1-60 for custom)
    float fadeIn = 0.0f;          // Fade in time in seconds (0-2.0)
    float fadeOut = 0.0f;         // Fade out time in seconds (0-2.0)
    bool zeroCrossing = false;    // Trim audio to start/end at zero crossings
    float masterGain = 1.0f;      // Master output gain (0-2)
    
    // ===== 12-BAND GRAPHIC EQ =====
    static constexpr int EQ_BANDS = 12;
    std::array<float, 12> eqGains = {0,0,0,0,0,0,0,0,0,0,0,0};
    
    // ===== PARALLEL PROCESSING GROUPS =====
    struct EffectGroup {
        std::string name;
        std::vector<std::string> effects;
    };
    std::vector<EffectGroup> parallelGroups;
    
    // ===== GROUP NOTE TRANSPOSITION =====
    float noteOffset = 0.0f;      // Note offset in semitones (-24 to +24) for group voices
    
    // ===== GROUP SAMPLE OFFSET =====
    float sampleOffset = 0.0f;    // Sample start offset in seconds (0-10) for delayed group playback
};

//=============================================================================
// OSCILLATOR - Wavetable-based with 60+ waveform presets
//=============================================================================
class Oscillator {
public:
    static constexpr float TWO_PI = 6.283185307179586f;
    static constexpr int WAVETABLE_SIZE = 256;
    
    void reset() { phase = 0.0f; }
    
    // Set pulse width (for legacy compatibility)
    void setPulseWidth(float pw) { pulseWidth = std::clamp(pw, 0.1f, 0.9f); }
    
    // Set custom waveform data (for custom waveform ID 59)
    void setCustomWaveform(const float* data, int size) {
        customWaveformData = data;
        customWaveformSize = size;
    }
    
    // Load a specific wavetable by ID
    void loadWaveform(int id) {
        if (id == currentWaveformId && wavetableLoaded) return;
        
        currentWaveformId = id;
        
        // Special case: custom waveform (ID 59)
        if (id == 59) {
            useCustomWaveform = true;
            return;
        }
        
        useCustomWaveform = false;
        
        // Generate wavetable using library
        for (int i = 0; i < WAVETABLE_SIZE; ++i) {
            float ph = static_cast<float>(i) / WAVETABLE_SIZE;  // 0 to 1
            wavetable[i] = generateSample(id, ph);
        }
        
        // Normalize
        float maxVal = 0.0f;
        for (auto s : wavetable) maxVal = std::max(maxVal, std::abs(s));
        if (maxVal > 0.0f) {
            for (auto& s : wavetable) s /= maxVal;
        }
        
        wavetableLoaded = true;
    }
    
    float process(float frequency, int waveform, float sampleRate, float pw = 0.5f) {
        // Load wavetable if waveform changed
        if (waveform != currentWaveformId) {
            loadWaveform(waveform);
        }
        
        this->pulseWidth = std::clamp(pw, 0.01f, 0.99f);
        
        // Advance phase
        float phaseIncrement = frequency / sampleRate;
        phase += phaseIncrement;
        if (phase >= 1.0f) phase -= 1.0f;
        
        // Read from wavetable with linear interpolation
        if (useCustomWaveform && customWaveformData != nullptr) {
            return readCustomWavetable();
        }
        
        return readWavetable();
    }
    
private:
    float phase = 0.0f;
    float pulseWidth = 0.5f;
    const float* customWaveformData = nullptr;
    int customWaveformSize = 64;
    
    std::array<float, WAVETABLE_SIZE> wavetable{};
    int currentWaveformId = -1;
    bool wavetableLoaded = false;
    bool useCustomWaveform = false;
    
    // Helper to warp phase for PWM effects
    float getWarpedPhase() const {
        if (std::abs(pulseWidth - 0.5f) < 0.01f) return phase;
        
        if (phase < pulseWidth) {
            return phase * (0.5f / pulseWidth);
        } else {
            return 0.5f + (phase - pulseWidth) * (0.5f / (1.0f - pulseWidth));
        }
    }
    
    // Read from loaded wavetable with linear interpolation
    float readWavetable() const {
        float warpedPhase = getWarpedPhase();
        float pos = warpedPhase * WAVETABLE_SIZE;
        int idx0 = static_cast<int>(pos) % WAVETABLE_SIZE;
        int idx1 = (idx0 + 1) % WAVETABLE_SIZE;
        float frac = pos - static_cast<int>(pos);
        
        return wavetable[idx0] * (1.0f - frac) + wavetable[idx1] * frac;
    }
    
    // Read from custom wavetable
    float readCustomWavetable() const {
        if (customWaveformData == nullptr) return 0.0f;
        
        float warpedPhase = getWarpedPhase();
        float pos = warpedPhase * customWaveformSize;
        int idx0 = static_cast<int>(pos) % customWaveformSize;
        int idx1 = (idx0 + 1) % customWaveformSize;
        float frac = pos - static_cast<int>(pos);
        
        return customWaveformData[idx0] * (1.0f - frac) + customWaveformData[idx1] * frac;
    }
    
    // Generate a single sample for wavetable (same logic as WaveformLibrary)
    static float generateSample(int id, float phase) {
        float p = phase * TWO_PI;
        
        switch (id) {
            // === BASIC ===
            case 0: return std::sin(p); // Sine
            case 1: return 2.0f * std::abs(2.0f * phase - 1.0f) - 1.0f; // Triangle
            case 2: return 2.0f * phase - 1.0f; // Saw
            case 3: return phase < 0.5f ? 1.0f : -1.0f; // Square
            case 4: return phase < 0.25f ? 1.0f : -1.0f; // Pulse (narrow)
            case 5: return 1.0f - 2.0f * phase; // Ramp
            
            // === ANALOG ===
            case 6: { // Supersaw
                float sum = 0.0f;
                for (int v = -3; v <= 3; ++v) {
                    float ph = phase + v * 0.01f;
                    ph = ph - std::floor(ph);
                    sum += 2.0f * ph - 1.0f;
                }
                return sum / 7.0f;
            }
            case 7: { // Fat Saw
                float p1 = phase, p2 = phase + 0.007f, p3 = phase - 0.007f;
                p2 = p2 - std::floor(p2); p3 = p3 - std::floor(p3);
                return ((2*p1-1) + (2*p2-1) + (2*p3-1)) / 3.0f;
            }
            case 8: { // Detuned Triangle
                float t1 = 2.0f * std::abs(2.0f * phase - 1.0f) - 1.0f;
                float ph2 = phase + 0.01f; ph2 -= std::floor(ph2);
                float t2 = 2.0f * std::abs(2.0f * ph2 - 1.0f) - 1.0f;
                return (t1 + t2) * 0.5f;
            }
            case 9: return std::sin(p) * 0.9f + std::sin(2*p) * 0.1f; // Warm Sine
            case 10: return std::sin(p * 2.5f) * (1.0f - phase * 0.5f); // Hard Sync
            case 11: return std::tanh(std::sin(p) * 2.0f) * 0.7f; // Soft Clip
            case 12: return phase < (0.3f + 0.2f * std::sin(p * 0.5f)) ? 1.0f : -1.0f; // PWM Thick
            case 13: return std::tanh((phase < 0.5f ? 1.0f : -1.0f) * 1.5f); // Vintage Sq
            
            // === DIGITAL ===
            case 14: return std::sin(static_cast<int>(phase * 16) * 123.456f); // Noise S&H
            case 15: return std::round(std::sin(p) * 4.0f) / 4.0f; // Stepped
            case 16: return std::round(std::sin(p) * 8.0f) / 8.0f; // Quant Sine
            case 17: return std::round((2.0f * phase - 1.0f) * 4.0f) / 4.0f; // Bitcrush
            case 18: { int x = static_cast<int>(phase * 256); return ((x ^ (x >> 3)) & 0xFF) / 127.5f - 1.0f; } // Glitch
            case 19: return std::sin(p) + std::sin(p * 3.01f) * 0.5f + std::sin(p * 5.02f) * 0.25f; // Digital
            
            // === FM/METALLIC ===
            case 20: return std::sin(p + std::sin(p * 3.0f) * 2.0f); // FM Bell
            case 21: return std::sin(p + std::sin(p * 2.0f) * (1.0f - phase) * 3.0f); // FM Pluck
            case 22: return std::sin(p + std::sin(p) * 1.5f); // FM Brass
            case 23: return std::sin(p + std::sin(p * 2.0f) * 0.8f); // FM Keys
            case 24: return std::sin(p) * std::sin(p * 7.0f); // Metallic
            case 25: return std::sin(p * 2.0f + std::sin(p * 5.0f) * 0.5f); // Glass
            case 26: return std::sin(p * 1.5f + std::sin(p * 4.0f) * 1.2f); // Chime
            case 27: return std::sin(p) * 0.5f + std::sin(p * 2.4f) * 0.3f + std::sin(p * 5.9f) * 0.2f; // Gong
            
            // === HARMONIC ===
            case 28: return std::sin(p) + std::sin(3*p)/3 + std::sin(5*p)/5 + std::sin(7*p)/7; // Odd
            case 29: return std::sin(2*p)/2 + std::sin(4*p)/4 + std::sin(6*p)/6; // Even
            case 30: return std::sin(p) + std::sin(2*p)*0.5f + std::sin(4*p)*0.25f; // Octaves
            case 31: return std::sin(p) + std::sin(p*1.5f)*0.7f + std::sin(2*p)*0.5f; // Fifths
            case 32: return std::sin(p) + std::sin(2*p)*0.9f + std::sin(3*p)*0.8f + std::sin(4*p)*0.7f; // Buzzy
            case 33: return std::sin(3*p)*0.7f + std::sin(5*p)*0.5f + std::sin(7*p)*0.3f; // Hollow
            case 34: return std::sin(p)*0.3f + std::sin(2*p)*0.8f + std::sin(3*p); // Nasal
            case 35: return std::sin(p) + std::sin(2*p)*0.8f + std::sin(3*p)*0.4f; // Vocal A
            case 36: return std::sin(p) + std::sin(2*p)*0.5f + std::sin(4*p)*0.6f; // Vocal E
            case 37: return std::sin(p) + std::sin(2*p)*0.9f + std::sin(3*p)*0.3f; // Vocal O
            
            // === PERCUSSIVE ===
            case 38: return std::sin(p) * 0.9f + std::sin(2*p) * 0.3f; // Kick Body
            case 39: return std::sin(p) + std::sin(2*p)*0.5f + std::sin(5*p)*0.3f; // Snare Tone
            case 40: return std::sin(p) + std::sin(2*p)*0.4f + std::sin(3*p)*0.1f; // Tom Tone
            case 41: return phase < 0.1f ? 1.0f - phase * 10.0f : 0.0f; // Click
            case 42: return std::sin(p) + std::sin(0.5f*p)*0.5f; // Thump
            case 43: return phase < 0.2f ? std::sin(p * 5.0f) * (1.0f - phase * 5.0f) : 0.0f; // Pop
            case 44: return std::sin(p) * 0.5f + std::sin(3.7f*p) * 0.3f + std::sin(7.3f*p) * 0.2f; // Knock
            case 45: return (phase < 0.05f ? 1.0f : 0.0f) + std::sin(p * 4.0f) * 0.3f; // Rim Shot
            
            // === ORGAN/KEYS ===
            case 46: return std::sin(p) + std::sin(2*p) + std::sin(3*p); // Organ 1
            case 47: return std::sin(p) + std::sin(2*p)*0.8f + std::sin(3*p)*0.6f + std::sin(4*p)*0.4f; // Organ 2
            case 48: return std::sin(p + std::sin(p * 2.0f) * 1.5f) * 0.7f + std::sin(p * 3.0f) * 0.3f; // E.Piano
            case 49: return (phase < 0.4f ? 1.0f : -1.0f) * 0.7f + std::sin(3*p) * 0.3f; // Clav
            case 50: return std::sin(p) + std::tanh(std::sin(p) * 2.0f) * 0.5f; // Wurli
            case 51: return std::sin(p) + std::sin(2*p)*0.5f + std::sin(3*p)*0.33f; // Reed
            
            // === BASS ===
            case 52: return std::sin(p); // Sub Bass
            case 53: return std::sin(p + std::sin(p * 0.5f) * 0.3f) + std::sin(2*p) * 0.5f; // Growl
            case 54: { float p2 = phase + 0.02f; p2 -= std::floor(p2); return (2*phase-1)*0.5f + (2*p2-1)*0.5f; } // Reese
            case 55: return std::tanh((phase < 0.5f ? 1.0f : -1.0f) * 2.0f + std::sin(p * 3.0f) * 0.5f); // Acid Sq
            case 56: return std::sin(p) * 0.95f + std::sin(2*p) * 0.05f; // 808 Style
            case 57: return std::sin(p + std::sin(p * 2.0f) * (1.0f - phase)); // Pluck Bass
            case 58: return std::sin(p + std::sin(p) * 1.0f); // FM Bass
            
            default: return std::sin(p);
        }
    }
};

//=============================================================================
// NOISE GENERATOR - White, Pink, Brown, Blue, Filtered, Velvet, Crackle
//=============================================================================
class NoiseGenerator {
public:
    NoiseGenerator() : gen(std::random_device{}()), dist(-1.0f, 1.0f), intDist(0, 100) {
        pinkState.fill(0.0f);
    }
    
    float process(int type) {
        switch (type) {
            case 0: return white();
            case 1: return pink();
            case 2: return filtered();
            case 3: return brown();
            case 4: return blue();
            case 5: return velvet();
            case 6: return crackle();
            default: return white();
        }
    }
    
private:
    std::mt19937 gen;
    std::uniform_real_distribution<float> dist;
    std::uniform_int_distribution<int> intDist;
    std::array<float, 7> pinkState;
    float filteredState = 0.0f;
    float brownState = 0.0f;
    float blueLastSample = 0.0f;
    
    float white() { return dist(gen); }
    
    float pink() {
        // Paul Kellet's improved pink noise algorithm
        float w = white();
        pinkState[0] = 0.99886f * pinkState[0] + w * 0.0555179f;
        pinkState[1] = 0.99332f * pinkState[1] + w * 0.0750759f;
        pinkState[2] = 0.96900f * pinkState[2] + w * 0.1538520f;
        pinkState[3] = 0.86650f * pinkState[3] + w * 0.3104856f;
        pinkState[4] = 0.55000f * pinkState[4] + w * 0.5329522f;
        pinkState[5] = -0.7616f * pinkState[5] - w * 0.0168980f;
        float output = pinkState[0] + pinkState[1] + pinkState[2] + 
                       pinkState[3] + pinkState[4] + pinkState[5] + 
                       pinkState[6] + w * 0.5362f;
        pinkState[6] = w * 0.115926f;
        return output * 0.11f;
    }
    
    float filtered() {
        // Simple low-passed noise
        float w = white();
        filteredState = filteredState * 0.9f + w * 0.1f;
        return filteredState * 3.0f;
    }
    
    float brown() {
        // Brownian/Red noise - deeper, bass-heavy noise
        // Integrate white noise with leaky integrator
        float w = white();
        brownState = brownState * 0.997f + w * 0.02f;
        brownState = std::clamp(brownState, -1.0f, 1.0f);
        return brownState * 3.5f;
    }
    
    float blue() {
        // Blue noise - high-frequency emphasis (derivative of white noise)
        float w = white();
        float output = w - blueLastSample;
        blueLastSample = w;
        return output * 0.5f;
    }
    
    float velvet() {
        // Velvet noise - sparse, smooth noise with random impulses
        // Produces occasional impulses instead of continuous noise
        int r = intDist(gen);
        if (r < 3) {
            return dist(gen);  // 3% chance of impulse
        }
        return 0.0f;
    }
    
    float crackle() {
        // Crackle noise - random pops and clicks for lo-fi/vinyl effect
        int r = intDist(gen);
        if (r < 2) {
            // Louder pop
            return dist(gen) * 0.8f;
        } else if (r < 8) {
            // Subtle crackle
            return dist(gen) * 0.2f;
        }
        return 0.0f;
    }
};

//=============================================================================
// ENVELOPE - ADSR with exponential curves
//=============================================================================
class Envelope {
public:
    enum class Stage { Attack, Decay, Sustain, Release, Off };
    
    void trigger() {
        stage = Stage::Attack;
        value = 0.0f;
    }
    
    void release() {
        if (stage != Stage::Off) {
            stage = Stage::Release;
        }
    }
    
    float process(float attack, float decay, float sustain, float release, float sampleRate) {
        float attackRate = 1.0f / (attack * sampleRate + 1.0f);
        float decayRate = 1.0f / (decay * sampleRate + 1.0f);
        float releaseRate = 1.0f / (release * sampleRate + 1.0f);
        
        switch (stage) {
            case Stage::Attack:
                value += attackRate;
                if (value >= 1.0f) {
                    value = 1.0f;
                    stage = Stage::Decay;
                }
                break;
            case Stage::Decay:
                value -= (value - sustain) * decayRate;
                if (value <= sustain + 0.001f) {
                    value = sustain;
                    stage = Stage::Sustain;
                }
                break;
            case Stage::Sustain:
                value = sustain;
                break;
            case Stage::Release:
                value -= value * releaseRate;
                if (value < 0.001f) {
                    value = 0.0f;
                    stage = Stage::Off;
                }
                break;
            case Stage::Off:
                value = 0.0f;
                break;
        }
        return value;
    }
    
    bool isActive() const { return stage != Stage::Off; }
    float getValue() const { return value; }
    
private:
    Stage stage = Stage::Off;
    float value = 0.0f;
};

//=============================================================================
// FILTER - State Variable Filter
//=============================================================================
class Filter {
public:
    // Filter types
    // 0=LP(12dB), 1=HP(12dB), 2=BP, 3=Notch, 4=LP(24dB), 5=HP(24dB), 6=Peak, 7=Shelf
    // 8=CombPos, 9=CombNeg, 10=Ladder, 11=Formant
    
    void reset() {
        low = high = band = notch = 0.0f;
        ladder[0] = ladder[1] = ladder[2] = ladder[3] = 0.0f;
        combBuffer.fill(0.0f);
        combIndex = 0;
        formantState[0] = formantState[1] = formantState[2] = 0.0f;
    }
    
    float process(float input, float cutoff, float resonance, float sampleRate, int filterType = 0) {
        // Cutoff from 0-1 to frequency (limit max freq to prevent instability)
        float freq = 20.0f + cutoff * cutoff * 8000.0f;  // Max ~8kHz to stay stable
        freq = std::min(freq, sampleRate * 0.45f);       // Nyquist safety
        
        // Coefficient calculation with stability clamp
        float f = 2.0f * std::sin(3.14159f * freq / sampleRate);
        f = std::min(f, 0.9f);  // Prevent instability
        
        float q = 1.0f - resonance * 0.9f;
        
        // State variable filter base calculations
        low += f * band;
        high = input - low - q * band;
        band += f * high;
        notch = high + low;
        
        // Prevent NaN/infinity
        if (!std::isfinite(low)) low = 0.0f;
        if (!std::isfinite(band)) band = 0.0f;
        if (!std::isfinite(high)) high = 0.0f;
        
        switch (filterType) {
            case 0:  // Low Pass 12dB
                return low;
                
            case 1:  // High Pass 12dB
                return high;
                
            case 2:  // Band Pass
                return band;
                
            case 3:  // Notch (Band Reject)
                return notch;
                
            case 4:  // Low Pass 24dB (2-pole cascade)
                return processLadderLP(input, cutoff, resonance, sampleRate);
                
            case 5:  // High Pass 24dB
                return processLadderHP(input, cutoff, resonance, sampleRate);
                
            case 6:  // Peak (Resonant Peak)
                return low * 0.5f + band * resonance * 2.0f;
                
            case 7:  // Low Shelf
                return input * 0.5f + low * 0.5f;
                
            case 8:  // Comb Filter (Positive feedback)
                return processComb(input, cutoff, resonance, sampleRate, true);
                
            case 9:  // Comb Filter (Negative feedback)
                return processComb(input, cutoff, resonance, sampleRate, false);
                
            case 10: // Ladder (Moog-style)
                return processMoogLadder(input, cutoff, resonance, sampleRate);
                
            case 11: // Formant Filter
                return processFormant(input, cutoff, resonance, sampleRate);
                
            default:
                return low;
        }
    }
    
private:
    float low = 0.0f, high = 0.0f, band = 0.0f, notch = 0.0f;
    
    // Ladder filter state
    std::array<float, 4> ladder{};
    
    // Comb filter state
    static constexpr int COMB_SIZE = 2048;
    std::array<float, COMB_SIZE> combBuffer{};
    int combIndex = 0;
    
    // Formant filter state
    std::array<float, 3> formantState{};
    
    // 24dB Low Pass (2-pole cascade)
    float processLadderLP(float input, float cutoff, float resonance, float sampleRate) {
        float freq = 20.0f + cutoff * cutoff * 10000.0f;
        float fc = freq / sampleRate;
        fc = std::min(fc, 0.45f);
        
        float k = resonance * 4.0f;
        float p = fc * 1.386249f;
        float t1 = (1.0f - p) * 1.4f;
        float t2 = 1.0f / (1.0f + k * (1.0f - t1 * t1));
        
        input -= k * ladder[3] * t2;
        
        ladder[0] += p * (std::tanh(input) - std::tanh(ladder[0]));
        ladder[1] += p * (std::tanh(ladder[0]) - std::tanh(ladder[1]));
        ladder[2] += p * (std::tanh(ladder[1]) - std::tanh(ladder[2]));
        ladder[3] += p * (std::tanh(ladder[2]) - std::tanh(ladder[3]));
        
        if (!std::isfinite(ladder[3])) ladder[3] = 0.0f;
        return ladder[3];
    }
    
    // 24dB High Pass
    float processLadderHP(float input, float cutoff, float resonance, float sampleRate) {
        float lpOut = processLadderLP(input, cutoff, resonance, sampleRate);
        return input - lpOut;
    }
    
    // Comb Filter
    float processComb(float input, float cutoff, float resonance, float sampleRate, bool positive) {
        // Delay time from cutoff (0-1 maps to 1ms-50ms)
        float delayMs = 1.0f + (1.0f - cutoff) * 49.0f;
        int delaySamples = static_cast<int>(delayMs * sampleRate / 1000.0f);
        delaySamples = std::min(delaySamples, COMB_SIZE - 1);
        
        int readIndex = (combIndex - delaySamples + COMB_SIZE) % COMB_SIZE;
        float delayed = combBuffer[readIndex];
        
        float feedback = resonance * 0.95f;
        float output = input + (positive ? feedback : -feedback) * delayed;
        
        combBuffer[combIndex] = output;
        combIndex = (combIndex + 1) % COMB_SIZE;
        
        if (!std::isfinite(output)) output = 0.0f;
        return output * 0.7f;  // Reduce gain to prevent clipping
    }
    
    // Moog Ladder Filter
    float processMoogLadder(float input, float cutoff, float resonance, float sampleRate) {
        float fc = (20.0f + cutoff * cutoff * 12000.0f) / sampleRate;
        fc = std::min(fc, 0.45f);
        
        float g = 1.0f - std::exp(-2.0f * 3.14159f * fc);
        float k = resonance * 4.0f;
        
        // Feedback
        float feedback = k * (ladder[3] - input * 0.5f);
        input -= feedback;
        
        // 4-pole cascade
        ladder[0] += g * (std::tanh(input) - std::tanh(ladder[0]));
        ladder[1] += g * (std::tanh(ladder[0]) - std::tanh(ladder[1]));
        ladder[2] += g * (std::tanh(ladder[1]) - std::tanh(ladder[2]));
        ladder[3] += g * (std::tanh(ladder[2]) - std::tanh(ladder[3]));
        
        if (!std::isfinite(ladder[3])) ladder[3] = 0.0f;
        return ladder[3];
    }
    
    // Simple Formant Filter (vowel-like)
    float processFormant(float input, float cutoff, float resonance, float sampleRate) {
        // 3 parallel bandpass filters at formant frequencies
        // Cutoff controls "vowel position"
        float vowelPos = cutoff * 4.0f;  // 0-4 range for vowels
        
        // Formant frequencies for different vowels (Hz)
        float f1, f2;
        if (vowelPos < 1.0f) {           // 'i' to 'e'
            f1 = 270.0f + vowelPos * 130.0f;
            f2 = 2300.0f - vowelPos * 200.0f;
        } else if (vowelPos < 2.0f) {    // 'e' to 'a'
            f1 = 400.0f + (vowelPos - 1.0f) * 330.0f;
            f2 = 2100.0f - (vowelPos - 1.0f) * 100.0f;
        } else if (vowelPos < 3.0f) {    // 'a' to 'o'
            f1 = 730.0f - (vowelPos - 2.0f) * 230.0f;
            f2 = 2000.0f - (vowelPos - 2.0f) * 150.0f;
        } else {                          // 'o' to 'u'
            f1 = 500.0f - (vowelPos - 3.0f) * 200.0f;
            f2 = 1850.0f - (vowelPos - 3.0f) * 700.0f;
        }
        
        // Simple 2-pole bandpass for each formant
        float w1 = 2.0f * 3.14159f * f1 / sampleRate;
        float w2 = 2.0f * 3.14159f * f2 / sampleRate;
        float bw = 0.1f + resonance * 0.2f;  // Bandwidth
        
        // First formant
        float c1 = std::exp(-bw * w1);
        formantState[0] = formantState[0] * c1 + input * (1.0f - c1) * std::sin(w1);
        
        // Second formant
        float c2 = std::exp(-bw * w2);
        formantState[1] = formantState[1] * c2 + input * (1.0f - c2) * std::sin(w2);
        
        float output = (formantState[0] + formantState[1] * 0.5f) * 2.0f;
        
        if (!std::isfinite(output)) output = 0.0f;
        return output;
    }
};

//=============================================================================
// BIQUAD EQ - 12-band parametric equalizer
//=============================================================================
class BiquadEQ {
public:
    static constexpr int NUM_BANDS = 12;
    static constexpr float PI = 3.14159265358979f;
    
    // ISO standard center frequencies for 12-band EQ
    static constexpr std::array<float, NUM_BANDS> FREQUENCIES = {
        31.25f, 62.5f, 125.0f, 250.0f, 500.0f, 1000.0f,
        2000.0f, 4000.0f, 8000.0f, 12000.0f, 16000.0f, 20000.0f
    };
    
    void reset() {
        for (int i = 0; i < NUM_BANDS; ++i) {
            x1[i] = x2[i] = y1[i] = y2[i] = 0.0f;
        }
    }
    
    // Calculate coefficients for all bands based on gains
    void setGains(const std::array<float, 12>& gainsDB, float sampleRate) {
        for (int i = 0; i < NUM_BANDS; ++i) {
            calculateCoefficients(i, FREQUENCIES[i], gainsDB[i], sampleRate);
        }
    }
    
    // Process a single sample through all 12 bands
    float process(float input) {
        float output = input;
        
        for (int i = 0; i < NUM_BANDS; ++i) {
            // Biquad difference equation
            float y = b0[i] * output + b1[i] * x1[i] + b2[i] * x2[i]
                    - a1[i] * y1[i] - a2[i] * y2[i];
            
            // Update state
            x2[i] = x1[i];
            x1[i] = output;
            y2[i] = y1[i];
            y1[i] = y;
            
            // Prevent NaN/infinity
            if (!std::isfinite(y)) y = 0.0f;
            
            output = y;
        }
        
        return output;
    }
    
    // Check if EQ is active (any non-zero gain)
    static bool isActive(const std::array<float, 12>& gains) {
        for (float g : gains) {
            if (std::abs(g) > 0.1f) return true;
        }
        return false;
    }
    
private:
    // Biquad coefficients for each band
    std::array<float, NUM_BANDS> b0{}, b1{}, b2{}, a1{}, a2{};
    
    // Filter state for each band
    std::array<float, NUM_BANDS> x1{}, x2{}, y1{}, y2{};
    
    // Calculate peak/notch EQ coefficients for one band
    void calculateCoefficients(int band, float freq, float gainDB, float sampleRate) {
        // Bandwidth in octaves (wider at extremes, tighter in mids)
        float bandwidth = (band < 2 || band > 9) ? 1.5f : 1.0f;
        
        // Convert gain to linear
        float A = std::pow(10.0f, gainDB / 40.0f);
        
        // Angular frequency
        float w0 = 2.0f * PI * freq / sampleRate;
        float cosw0 = std::cos(w0);
        float sinw0 = std::sin(w0);
        
        // Bandwidth coefficient
        float alpha = sinw0 * std::sinh(std::log(2.0f) / 2.0f * bandwidth * w0 / sinw0);
        
        // Prevent division by zero for very high frequencies
        if (!std::isfinite(alpha) || alpha < 0.0001f) {
            // Bypass this band
            b0[band] = 1.0f;
            b1[band] = 0.0f;
            b2[band] = 0.0f;
            a1[band] = 0.0f;
            a2[band] = 0.0f;
            return;
        }
        
        // Peak EQ coefficients
        float a0_inv = 1.0f / (1.0f + alpha / A);
        
        b0[band] = (1.0f + alpha * A) * a0_inv;
        b1[band] = (-2.0f * cosw0) * a0_inv;
        b2[band] = (1.0f - alpha * A) * a0_inv;
        a1[band] = b1[band];  // a1 = b1 for peak EQ
        a2[band] = (1.0f - alpha / A) * a0_inv;
    }
};

//=============================================================================
// LFO - Low Frequency Oscillator
//=============================================================================
class LFO {
public:
    static constexpr float TWO_PI = 6.283185307179586f;
    
    void reset() { phase = 0.0f; }
    
    void setCustomWaveform(const float* data, int size) {
        customWaveformData = data;
        customWaveformSize = size;
    }

    float process(float rate, int waveform, float sampleRate) {
        if (rate <= 0.0f) return 0.0f;
        
        phase += rate / sampleRate;
        if (phase >= 1.0f) phase -= 1.0f;
        
        switch (waveform) {
            case 0: return std::sin(phase * TWO_PI);
            case 1: { // Triangle
                float t = phase * 4.0f;
                if (t < 1.0f) return t;
                if (t < 3.0f) return 2.0f - t;
                return t - 4.0f;
            }
            case 2: return phase < 0.5f ? 1.0f : -1.0f; // Square
            case 3: return custom();
            default: return std::sin(phase * TWO_PI);
        }
    }
    
private:
    float phase = 0.0f;
    const float* customWaveformData = nullptr;
    int customWaveformSize = 64;
    
    float custom() const {
        if (customWaveformData == nullptr) return std::sin(phase * TWO_PI);
        
        float pos = phase * customWaveformSize;
        int idx0 = static_cast<int>(pos) % customWaveformSize;
        int idx1 = (idx0 + 1) % customWaveformSize;
        float frac = pos - static_cast<int>(pos);
        
        return customWaveformData[idx0] * (1.0f - frac) + customWaveformData[idx1] * frac;
    }
};

//=============================================================================
// SIMPLE DELAY - Mono delay with feedback and filtering
//=============================================================================
class SimpleDelay {
public:
    static constexpr int MAX_DELAY_SAMPLES = 88200; // 2 seconds at 44.1kHz
    
    SimpleDelay() {
        buffer.resize(MAX_DELAY_SAMPLES, 0.0f);
    }
    
    void reset() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        filterState = 0.0f;
    }
    
    float process(float input, float delayTime, float feedback, float filterAmount, float sampleRate) {
        int delaySamples = static_cast<int>(delayTime * sampleRate);
        delaySamples = std::clamp(delaySamples, 1, MAX_DELAY_SAMPLES - 1);
        
        int readPos = writePos - delaySamples;
        if (readPos < 0) readPos += MAX_DELAY_SAMPLES;
        
        float delayed = buffer[readPos];
        
        // Simple LP/HP filter on delay line
        float filterCoeff = 0.1f + filterAmount * 0.8f;
        filterState = filterState * (1.0f - filterCoeff) + delayed * filterCoeff;
        float filtered = filterAmount > 0.5f ? delayed : filterState * 2.0f;
        
        // Write to buffer with feedback
        buffer[writePos] = input + filtered * feedback * 0.95f;
        
        writePos = (writePos + 1) % MAX_DELAY_SAMPLES;
        
        return filtered;
    }
    
private:
    std::vector<float> buffer;
    int writePos = 0;
    float filterState = 0.0f;
};

//=============================================================================
// SIMPLE REVERB - Schroeder-style reverb
//=============================================================================
class SimpleReverb {
public:
    SimpleReverb() {
        // Initialize comb filters with prime-ish delay times
        for (int i = 0; i < 4; ++i) {
            int delayLen = combDelays[i];
            combBuffers[i].resize(delayLen, 0.0f);
        }
        // Initialize allpass filters
        for (int i = 0; i < 2; ++i) {
            allpassBuffers[i].resize(allpassDelays[i], 0.0f);
        }
        // Initialize predelay buffer (max 200ms at 44.1kHz)
        predelayBuffer.resize(PREDELAY_MAX, 0.0f);
    }
    
    void reset() {
        for (auto& buf : combBuffers) std::fill(buf.begin(), buf.end(), 0.0f);
        for (auto& buf : allpassBuffers) std::fill(buf.begin(), buf.end(), 0.0f);
        for (auto& pos : combPos) pos = 0;
        for (auto& pos : allpassPos) pos = 0;
        std::fill(predelayBuffer.begin(), predelayBuffer.end(), 0.0f);
        predelayWritePos = 0;
    }
    
    float process(float input, float roomSize, float damping, float predelayTime = 0.0f) {
        // Apply predelay (0-0.1 maps to 0-100ms)
        float delayedInput = input;
        if (predelayTime > 0.001f) {
            int predelaySamples = static_cast<int>(predelayTime * 44100.0f);
            predelaySamples = std::min(predelaySamples, PREDELAY_MAX - 1);
            
            int readPos = (predelayWritePos - predelaySamples + PREDELAY_MAX) % PREDELAY_MAX;
            delayedInput = predelayBuffer[readPos];
            predelayBuffer[predelayWritePos] = input;
            predelayWritePos = (predelayWritePos + 1) % PREDELAY_MAX;
        }
        
        float combOut = 0.0f;
        
        // Process 4 parallel comb filters
        for (int i = 0; i < 4; ++i) {
            float delayed = combBuffers[i][combPos[i]];
            float feedback = roomSize * 0.7f + 0.2f;
            
            // Damping acts as a lowpass filter on feedback (higher = darker reverb)
            float dampCoeff = 0.1f + damping * 0.8f;  // 0.1 to 0.9
            combFilterState[i] = combFilterState[i] * dampCoeff + delayed * (1.0f - dampCoeff);
            
            combBuffers[i][combPos[i]] = delayedInput + combFilterState[i] * feedback;
            combPos[i] = (combPos[i] + 1) % combBuffers[i].size();
            combOut += delayed;
        }
        combOut *= 0.25f;
        
        // Process 2 series allpass filters
        float allpassOut = combOut;
        for (int i = 0; i < 2; ++i) {
            float delayed = allpassBuffers[i][allpassPos[i]];
            float newVal = allpassOut + delayed * 0.5f;
            allpassBuffers[i][allpassPos[i]] = newVal;
            allpassOut = delayed - allpassOut * 0.5f;
            allpassPos[i] = (allpassPos[i] + 1) % allpassBuffers[i].size();
        }
        
        return allpassOut;
    }
    
private:
    static constexpr int combDelays[4] = {1557, 1617, 1491, 1422};
    static constexpr int allpassDelays[2] = {225, 556};
    static constexpr int PREDELAY_MAX = 8820; // 200ms at 44.1kHz
    
    std::vector<float> combBuffers[4];
    std::vector<float> allpassBuffers[2];
    std::vector<float> predelayBuffer;
    int combPos[4] = {0};
    int allpassPos[2] = {0};
    int predelayWritePos = 0;
    float combFilterState[4] = {0};
};

//=============================================================================
// CHORUS - Modulated delay for thickening
//=============================================================================
class Chorus {
public:
    static constexpr int BUFFER_SIZE = 4410; // 100ms at 44.1kHz
    
    Chorus() {
        buffer.resize(BUFFER_SIZE, 0.0f);
    }
    
    void reset() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        lfoPhase = 0.0f;
    }
    
    float process(float input, float rate, float depth, float feedback, float sampleRate) {
        // LFO for modulation
        lfoPhase += rate / sampleRate;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        float lfo = std::sin(lfoPhase * 6.283185f);
        
        // Modulated delay time (5-25ms range)
        float baseDelay = 0.015f * sampleRate; // 15ms center
        float modAmount = depth * 0.01f * sampleRate; // ±10ms
        float delaySamples = baseDelay + lfo * modAmount;
        
        // Interpolated read
        int readPos = static_cast<int>(writePos - delaySamples);
        while (readPos < 0) readPos += BUFFER_SIZE;
        float frac = (writePos - delaySamples) - static_cast<int>(writePos - delaySamples);
        if (frac < 0) frac += 1.0f;
        
        int pos1 = readPos % BUFFER_SIZE;
        int pos2 = (readPos + 1) % BUFFER_SIZE;
        float delayed = buffer[pos1] * (1.0f - frac) + buffer[pos2] * frac;
        
        // Write with feedback
        buffer[writePos] = input + delayed * feedback;
        writePos = (writePos + 1) % BUFFER_SIZE;
        
        return delayed;
    }
    
private:
    std::vector<float> buffer;
    int writePos = 0;
    float lfoPhase = 0.0f;
};

//=============================================================================
// PHASER - Cascaded allpass filters
//=============================================================================
class Phaser {
public:
    static constexpr int MAX_STAGES = 12;
    
    void reset() {
        for (auto& s : state) s = 0.0f;
        lfoPhase = 0.0f;
    }
    
    float process(float input, float rate, float depth, float feedback, int stages, float sampleRate) {
        // LFO
        lfoPhase += rate / sampleRate;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        float lfo = (std::sin(lfoPhase * 6.283185f) + 1.0f) * 0.5f; // 0-1
        
        // Calculate allpass coefficient based on LFO
        float minFreq = 100.0f;
        float maxFreq = 4000.0f;
        float freq = minFreq + lfo * depth * (maxFreq - minFreq);
        float coeff = (1.0f - std::tan(3.14159f * freq / sampleRate)) / 
                     (1.0f + std::tan(3.14159f * freq / sampleRate));
        
        // Cascade allpass filters
        float output = input + feedbackState * feedback;
        for (int i = 0; i < std::min(stages, MAX_STAGES); ++i) {
            float newState = output - coeff * state[i];
            output = state[i] + coeff * newState;
            state[i] = newState;
        }
        
        feedbackState = output;
        return output;
    }
    
private:
    float state[MAX_STAGES] = {0};
    float feedbackState = 0.0f;
    float lfoPhase = 0.0f;
};

//=============================================================================
// FLANGER - Short modulated delay
//=============================================================================
class Flanger {
public:
    static constexpr int BUFFER_SIZE = 2205; // 50ms at 44.1kHz
    
    Flanger() {
        buffer.resize(BUFFER_SIZE, 0.0f);
    }
    
    void reset() {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        lfoPhase = 0.0f;
    }
    
    float process(float input, float rate, float depth, float feedback, float sampleRate) {
        // LFO
        lfoPhase += rate / sampleRate;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        float lfo = std::sin(lfoPhase * 6.283185f);
        
        // Very short modulated delay (0.5-5ms)
        float baseDelay = 0.002f * sampleRate;
        float modAmount = depth * 0.003f * sampleRate;
        float delaySamples = baseDelay + lfo * modAmount;
        delaySamples = std::max(1.0f, delaySamples);
        
        // Interpolated read
        float readPosF = writePos - delaySamples;
        while (readPosF < 0) readPosF += BUFFER_SIZE;
        int readPos = static_cast<int>(readPosF) % BUFFER_SIZE;
        float frac = readPosF - static_cast<int>(readPosF);
        
        int pos2 = (readPos + 1) % BUFFER_SIZE;
        float delayed = buffer[readPos] * (1.0f - frac) + buffer[pos2] * frac;
        
        // Write with feedback (can be negative for jet effect)
        buffer[writePos] = input + delayed * feedback;
        writePos = (writePos + 1) % BUFFER_SIZE;
        
        return delayed;
    }
    
private:
    std::vector<float> buffer;
    int writePos = 0;
    float lfoPhase = 0.0f;
};

//=============================================================================
// BITCRUSHER - Bit depth and sample rate reduction
//=============================================================================
class Bitcrusher {
public:
    void reset() {
        holdCounter = 0;
        holdSample = 0.0f;
    }
    
    float process(float input, float bitDepth, float sampleReduce) {
        // Sample rate reduction
        holdCounter++;
        if (holdCounter >= static_cast<int>(sampleReduce)) {
            holdCounter = 0;
            
            // Bit depth reduction
            float levels = std::pow(2.0f, bitDepth);
            holdSample = std::round(input * levels) / levels;
        }
        
        return holdSample;
    }
    
private:
    int holdCounter = 0;
    float holdSample = 0.0f;
};

//=============================================================================
// RING MODULATOR
//=============================================================================
class RingModulator {
public:
    void reset() { phase = 0.0f; }
    
    float process(float input, float modFreq, float sampleRate) {
        phase += modFreq / sampleRate;
        if (phase >= 1.0f) phase -= 1.0f;
        
        float modulator = std::sin(phase * 6.283185f);
        return input * modulator;
    }
    
private:
    float phase = 0.0f;
};

//=============================================================================
// MAIN SYNTH CLASS
//=============================================================================
class DrumSynth {
public:
    static constexpr float SAMPLE_RATE = 44100.0f;
    
    DrumSynth() = default;
    
    void setPatch(const Patch& p) { patch = p; }
    const Patch& getPatch() const { return patch; }
    
    // Render a sound to a buffer
    std::vector<float> render(float durationSeconds = 1.0f) {
        const int numSamples = static_cast<int>(durationSeconds * SAMPLE_RATE);
        std::vector<float> buffer(numSamples);
        
        // Reset all components
        if (mainOscs.size() != 8) mainOscs.resize(8);
        for (auto& osc : mainOscs) osc.reset();
        subOsc.reset();
        clickOsc.reset();
        fmOsc.reset();
        filter.reset();
        lfo.reset();
        lfo2.reset();
        ampEnv.trigger();
        toneState = 0.0f;
        
        // Reset new effect processors
        ringMod.reset();
        delay.reset();
        reverb.reset();
        chorus.reset();
        phaser.reset();
        flanger.reset();
        bitcrusher.reset();
        
        // Set custom waveform data if using custom waveform
        if (patch.waveform == 59) {
            for (auto& osc : mainOscs) {
                osc.setCustomWaveform(patch.customWaveform.data(), Patch::CUSTOM_WAVEFORM_SIZE);
            }
        }
        
        if (patch.lfoWaveform == 3) {
            lfo.setCustomWaveform(patch.customLfoWaveform.data(), Patch::CUSTOM_WAVEFORM_SIZE);
        }
        
        for (int i = 0; i < numSamples; ++i) {
            const float t = static_cast<float>(i) / SAMPLE_RATE;
            buffer[i] = processSample(t);
        }
        
        // Apply post-processing effects (buffer-based)
        
        // Bitcrusher
        if (patch.bitDepth < 16.0f || patch.sampleReduce > 1.0f) {
            for (float& sample : buffer) {
                sample = bitcrusher.process(sample, patch.bitDepth, patch.sampleReduce);
            }
        }
        
        // Delay
        if (patch.delayMix > 0.0f) {
            for (float& sample : buffer) {
                float wet = delay.process(sample, patch.delayTime, patch.delayFeedback, 
                                          patch.delayFilter, SAMPLE_RATE);
                sample = sample * (1.0f - patch.delayMix) + wet * patch.delayMix;
            }
        }
        
        // Chorus
        if (patch.chorusMix > 0.0f) {
            for (float& sample : buffer) {
                float wet = chorus.process(sample, patch.chorusRate, patch.chorusDepth, 
                                           patch.chorusFeedback, SAMPLE_RATE);
                sample = sample * (1.0f - patch.chorusMix) + wet * patch.chorusMix;
            }
        }
        
        // Phaser
        if (patch.phaserMix > 0.0f) {
            for (float& sample : buffer) {
                float wet = phaser.process(sample, patch.phaserRate, patch.phaserDepth, 
                                           patch.phaserFeedback, patch.phaserStages, SAMPLE_RATE);
                sample = sample * (1.0f - patch.phaserMix) + wet * patch.phaserMix;
            }
        }
        
        // Flanger
        if (patch.flangerMix > 0.0f) {
            for (float& sample : buffer) {
                float wet = flanger.process(sample, patch.flangerRate, patch.flangerDepth, 
                                            patch.flangerFeedback, SAMPLE_RATE);
                sample = sample * (1.0f - patch.flangerMix) + wet * patch.flangerMix;
            }
        }
        
        // Reverb
        if (patch.reverbMix > 0.0f) {
            for (float& sample : buffer) {
                float wet = reverb.process(sample, patch.reverbSize, patch.reverbDamping, patch.reverbPreDelay);
                sample = sample * (1.0f - patch.reverbMix) + wet * patch.reverbMix;
            }
        }
        
        // Apply 12-band EQ if any bands are non-zero
        if (BiquadEQ::isActive(patch.eqGains)) {
            applyEQ(buffer);
        }
        
        // Apply compression if enabled
        if (patch.compression > 0.0f || patch.compRatio > 1.0f) {
            applyCompression(buffer);
        }
        
        // Apply master gain
        if (patch.masterGain != 1.0f) {
            for (float& sample : buffer) {
                sample *= patch.masterGain;
            }
        }
        
        return buffer;
    }
    
    // Multi-voice rendering: Render main patch +  group patches and mix them
    std::vector<float> renderMultiVoice(
        float durationSeconds,
        const std::vector<Patch>& groupPatches
    ) {
        if (groupPatches.empty()) {
            // No groups - just render main patch
            return render(durationSeconds);
        }
        
        // Render main patch (Voice 0)
        std::vector<float> mixedBuffer = render(durationSeconds);
        
        // Render each group patch and mix
        Patch originalPatch = patch; // Save original
        
        for (const auto& groupPatch : groupPatches) {
            // Apply note offset to frequency
            Patch modifiedPatch = groupPatch;
            if (std::abs(groupPatch.noteOffset) > 0.01f) {
                modifiedPatch.freq = groupPatch.freq * std::pow(2.0f, groupPatch.noteOffset / 12.0f);
            }
            
            // Render this group voice
            setPatch(modifiedPatch);
            std::vector<float> groupBuffer = render(durationSeconds);
            
            // Apply sample offset by trimming the start (audio from offset becomes start)
            if (modifiedPatch.sampleOffset > 0.0f) {
                // Use sampleOffset as a normalized multiplier (0.0 to 1.0) of the buffer size
                int offsetSamples = static_cast<int>(std::clamp(modifiedPatch.sampleOffset, 0.0f, 1.0f) * groupBuffer.size());
                if (offsetSamples > 0 && static_cast<size_t>(offsetSamples) < groupBuffer.size()) {
                    // Create new buffer and shift the audio rightwards (delay)
                    std::vector<float> offsetBuffer(groupBuffer.size(), 0.0f);
                    size_t copySize = groupBuffer.size() - offsetSamples;
                    // Copy from original start to the offset position in the new buffer
                    std::copy(groupBuffer.begin(), groupBuffer.begin() + copySize, offsetBuffer.begin() + offsetSamples);
                    groupBuffer = std::move(offsetBuffer);
                } else if (static_cast<size_t>(offsetSamples) >= groupBuffer.size()) {
                    // Offset is larger than buffer - result is silence
                    std::fill(groupBuffer.begin(), groupBuffer.end(), 0.0f);
                }
            }
            
            // Mix into main buffer (average mix to prevent clipping)
            size_t maxLen = std::max(mixedBuffer.size(), groupBuffer.size());
            if (mixedBuffer.size() < maxLen) {
                mixedBuffer.resize(maxLen, 0.0f);
            }
            
            float mixFactor = 1.0f / (groupPatches.size() + 1); // +1 for main voice
            for (size_t i = 0; i < groupBuffer.size(); ++i) {
                mixedBuffer[i] = mixedBuffer[i] * (1.0f - mixFactor) + groupBuffer[i] * mixFactor;
            }
        }
        
        // Restore original patch
        setPatch(originalPatch);
        
        return mixedBuffer;
    }
    
    // Trigger convenience method
    std::vector<float> trigger() {
        float soundDuration;
        if (patch.duration > 0.0f) {
            // Use user-specified duration (up to 60 seconds)
            soundDuration = std::clamp(patch.duration, 0.1f, 60.0f);
        } else {
            // Auto-calculate based on envelope settings
            soundDuration = patch.decay * 3.0f + patch.release + 0.1f;
            soundDuration = std::min(soundDuration, 3.0f);
        }
        std::vector<float> buffer = render(soundDuration);
        
        // Apply fade in/out if enabled
        if (patch.fadeIn > 0.0f || patch.fadeOut > 0.0f) {
            applyFades(buffer);
        }
        
        // Apply zero-crossing trimming if enabled
        if (patch.zeroCrossing && buffer.size() > 2) {
            buffer = trimToZeroCrossings(buffer);
        }
        
        return buffer;
    }
    
    // Find zero crossings and trim audio to start/end at them
    std::vector<float> trimToZeroCrossings(const std::vector<float>& input) {
        if (input.size() < 3) return input;
        
        // Find first zero crossing (from start)
        size_t startIdx = 0;
        for (size_t i = 1; i < input.size() / 4; ++i) {  // Search first 25% of audio
            if ((input[i-1] <= 0.0f && input[i] >= 0.0f) || 
                (input[i-1] >= 0.0f && input[i] <= 0.0f)) {
                startIdx = i;
                break;
            }
        }
        
        // Find last zero crossing (from end)
        size_t endIdx = input.size() - 1;
        for (size_t i = input.size() - 2; i > input.size() * 3 / 4; --i) {  // Search last 25%
            if ((input[i] <= 0.0f && input[i+1] >= 0.0f) || 
                (input[i] >= 0.0f && input[i+1] <= 0.0f)) {
                endIdx = i + 1;
                break;
            }
        }
        
        // Make sure we have a valid range. If not, force a fade on the original.
        if (endIdx <= startIdx || endIdx - startIdx < 100) {
            std::vector<float> forced = input;
            // Force 5ms fade (approx 220 samples) to guarantee zero crossing at edges
            const int fadeLen = std::min(220, static_cast<int>(forced.size() / 10));
            
            for (int i = 0; i < fadeLen; ++i) {
                float fade = static_cast<float>(i) / fadeLen;
                // Cubic fade for smoother start
                fade = fade * fade * (3.0f - 2.0f * fade); 
                
                forced[i] *= fade;
                forced[forced.size() - 1 - i] *= fade;
            }
            // Explicitly snap ends to exactly zero
            if (!forced.empty()) {
                forced[0] = 0.0f;
                forced[forced.size() - 1] = 0.0f;
            }
            return forced;
        }
        
        // Create trimmed buffer with short fade in/out to ensure smooth edges
        std::vector<float> trimmed(input.begin() + startIdx, input.begin() + endIdx);
        
        // Apply tiny fade (1ms) to ensure clean edges
        const int fadeSamples = std::min(44, static_cast<int>(trimmed.size() / 10));
        for (int i = 0; i < fadeSamples; ++i) {
            float fade = static_cast<float>(i) / fadeSamples;
            trimmed[i] *= fade;
            trimmed[trimmed.size() - 1 - i] *= fade;
        }
        
        return trimmed;
    }
    
    Patch patch;
    std::vector<Oscillator> mainOscs{32}; // Support up to 32 voices (Unison * Polyphony)
    Oscillator subOsc, clickOsc, fmOsc;
    NoiseGenerator noiseGen;
    Filter filter;
    LFO lfo, lfo2;
    Envelope ampEnv;
    BiquadEQ eq;  // 12-band graphic equalizer
    
    // New effect processors
    RingModulator ringMod;
    SimpleDelay delay;
    SimpleReverb reverb;
    Chorus chorus;
    Phaser phaser;
    Flanger flanger;
    Bitcrusher bitcrusher;
    
    float toneState = 0.0f;  // State for tone filter
    float compEnvelope = 0.0f; // Compressor envelope state
    
    float processSample(float t) {
        // --- LFO ---
        float lfoValue = lfo.process(patch.lfoRate, patch.lfoWaveform, SAMPLE_RATE) * patch.lfoDepth;
        
        // Determine LFO targets - use bitfield if set, otherwise legacy target
        bool useBitfield = (patch.lfoTargets != 0);
        bool lfoToFreq = useBitfield ? (patch.lfoTargets & Patch::LFO_TARGET_FREQ) : (patch.lfoTarget == 0);
        bool lfoToCutoff = useBitfield ? (patch.lfoTargets & Patch::LFO_TARGET_CUTOFF) : (patch.lfoTarget == 1);
        bool lfoToAmp = useBitfield ? (patch.lfoTargets & Patch::LFO_TARGET_AMP) : (patch.lfoTarget == 2);
        bool lfoToResonance = useBitfield && (patch.lfoTargets & Patch::LFO_TARGET_RESONANCE);
        bool lfoToDrive = useBitfield && (patch.lfoTargets & Patch::LFO_TARGET_DRIVE);
        bool lfoToNoise = useBitfield && (patch.lfoTargets & Patch::LFO_TARGET_NOISE);
        bool lfoToSub = useBitfield && (patch.lfoTargets & Patch::LFO_TARGET_SUB);
        bool lfoToGrit = useBitfield && (patch.lfoTargets & Patch::LFO_TARGET_GRIT);
        
        // --- Pitch Envelope ---
        float pitchDecayRate = 1.0f / std::max(patch.pitchDecay, 0.01f);
        float pitchEnv = std::exp(-t * pitchDecayRate * 20.0f);
        float startFreq = patch.freq * (1.0f + patch.punch * 10.0f);
        float currentFreq = patch.freq + (startFreq - patch.freq) * pitchEnv;
        
        // Apply extended pitch envelope if depth is set
        if (patch.pitchEnvDepth != 0.0f) {
            // Simple ADSR-style pitch envelope
            float pEnvValue = 0.0f;
            float pAttackTime = patch.pitchEnvAttack;
            float pDecayTime = patch.pitchEnvDecay;
            
            if (t < pAttackTime) {
                pEnvValue = t / pAttackTime;
            } else if (t < pAttackTime + pDecayTime) {
                float decayProgress = (t - pAttackTime) / pDecayTime;
                pEnvValue = 1.0f - decayProgress * (1.0f - patch.pitchEnvSustain);
            } else {
                pEnvValue = patch.pitchEnvSustain;
            }
            
            // Apply pitch envelope in semitones
            float semitoneOffset = patch.pitchEnvDepth * pEnvValue;
            currentFreq *= std::pow(2.0f, semitoneOffset / 12.0f);
        }
        
        // Apply LFO to pitch if targeted
        if (lfoToFreq) {
            currentFreq *= (1.0f + lfoValue * 0.1f);
        }
        
        // Apply detune
        if (patch.detune != 0.0f) {
            currentFreq *= std::pow(2.0f, patch.detune / 1200.0f);
        }
        
        // --- FM Synthesis ---
        float fmModulation = 0.0f;
        if (patch.fmDepth > 0.0f) {
            float fmFreq = currentFreq * patch.fmRatio;
            float fmModulator = fmOsc.process(fmFreq, 0, SAMPLE_RATE); // Sine modulator
            
            // Apply FM envelope if set
            float fmEnvMod = 1.0f;
            if (patch.fmEnvDepth != 0.0f) {
                fmEnvMod += patch.fmEnvDepth * pitchEnv;
            }
            
            fmModulation = fmModulator * patch.fmDepth * fmEnvMod * currentFreq * 4.0f;
        }
        
        // --- Main Oscillator with FM (Unison + Polyphony) ---
        float sample = 0.0f;
        int numUnisonVoices = std::max(1, std::min(patch.unisonVoices, 8));
        int numNotes = 1 + static_cast<int>(patch.extraNotes.size());
        
        // Cap total voices to available oscillators (32)
        if (numNotes * numUnisonVoices > 32) {
            // Priority to notes over extensive unison if we hit the limit? 
            // Or just cap notes? Let's just clamp the loop safely.
        }
        
        int totalVoices = numUnisonVoices * numNotes;
        if (totalVoices > 32) totalVoices = 32;

        // Calculate gain normalization (1/sqrt(N) is standard to maintain energy)
        float voiceGain = 1.0f / std::sqrt(static_cast<float>(totalVoices));
        
        int oscIdx = 0;
        
        for (int n = 0; n < numNotes; ++n) {
            // Determine semi-tone offset for this note
            float noteOffset = 0.0f;
            if (n > 0) noteOffset = static_cast<float>(patch.extraNotes[n-1]);
            
            // Apply offset to current frequency (after envelopes/LFOs)
            float noteFreqBase = currentFreq * std::pow(2.0f, noteOffset / 12.0f);
            
            for (int i = 0; i < numUnisonVoices; ++i) {
                if (oscIdx >= 32) break;
                
                float voiceFreq = noteFreqBase + fmModulation; // Apply FM to all notes? Yes
                
                // Apply Unison Detune
                if (numUnisonVoices > 1) {
                    float spread = (static_cast<float>(i) / (numUnisonVoices - 1)) * 2.0f - 1.0f;
                    float cents = spread * patch.unisonDetune;
                    voiceFreq *= std::pow(2.0f, cents / 1200.0f);
                }
                
                sample += mainOscs[oscIdx++].process(voiceFreq, patch.waveform, SAMPLE_RATE, patch.pulseWidth) * voiceGain;
            }
            if (oscIdx >= 32) break;
        }
        
        // --- Ring Modulation ---
        if (patch.ringModMix > 0.0f) {
            float ringModulated = ringMod.process(sample, patch.ringModFreq, SAMPLE_RATE);
            sample = sample * (1.0f - patch.ringModMix) + ringModulated * patch.ringModMix;
        }
        
        // --- Sub Oscillator ---
        if (patch.sub > 0.0f) {
            float subFreq = currentFreq / (patch.subOctave == 2 ? 4.0f : 2.0f);
            float subSample = subOsc.process(subFreq, 0, SAMPLE_RATE); // Sub always sine
            float subMix = patch.sub;
            if (lfoToSub) subMix *= (1.0f + lfoValue * 0.5f);
            subMix = std::clamp(subMix, 0.0f, 1.0f);
            sample = sample * (1.0f - subMix) + subSample * subMix;
        }
        
        // --- Click/Transient ---
        if (patch.click > 0.0f && t < 0.01f) {
            float clickEnv = 1.0f - t * 100.0f;
            float clickSample = clickOsc.process(patch.clickFreq, 0, SAMPLE_RATE);
            sample += clickSample * patch.click * clickEnv;
        }
        
        // --- Noise ---
        if (patch.noise > 0.0f) {
            float noiseSample = noiseGen.process(patch.noiseType);
            float noiseMix = patch.noise;
            if (lfoToNoise) noiseMix *= (1.0f + lfoValue * 0.5f);
            noiseMix = std::clamp(noiseMix, 0.0f, 1.0f);
            sample = sample * (1.0f - noiseMix) + noiseSample * noiseMix;
        }
        
        // --- Filter ---
        if (patch.cutoff < 1.0f || patch.filterEnv != 0.0f || patch.filterDrive > 0.0f || 
            patch.filterKeyTrack > 0.0f || lfoToCutoff || lfoToResonance) {
            float filterDecayRate = 1.0f / std::max(patch.filterDecay, 0.01f);
            float filterEnvValue = std::exp(-t * filterDecayRate * 10.0f);
            float cutoff = patch.cutoff + patch.filterEnv * filterEnvValue;
            float resonance = patch.resonance;
            
            // Apply key tracking (pitch frequency modulates cutoff)
            if (patch.filterKeyTrack > 0.0f) {
                // Map frequency to cutoff contribution (higher freq = higher cutoff)
                float freqCutoffMod = (patch.freq - 60.0f) / 500.0f;  // Normalize around 60Hz
                cutoff += freqCutoffMod * patch.filterKeyTrack * 0.5f;
            }
            
            // Apply LFO to filter params
            if (lfoToCutoff) cutoff += lfoValue * 0.3f;
            if (lfoToResonance) resonance += lfoValue * 0.3f;
            
            cutoff = std::clamp(cutoff, 0.0f, 1.0f);
            resonance = std::clamp(resonance, 0.0f, 1.0f);
            
            // Apply pre-filter drive/saturation
            if (patch.filterDrive > 0.0f) {
                float driveAmount = 1.0f + patch.filterDrive * 4.0f;
                sample = std::tanh(sample * driveAmount) / std::tanh(driveAmount);
            }
            
            sample = filter.process(sample, cutoff, resonance, SAMPLE_RATE, patch.filterType);
        }
        
        // --- Saturation/Drive ---
        if (patch.grit > 0.0f || patch.drive > 0.0f || lfoToDrive || lfoToGrit) {
            float grit = patch.grit;
            float drive = patch.drive;
            if (lfoToGrit) grit += lfoValue * 0.3f;
            if (lfoToDrive) drive += lfoValue * 0.3f;
            grit = std::clamp(grit, 0.0f, 1.0f);
            drive = std::clamp(drive, 0.0f, 1.0f);
            
            float driveAmount = 1.0f + grit * 4.0f + drive * 4.0f;
            float signal = sample * driveAmount;

            switch (patch.distType) {
                case 0: // SOFT (Tanh)
                default:
                    sample = std::tanh(signal);
                    break;
                    
                case 1: // HARD (Clip)
                    sample = std::clamp(signal, -1.0f, 1.0f);
                    break;
                    
                case 2: // FOLD (Wavefold)
                    // Simple sine fold
                    sample = std::sin(signal * 1.5f); 
                    break;
                    
                case 3: // BIT (Quantize amplitude)
                {
                    float steps = 2.0f + (1.0f - grit) * 14.0f; // 2 to 16 steps
                    sample = std::round(signal * steps) / steps;
                    sample = std::clamp(sample, -1.0f, 1.0f);
                    break;
                }
                    
                case 4: // TUBE (Asymmetric)
                {
                    // Add DC offset before clipping for asymmetry (even harmonics)
                    float bias = 0.3f;
                    float asym = signal + bias;
                    // Soft clip the biased signal
                    float sat = std::tanh(asym);
                    // Remove bias from result
                    sample = sat - std::tanh(bias); 
                    // Make up gain slightly
                    sample *= 1.2f;
                    break;
                }
                    
                case 5: // FUZZ (Gateway / Hard Threshold)
                {
                    float fuzzGain = 5.0f * driveAmount; // Extra gain
                    float fuzzed = signal * fuzzGain;
                    // Hard clipper with a bit of slew limiting feel (simple clamp for now)
                    sample = std::clamp(fuzzed, -0.9f, 0.9f);
                    break;
                }
            }
        }
        
        // --- Amplitude Envelope ---
        float ampEnvValue = ampEnv.process(patch.attack, patch.decay, patch.sustain, patch.release, SAMPLE_RATE);
        
        // Apply LFO to amplitude if targeted
        if (lfoToAmp) {
            ampEnvValue *= (1.0f + lfoValue * 0.5f);
        }
        
        sample *= ampEnvValue;
        
        // --- Tone (simple tilt EQ) ---
        if (patch.tone != 0.5f) {
            float toneCoeff = 0.1f + patch.tone * 0.4f;
            toneState = toneState * (1.0f - toneCoeff) + sample * toneCoeff;
            sample = patch.tone > 0.5f ? sample : toneState * 2.0f;
        }
        
        // --- Granular texture ---
        if (patch.granular > 0.0f) {
            // Add grain-like texture through random amplitude modulation
            float grain = noiseGen.process(0) * patch.granular * 0.3f;
            sample *= (1.0f + grain);
        }
        
        return sample * 0.9f; // Headroom
    }
    
    void applyCompression(std::vector<float>& buffer) {
        // Parameters
        float threshold = patch.compThreshold;
        float ratio = patch.compRatio;
        float attack = patch.compAttack;   // In seconds
        float release = patch.compRelease; // In seconds
        float makeup = std::pow(10.0f, (patch.compMakeup * 24.0f) / 20.0f); // Map 0-1 to 0-24dB makeup
        
        // If legacy compression is used, blend it in
        if (patch.compression > 0.0f) {
            ratio = std::max(ratio, 1.0f + patch.compression * 10.0f);
            makeup *= (1.0f + patch.compression * 1.0f);
        }
        
        if (ratio <= 1.0f && patch.compression <= 0.0f) return;

        // Constants for attack/release coefficients
        float attackCoeff = std::exp(-1.0f / (attack * 44100.0f));
        float releaseCoeff = std::exp(-1.0f / (release * 44100.0f));
        
        for (float& sample : buffer) {
            // Envelope follower (peak detection)
            float inputAbs = std::abs(sample);
            
            if (inputAbs > compEnvelope)
                compEnvelope = compEnvelope * attackCoeff + inputAbs * (1.0f - attackCoeff);
            else
                compEnvelope = compEnvelope * releaseCoeff + inputAbs * (1.0f - releaseCoeff);
            
            // Re-calculate gain reduction
            float gain = 1.0f;
            if (compEnvelope > threshold && compEnvelope > 0.0001f) {
                // dB calculation
                float envdB = 20.0f * std::log10(compEnvelope);
                float thresholddB = 20.0f * std::log10(std::max(threshold, 0.0001f));
                
                float overdB = envdB - thresholddB;
                float reduceddB = overdB / ratio;
                float gaindB = reduceddB - overdB;
                
                gain = std::pow(10.0f, gaindB / 20.0f);
            }
            
            sample *= gain * makeup;
        }
    }
    
    void applyEQ(std::vector<float>& buffer) {
        eq.reset();
        eq.setGains(patch.eqGains, SAMPLE_RATE);
        
        for (float& sample : buffer) {
            sample = eq.process(sample);
        }
    }
    
    void applyFades(std::vector<float>& buffer) {
        const int numSamples = static_cast<int>(buffer.size());
        
        // Fade in
        if (patch.fadeIn > 0.0f) {
            int fadeInSamples = static_cast<int>(patch.fadeIn * SAMPLE_RATE);
            fadeInSamples = std::min(fadeInSamples, numSamples);
            for (int i = 0; i < fadeInSamples; ++i) {
                // Smooth cosine fade for natural sound
                float t = static_cast<float>(i) / fadeInSamples;
                float fade = 0.5f * (1.0f - std::cos(t * 3.14159265f));
                buffer[i] *= fade;
            }
        }
        
        // Fade out
        if (patch.fadeOut > 0.0f) {
            int fadeOutSamples = static_cast<int>(patch.fadeOut * SAMPLE_RATE);
            fadeOutSamples = std::min(fadeOutSamples, numSamples);
            int startIdx = numSamples - fadeOutSamples;
            for (int i = 0; i < fadeOutSamples; ++i) {
                // Smooth cosine fade for natural sound
                float t = static_cast<float>(i) / fadeOutSamples;
                float fade = 0.5f * (1.0f + std::cos(t * 3.14159265f));
                buffer[startIdx + i] *= fade;
            }
        }
    }
};
} // namespace SoundFriend
