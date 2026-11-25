/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (RAND) */
/* RAND – Messungen am Klangkern (für die Prüfung, nicht für die Firmware):
 *   measure tune   Selbstoszillation des Filters: gemessene Frequenz gegen eingestellten Cutoff
 *   measure alias  ein hoher Puls ohne Filter, roh als float32 auf stdout (für eine FFT)
 *   measure cpu    Rechenzeit pro Sekunde Audio (Preset für Preset)
 *   cc -O2 -Idsp dsp/rand_core.c tools/measure.c -lm -o measure */
#include "rand_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define SR 44100

static float REV[RND_REV_FLOATS];
static int16_t DL[RND_DLY_FRAMES], DR[RND_DLY_FRAMES];

static void dry(rnd_engine *e)
{
    rnd_init(e, SR);
    rnd_set(e, RP_REVERB, 0.0f);
    rnd_set(e, RP_DRIVE, 0.0f);
    rnd_set(e, RP_VCO1_EG, 0.5f);
    rnd_set(e, RP_VCF_EG, 0.5f);
    rnd_set(e, RP_VEL_VCF, 0.0f);
    rnd_set(e, RP_VCA_DECAY, 1.0f);
}

int main(int argc, char **argv)
{
    static rnd_engine e;
    static float l[SR * 2], r[SR * 2];
    const char *what = argc > 1 ? argv[1] : "tune";
    int i;
    if (!strcmp(what, "tune")) {
        static const float HZ[] = {110.f, 220.f, 440.f, 880.f, 1760.f, 3520.f, 7040.f};
        for (i = 0; i < 7; i++) {
            int j, zc = 0, first = -1, last = 0;
            float cut = log2f(HZ[i] / 20.0f) / 10.0f;
            dry(&e);
            rnd_set(&e, RP_VCO1_LVL, 0.0f);
            rnd_set(&e, RP_NOISE_LVL, 0.05f);
            rnd_set(&e, RP_CUTOFF, cut);
            rnd_set(&e, RP_RESO, 1.0f);
            e.v.drift_amt = 0.0f;
            e.v.eco = argc > 2;
            rnd_trigger(&e, 0.5f, 1.0f);
            for (j = 0; j < SR; j += 128) {
                if (j == 4096)                   /* nur ein kurzer Rausch-Anstoß, dann schwingt der Filter allein */
                    rnd_set(&e, RP_NOISE_LVL, 0.0f);
                rnd_render(&e, l + j, r + j, 128);
            }
            for (j = SR / 2; j < SR - 1; j++)    /* Nulldurchgänge in der zweiten halben Sekunde */
                if (l[j] <= 0.0f && l[j + 1] > 0.0f) {
                    if (first < 0)
                        first = j;
                    last = j;
                    zc++;
                }
            printf("cutoff %6.0f Hz  -> schwingt bei %7.1f Hz  (%+5.1f Cent)\n", HZ[i],
                   zc > 1 ? (float)(zc - 1) * SR / (float)(last - first) : 0.0f,
                   zc > 1 ? 1200.0f * log2f((float)(zc - 1) * SR / (float)(last - first) / HZ[i]) : 0.0f);
        }
    } else if (!strcmp(what, "alias")) {
        int j;
        dry(&e);
        rnd_set(&e, RP_VCO1_FREQ, log2f(2637.0f / 8.0f) / 10.0f);
        rnd_set(&e, RP_VCO1_WAVE, argc > 2 ? 0.0f : 1.0f);
        rnd_set(&e, RP_CUTOFF, 1.0f);
        rnd_set(&e, RP_RESO, 0.0f);
        rnd_set(&e, RP_VCO1_LVL, 0.5f);
        e.v.drift_amt = 0.0f;
        rnd_trigger(&e, 0.5f, 1.0f);
        for (j = 0; j < SR; j += 128)
            rnd_render(&e, l + j, r + j, 128);
        fwrite(l, sizeof(float), SR, stdout);
    } else if (!strcmp(what, "cpu")) {
        int p;
        for (p = 0; p < RND_NPRESETS; p++) {
            clock_t t0;
            int j, k;
            rnd_init(&e, SR);
            rnd_fx_memory(&e, REV, DL, DR);
            rnd_preset_load(&e, p);
            rnd_start(&e);
            t0 = clock();
            for (k = 0; k < 20; k++)
                for (j = 0; j < SR; j += 128)
                    rnd_render(&e, l, r, 128);
            printf("%-12s %.2f ms Rechenzeit pro Sekunde Audio\n", rnd_preset_name(p),
                   1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC / 20.0);
        }
    }
    return 0;
}
