#pragma once

#include <string>
#include <vector>
#include <map>
#include <raylib.h>

namespace fx {

// ============================================
// FX Type IDs
// ============================================
enum FXType : int {
    FX_NONE     = 0,
    FX_CUTOFF   = 1,   // Cut playback at specified time
    FX_SLIDE    = 2,   // Pitch slide between steps
    FX_STUTTER  = 3,   // Repeat portions of the sample
    FX_NUDGE    = 4,   // Offset step timing
    FX_SLICE    = 5,   // Play specific slice of sample
    
    // Future FX (reserve IDs)
    FX_REVERSE  = 6,   // Reverse playback
    FX_RETRIG   = 7,   // Retrigger within step
    FX_FILTER   = 8,   // Low/High pass filter
    FX_BITCRUSH = 9,   // Bit depth reduction
    FX_DELAY    = 10,  // Echo/delay effect
    FX_PAN      = 11,  // Stereo panning
    FX_PITCH    = 12,  // Pitch shift (non-melodic)
    FX_VOLUME   = 13,  // Volume automation
    FX_REVERB   = 14,  // Room Reverb
    FX_COMPRESSOR = 15,// Dynamic Compressor
    FX_ATTACK   = 16,  // ADSR Attack
    FX_DECAY    = 17,  // ADSR Decay
    FX_SUSTAIN  = 18,  // ADSR Sustain
    FX_RELEASE  = 19,  // ADSR Release
    FX_ADSR     = 20,  // Consolidated ADSR
    FX_EQ       = 21,  // Parametric EQ
    FX_SATURATION = 22,
    FX_OVERDRIVE = 23,
    FX_CHORUS   = 24,
    FX_FLANGER  = 25,
    
    FX_COUNT    = 26   // Total count
};

// ============================================
// FX Parameter IDs
// ============================================
enum FXParam : int {
    // Stutter params (100-199)
    PAR_STUTTER_RATE   = 100,  // Number of repeats
    PAR_STUTTER_SPEED  = 101,  // Speed multiplier
    
    // Slide params (200-299)
    PAR_SLIDE_TIME     = 200,  // Slide duration
    PAR_SLIDE_SQUELCH  = 201,  // Cut previous note
    
    // Nudge params (300-399)
    PAR_NUDGE_OFFSET   = 300,  // Timing offset [0-1]
    
    // Slice params (400-499)
    PAR_SLICE_INDEX    = 400,  // Which slice to play
    PAR_SLICE_CUTOFF   = 401,  // Cut at next slice boundary
    
    // Reverse params (500-599)
    PAR_REVERSE_MODE   = 500,  // 0=full, 1=partial
    
    // Retrig params (600-699)
    PAR_RETRIG_COUNT   = 600,  // Number of retrigs
    PAR_RETRIG_DECAY   = 601,  // Volume decay per retrig
    
    // Filter params (700-799)
    PAR_FILTER_CUTOFF  = 700,  // Filter cutoff frequency
    PAR_FILTER_RESO    = 701,  // Resonance
    PAR_FILTER_TYPE    = 702,  // 0=LP, 1=HP, 2=BP
    
    // Bitcrush params (800-899)
    PAR_CRUSH_BITS     = 800,  // Bit depth
    PAR_CRUSH_RATE     = 801,  // Sample rate reduction
    
    // Delay params (900-999)
    PAR_DELAY_TIME     = 900,  // Delay time
    PAR_DELAY_FEEDBACK = 901,  // Feedback amount
    PAR_DELAY_MIX      = 902,  // Wet/dry mix
    
    // Pan params (1000-1099)
    PAR_PAN_POSITION   = 1000, // -1 to 1 (L to R)
    
    // Pitch params (1100-1199)
    PAR_PITCH_SHIFT    = 1100, // Semitones
    
    // Volume params (1200-1299)
    PAR_VOLUME_LEVEL   = 1200, // 0-1
    
