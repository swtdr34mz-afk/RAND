/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (RAND) */
/* RAND – der Klangkern. Reines C, keine Abhängigkeit von der FM-1-Plattform:
 * dieselben Dateien laufen in der Firmware (app.c) und später im VST-Plugin.
 *
 * Eine Stimme (2 VCOs, FM 1>2, Sync, Rauschen, 24-dB-Ladder LP/HP, drei Hüllkurven, Drive)
 * und ein 8-Step-Sequencer mit zwei Reihen (Pitch + Velocity), dessen Takt bis in den
 * hörbaren Bereich gedreht werden kann. Alle Parameter sind normiert 0..1. */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    /* Oszillatoren */
    RP_VCO1_FREQ, RP_VCO1_WAVE, RP_VCO1_EG, RP_VCO1_SEQ,
    RP_VCO2_FREQ, RP_VCO2_WAVE, RP_VCO2_EG, RP_VCO2_SEQ,
    RP_FM, RP_SYNC, RP_PITCH_DECAY,
    /* Mixer */
    RP_VCO1_LVL, RP_VCO2_LVL, RP_NOISE_LVL, RP_NOISE_COLOR,
    /* Filter */
    RP_CUTOFF, RP_RESO, RP_FILTER_MODE, RP_VCF_EG, RP_VCF_DECAY, RP_NOISE_VCF,
    /* VCA */
    RP_VCA_DECAY, RP_VCA_ATTACK, RP_VEL_VCA, RP_VEL_VCF,
    /* Ausgang */
    RP_DRIVE, RP_VOLUME,
    /* Sequencer */
    RP_TEMPO, RP_CLOCK, RP_LENGTH,
    RP_COUNT
};

/* Art eines Parameters (für Anzeige und Bedienung) */
enum { RK_UNI, RK_BIP, RK_SWITCH };

typedef struct {
    const char *name;     /* langer Name (VST) */
    const char *label;    /* kurz, fürs FM-1-Display */
    float def;
    uint8_t kind;         /* RK_* */
    const char *off, *on; /* RK_SWITCH: die beiden Namen */
} rnd_param_info;

extern const rnd_param_info RND_PARAMS[RP_COUNT];

#define RND_STEPS 8
#define RND_NCLOCK 11
extern const float RND_CLOCK_MULT[RND_NCLOCK];
extern const char *const RND_CLOCK_NAME[RND_NCLOCK];

typedef struct {
    float pitch;   /* 0..1, 0.5 = Grundton */
    float vel;     /* 0..1 */
    uint8_t on;
} rnd_step;

/* --- interne Bausteine (hier, damit rnd_engine eine feste Größe hat; kein Heap) --- */
typedef struct { float phase, wrap_frac; int wrapped; } rnd_osc;
typedef struct { float y1, y2, y3, y4; } rnd_ladder;
typedef struct { float level, atk_inc, dec_coef; int attacking; } rnd_env;
typedef struct { uint32_t s; float b0, b1, b2; } rnd_noise;

typedef struct {
    float sr;
    float p[RP_COUNT];
    int dirty;
    rnd_osc o1, o2;
    rnd_ladder vcf;
    rnd_env eg_p, eg_f, eg_a;
    rnd_noise nz;
    float dc_x1, dc_y1;
    float step_pitch, vel;
    /* umgerechnete Werte */
    float f1, f2, eg1, eg2, sq1, sq2, fm, l1, l2, ln, color;
    float cut, reso, egf, nf, vvca, vvcf, drive, vol;
    int w1, w2, sync, hp;
} rnd_voice;

typedef struct {
    float sr;
    float p[RP_COUNT];
    rnd_voice v;
    rnd_step steps[RND_STEPS];
    int running, cur;
    double to_next;
    int clock_override;      /* -1: Knopf; sonst Index in RND_CLOCK_MULT (gehaltene Taste) */
    /* externe MIDI-Clock */
    double counter, last_clock, ext_bpm;
    int ext;                 /* 1: folgt gerade einer MIDI-Clock */
} rnd_engine;

/* --- die Schnittstelle --- */
void rnd_init(rnd_engine *e, float sample_rate);
void rnd_set(rnd_engine *e, int id, float norm);
float rnd_get(const rnd_engine *e, int id);

void rnd_start(rnd_engine *e);           /* von Step 1 */
void rnd_stop(rnd_engine *e);
void rnd_continue(rnd_engine *e);
int rnd_running(const rnd_engine *e);
int rnd_step_now(const rnd_engine *e);   /* -1: gestoppt */

void rnd_midi_clock(rnd_engine *e);      /* bei jedem 0xF8 */
void rnd_trigger(rnd_engine *e, float pitch, float vel);   /* ein Schlag von Hand */
void rnd_clock_override(rnd_engine *e, int idx);           /* -1 = aus */

void rnd_render(rnd_engine *e, float *out, int n);         /* mono, überschreibt out */

float rnd_bpm(const rnd_engine *e);
int rnd_clock_index(const rnd_engine *e);
int rnd_length(const rnd_engine *e);
float rnd_level(const rnd_engine *e);    /* VCA-Hüllkurve jetzt (Anzeige) */

/* Presets */
#define RND_NPRESETS 8
const char *rnd_preset_name(int i);
void rnd_preset_load(rnd_engine *e, int i);

/* Zufall für die Pitch-/Velocity-Reihen (ARP-Knopf: RAND) */
void rnd_randomize(rnd_engine *e, uint32_t seed);

#ifdef __cplusplus
}
#endif
