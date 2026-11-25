/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (RAND) */
/* RAND – prüft jeden Regler in jedem Preset: wie stark ändert er den Klang?
 * Für jeden Parameter: zwei Sekunden mit dem Regler auf 0, auf 0.25, 0.5, 0.75 und 1 (sonst alles gleich,
 * gleiches Rauschen, keine Drift). Ausgegeben wird der Unterschied zum Preset in dB (Energie der Differenz
 * relativ zum Preset); -40 dB und weniger hört man praktisch nicht.
 *   cc -O2 -Idsp dsp/rand_core.c tools/knobcheck.c -lm -o knobcheck && ./knobcheck */
#include "rand_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define SR 44100
#define LEN (SR * 2)

static float REV[RND_REV_FLOATS];
static int16_t DL[RND_DLY_FRAMES], DR[RND_DLY_FRAMES];
static float ref_l[LEN], ref_r[LEN], l[LEN], r[LEN];

static void render(int preset, int id, float v, float *ol, float *or_)
{
    static rnd_engine e;
    int j;
    rnd_init(&e, SR);
    rnd_fx_memory(&e, REV, DL, DR);
    rnd_preset_load(&e, preset);
    e.v.drift_amt = 0.0f;
    if (id >= 0)
        rnd_set(&e, id, v);
    rnd_start(&e);
    for (j = 0; j < LEN; j += 128)
        rnd_render(&e, ol + j, or_ + j, LEN - j < 128 ? LEN - j : 128);
}

static float diff_db(const float *a, const float *b, const float *c, const float *d)
{
    double s = 0, n = 0;
    int j;
    for (j = 0; j < LEN; j++) {
        s += (a[j] - c[j]) * (a[j] - c[j]) + (b[j] - d[j]) * (b[j] - d[j]);
        n += a[j] * a[j] + b[j] * b[j];
    }
    return (float)(10.0 * log10(s / (n + 1e-20) + 1e-12));
}

int main(void)
{
    static float pl[5][LEN], pr[5][LEN];
    static const float V[5] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    int id, p, k;
    printf("%-10s", "");
    for (p = 0; p < RND_NPRESETS; p++)
        printf(" %10.10s", rnd_preset_name(p));
    printf("   (0 gegen 1, dB)   tote Strecke\n");
    for (id = 0; id < RP_COUNT; id++) {
        float worst_step = 0.0f;
        int worst_at = -1;
        if (id == RP_TEMPO)
            continue;
        printf("%-10s", RND_PARAMS[id].label);
        for (p = 0; p < RND_NPRESETS; p++) {
            for (k = 0; k < 5; k++)
                render(p, id, V[k], pl[k], pr[k]);
            printf(" %10.1f", diff_db(pl[0], pr[0], pl[4], pr[4]));
            if (RND_PARAMS[id].kind != RK_SWITCH)
                for (k = 0; k < 4; k++) {   /* der leiseste Schritt im Regelweg, über alle Presets */
                    float d = diff_db(pl[k], pr[k], pl[k + 1], pr[k + 1]);
                    if (worst_at < 0 || d < worst_step) {
                        worst_step = d;
                        worst_at = k;
                    }
                }
        }
        if (worst_at >= 0)
            printf("   %.2f-%.2f: %.1f", V[worst_at], V[worst_at + 1], worst_step);
        printf("\n");
        fflush(stdout);
    }
    (void)ref_l; (void)ref_r; (void)l; (void)r;
    return 0;
}