    // ADSR Params (1300-1399)
    PAR_ATTACK_TIME    = 1300, // 0-1 (mapped to time)
    PAR_DECAY_TIME     = 1301, // 0-1 (mapped to time)
    PAR_SUSTAIN_LEVEL  = 1302, // 0-1 (level)
    PAR_RELEASE_TIME   = 1303  // 0-1 (mapped to time)
};

// ============================================
// FX Metadata
// ============================================
struct FXInfo {
    FXType id;
    std::string name;
    std::string shortName;   // 3-4 char for compact display
    Color color;             // UI color (forward declared or use uint32)
    bool implemented;        // Is this FX currently working?
};

// Get info for an FX type
inline const char* GetFXName(FXType type) {
    switch (type) {
        case FX_NONE:     return "None";
        case FX_CUTOFF:   return "Cutoff";
        case FX_SLIDE:    return "Slide";
        case FX_STUTTER:  return "Stutter";
        case FX_NUDGE:    return "Nudge";
        case FX_SLICE:    return "Slice";
        case FX_REVERSE:  return "Reverse";
        case FX_RETRIG:   return "Retrig";
        case FX_FILTER:   return "Filter";
        case FX_BITCRUSH: return "Bitcrush";
        case FX_DELAY:    return "Delay";
        case FX_PAN:      return "Pan";
        case FX_PITCH:    return "Pitch";
        case FX_VOLUME:   return "Volume";
        case FX_COMPRESSOR: return "Compressor";
        case FX_ATTACK:   return "Attack";
        case FX_DECAY:    return "Decay";
        case FX_SUSTAIN:  return "Sustain";
        case FX_RELEASE:  return "Release";
        case FX_ADSR:     return "ADSR";
        case FX_EQ:       return "EQ";
        case FX_SATURATION: return "Saturation";
        case FX_OVERDRIVE: return "Overdrive";
        case FX_CHORUS:   return "Chorus";
        case FX_FLANGER:  return "Flanger";
        case FX_COUNT:    return "Count";
        default:          return "Unknown";
    }
}

inline const char* GetFXShortName(FXType type) {
    switch (type) {
        case FX_NONE:     return "-";
        case FX_CUTOFF:   return "CUT";
        case FX_SLIDE:    return "SLD";
        case FX_STUTTER:  return "STT";
        case FX_NUDGE:    return "NUD";
        case FX_SLICE:    return "SLC";
        case FX_REVERSE:  return "REV";
        case FX_RETRIG:   return "RTG";
        case FX_FILTER:   return "FLT";
        case FX_BITCRUSH: return "BIT";
        case FX_DELAY:    return "DLY";
        case FX_PAN:      return "PAN";
        case FX_PITCH:    return "PIT";
        case FX_VOLUME:   return "VOL";
        case FX_COMPRESSOR: return "CMP";
        case FX_ATTACK:   return "ATT";
        case FX_DECAY:    return "DEC";
        case FX_SUSTAIN:  return "SUS";
        case FX_RELEASE:  return "REL";
        case FX_ADSR:     return "ENV";
        case FX_EQ:       return "EQ";
        case FX_SATURATION: return "SAT";
        case FX_OVERDRIVE: return "OD";
        case FX_CHORUS:   return "CHO";
        case FX_FLANGER:  return "FLG";
        case FX_COUNT:    return "CNT";
        default:          return "???";
    }
}

inline bool IsFXImplemented(FXType type) {
    switch (type) {
        case FX_CUTOFF:
        case FX_SLIDE:
        case FX_STUTTER:
        case FX_NUDGE:
        case FX_REVERSE:
        case FX_ADSR:
            return true;
        default:
            return false;
    }
}

// Get list of all implemented FX
inline std::vector<FXType> GetImplementedFX() {
    std::vector<FXType> result;
    for (int i = 1; i < FX_COUNT; ++i) {
        if (IsFXImplemented(static_cast<FXType>(i))) {
            result.push_back(static_cast<FXType>(i));
        }
    }
    return result;
}

// Get list of all FX (including unimplemented)
inline std::vector<FXType> GetAllFX() {
    std::vector<FXType> result;
    for (int i = 1; i < FX_COUNT; ++i) {
        result.push_back(static_cast<FXType>(i));
    }
    return result;
}

} // namespace fx
