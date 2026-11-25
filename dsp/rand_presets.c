/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (RAND) */
/* RAND – die Werks-Presets: Klang + 8-Step-Pattern. Teil von rand_core.c
 *
 * Spickzettel für die Werte (alle 0..1):
 *   VCO-Frequenz  8 Hz * 2^(n*10)    0.25 = 45 Hz, 0.38 = 110 Hz, 0.58 = 440 Hz, 0.80 = 2 kHz
 *   Cutoff       20 Hz * 2^(n*10)    0.39 = 300 Hz, 0.56 = 1 kHz, 0.73 = 3.2 kHz
 *   Decay         4 ms * 1000^n      0.23 = 20 ms, 0.37 = 50 ms, 0.47 = 100 ms, 0.63 = 300 ms
 *   EG / SEQ      0.5 = aus, darüber positiv, darunter negativ
 *   Tempo         (BPM - 40) / 240   0.367 = 128, 0.417 = 140, 0.542 = 170
 *   Clock         Index / 10         0.2 = x1, 0.3 = x2, 0.9 = x64 */

typedef struct { uint8_t id; float v; } rnd_pv;
typedef struct {
    const char *name;
    const rnd_pv *pv;
    int npv;
    float pitch[RND_STEPS], vel[RND_STEPS];
    uint8_t gates;            /* Bit 0 = Step 1 */
    rnd_cable cab[2];         /* gesteckte Kabel (amt 0: keins) */
} rnd_preset;

