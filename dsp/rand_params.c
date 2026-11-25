/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (RAND) */
/* RAND – die Parameterliste (FM-1 und VST lesen dieselbe). Teil von rand_core.c */

const rnd_param_info RND_PARAMS[RP_COUNT] = {
    {"VCO 1 Frequency",  "VCO 1",  0.35f, RK_UNI, 0, 0},
    {"VCO 1 Wave",       "WAVE 1", 0.0f,  RK_SWITCH, "TRI", "PULSE"},
    {"VCO 1 EG Amount",  "EG>1",   0.75f, RK_BIP, 0, 0},
    {"VCO 1 Seq Amount", "SEQ>1",  0.5f,  RK_BIP, 0, 0},
    {"VCO 2 Frequency",  "VCO 2",  0.45f, RK_UNI, 0, 0},
    {"VCO 2 Wave",       "WAVE 2", 1.0f,  RK_SWITCH, "TRI", "PULSE"},
    {"VCO 2 EG Amount",  "EG>2",   0.5f,  RK_BIP, 0, 0},
    {"VCO 2 Seq Amount", "SEQ>2",  0.5f,  RK_BIP, 0, 0},
    {"1>2 FM Amount",    "FM 1>2", 0.0f,  RK_UNI, 0, 0},
    {"Hard Sync",        "SYNC",   0.0f,  RK_SWITCH, "OFF", "ON"},
    {"Pitch Decay",      "P.DECAY",0.3f,  RK_UNI, 0, 0},
    {"VCO 1 Level",      "LEVEL 1",0.9f,  RK_UNI, 0, 0},
    {"VCO 2 Level",      "LEVEL 2",0.0f,  RK_UNI, 0, 0},
    {"Noise Level",      "NOISE",  0.0f,  RK_UNI, 0, 0},
    {"Noise Color",      "COLOR",  0.0f,  RK_UNI, 0, 0},
    {"Cutoff",           "CUTOFF", 0.6f,  RK_UNI, 0, 0},
    {"Resonance",        "RESO",   0.2f,  RK_UNI, 0, 0},
    {"Filter Mode",      "FILTER", 0.0f,  RK_SWITCH, "LOW", "HIGH"},
    {"VCF EG Amount",    "EG>VCF", 0.6f,  RK_BIP, 0, 0},
    {"VCF Decay",        "F.DECAY",0.3f,  RK_UNI, 0, 0},
    {"Noise > VCF",      "NOI>VCF",0.0f,  RK_UNI, 0, 0},
    {"VCA Decay",        "DECAY",  0.4f,  RK_UNI, 0, 0},
    {"VCA Attack",       "ATTACK", 0.0f,  RK_SWITCH, "FAST", "SLOW"},
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
