// RAND – kleine DSP-Helfer (plattformunabhängig, kein Heap, keine Abhängigkeiten)
#pragma once
#include <cstdint>
#include <cmath>

namespace rnd {

constexpr float kPi = 3.14159265358979f;

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

// Weiche Sättigung (rationale tanh-Näherung, günstig auf ARM)
inline float softClip(float x) {
    if (x < -3.f) return -1.f;
    if (x > 3.f) return 1.f;
    const float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// Schnelles 2^x (Genauigkeit ~0.1 Cent reicht für Drums dicke)
inline float fastExp2(float x) {
    x = clampf(x, -30.f, 30.f);
    const float fl = std::floor(x);
    const float f = x - fl;
    // Polynom für 2^f auf [0,1)
    const float p = 1.f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * 0.0096181f)));
    return std::ldexp(p, (int)fl);
}

// Rauschen: xorshift32 + Pink-Filter (Paul Kellet, sparsame Version)
struct Noise {
    uint32_t s = 0x1234567u;
    float b0 = 0, b1 = 0, b2 = 0;
    inline float white() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float)(int32_t)s * (1.f / 2147483648.f);
    }
    // color: 0 = weiß, 1 = pink
    inline float process(float color) {
        const float w = white();
        b0 = 0.99765f * b0 + w * 0.0990460f;
        b1 = 0.96300f * b1 + w * 0.2965164f;
        b2 = 0.57000f * b2 + w * 1.0526913f;
        const float pink = (b0 + b1 + b2 + w * 0.1848f) * 0.25f;
        return w + (pink - w) * color;
    }
};

// DC-Blocker am Ausgang
struct DcBlock {
    float x1 = 0, y1 = 0;
    inline float process(float x) {
        const float y = x - x1 + 0.995f * y1;
        x1 = x; y1 = y;
        return y;
    }
};

} // namespace rnd
