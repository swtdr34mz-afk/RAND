#include "rand_voice.h"

namespace rnd {

static inline float bip(float n) { return n * 2.f - 1.f; }           // 0..1 -> -1..1
static inline float decaySec(float n) { return 0.004f * std::pow(1000.f, n); } // 4 ms .. 4 s

void Voice::init(float sampleRate) {
    sr_ = sampleRate;
    vcf_.sr = sampleRate;
    for (int i = 0; i < P_COUNT; ++i) p_[i] = kParamInfo[i].def;
    dirty_ = true;
    update();
}

void Voice::setParam(int id, float norm) {
    if (id < 0 || id >= P_COUNT) return;
    p_[id] = clampf(norm, 0.f, 1.f);
    dirty_ = true;
}

void Voice::update() {
    // Frequenzen als Oktaven über 8 Hz: 0..1 -> 8 Hz .. 8 kHz (10 Oktaven, kein Oktavschalter nötig)
    f1Oct_ = p_[P_VCO1_FREQ] * 10.f;
    f2Oct_ = p_[P_VCO2_FREQ] * 10.f;
    w1_ = p_[P_VCO1_WAVE] >= 0.5f ? 1 : 0;
    w2_ = p_[P_VCO2_WAVE] >= 0.5f ? 1 : 0;
    eg1_ = bip(p_[P_VCO1_EG_AMT]) * 6.f;          // ±6 Oktaven Pitch-Hüllkurve
    eg2_ = bip(p_[P_VCO2_EG_AMT]) * 6.f;
    seq1_ = bip(p_[P_VCO1_SEQ_AMT]) * 2.f;        // ±2 Oktaven Sequencer-Pitch
    seq2_ = bip(p_[P_VCO2_SEQ_AMT]) * 2.f;
    fm_ = p_[P_FM_AMT] * p_[P_FM_AMT] * 6.f;      // FM-Index, quadratisch für feinen Anfang
    sync_ = p_[P_SYNC] >= 0.5f;
    l1_ = p_[P_VCO1_LVL];
    l2_ = p_[P_VCO2_LVL];
    ln_ = p_[P_NOISE_LVL];
    color_ = p_[P_NOISE_COLOR];
    cutOct_ = p_[P_CUTOFF] * 10.f;                // 20 Hz .. 20 kHz
    reso_ = p_[P_RESO];
    hp_ = p_[P_FILTER_MODE] >= 0.5f;
    egF_ = bip(p_[P_VCF_EG_AMT]) * 8.f;
    nF_ = p_[P_NOISE_VCF] * 4.f;
    velVca_ = p_[P_VEL_VCA];
    velVcf_ = p_[P_VEL_VCF] * 4.f;
    drive_ = 1.f + p_[P_DRIVE] * p_[P_DRIVE] * 14.f;
    vol_ = p_[P_VOLUME];

    egPitch_.setTimes(0.f, decaySec(p_[P_PITCH_DECAY]), sr_);
    egVcf_.setTimes(0.f, decaySec(p_[P_VCF_DECAY]), sr_);
    egVca_.setTimes(p_[P_VCA_ATTACK] >= 0.5f ? 0.012f : 0.0004f, decaySec(p_[P_VCA_DECAY]), sr_);
    dirty_ = false;
}

void Voice::trigger(float stepPitch, float velocity) {
    stepPitch_ = bip(clampf(stepPitch, 0.f, 1.f));
    vel_ = clampf(velocity, 0.f, 1.f);
    egPitch_.trigger();
    egVcf_.trigger();
    egVca_.trigger();
}

void Voice::process(float* out, int n) {
    if (dirty_) update();
    const float invSr = 1.f / sr_;
    // Velocity wirkt auf Lautstärke und Filter
    const float ampVel = 1.f - velVca_ + velVca_ * vel_;
    const float fVel = velVcf_ * (vel_ - 0.5f);
    // Die drive-Kompensation hält den Pegel einigermaßen konstant
    const float outGain = 1.5f * vol_ * (1.f / (0.6f + 0.4f * std::sqrt(drive_)));

    for (int i = 0; i < n; ++i) {
        const float ep = egPitch_.process();
        const float ef = egVcf_.process();
        const float ea = egVca_.process();

        const float hz1 = 8.f * fastExp2(f1Oct_ + eg1_ * ep + seq1_ * stepPitch_);
        const float hz2 = 8.f * fastExp2(f2Oct_ + eg2_ * ep + seq2_ * stepPitch_);
        const float inc1 = clampf(hz1 * invSr, 0.f, 0.45f);

        const float o1 = osc1_.process(inc1, w1_);
        // FM 1>2: linear, darf durch Null gehen -> metallisch/glockig
        const float inc2 = clampf(hz2 * invSr * (1.f + fm_ * o1), -0.45f, 0.45f);
        if (sync_ && osc1_.wrapped) osc2_.sync(osc1_.wrapFrac, inc2);
        const float o2 = osc2_.process(inc2, w2_);
        const float nz = noise_.process(color_);

        const float mix = (o1 * l1_ + o2 * l2_ + nz * ln_) * 0.6f;

        const float cutHz = 20.f * fastExp2(cutOct_ + egF_ * ef + nF_ * nz + fVel);
        float y = vcf_.process(mix, cutHz, reso_, hp_);
        y *= ea * ampVel;
        y = softClip(y * drive_);
        out[i] = softClip(dc_.process(y) * outGain);  // Sicherheits-Limiter
    }
}

} // namespace rnd
