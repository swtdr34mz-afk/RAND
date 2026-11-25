/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (RAND) */
/* RAND – der 8-Step-Sequencer und die Schnittstelle. Teil von rand_core.c */

void rnd_init(rnd_engine *e, float sample_rate)
{
    int i;
    char *z = (char *)e;
    for (i = 0; i < (int)sizeof *e; i++)
        z[i] = 0;
    e->sr = sample_rate;
    voice_init(&e->v, sample_rate);
    for (i = 0; i < RP_COUNT; i++)
        e->p[i] = RND_PARAMS[i].def;
    for (i = 0; i < RND_STEPS; i++) {
        e->steps[i].pitch = 0.5f;
        e->steps[i].vel = 0.8f;
        e->steps[i].on = 1;
    }
    e->cur = -1;
    e->clock_override = -1;
    e->last_clock = -1.0;
}

void rnd_set(rnd_engine *e, int id, float norm)
{
    if (id < 0 || id >= RP_COUNT)
        return;
    e->p[id] = rnd_clamp(norm, 0.0f, 1.0f);
    if (id < RP_TEMPO) {
        e->v.p[id] = e->p[id];
        e->v.dirty = 1;
    }
}

float rnd_get(const rnd_engine *e, int id) { return id >= 0 && id < RP_COUNT ? e->p[id] : 0.0f; }

float rnd_bpm(const rnd_engine *e)
{
    if (e->ext && e->ext_bpm > 0.0)
        return (float)e->ext_bpm;
    return 40.0f + e->p[RP_TEMPO] * 240.0f;
}

int rnd_clock_index(const rnd_engine *e)
{
    int i = (int)(e->p[RP_CLOCK] * (RND_NCLOCK - 1) + 0.5f);
    return i < 0 ? 0 : i >= RND_NCLOCK ? RND_NCLOCK - 1 : i;
}

int rnd_length(const rnd_engine *e)
{
    int l = 1 + (int)(e->p[RP_LENGTH] * (RND_STEPS - 1) + 0.5f);
    return l < 1 ? 1 : l > RND_STEPS ? RND_STEPS : l;
}

static double samples_per_step(const rnd_engine *e)
{
    int ci = e->clock_override >= 0 ? e->clock_override : rnd_clock_index(e);
    double sps = rnd_bpm(e) / 60.0 * 4.0 * RND_CLOCK_MULT[ci], s = e->sr / sps;
    return s < 4.0 ? 4.0 : s;
}

void rnd_start(rnd_engine *e)
{
    e->running = 1;
    e->cur = -1;
    e->to_next = 0.0;
}
void rnd_stop(rnd_engine *e)
{
    e->running = 0;
    e->cur = -1;
}
void rnd_continue(rnd_engine *e) { e->running = 1; }
int rnd_running(const rnd_engine *e) { return e->running; }
int rnd_step_now(const rnd_engine *e) { return e->running ? e->cur : -1; }
void rnd_clock_override(rnd_engine *e, int idx) { e->clock_override = idx < RND_NCLOCK ? idx : RND_NCLOCK - 1; }
float rnd_level(const rnd_engine *e) { return e->v.eg_a.level; }

void rnd_trigger(rnd_engine *e, float pitch, float vel) { voice_trigger(&e->v, pitch, vel); }

/* 24 Pulse pro Viertel: das Tempo aus dem Abstand gemessen, leicht geglättet */
void rnd_midi_clock(rnd_engine *e)
{
    if (e->last_clock >= 0.0) {
        double d = e->counter - e->last_clock;
        if (d > 0.0 && d < e->sr) {
            double b = e->sr * 60.0 / (d * 24.0);
            if (b >= 20.0 && b <= 400.0)
                e->ext_bpm = e->ext_bpm <= 0.0 ? b : e->ext_bpm * 0.9 + b * 0.1;
        }
    }
    e->last_clock = e->counter;
    e->ext = 1;
}

static void advance(rnd_engine *e)
{
    const rnd_step *s;
    e->cur = (e->cur + 1) % rnd_length(e);
    s = &e->steps[e->cur];
    if (s->on)
        voice_trigger(&e->v, s->pitch, s->vel);
}

void rnd_render(rnd_engine *e, float *out, int n)
{
    int done = 0;
    while (done < n) {
        int chunk = n - done;
        if (e->running) {
            int until;
            if (e->to_next <= 0.0) {
                advance(e);
                e->to_next += samples_per_step(e);
            }
            until = (int)e->to_next;
            if ((double)until < e->to_next)
                until++;
            if (until < chunk)
                chunk = until < 1 ? 1 : until;
        }
        voice_render(&e->v, out + done, chunk);
        if (e->running)
            e->to_next -= chunk;
        done += chunk;
    }
    e->counter += n;
    if (e->ext && e->counter - e->last_clock > e->sr)   /* eine Sekunde ohne Clock: wieder eigenes Tempo */
        e->ext = 0;
}

void rnd_randomize(rnd_engine *e, uint32_t seed)
{
    int i;
    uint32_t s = seed ? seed : 0x9E3779B9u;
    for (i = 0; i < RND_STEPS; i++) {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        e->steps[i].pitch = (float)(s & 1023u) / 1023.0f;
        e->steps[i].vel = 0.25f + 0.75f * (float)((s >> 10) & 1023u) / 1023.0f;
        e->steps[i].on = ((s >> 20) & 7u) != 0;   /* meistens an */
    }
    e->steps[0].on = 1;
}
