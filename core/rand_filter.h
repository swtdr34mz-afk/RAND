// RAND – 24-dB-Ladder-Filter (Lowpass / Highpass) mit Sättigung in der Rückkopplung
#pragma once
#include "rand_dsp.h"

namespace rnd {

struct Ladder {
    float y1 = 0, y2 = 0, y3 = 0, y4 = 0;
    float sr = 48000.f;

    void reset() { y1 = y2 = y3 = y4 = 0; }

    // cutoffHz, reso 0..1 (ab ~0.9 Selbstoszillation), hp = Highpass-Modus
    inline float process(float x, float cutoffHz, float reso, bool hp) {
        const float fc = clampf(cutoffHz, 15.f, sr * 0.45f);
        // Ein-Pol-Koeffizient (bilinear-nah, günstig)
        const float wc = 2.f * kPi * fc / sr;
        const float g = wc / (1.f + wc);
        const float k = reso * 4.2f;
        const float in = softClip((x - k * y4) * (1.f + reso * 0.6f));
        y1 += g * (in - y1);
        y2 += g * (y1 - y2);
        y3 += g * (y2 - y3);
        y4 += g * (y3 - y4);
        y4 = clampf(y4, -4.f, 4.f);
        if (!hp) return y4;
        // Moog-HP-Kombination aus den Stufen
        return in - 4.f * y1 + 6.f * y2 - 4.f * y3 + y4;
    }
};

} // namespace rnd
