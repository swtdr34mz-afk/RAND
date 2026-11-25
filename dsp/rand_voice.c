/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (RAND) */
/* RAND – die Stimme. Teil von rand_core.c */

/* ---- Oszillator: Dreieck / Puls (PolyBLEP), FM-fest (auch rückwärts) ---- */
static inline float blep(float t, float dt)
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

static inline float osc_run(rnd_osc *o, float inc, int pulse)
{
    float y, dt, t2;
    o->wrapped = 0;
    o->phase += inc;
    if (o->phase >= 1.0f) {
        o->phase -= (float)(int)o->phase;
        o->wrapped = 1;
        o->wrap_frac = inc > 0.0f ? o->phase / inc : 0.0f;
    } else if (o->phase < 0.0f) {
        o->phase += 1.0f + (float)(int)(-o->phase);
        if (o->phase >= 1.0f)
            o->phase -= 1.0f;
    }
    if (!pulse)
        return 4.0f * rnd_abs(o->phase - 0.5f) - 1.0f;
    dt = rnd_clamp(rnd_abs(inc), 1e-6f, 0.5f);
    y = o->phase < 0.5f ? 1.0f : -1.0f;
    y += blep(o->phase, dt);
    t2 = o->phase - 0.5f;
    if (t2 < 0.0f)
        t2 += 1.0f;
    return y - blep(t2, dt);
}

/* ---- 24-dB-Ladder, Sättigung in der Rückkopplung; HP aus den Stufen gemischt ---- */
static inline float ladder_run(rnd_ladder *f, float x, float g, float reso, int hp)
{
    float k = reso * 4.2f;
    float in = rnd_soft((x - k * f->y4) * (1.0f + reso * 0.6f));
    f->y1 += g * (in - f->y1);
    f->y2 += g * (f->y1 - f->y2);
    f->y3 += g * (f->y2 - f->y3);
    f->y4 += g * (f->y3 - f->y4);
    f->y4 = rnd_clamp(f->y4, -4.0f, 4.0f);
    if (!hp)
        return f->y4;
    return in - 4.0f * f->y1 + 6.0f * f->y2 - 4.0f * f->y3 + f->y4;
}

/* ---- Attack/Decay-Hüllkurve, Neustart vom aktuellen Pegel (kein Knacksen, auch im Audio-Takt) ---- */
static void env_times(rnd_env *e, float atk, float dec, float sr)
{
    e->atk_inc = atk <= 0.0f ? 1.0f : 1.0f / (atk * sr);
    e->dec_coef = rnd_expf(-6.9f / (rnd_clamp(dec, 0.0005f, 30.0f) * sr));   /* nach dec: -60 dB */
}
static inline float env_run(rnd_env *e)
{
    if (e->attacking) {
        e->level += e->atk_inc;
        if (e->level >= 1.0f) {
            e->level = 1.0f;
            e->attacking = 0;
        }
    } else {
        e->level *= e->dec_coef;
    }
    return e->level;
}

/* ---- Rauschen: xorshift + Pink-Filter, color 0 weiß .. 1 pink ---- */
static inline float noise_run(rnd_noise *n, float color)
{
    float w, pink;
    n->s ^= n->s << 13;
    n->s ^= n->s >> 17;
    n->s ^= n->s << 5;
    w = (float)(int32_t)n->s * (1.0f / 2147483648.0f);
    n->b0 = 0.99765f * n->b0 + w * 0.0990460f;
    n->b1 = 0.96300f * n->b1 + w * 0.2965164f;
    n->b2 = 0.57000f * n->b2 + w * 1.0526913f;
    pink = (n->b0 + n->b1 + n->b2 + w * 0.1848f) * 0.25f;
    return w + (pink - w) * color;
}

/* ---- die Stimme ---- */
static float bip(float n) { return n * 2.0f - 1.0f; }
static float decay_sec(float n) { return 0.004f * rnd_exp2(n * 9.9658f); }   /* 4 ms .. 4 s */

static void voice_init(rnd_voice *v, float sr)
{
    int i;
    char *z = (char *)v;
    for (i = 0; i < (int)sizeof *v; i++)
        z[i] = 0;
    v->sr = sr;
    v->nz.s = 0x2545F491u;
    v->rs = 0x6C8E9CF5u;
    v->trig_coef = rnd_expf(-6.9f / (0.015f * sr));    /* TRIGGER: 15 ms Puls */
    v->vel = 1.0f;
    for (i = 0; i < RP_COUNT; i++)
        v->p[i] = RND_PARAMS[i].def;
    v->dirty = 1;
}

