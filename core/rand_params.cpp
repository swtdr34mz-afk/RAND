#include "rand_params.h"

namespace rnd {

const ParamInfo kParamInfo[P_COUNT] = {
    {"VCO 1 Frequency",   "VCO1",   0.35f, false},
    {"VCO 1 Wave",        "WAVE1",  0.0f,  true },
    {"VCO 1 EG Amount",   "EG>1",   0.75f, false},
    {"VCO 1 Seq Amount",  "SEQ>1",  0.5f,  false},
    {"VCO 2 Frequency",   "VCO2",   0.45f, false},
    {"VCO 2 Wave",        "WAVE2",  1.0f,  true },
    {"VCO 2 EG Amount",   "EG>2",   0.5f,  false},
    {"VCO 2 Seq Amount",  "SEQ>2",  0.5f,  false},
    {"1>2 FM Amount",     "FM",     0.0f,  false},
    {"Hard Sync",         "SYNC",   0.0f,  true },
    {"Pitch Decay",       "P.DEC",  0.3f,  false},
    {"VCO 1 Level",       "LVL1",   0.9f,  false},
    {"VCO 2 Level",       "LVL2",   0.0f,  false},
    {"Noise Level",       "NOISE",  0.0f,  false},
    {"Noise Color",       "COLOR",  0.0f,  false},
    {"Cutoff",            "CUTOFF", 0.6f,  false},
    {"Resonance",         "RESO",   0.2f,  false},
    {"Filter Mode",       "LP/HP",  0.0f,  true },
    {"VCF EG Amount",     "EG>F",   0.6f,  false},
    {"VCF Decay",         "F.DEC",  0.3f,  false},
    {"Noise > VCF",       "N>F",    0.0f,  false},
    {"VCA Decay",         "DECAY",  0.4f,  false},
    {"VCA Attack Slow",   "ATK",    0.0f,  true },
    {"Velocity > VCA",    "V>AMP",  0.8f,  false},
    {"Velocity > VCF",    "V>F",    0.3f,  false},
    {"Drive",             "DRIVE",  0.3f,  false},
    {"Volume",            "VOL",    0.8f,  false},
    {"Tempo",             "BPM",    0.333f,false},  // 0..1 -> 40..280 BPM, 0.333 = 120
    {"Clock Multiplier",  "CLOCK",  0.2f,  true },  // Index in kClockMults
    {"Sequence Length",   "LEN",    1.0f,  true },  // 1..8
};

// x1 = 16tel-Noten. Ab x32 kippt der Takt in den hörbaren Bereich (der EDGE-Trick).
const float kClockMults[kClockMultCount] = {0.25f, 0.5f, 1.f, 2.f, 3.f, 4.f, 8.f, 16.f, 32.f, 64.f, 128.f};
const char* const kClockMultNames[kClockMultCount] = {"/4", "/2", "x1", "x2", "x3", "x4", "x8", "x16", "x32", "x64", "x128"};

} // namespace rnd