static const rnd_pv PV_KICK[] = {
    {RP_REVERB, 0.1f},
    {RP_VCO1_FREQ, 0.25f}, {RP_VCO1_WAVE, 0.f}, {RP_VCO1_EG, 0.80f}, {RP_PITCH_DECAY, 0.33f},
    {RP_VCO1_LVL, 1.f}, {RP_CUTOFF, 0.72f}, {RP_RESO, 0.15f}, {RP_VCF_EG, 0.62f}, {RP_VCF_DECAY, 0.40f},
    {RP_VCA_DECAY, 0.62f}, {RP_DRIVE, 0.55f}, {RP_VEL_VCA, 0.7f}, {RP_TEMPO, 0.367f}, {RP_CLOCK, 0.2f},
};
static const rnd_pv PV_EBM[] = {
    {RP_DELAY, 0.3f}, {RP_DELAY_TIME, 0.4f}, {RP_REVERB, 0.1f},
    {RP_VCO1_FREQ, 0.28f}, {RP_VCO1_WAVE, 1.f}, {RP_VCO2_FREQ, 0.283f}, {RP_VCO2_WAVE, 1.f},
    {RP_VCO1_LVL, 0.8f}, {RP_VCO2_LVL, 0.6f}, {RP_VCO1_SEQ, 0.75f}, {RP_VCO2_SEQ, 0.75f},
    {RP_VCO1_EG, 0.53f}, {RP_VCO2_EG, 0.5f}, {RP_CUTOFF, 0.40f}, {RP_RESO, 0.55f},
    {RP_VCF_EG, 0.78f}, {RP_VCF_DECAY, 0.50f}, {RP_VCA_DECAY, 0.52f}, {RP_DRIVE, 0.6f},
    {RP_VEL_VCF, 0.6f}, {RP_TEMPO, 0.354f}, {RP_CLOCK, 0.2f},
};
static const rnd_pv PV_HATS[] = {
    {RP_REVERB, 0.2f},
    {RP_VCO1_FREQ, 0.72f}, {RP_VCO1_WAVE, 0.f}, {RP_VCO2_FREQ, 0.83f}, {RP_VCO2_WAVE, 1.f},
    {RP_FM, 0.8f}, {RP_VCO1_LVL, 0.f}, {RP_VCO2_LVL, 0.7f}, {RP_NOISE_LVL, 0.5f},
    {RP_VCO1_EG, 0.5f}, {RP_VCO2_EG, 0.5f}, {RP_FILTER_MODE, 1.f}, {RP_CUTOFF, 0.74f},
    {RP_RESO, 0.3f}, {RP_VCF_EG, 0.5f}, {RP_VCA_DECAY, 0.30f}, {RP_VEL_VCA, 0.9f},
    {RP_DRIVE, 0.75f}, {RP_VOLUME, 1.f}, {RP_TEMPO, 0.367f}, {RP_CLOCK, 0.3f},
};
static const rnd_pv PV_TOMS[] = {
    {RP_REVERB, 0.3f}, {RP_DELAY, 0.2f}, {RP_DELAY_TIME, 0.6f},
    {RP_VCO1_FREQ, 0.32f}, {RP_VCO1_WAVE, 0.f}, {RP_VCO1_EG, 0.67f}, {RP_PITCH_DECAY, 0.47f},
    {RP_VCO1_SEQ, 0.75f}, {RP_VCO1_LVL, 1.f}, {RP_NOISE_LVL, 0.3f}, {RP_NOISE_COLOR, 1.f},
    {RP_NOISE_VCF, 0.4f}, {RP_CUTOFF, 0.55f}, {RP_RESO, 0.6f}, {RP_VCF_EG, 0.7f},
    {RP_VCF_DECAY, 0.45f}, {RP_VCA_DECAY, 0.58f}, {RP_DRIVE, 0.8f}, {RP_TEMPO, 0.333f}, {RP_CLOCK, 0.2f},
};
static const rnd_pv PV_CRACK[] = {
    {RP_REVERB, 0.35f},
    {RP_VCO1_FREQ, 0.49f}, {RP_VCO1_WAVE, 0.f}, {RP_VCO1_EG, 0.62f}, {RP_PITCH_DECAY, 0.30f},
    {RP_VCO1_LVL, 0.7f}, {RP_NOISE_LVL, 0.9f}, {RP_CUTOFF, 0.74f}, {RP_RESO, 0.2f}, {RP_VOLUME, 1.f},
    {RP_VCF_EG, 0.65f}, {RP_VCF_DECAY, 0.40f}, {RP_VCA_DECAY, 0.50f}, {RP_DRIVE, 0.6f},
    {RP_TEMPO, 0.367f}, {RP_CLOCK, 0.2f},
};
static const rnd_pv PV_ZAP[] = {
    {RP_DELAY, 0.45f}, {RP_DELAY_TIME, 0.6f}, {RP_REVERB, 0.15f},
    {RP_VCO1_FREQ, 0.60f}, {RP_VCO1_WAVE, 1.f}, {RP_VCO1_EG, 0.92f}, {RP_PITCH_DECAY, 0.50f},
    {RP_VCO2_FREQ, 0.70f}, {RP_VCO2_WAVE, 1.f}, {RP_VCO2_EG, 0.75f}, {RP_SYNC, 1.f},
    {RP_VCO1_LVL, 0.f}, {RP_VCO2_LVL, 0.8f}, {RP_CUTOFF, 0.62f}, {RP_RESO, 0.7f},
    {RP_VCF_EG, 0.7f}, {RP_VCF_DECAY, 0.45f}, {RP_VCA_DECAY, 0.50f}, {RP_DRIVE, 0.5f},
    {RP_VCO2_SEQ, 0.8f}, {RP_TEMPO, 0.375f}, {RP_CLOCK, 0.2f},
};
static const rnd_pv PV_AUDIO[] = {
    {RP_REVERB, 0.2f},
    {RP_VCO1_FREQ, 0.50f}, {RP_VCO1_WAVE, 0.f}, {RP_VCO1_EG, 0.6f}, {RP_PITCH_DECAY, 0.15f},
    {RP_VCO1_SEQ, 0.7f}, {RP_VCO1_LVL, 0.9f}, {RP_CUTOFF, 0.60f}, {RP_RESO, 0.4f},
    {RP_VCF_EG, 0.65f}, {RP_VCF_DECAY, 0.12f}, {RP_VCA_DECAY, 0.13f}, {RP_DRIVE, 0.4f},
    {RP_TEMPO, 0.333f}, {RP_CLOCK, 0.9f},
};
static const rnd_pv PV_BREAK[] = {
    {RP_REVERB, 0.12f},
    {RP_VCO1_FREQ, 0.30f}, {RP_VCO1_WAVE, 0.f}, {RP_VCO1_EG, 0.78f}, {RP_PITCH_DECAY, 0.36f},
    {RP_VCO1_SEQ, 0.85f}, {RP_VCO2_FREQ, 0.55f}, {RP_VCO2_WAVE, 1.f}, {RP_FM, 0.35f},
    {RP_VCO1_LVL, 0.9f}, {RP_VCO2_LVL, 0.2f}, {RP_NOISE_LVL, 0.45f}, {RP_CUTOFF, 0.66f},
    {RP_RESO, 0.35f}, {RP_VCF_EG, 0.68f}, {RP_VCF_DECAY, 0.38f}, {RP_VCA_DECAY, 0.48f},
    {RP_DRIVE, 0.85f}, {RP_TEMPO, 0.542f}, {RP_CLOCK, 0.3f},
};

