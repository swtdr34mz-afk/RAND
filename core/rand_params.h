// RAND – gemeinsame Parameterliste für FM-1-Firmware UND VST.
// Alle Werte sind normiert 0..1; die Umrechnung in Hz/Sekunden passiert in rand_voice.cpp.
#pragma once

namespace rnd {

enum ParamId {
    // Oszillatoren
    P_VCO1_FREQ, P_VCO1_WAVE, P_VCO1_EG_AMT, P_VCO1_SEQ_AMT,
    P_VCO2_FREQ, P_VCO2_WAVE, P_VCO2_EG_AMT, P_VCO2_SEQ_AMT,
    P_FM_AMT, P_SYNC, P_PITCH_DECAY,
    // Mixer
    P_VCO1_LVL, P_VCO2_LVL, P_NOISE_LVL, P_NOISE_COLOR,
    // Filter
    P_CUTOFF, P_RESO, P_FILTER_MODE, P_VCF_EG_AMT, P_VCF_DECAY, P_NOISE_VCF,
    // VCA
    P_VCA_DECAY, P_VCA_ATTACK, P_VEL_VCA, P_VEL_VCF,
    // Ausgang
    P_DRIVE, P_VOLUME,
    // Sequencer
    P_TEMPO, P_CLOCK_MULT, P_SEQ_LENGTH,
    P_COUNT
};

struct ParamInfo {
    const char* name;   // langer Name (VST)
    const char* label;  // kurzes Label (FM-1-Display, max. 6 Zeichen)
    float def;
    bool stepped;       // Schalter / Rasterwert
};

extern const ParamInfo kParamInfo[P_COUNT];

constexpr int kSteps = 8;
constexpr int kClockMultCount = 11;
extern const float kClockMults[kClockMultCount];
extern const char* const kClockMultNames[kClockMultCount];

} // namespace rnd
