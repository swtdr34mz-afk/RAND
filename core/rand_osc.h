// RAND – Oszillator (Dreieck / Puls mit PolyBLEP), FM-fest (auch negative Frequenz)
#pragma once
#include "rand_dsp.h"

namespace rnd {

struct Osc {
    float phase = 0.f;
    bool wrapped = false;   // true, wenn in diesem Sample ein Zyklus begann (für Sync)
    float wrapFrac = 0.f;   // Bruchteil des Samples nach dem Umbruch

    static inline float blep(float t, float dt) {
        if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
        if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }

    // inc = Frequenz / Samplerate (darf negativ sein); wave 0 = Dreieck, 1 = Puls
    inline float process(float inc, int wave, float pw = 0.5f) {
        wrapped = false;
        phase += inc;
        if (phase >= 1.f) {
            phase -= (float)(int)phase;
            wrapped = true;
            wrapFrac = (inc > 0.f) ? phase / inc : 0.f;
        } else if (phase < 0.f) {
            phase += 1.f + (float)(int)(-phase);
            if (phase >= 1.f) phase -= 1.f;
        }
        if (wave == 0) {
            return 4.f * std::fabs(phase - 0.5f) - 1.f;
        }
        float dt = clampf(std::fabs(inc), 1e-6f, 0.5f);
        float y = phase < pw ? 1.f : -1.f;
        y += blep(phase, dt);
        float t2 = phase - pw;
        if (t2 < 0.f) t2 += 1.f;
        y -= blep(t2, dt);
        return y;
    }

    // Hard Sync: Phase zurücksetzen (mit Bruchteil, damit es nicht ganz so hart aliasiert)
    inline void sync(float frac, float inc) {
        phase = clampf(frac * inc, 0.f, 0.999f);
    }
};

} // namespace rnd