#define NPV(a) (int)(sizeof(a) / sizeof(a[0]))
static const rnd_preset PRESETS[RND_NPRESETS] = {
    {"RAND KICK", PV_KICK, NPV(PV_KICK),
     {.5f, .5f, .5f, .5f, .5f, .5f, .5f, .5f}, {1.f, .3f, .5f, .35f, 1.f, .3f, .7f, .4f}, 0xDD, {{0, 0, 0.f}, {0, 0, 0.f}}},
    {"EBM BASS", PV_EBM, NPV(PV_EBM),
     {.5f, .5f, .75f, .5f, .5f, .625f, .5f, .875f}, {1.f, .6f, .8f, .5f, 1.f, .6f, .7f, .9f}, 0xFF, {{RS_LFO, RD_CUTOFF, 0.4f}, {0, 0, 0.f}}},
    {"METAL HATS", PV_HATS, NPV(PV_HATS),
     {.5f, .5f, .5f, .5f, .5f, .5f, .5f, .5f}, {1.f, .3f, .6f, .3f, .9f, .3f, .6f, .5f}, 0xFF, {{RS_RANDOM, RD_CUTOFF, 0.3f}, {0, 0, 0.f}}},
    {"IRON TOMS", PV_TOMS, NPV(PV_TOMS),
     {.75f, .5f, .62f, .4f, .5f, .75f, .3f, .55f}, {1.f, .5f, .8f, .6f, .9f, .4f, .7f, .8f}, 0xB5, {{0, 0, 0.f}, {0, 0, 0.f}}},
    {"CRACK", PV_CRACK, NPV(PV_CRACK),
     {.5f, .5f, .5f, .5f, .5f, .5f, .5f, .55f}, {1.f, .5f, .5f, .5f, 1.f, .5f, .5f, .35f}, 0x90, {{0, 0, 0.f}, {0, 0, 0.f}}},
    {"ZAP", PV_ZAP, NPV(PV_ZAP),
     {.5f, .7f, .5f, .3f, .6f, .5f, .8f, .4f}, {1.f, .6f, .8f, .5f, 1.f, .4f, .7f, .6f}, 0x6B, {{RS_RANDOM, RD_VCO2, 0.3f}, {0, 0, 0.f}}},
    {"AUDIO RATE", PV_AUDIO, NPV(PV_AUDIO),
     {.5f, .9f, .2f, .7f, .4f, .8f, .1f, .6f}, {1.f, .4f, .8f, .2f, .9f, .5f, .3f, .7f}, 0xFF, {{RS_LFO, RD_TEMPO, 0.5f}, {0, 0, 0.f}}},
    {"BREAKCORE", PV_BREAK, NPV(PV_BREAK),
     {.5f, .8f, .3f, .5f, .9f, .2f, .6f, .4f}, {1.f, .7f, .5f, .9f, .6f, 1.f, .4f, .8f}, 0xEB, {{RS_RANDOM, RD_TEMPO, 0.5f}, {RS_RANDOM, RD_CRUSH, 0.6f}}},
};

const char *rnd_preset_name(int i) { return i >= 0 && i < RND_NPRESETS ? PRESETS[i].name : ""; }

void rnd_preset_load(rnd_engine *e, int idx)
{
    const rnd_preset *pr;
    int i;
    if (idx < 0 || idx >= RND_NPRESETS)
        return;
    pr = &PRESETS[idx];
    for (i = 0; i < RP_COUNT; i++)
        rnd_set(e, i, RND_PARAMS[i].def);
    for (i = 0; i < pr->npv; i++)
        rnd_set(e, pr->pv[i].id, pr->pv[i].v);
    rnd_patch_clear(e, -1);
    for (i = 0; i < 2; i++)
        if (pr->cab[i].amt != 0.0f) {
            rnd_patch_toggle(e, pr->cab[i].src, pr->cab[i].dst);
            rnd_patch_amount(e, pr->cab[i].src, pr->cab[i].amt);
        }
    for (i = 0; i < RND_STEPS; i++) {
        e->steps[i].pitch = pr->pitch[i];
        e->steps[i].vel = ((pr->gates >> i) & 1u) ? pr->vel[i] : 0.0f;   /* aus = Velocity 0, wie am DFAM */
        e->steps[i].on = 1;
    }
}
