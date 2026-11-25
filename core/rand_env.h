// RAND – Attack/Decay-Hüllkurve, wird bei jedem Step neu ausgelöst.
// Retrigger startet vom aktuellen Pegel (kein Knacksen), auch bei Audio-Rate-Takt.
#pragma once
#include "rand_dsp.h"

namespace rnd {

struct Env {
    float level = 0.f;
    float atkInc = 1.f;   // pro Sample
    float decCoef = 0.999f;
    bool attacking = false;

    void setTimes(float attackSec, float decaySec, float sr) {
        atkInc = attackSec <= 0.f ? 1.f : 1.f / (attackSec * sr);
        // Zeitkonstante so gewählt, dass nach decaySec ca. -60 dB erreicht sind
        decCoef = std::exp(-6.9f / (clampf(decaySec, 0.0005f, 30.f) * sr));
    }
    inline void trigger() { attacking = true; }
    inline float process() {
        if (attacking) {
            level += atkInc;
            if (level >= 1.f) { level = 1.f; attacking = false; }
        } else {
            level *= decCoef;
        }
        return level;
    }
};

} // namespace rnd
