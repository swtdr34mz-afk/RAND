// RAND – Offline-Renderer: schreibt jedes Preset als WAV + eine Demo der Clock-Rampe.
// Aufruf: render_demo <ausgabeordner>
#include "../core/rand_presets.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace rnd;

static const int SR = 48000;

static void writeWav(const std::string& path, const std::vector<float>& d) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { std::perror(path.c_str()); return; }
    const uint32_t n = (uint32_t)d.size(), bytes = n * 2;
    auto w32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); w32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(1); w16(1); w32(SR); w32(SR * 2); w16(2); w16(16);
    std::fwrite("data", 1, 4, f); w32(bytes);
    for (float x : d) {
        float c = x < -1.f ? -1.f : (x > 1.f ? 1.f : x);
        int16_t s = (int16_t)(c * 32767.f);
        std::fwrite(&s, 2, 1, f);
    }
    std::fclose(f);
}

static void stats(const char* name, const std::vector<float>& d) {
    double peak = 0, sum = 0; int bad = 0;
    for (float x : d) {
        if (!std::isfinite(x)) { ++bad; continue; }
        double a = std::fabs(x); if (a > peak) peak = a; sum += x * x;
    }
    double rms = std::sqrt(sum / d.size());
    std::printf("%-12s peak %5.2f  rms %5.3f (%6.1f dB)  NaN/Inf %d\n", name, peak, rms,
                20.0 * std::log10(rms + 1e-9), bad);
}

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : ".";
    const int block = 64;   // typische FM-1-Blockgröße
    std::vector<float> all;

    for (int p = 0; p < kPresetCount; ++p) {
        Engine e; e.init((float)SR);
        loadPreset(e, p);
        e.start();
        const int len = SR * 6;
        std::vector<float> buf(len);
        for (int i = 0; i < len; i += block) e.process(buf.data() + i, std::min(block, len - i));
        stats(kPresets[p].name, buf);
        std::string fn = dir + "/" + std::to_string(p + 1) + "_" + kPresets[p].name + ".wav";
        for (auto& c : fn) if (c == ' ') c = '_';
        writeWav(fn, buf);
        all.insert(all.end(), buf.begin(), buf.begin() + SR * 4);
        all.insert(all.end(), SR / 4, 0.f);
    }

    // Der EDGE-Trick: Kick-Preset, Clock dreht in 8 s von x1 bis x128 hoch
    {
        Engine e; e.init((float)SR);
        loadPreset(e, 0);
        e.setParam(P_VCA_DECAY, 0.3f);
        e.start();
        const int len = SR * 10;
        std::vector<float> buf(len);
        for (int i = 0; i < len; i += block) {
            float t = (float)i / (SR * 8.f);
            e.setParam(P_CLOCK_MULT, 0.2f + 0.8f * (t > 1.f ? 1.f : t));
            e.process(buf.data() + i, std::min(block, len - i));
        }
        stats("CLOCK RAMP", buf);
        writeWav(dir + "/9_CLOCK_RAMP.wav", buf);
        all.insert(all.end(), buf.begin(), buf.end());
    }
    writeWav(dir + "/RAND_demo_all.wav", all);
    return 0;
}
