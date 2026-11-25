/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (RAND) */
/* RAND – Offline-Renderer am PC: jedes Preset als WAV, dazu die Clock-Rampe (der EDGE-Trick).
 *   cc -O2 -Idsp dsp/rand_core.c tools/render_demo.c -lm -o render_demo && ./render_demo out/ */
#include "rand_core.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SR 44100
#define BLOCK 128

static void w32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void w16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

static void write_wav(const char *path, const float *d, int n)
{
    FILE *f = fopen(path, "wb");
    int i;
    if (!f) {
        perror(path);
        return;
    }
    fwrite("RIFF", 1, 4, f); w32(f, 36u + (uint32_t)n * 2u); fwrite("WAVEfmt ", 1, 8, f);
    w32(f, 16); w16(f, 1); w16(f, 1); w32(f, SR); w32(f, SR * 2); w16(f, 2); w16(f, 16);
    fwrite("data", 1, 4, f); w32(f, (uint32_t)n * 2u);
    for (i = 0; i < n; i++) {
        float c = d[i] < -1.f ? -1.f : d[i] > 1.f ? 1.f : d[i];
        int16_t s = (int16_t)(c * 32767.f);
        fwrite(&s, 2, 1, f);
    }
    fclose(f);
}

static void stats(const char *name, const float *d, int n)
{
    double peak = 0, sum = 0;
    int i, bad = 0;
    for (i = 0; i < n; i++) {
        if (!isfinite(d[i])) { bad++; continue; }
        if (fabs(d[i]) > peak) peak = fabs(d[i]);
        sum += d[i] * d[i];
    }
    printf("%-12s peak %5.2f  rms %6.1f dB  NaN %d\n", name, peak, 10.0 * log10(sum / n + 1e-12), bad);
}

static void run(rnd_engine *e, float *buf, int n, int ramp)
{
    int i;
    for (i = 0; i < n; i += BLOCK) {
        if (ramp) {
            float t = (float)i / (SR * 8.f);
            rnd_set(e, RP_CLOCK, 0.2f + 0.8f * (t > 1.f ? 1.f : t));
        }
        rnd_render(e, buf + i, n - i < BLOCK ? n - i : BLOCK);
    }
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    static rnd_engine e;
    int p, len = SR * 6, all_n = 0;
    float *buf = malloc(sizeof(float) * SR * 10), *all = malloc(sizeof(float) * SR * 60);
    char fn[512];
    for (p = 0; p < RND_NPRESETS; p++) {
        char nm[32];
        int k;
        rnd_init(&e, SR);
        rnd_preset_load(&e, p);
        rnd_start(&e);
        run(&e, buf, len, 0);
        stats(rnd_preset_name(p), buf, len);
        strcpy(nm, rnd_preset_name(p));
        for (k = 0; nm[k]; k++) if (nm[k] == ' ') nm[k] = '_';
        snprintf(fn, sizeof fn, "%s/%d_%s.wav", dir, p + 1, nm);
        write_wav(fn, buf, len);
        memcpy(all + all_n, buf, sizeof(float) * SR * 4); all_n += SR * 4;
        memset(all + all_n, 0, sizeof(float) * SR / 4); all_n += SR / 4;
    }
    rnd_init(&e, SR);
    rnd_preset_load(&e, 0);
    rnd_set(&e, RP_VCA_DECAY, 0.3f);
    rnd_start(&e);
    run(&e, buf, SR * 10, 1);
    stats("CLOCK RAMP", buf, SR * 10);
    snprintf(fn, sizeof fn, "%s/9_CLOCK_RAMP.wav", dir);
    write_wav(fn, buf, SR * 10);
    memcpy(all + all_n, buf, sizeof(float) * SR * 10); all_n += SR * 10;
    snprintf(fn, sizeof fn, "%s/RAND_demo_all.wav", dir);
    write_wav(fn, all, all_n);
    return 0;
}