static void voice_update(rnd_voice *v)
{
    const float *p = v->p;
    v->f1 = p[RP_VCO1_FREQ] * 10.0f;              /* Oktaven über 8 Hz: 8 Hz .. 8 kHz, ohne Oktavschalter */
    v->f2 = p[RP_VCO2_FREQ] * 10.0f;
    v->w1 = p[RP_VCO1_WAVE] >= 0.5f;
    v->w2 = p[RP_VCO2_WAVE] >= 0.5f;
    v->eg1 = bip(p[RP_VCO1_EG]) * 6.0f;           /* ±6 Oktaven Pitch-Hüllkurve */
    v->eg2 = bip(p[RP_VCO2_EG]) * 6.0f;
    v->sq1 = bip(p[RP_VCO1_SEQ]) * 2.0f;          /* ±2 Oktaven aus der Pitch-Reihe */
    v->sq2 = bip(p[RP_VCO2_SEQ]) * 2.0f;
    v->fm = p[RP_FM] * p[RP_FM] * 6.0f;           /* FM-Index, quadratisch: fein am Anfang */
    v->sync = p[RP_SYNC] >= 0.5f;
    v->l1 = p[RP_VCO1_LVL];
    v->l2 = p[RP_VCO2_LVL];
    v->ln = p[RP_NOISE_LVL];
    v->color = p[RP_NOISE_COLOR];
    v->cut = p[RP_CUTOFF] * 10.0f;                /* 20 Hz .. 20 kHz */
    v->reso = p[RP_RESO];
    v->hp = p[RP_FILTER_MODE] >= 0.5f;
    v->egf = bip(p[RP_VCF_EG]) * 8.0f;
    v->nf = p[RP_NOISE_VCF] * 4.0f;
    v->vvca = p[RP_VEL_VCA];
    v->vvcf = p[RP_VEL_VCF] * 4.0f;
    v->drive = 1.0f + p[RP_DRIVE] * p[RP_DRIVE] * 14.0f;
    v->vol = p[RP_VOLUME];
    env_times(&v->eg_p, 0.0f, decay_sec(p[RP_PITCH_DECAY]), v->sr);
    env_times(&v->eg_f, 0.0f, decay_sec(p[RP_VCF_DECAY]), v->sr);
    env_times(&v->eg_a, p[RP_VCA_ATTACK] >= 0.5f ? 0.012f : 0.0004f, decay_sec(p[RP_VCA_DECAY]), v->sr);
    v->dec_a0 = v->eg_a.dec_coef;
    v->dirty = 0;
}

static void voice_trigger(rnd_voice *v, float pitch, float vel)
{
    v->step_pitch = bip(rnd_clamp(pitch, 0.0f, 1.0f));
    v->vel = rnd_clamp(vel, 0.0f, 1.0f);
    v->eg_p.attacking = v->eg_f.attacking = v->eg_a.attacking = 1;
    v->trig = 1.0f;
    v->rs ^= v->rs << 13;                /* RANDOM: ein neuer Wert pro Schlag */
    v->rs ^= v->rs >> 17;
    v->rs ^= v->rs << 5;
    v->rnd = (float)(int32_t)v->rs * (1.0f / 2147483648.0f);
}

/* die Ausgänge der Patchbay, wie sie gerade stehen (bipolar -1..1 oder 0..1) */
static inline void sources(const rnd_voice *v, float *s, float ep, float ef, float ea)
{
    s[RS_VCO1] = v->o1v;
    s[RS_VCO2] = v->o2v;
    s[RS_NOISE] = v->nzv;
    s[RS_PITCH_EG] = ep;
    s[RS_VCF_EG] = ef;
    s[RS_VCA_EG] = ea;
    s[RS_VEL] = v->vel;
    s[RS_PITCH] = v->step_pitch;
    s[RS_LFO] = 4.0f * rnd_abs(v->lfo_ph - 0.5f) - 1.0f;
    s[RS_RANDOM] = v->rnd;
    s[RS_TRIGGER] = v->trig;
}

