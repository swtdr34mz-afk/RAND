// RAND – die eine Stimme: 2 VCOs, FM 1>2, Sync, Rauschen, Ladder LP/HP, 3 Hüllkurven, Drive
#pragma once
#include "rand_params.h"
#include "rand_osc.h"
#include "rand_filter.h"
#include "rand_env.h"

namespace rnd {

class Voice {
public:
    void init(float sampleRate);
    void setParam(int id, float norm);   // norm 0..1
    float getParam(int id) const { return p_[id]; }

    // stepPitch / velocity 0..1 (Pitch 0.5 = keine Verstimmung)
    void trigger(float stepPitch, float velocity);
    void process(float* out, int n);     // mono, überschreibt out

private:
    void update();                       // norm -> physikalische Werte

    float sr_ = 48000.f;
    float p_[P_COUNT] = {};
    bool dirty_ = true;

    Osc osc1_, osc2_;
    Ladder vcf_;
    Env egPitch_, egVcf_, egVca_;
    Noise noise_;
    DcBlock dc_;

    // aktueller Step
    float stepPitch_ = 0.f;   // -1..1
    float vel_ = 1.f;

    // umgerechnete Werte
    float f1Oct_ = 0, f2Oct_ = 0, eg1_ = 0, eg2_ = 0, seq1_ = 0, seq2_ = 0;
    float fm_ = 0, l1_ = 0, l2_ = 0, ln_ = 0, color_ = 0;
    float cutOct_ = 0, reso_ = 0, egF_ = 0, nF_ = 0, velVca_ = 0, velVcf_ = 0;
    float drive_ = 1, vol_ = 1;
    int w1_ = 0, w2_ = 1;
    bool sync_ = false, hp_ = false;
};

} // namespace rnd
