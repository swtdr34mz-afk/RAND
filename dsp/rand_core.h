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
    /* Ausgang und FX */
    RP_DRIVE, RP_VOLUME, RP_REVERB, RP_DELAY, RP_DELAY_TIME, RP_MFILTER,
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

/* --- die Patchbay: 11 Ausgänge (schwarze Tasten), 16 Eingänge (weiße Tasten), bis zu 8 Kabel --- */
enum { RS_VCO1, RS_VCO2, RS_NOISE, RS_PITCH_EG, RS_VCF_EG, RS_VCA_EG, RS_VEL, RS_PITCH, RS_LFO, RS_RANDOM,
       RS_TRIGGER, RND_NSRC };
enum { RD_VCO1, RD_VCO2, RD_FM, RD_LVL1, RD_LVL2, RD_NOISE, RD_CUTOFF, RD_RESO, RD_VCA, RD_DRIVE,
       /* ab hier einmal pro Block */
       RD_DECAY, RD_TEMPO, RD_DELAY, RD_REVERB, RD_MFILTER, RD_CRUSH, RND_NDST };
#define RD_BLOCK RD_DECAY
#define RND_NCABLES 8
extern const char *const RND_SRC_NAME[RND_NSRC];
extern const char *const RND_DST_NAME[RND_NDST];
typedef struct { uint8_t src, dst; float amt; } rnd_cable;   /* amt -1..1 */

/* --- interne Bausteine (hier, damit rnd_engine eine feste Größe hat; kein Heap) --- */
typedef struct { float phase, wrap_frac; int wrapped; } rnd_osc;
typedef struct { float s0, s1, s2, s3, t0, t1, t2, last, p3; } rnd_ladder;   /* Huovilainen */
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
    float hb[46];                        /* Halbband-Dezimierer (2x Oversampling), Ringpuffer doppelt */
    int hb_pos;
    float drift1, drift2, drift_amt;
    int eco;                             /* 1: ohne Oversampling (spart ~40 % Rechenzeit) */                /* langsame Verstimmung wie bei echten VCOs, in Oktaven */
    float nz_hold;
    float step_pitch, vel;
    /* Patchbay */
    rnd_cable cab[RND_NCABLES];
    int ncab;
    float o1v, o2v, nzv, trig, trig_coef, rnd, lfo_ph, lfo_inc;
    uint32_t rs;
    float dec_a0;
    float mod_blk[RND_NDST];             /* die Block-Ziele, vom Engine gelesen */
    /* umgerechnete Werte */
    float f1, f2, eg1, eg2, sq1, sq2, fm, l1, l2, ln, color;
    float cut, reso, egf, nf, vvca, vvcf, drive, vol;
    int w1, w2, sync, hp;
} rnd_voice;

/* FX: Reverb (je Kanal 4 Kämme + 2 Allpässe), Ping-Pong-Delay, Master-Filter (LP <- Mitte -> HP).
 * Den Speicher gibt der Host (FM-1: im großen RAM-Pool), ohne Speicher laufen die FX nicht. */
#define RND_REV_FLOATS 12500
#define RND_DLY_FRAMES 22050             /* 0.5 s bei 44.1 kHz */
#define RND_NDIV 6
extern const char *const RND_DIV_NAME[RND_NDIV];
typedef struct {
    float *comb[8], *ap[4];
    int comb_len[8], comb_pos[8], ap_len[4], ap_pos[4];
    float comb_f[8];
    int16_t *dl, *dr;
    int dpos;
    float dsm;                           /* Delayzeit, geglättet */
    float ml1, ml2, mr1, mr2;            /* Master-Filter */
    float crush_v, dlp_l, dlp_r;
    int crush_n;
    int ready;
} rnd_fx;

typedef struct {
    float sr;
    float p[RP_COUNT];
    rnd_voice v;
    rnd_fx fx;
    float mono[256];
    rnd_step steps[RND_STEPS];
    int running, cur;
    float to_next;          /* Samples bis zum nächsten Step */
    int clock_override;      /* -1: Knopf; sonst Index in RND_CLOCK_MULT (gehaltene Taste) */
    /* externe MIDI-Clock */
    uint32_t counter, last_clock;   /* Samples (last_clock: 0 = noch keine) */
    float ext_bpm;
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

void rnd_fx_memory(rnd_engine *e, float *rev, int16_t *dl, int16_t *dr);   /* RND_REV_FLOATS, 2x RND_DLY_FRAMES */
void rnd_render(rnd_engine *e, float *l, float *r, int n);   /* stereo, überschreibt l und r; n <= 256 */

float rnd_bpm(const rnd_engine *e);
int rnd_clock_index(const rnd_engine *e);
int rnd_length(const rnd_engine *e);
float rnd_level(const rnd_engine *e);    /* VCA-Hüllkurve jetzt (Anzeige) */

/* Presets */
#define RND_NPRESETS 8
const char *rnd_preset_name(int i);
void rnd_preset_load(rnd_engine *e, int i);

/* Patchbay */
int rnd_patch_toggle(rnd_engine *e, int src, int dst);    /* 1 gesteckt, 0 gezogen, -1 alle Plätze voll */
int rnd_patch_has(const rnd_engine *e, int src, int dst);
void rnd_patch_amount(rnd_engine *e, int src, float amt); /* alle Kabel dieses Ausgangs */
float rnd_patch_get_amount(const rnd_engine *e, int src);
void rnd_patch_clear(rnd_engine *e, int src);             /* -1: alle */

/* Zufall für die Pitch-/Velocity-Reihen (ARP-Knopf: RAND) */
void rnd_randomize(rnd_engine *e, uint32_t seed);

#ifdef __cplusplus
}
#endif
