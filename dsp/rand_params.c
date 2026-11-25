/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (RAND) */
/* RAND – die Parameterliste (FM-1 und VST lesen dieselbe). Teil von rand_core.c */

const rnd_param_info RND_PARAMS[RP_COUNT] = {
    {"VCO 1 Frequency",  "FREQ 1", 0.35f, RK_UNI, 0, 0},
    {"VCO 1 Wave",       "SHAPE 1",0.0f,  RK_SWITCH, "TRI", "PULSE"},
    {"VCO 1 EG Amount",  "ENV 1",  0.75f, RK_BIP, 0, 0},
    {"VCO 1 Seq Amount", "SEQ PTCH",0.5f,  RK_BIP, 0, 0},
    {"VCO 2 Frequency",  "FREQ 2", 0.45f, RK_UNI, 0, 0},
    {"VCO 2 Wave",       "SHAPE 2",1.0f,  RK_SWITCH, "TRI", "PULSE"},
    {"VCO 2 EG Amount",  "ENV 2",  0.5f,  RK_BIP, 0, 0},
    {"VCO 2 Seq Amount", "SEQ PTCH",0.5f,  RK_BIP, 0, 0},
    {"1>2 FM Amount",    "1-2 FM", 0.0f,  RK_UNI, 0, 0},
    {"Hard Sync",        "HARD SYNC",0.0f,  RK_SWITCH, "OFF", "ON"},
    {"Pitch Decay",      "EG DECAY",0.3f,  RK_UNI, 0, 0},
    {"VCO 1 Level",      "LEVEL",  0.9f,  RK_UNI, 0, 0},
    {"VCO 2 Level",      "LEVEL",  0.0f,  RK_UNI, 0, 0},
    {"Noise Level",      "NOISE",  0.0f,  RK_UNI, 0, 0},
    {"Noise Color",      "COLOR",  0.0f,  RK_UNI, 0, 0},
    {"Cutoff",           "CUTOFF", 0.6f,  RK_UNI, 0, 0},
    {"Resonance",        "RESONANCE",0.2f,  RK_UNI, 0, 0},
    {"Filter Mode",      "VCF MODE",0.0f, RK_SWITCH, "LP", "HP"},
    {"VCF EG Amount",    "ENVELOPE",0.6f,  RK_BIP, 0, 0},
    {"VCF Decay",        "DECAY",  0.3f,  RK_UNI, 0, 0},
    {"Noise > VCF",      "NOISE/VCF",0.0f,  RK_UNI, 0, 0},
    {"VCA Decay",        "DECAY",  0.4f,  RK_UNI, 0, 0},
    {"VCA Attack",       "VCA MODE",0.0f,  RK_SWITCH, "FAST", "SLOW"},
    {"Velocity > VCA",   "VEL>AMP",0.8f,  RK_UNI, 0, 0},
    {"Velocity > VCF",   "VEL>VCF",0.3f,  RK_UNI, 0, 0},
    {"Drive",            "DRIVE",  0.3f,  RK_UNI, 0, 0},
    {"Volume",           "VOLUME", 0.8f,  RK_UNI, 0, 0},
    {"Tempo",            "TEMPO",  0.333f,RK_UNI, 0, 0},   /* 0..1 -> 40..280 BPM */
    {"Clock",            "CLOCK",  0.2f,  RK_UNI, 0, 0},   /* Index in RND_CLOCK_MULT */
    {"Length",           "LENGTH", 1.0f,  RK_UNI, 0, 0},   /* 1..8 Steps */
};

/* x1 = Sechzehntel. Ab etwa x32 kippt der Takt in den hörbaren Bereich: aus dem Rhythmus wird ein Ton */
const float RND_CLOCK_MULT[RND_NCLOCK] = {0.25f, 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f};
const char *const RND_CLOCK_NAME[RND_NCLOCK] = {"/4", "/2", "x1", "x2", "x3", "x4", "x8", "x16", "x32", "x64", "x128"};
