/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (RAND) */
/* RAND – die FX nach der Stimme: Ping-Pong-Delay im Tempo, Reverb, Master-Filter. Teil von rand_core.c */

static const int COMB_L[4] = {1116, 1188, 1277, 1356}, AP_L[2] = {556, 441};
#define SPREAD 23
static const float DIV_16TH[RND_NDIV] = {0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 6.0f};

void rnd_fx_memory(rnd_engine *e, float *rev, int16_t *dl, int16_t *dr)
{
    rnd_fx *f = &e->fx;
    int i, at = 0;
    if (!rev || !dl || !dr) {
        f->ready = 0;
        return;
    }
    for (i = 0; i < 8; i++) {
        f->comb_len[i] = COMB_L[i & 3] + (i >= 4 ? SPREAD : 0);
        f->comb[i] = rev + at;
        at += f->comb_len[i];
    }
    for (i = 0; i < 4; i++) {
        f->ap_len[i] = AP_L[i & 1] + (i >= 2 ? SPREAD : 0);
        f->ap[i] = rev + at;
        at += f->ap_len[i];
    }
    for (i = 0; i < at && i < RND_REV_FLOATS; i++)
        rev[i] = 0.0f;
    for (i = 0; i < RND_DLY_FRAMES; i++)
        dl[i] = dr[i] = 0;
    f->dl = dl;
    f->dr = dr;
    f->ready = 1;
}

static inline int16_t to16(float x)
{
    x *= 8192.0f;
    return (int16_t)(x > 32767.0f ? 32767.0f : x < -32767.0f ? -32767.0f : x);
}

static inline float comb_run(rnd_fx *f, int c, float x, float fb, float damp)
{
    float y = f->comb[c][f->comb_pos[c]];
    f->comb_f[c] = y * (1.0f - damp) + f->comb_f[c] * damp;
    f->comb[c][f->comb_pos[c]] = x + f->comb_f[c] * fb;
    if (++f->comb_pos[c] >= f->comb_len[c])
        f->comb_pos[c] = 0;
    return y;
}

static inline float ap_run(rnd_fx *f, int a, float x)
{
    float b = f->ap[a][f->ap_pos[a]], y = b - x;
    f->ap[a][f->ap_pos[a]] = x + b * 0.5f;
    if (++f->ap_pos[a] >= f->ap_len[a])
        f->ap_pos[a] = 0;
    return y;
}