static void voice_render(rnd_voice *v, float *out, int n)
{
    int i;
    float inv_sr, amp_vel, f_vel, out_gain, wmax, sq;
    float src[RND_NSRC], m[RD_BLOCK];
    int c, any = 0;
    if (v->dirty)
        voice_update(v);
    /* die Block-Ziele: einmal pro Block aus den Ausgängen */
    for (c = 0; c < RND_NDST; c++)
        v->mod_blk[c] = 0.0f;
    if (v->ncab) {
        sources(v, src, v->eg_p.level, v->eg_f.level, v->eg_a.level);
        for (c = 0; c < v->ncab; c++)
            if (v->cab[c].dst >= RD_BLOCK)
                v->mod_blk[v->cab[c].dst] += src[v->cab[c].src] * v->cab[c].amt;
            else
                any = 1;
        if (v->mod_blk[RD_DECAY] != 0.0f)   /* DECAY: die VCA-Hüllkurve, bis 3 Oktaven länger / kürzer */
            env_times(&v->eg_a, v->p[RP_VCA_ATTACK] >= 0.5f ? 0.012f : 0.0004f,
                      decay_sec(v->p[RP_VCA_DECAY]) * rnd_exp2(v->mod_blk[RD_DECAY] * 3.0f), v->sr);
        else if (v->eg_a.dec_coef != v->dec_a0)
            v->dirty = 1;
    }
    inv_sr = 1.0f / v->sr;
    wmax = 0.45f * v->sr;
    amp_vel = 1.0f - v->vvca + v->vvca * v->vel;
    f_vel = v->vvcf * (v->vel - 0.5f);
    /* der Drive wird ausgeglichen, damit die Lautstärke ungefähr bleibt */
    sq = 0.6f + 0.4f * (1.0f + (v->drive - 1.0f) * 0.26f);   /* ~ sqrt(drive), ohne libm */
    out_gain = 1.5f * v->vol / sq;
    for (i = 0; i < n; i++) {
        float ep = env_run(&v->eg_p), ef = env_run(&v->eg_f), ea = env_run(&v->eg_a);
        float hz1, hz2, inc1, inc2, o1, o2, nz, mix, fc, wc, y, amp;
        if (any) {                       /* die Kabel zu den Ziele pro Sample */
            for (c = 0; c < RD_BLOCK; c++)
                m[c] = 0.0f;
            sources(v, src, ep, ef, ea);
            for (c = 0; c < v->ncab; c++)
                if (v->cab[c].dst < RD_BLOCK)
                    m[v->cab[c].dst] += src[v->cab[c].src] * v->cab[c].amt;
        } else {
            for (c = 0; c < RD_BLOCK; c++)
                m[c] = 0.0f;
        }
        v->trig *= v->trig_coef;
        v->lfo_ph += v->lfo_inc;
        if (v->lfo_ph >= 1.0f)
            v->lfo_ph -= 1.0f;
        hz1 = 8.0f * rnd_exp2(v->f1 + v->eg1 * ep + v->sq1 * v->step_pitch + m[RD_VCO1] * 3.0f);
        hz2 = 8.0f * rnd_exp2(v->f2 + v->eg2 * ep + v->sq2 * v->step_pitch + m[RD_VCO2] * 3.0f);
        inc1 = rnd_clamp(hz1 * inv_sr, 0.0f, 0.45f);
        o1 = osc_run(&v->o1, inc1, v->w1);
        inc2 = hz2 * inv_sr * (1.0f + (v->fm + m[RD_FM] * 4.0f) * o1);   /* FM 1>2, durch Null: Metall */
        inc2 = rnd_clamp(inc2, -0.45f, 0.45f);
        if (v->sync && v->o1.wrapped)
            v->o2.phase = rnd_clamp(v->o1.wrap_frac * inc2, 0.0f, 0.999f);
        o2 = osc_run(&v->o2, inc2, v->w2);
        nz = noise_run(&v->nz, v->color);
        v->o1v = o1;
        v->o2v = o2;
        v->nzv = nz;
        mix = (o1 * rnd_clamp(v->l1 + m[RD_LVL1], 0.0f, 1.5f) + o2 * rnd_clamp(v->l2 + m[RD_LVL2], 0.0f, 1.5f)
               + nz * rnd_clamp(v->ln + m[RD_NOISE], 0.0f, 1.5f)) * 0.6f;
        fc = 20.0f * rnd_exp2(v->cut + v->egf * ef + v->nf * nz + f_vel + m[RD_CUTOFF] * 4.0f);
        fc = rnd_clamp(fc, 15.0f, wmax);
        wc = 2.0f * RND_PI * fc * inv_sr;
        y = ladder_run(&v->vcf, mix, wc / (1.0f + wc), rnd_clamp(v->reso + m[RD_RESO] * 0.8f, 0.0f, 1.0f), v->hp);
        amp = ea * amp_vel * (1.0f + m[RD_VCA]);   /* VCA: mit VCO oder NOISE wird das Ringmodulation */
        y = rnd_soft(y * amp * v->drive * (1.0f + rnd_clamp(m[RD_DRIVE], -0.9f, 4.0f) * 2.0f));
        {   /* DC-Blocker, dann Sicherheits-Limiter */
            float d = y - v->dc_x1 + 0.995f * v->dc_y1;
            v->dc_x1 = y;
            v->dc_y1 = d;
            out[i] = rnd_soft(d * out_gain);
        }
    }
}
