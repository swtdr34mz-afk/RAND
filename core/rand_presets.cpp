#include "rand_presets.h"

namespace rnd {

// Spickzettel für die normierten Werte:
//   VCO-Frequenz:  8 Hz * 2^(n*10)   -> 0.25 = 45 Hz, 0.38 = 110 Hz, 0.58 = 440 Hz, 0.80 = 2 kHz
//   Cutoff:       20 Hz * 2^(n*10)   -> 0.39 = 300 Hz, 0.56 = 1 kHz, 0.73 = 3.2 kHz
//   Decay:        4 ms * 1000^n      -> 0.23 = 20 ms, 0.37 = 50 ms, 0.47 = 100 ms, 0.63 = 300 ms
//   EG-/SEQ-Mengen: 0.5 = aus, darüber positiv, darunter negativ
//   Tempo: (BPM-40)/240              -> 0.367 = 128, 0.417 = 140, 0.542 = 170
//   Clock: Index/10                  -> 0.2 = x1, 0.3 = x2, 0.9 = x64

static const PresetParam kKick[] = {
    {P_VCO1_FREQ, 0.25f}, {P_VCO1_WAVE, 0.f}, {P_VCO1_EG_AMT, 0.80f}, {P_PITCH_DECAY, 0.33f},
    {P_VCO1_LVL, 1.f}, {P_CUTOFF, 0.72f}, {P_RESO, 0.15f}, {P_VCF_EG_AMT, 0.62f}, {P_VCF_DECAY, 0.40f},
    {P_VCA_DECAY, 0.62f}, {P_DRIVE, 0.55f}, {P_VEL_VCA, 0.7f}, {P_TEMPO, 0.367f}, {P_CLOCK_MULT, 0.2f},
};
static const PresetParam kEbm[] = {
    {P_VCO1_FREQ, 0.28f}, {P_VCO1_WAVE, 1.f}, {P_VCO2_FREQ, 0.283f}, {P_VCO2_WAVE, 1.f},
    {P_VCO1_LVL, 0.8f}, {P_VCO2_LVL, 0.6f}, {P_VCO1_SEQ_AMT, 0.75f}, {P_VCO2_SEQ_AMT, 0.75f},
    {P_VCO1_EG_AMT, 0.53f}, {P_VCO2_EG_AMT, 0.5f}, {P_CUTOFF, 0.40f}, {P_RESO, 0.55f},
    {P_VCF_EG_AMT, 0.78f}, {P_VCF_DECAY, 0.50f}, {P_VCA_DECAY, 0.52f}, {P_DRIVE, 0.6f},
    {P_VEL_VCF, 0.6f}, {P_TEMPO, 0.354f}, {P_CLOCK_MULT, 0.2f},
};
static const PresetParam kHats[] = {
    {P_VCO1_FREQ, 0.72f}, {P_VCO1_WAVE, 0.f}, {P_VCO2_FREQ, 0.83f}, {P_VCO2_WAVE, 1.f},
    {P_FM_AMT, 0.8f}, {P_VCO1_LVL, 0.f}, {P_VCO2_LVL, 0.7f}, {P_NOISE_LVL, 0.5f},
    {P_VCO1_EG_AMT, 0.5f}, {P_VCO2_EG_AMT, 0.5f}, {P_FILTER_MODE, 1.f}, {P_CUTOFF, 0.74f},
    {P_RESO, 0.3f}, {P_VCF_EG_AMT, 0.5f}, {P_VCA_DECAY, 0.30f}, {P_VEL_VCA, 0.9f},
    {P_DRIVE, 0.75f}, {P_VOLUME, 1.f}, {P_TEMPO, 0.367f}, {P_CLOCK_MULT, 0.3f},
};
static const PresetParam kTom[] = {
    {P_VCO1_FREQ, 0.32f}, {P_VCO1_WAVE, 0.f}, {P_VCO1_EG_AMT, 0.67f}, {P_PITCH_DECAY, 0.47f},
    {P_VCO1_SEQ_AMT, 0.75f}, {P_VCO1_LVL, 1.f}, {P_NOISE_LVL, 0.3f}, {P_NOISE_COLOR, 1.f},
    {P_NOISE_VCF, 0.4f}, {P_CUTOFF, 0.55f}, {P_RESO, 0.6f}, {P_VCF_EG_AMT, 0.7f},
    {P_VCF_DECAY, 0.45f}, {P_VCA_DECAY, 0.58f}, {P_DRIVE, 0.8f}, {P_TEMPO, 0.333f}, {P_CLOCK_MULT, 0.2f},
};
static const PresetParam kSnare[] = {
    {P_VCO1_FREQ, 0.49f}, {P_VCO1_WAVE, 0.f}, {P_VCO1_EG_AMT, 0.62f}, {P_PITCH_DECAY, 0.30f},
    {P_VCO1_LVL, 0.7f}, {P_NOISE_LVL, 0.9f}, {P_CUTOFF, 0.74f}, {P_RESO, 0.2f}, {P_VOLUME, 1.f},
    {P_VCF_EG_AMT, 0.65f}, {P_VCF_DECAY, 0.40f}, {P_VCA_DECAY, 0.50f}, {P_DRIVE, 0.6f},
    {P_TEMPO, 0.367f}, {P_CLOCK_MULT, 0.2f},
};
static const PresetParam kZap[] = {
    {P_VCO1_FREQ, 0.60f}, {P_VCO1_WAVE, 1.f}, {P_VCO1_EG_AMT, 0.92f}, {P_PITCH_DECAY, 0.50f},
    {P_VCO2_FREQ, 0.70f}, {P_VCO2_WAVE, 1.f}, {P_VCO2_EG_AMT, 0.75f}, {P_SYNC, 1.f},
    {P_VCO1_LVL, 0.f}, {P_VCO2_LVL, 0.8f}, {P_CUTOFF, 0.62f}, {P_RESO, 0.7f},
    {P_VCF_EG_AMT, 0.7f}, {P_VCF_DECAY, 0.45f}, {P_VCA_DECAY, 0.50f}, {P_DRIVE, 0.5f},
    {P_VCO2_SEQ_AMT, 0.8f}, {P_TEMPO, 0.375f}, {P_CLOCK_MULT, 0.2f},
};
static const PresetParam kAudio[] = {
    {P_VCO1_FREQ, 0.50f}, {P_VCO1_WAVE, 0.f}, {P_VCO1_EG_AMT, 0.6f}, {P_PITCH_DECAY, 0.15f},
    {P_VCO1_SEQ_AMT, 0.7f}, {P_VCO1_LVL, 0.9f}, {P_CUTOFF, 0.60f}, {P_RESO, 0.4f},
    {P_VCF_EG_AMT, 0.65f}, {P_VCF_DECAY, 0.12f}, {P_VCA_DECAY, 0.13f}, {P_DRIVE, 0.4f},
    {P_TEMPO, 0.333f}, {P_CLOCK_MULT, 0.9f},
};
static const PresetParam kBreak[] = {
    {P_VCO1_FREQ, 0.30f}, {P_VCO1_WAVE, 0.f}, {P_VCO1_EG_AMT, 0.78f}, {P_PITCH_DECAY, 0.36f},
    {P_VCO1_SEQ_AMT, 0.85f}, {P_VCO2_FREQ, 0.55f}, {P_VCO2_WAVE, 1.f}, {P_FM_AMT, 0.35f},
    {P_VCO1_LVL, 0.9f}, {P_VCO2_LVL, 0.2f}, {P_NOISE_LVL, 0.45f}, {P_CUTOFF, 0.66f},
    {P_RESO, 0.35f}, {P_VCF_EG_AMT, 0.68f}, {P_VCF_DECAY, 0.38f}, {P_VCA_DECAY, 0.48f},
    {P_DRIVE, 0.85f}, {P_TEMPO, 0.542f}, {P_CLOCK_MULT, 0.3f},
};

#define RND_N(a) (int)(sizeof(a) / sizeof(a[0]))

const Preset kPresets[] = {
    {"RAND KICK",  kKick,  RND_N(kKick),
        {.5f,.5f,.5f,.5f,.5f,.5f,.5f,.5f}, {1.f,.3f,.5f,.35f,1.f,.3f,.7f,.4f}, 0b11011101},
    {"EBM BASS",   kEbm,   RND_N(kEbm),
        {.5f,.5f,.75f,.5f,.5f,.625f,.5f,.875f}, {1.f,.6f,.8f,.5f,1.f,.6f,.7f,.9f}, 0b11111111},
    {"METAL HATS", kHats,  RND_N(kHats),
        {.5f,.5f,.5f,.5f,.5f,.5f,.5f,.5f}, {1.f,.3f,.6f,.3f,.9f,.3f,.6f,.5f}, 0b11111111},
    {"IRON TOMS",  kTom,   RND_N(kTom),
        {.75f,.5f,.62f,.4f,.5f,.75f,.3f,.55f}, {1.f,.5f,.8f,.6f,.9f,.4f,.7f,.8f}, 0b10110101},
    {"CRACK",      kSnare, RND_N(kSnare),
        {.5f,.5f,.5f,.5f,.5f,.5f,.5f,.55f}, {1.f,.5f,.5f,.5f,1.f,.5f,.5f,.35f}, 0b10010000},
    {"ZAP",        kZap,   RND_N(kZap),
        {.5f,.7f,.5f,.3f,.6f,.5f,.8f,.4f}, {1.f,.6f,.8f,.5f,1.f,.4f,.7f,.6f}, 0b01101011},
    {"AUDIO RATE", kAudio, RND_N(kAudio),
        {.5f,.9f,.2f,.7f,.4f,.8f,.1f,.6f}, {1.f,.4f,.8f,.2f,.9f,.5f,.3f,.7f}, 0b11111111},
    {"BREAKCORE",  kBreak, RND_N(kBreak),
        {.5f,.8f,.3f,.5f,.9f,.2f,.6f,.4f}, {1.f,.7f,.5f,.9f,.6f,1.f,.4f,.8f}, 0b11101011},
};
const int kPresetCount = RND_N(kPresets);

void loadPreset(Engine& e, int index) {
    if (index < 0 || index >= kPresetCount) return;
    const Preset& pr = kPresets[index];
    for (int i = 0; i < P_COUNT; ++i) e.setParam(i, kParamInfo[i].def);
    for (int i = 0; i < pr.paramCount; ++i) e.setParam(pr.params[i].id, pr.params[i].value);
    for (int s = 0; s < kSteps; ++s) {
        e.step(s).pitch = pr.pitch[s];
        e.step(s).vel = pr.vel[s];
        e.step(s).on = (pr.gates >> s) & 1;
    }
}

} // namespace rnd