static void fx_render(rnd_engine *e, const float *in, float *l, float *r, int n)
{
    rnd_fx *f = &e->fx;
    const float *mb = e->v.mod_blk;
    float rv = rnd_clamp(e->p[RP_REVERB] + mb[RD_REVERB], 0.0f, 1.0f);
    float dv = rnd_clamp(e->p[RP_DELAY] + mb[RD_DELAY], 0.0f, 1.0f);
    float mf = rnd_clamp(e->p[RP_MFILTER] + mb[RD_MFILTER] * 0.5f, 0.0f, 1.0f);
    float cr = rnd_clamp(rnd_abs(mb[RD_CRUSH]), 0.0f, 1.0f), q = 1.0f;
    int hold = 1 + (int)(cr * 15.0f);
    float wet = dv * 0.6f, fb = dv * 0.72f, rwet = rv * 0.5f, rfb = 0.72f + 0.25f * rv;
    float dtarget, gl = 0.0f, gh = 0.0f;
    int i, di = (int)(e->p[RP_DELAY_TIME] * (RND_NDIV - 1) + 0.5f), lp = 0, hp = 0;
    if (!f->ready) {
        for (i = 0; i < n; i++)
            l[i] = r[i] = in[i];
        return;
    }
    di = di < 0 ? 0 : di >= RND_NDIV ? RND_NDIV - 1 : di;
    dtarget = e->sr * 15.0f / rnd_bpm(e) * DIV_16TH[di];   /* eine Sechzehntel = sr * 60 / bpm / 4 */
    dtarget = rnd_clamp(dtarget, 16.0f, (float)(RND_DLY_FRAMES - 2));
    if (f->dsm <= 0.0f)
        f->dsm = dtarget;
    /* Master-Filter: unter 0.47 schließt ein Lowpass, über 0.53 öffnet ein Highpass (je 2 Pole) */
    if (mf < 0.47f) {
        float w = 2.0f * RND_PI * 20000.0f * rnd_exp2(-(0.47f - mf) / 0.47f * 9.0f) / e->sr;
        w = rnd_clamp(w, 0.001f, 2.0f);
        gl = w / (1.0f + w);
        lp = 1;
    } else if (mf > 0.53f) {
        float w = 2.0f * RND_PI * 20.0f * rnd_exp2((mf - 0.53f) / 0.47f * 9.0f) / e->sr;
        w = rnd_clamp(w, 0.001f, 2.0f);
        gh = w / (1.0f + w);
        hp = 1;
    }
    if (cr > 0.0f)
        q = rnd_exp2(11.0f - cr * 8.0f);  /* CRUSH: weniger Bits, weniger Samplerate */
    for (i = 0; i < n; i++) {
        float x = in[i], yl, yr, L, R, rp, fr;
        if (cr > 0.0f) {
            if (++f->crush_n >= hold) {
                f->crush_n = 0;
                f->crush_v = (float)(int32_t)(x * q) / q;
            }
            x = f->crush_v;
        }
        int p0, p1;
        /* Ping-Pong: links bekommt den Eingang und das rechte Echo, rechts das linke Echo */
        f->dsm += (dtarget - f->dsm) * 0.0005f;
        rp = (float)f->dpos - f->dsm;
        if (rp < 0.0f)
            rp += (float)RND_DLY_FRAMES;
        p0 = (int)rp;
        fr = rp - (float)p0;
        p1 = p0 + 1 >= RND_DLY_FRAMES ? 0 : p0 + 1;
        yl = ((float)f->dl[p0] + ((float)f->dl[p1] - (float)f->dl[p0]) * fr) * (1.0f / 8192.0f);
        yr = ((float)f->dr[p0] + ((float)f->dr[p1] - (float)f->dr[p0]) * fr) * (1.0f / 8192.0f);
        f->dl[f->dpos] = to16(dv > 0.0f ? x + yr * fb : 0.0f);
        f->dr[f->dpos] = to16(yl * fb);
        if (++f->dpos >= RND_DLY_FRAMES)
            f->dpos = 0;
        L = x + yl * wet;
        R = x + yr * wet;
        if (rv > 0.001f) {
            float ri = (x + (yl + yr) * 0.5f * wet) * 0.04f, sl = 0.0f, sr = 0.0f;
            int c;
            for (c = 0; c < 4; c++) {
                sl += comb_run(f, c, ri, rfb, 0.3f);
                sr += comb_run(f, c + 4, ri, rfb, 0.3f);
            }
            sl = ap_run(f, 1, ap_run(f, 0, sl));
            sr = ap_run(f, 3, ap_run(f, 2, sr));
            L += sl * rwet;
            R += sr * rwet;
        }
        if (lp) {
            f->ml1 += gl * (L - f->ml1);
            f->ml2 += gl * (f->ml1 - f->ml2);
            f->mr1 += gl * (R - f->mr1);
            f->mr2 += gl * (f->mr1 - f->mr2);
            L = f->ml2;
            R = f->mr2;
        } else if (hp) {                 /* HP: das Signal minus seinem Lowpass, zweimal */
            float a;
            f->ml1 += gh * (L - f->ml1);
            a = L - f->ml1;
            f->ml2 += gh * (a - f->ml2);
            L = a - f->ml2;
            f->mr1 += gh * (R - f->mr1);
            a = R - f->mr1;
            f->mr2 += gh * (a - f->mr2);
            R = a - f->mr2;
        }
        l[i] = rnd_soft(L);
        r[i] = rnd_soft(R);
    }
}
