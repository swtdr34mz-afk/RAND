/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (RAND) */
/* RAND: Bedienung, Seiten, Patchbay, Bildschirm, Einstellungen. Teil des Unity-Builds (device/unity.c,
 * host/plat_host.c), nach der Plattform: gfx.c und announce.c sind sichtbar. Der Klang ist dsp/rand_core.c.
 *
 * Bedienung
 *   K1..K4              die vier Regler der Seite. Der Sequencer wie am DFAM: SEQ, und K1..K4 sind vier Steps
 *   HOME   OSC 1 / OSC 2 (nochmal drücken)     SEL   FM / NOISE        ENV   die Hüllkurven (VCO, VCF, VCA)
 *   LFO    FILTER       EDIT   OUTPUT       FX   REVERB, DELAY, DRIVE, MASTER FILTER       GLO   die Kippschalter
 *   SEQ    PITCH 1-4, PITCH 5-8, VEL 1-4, VEL 5-8 (Velocity 0: der Step schweigt)
 *   Tasten = die PATCHBAY: schwarze Tasten sind Ausgänge (OUT), weiße Eingänge (IN). Eine schwarze halten und eine
 *            weiße tippen: ein Kabel gesteckt (nochmal: gezogen). Gehalten: K1 die Stärke ihrer Kabel, REC: alle weg
 *   SELECT Tempo     ALGORITHM   CLOCK /4 .. x128     PRESET   die Presets     OCT- / OCT+   Länge 1..8
 *   PLAY   Start / Stop     SAVE   TRIGGER (ein Schlag)     ARP   RAND: neue Zufalls-Reihen
 *   REC 1 s halten      das Preset ganz zurück (Klang, Pattern, Kabel)
 *   MIDI über USB: Clock, START/STOP/CONTINUE (RAND folgt z.B. SIX MACHINES), Noten spielen die Stimme.
 *   Einstellungen speichern sich selbst, sobald der Sequencer steht. */
#include "fastmath.h"
#include "rand_core.h"

#define SR 44100.0f

static rnd_engine eng;

/* ------------------------------------------------------------- Einstellungen --- */
#define SET_MAGIC 0x32444E52u            /* "RND2" */
typedef struct {
    uint32_t magic;
    uint8_t v[RP_COUNT];                 /* die Regler 0..127 (TEMPO steht in bpm) */
    uint8_t pitch[RND_STEPS], vel[RND_STEPS];
    uint8_t preset, ncab;
    uint16_t bpm;
    uint8_t cab_src[RND_NCABLES], cab_dst[RND_NCABLES], cab_amt[RND_NCABLES];   /* amt 0..127, 64 = 0 */
} settings_t;
_Static_assert(sizeof(settings_t) <= PLAT_STORE_MAX, "die Einstellungen passen nicht in ein Speicherobjekt");
static settings_t set;
static uint8_t set_dirty;
static uint32_t dirty_at;

static const uint8_t WK[16] = PLAT_WHITE_KEYS, BK[11] = PLAT_BLACK_KEYS;

static uint8_t to127(float x) { return (uint8_t)clamp((int32_t)(x * 127.0f + 0.5f), 0, 127); }

static void engine_from_settings(void)
{
    uint32_t i;
    for (i = 0; i < RP_COUNT; i++)
        if (i != RP_TEMPO)
            rnd_set(&eng, (int)i, (float)set.v[i] / 127.0f);
    rnd_set(&eng, RP_TEMPO, ((float)set.bpm - 40.0f) / 240.0f);
    for (i = 0; i < RND_STEPS; i++) {
        eng.steps[i].pitch = (float)set.pitch[i] / 127.0f;
        eng.steps[i].vel = (float)set.vel[i] / 127.0f;
        eng.steps[i].on = 1;
    }
    rnd_patch_clear(&eng, -1);
    for (i = 0; i < set.ncab; i++) {
        rnd_patch_toggle(&eng, set.cab_src[i], set.cab_dst[i]);
        eng.v.cab[eng.v.ncab - 1].amt = ((float)set.cab_amt[i] - 64.0f) / 63.0f;
    }
}

static void cables_from_engine(void)
{
    int i;
    set.ncab = (uint8_t)eng.v.ncab;
    for (i = 0; i < eng.v.ncab; i++) {
        set.cab_src[i] = eng.v.cab[i].src;
        set.cab_dst[i] = eng.v.cab[i].dst;
        set.cab_amt[i] = (uint8_t)clamp((int32_t)(64.0f + eng.v.cab[i].amt * 63.0f + 0.5f), 1, 127);
    }
}

static void settings_from_engine(void)
{
    uint32_t i;
    for (i = 0; i < RP_COUNT; i++)
        set.v[i] = to127(rnd_get(&eng, (int)i));
    set.bpm = (uint16_t)clamp((int32_t)(40.0f + rnd_get(&eng, RP_TEMPO) * 240.0f + 0.5f), 40, 280);
    for (i = 0; i < RND_STEPS; i++) {
        set.pitch[i] = to127(eng.steps[i].pitch);
        set.vel[i] = to127(eng.steps[i].vel);
    }
    cables_from_engine();
}

static void settings_defaults(void)
{
    memset(&set, 0, sizeof set);
    set.magic = SET_MAGIC;
    set.preset = 0;
    rnd_preset_load(&eng, 0);
    settings_from_engine();
}

static int settings_ok(void)
{
    uint32_t i;
    for (i = 0; i < RP_COUNT; i++)
        if (set.v[i] > 127u)
            return 0;
    for (i = 0; i < RND_STEPS; i++)
        if (set.pitch[i] > 127u || set.vel[i] > 127u)
            return 0;
    if (set.ncab > RND_NCABLES)
        return 0;
    for (i = 0; i < set.ncab; i++)
        if (set.cab_src[i] >= RND_NSRC || set.cab_dst[i] >= RND_NDST || set.cab_amt[i] > 127u)
            return 0;
    return set.magic == SET_MAGIC && set.bpm >= 40u && set.bpm <= 280u && set.preset < RND_NPRESETS;
}

static void settings_load(void)
{
    if (plat_store_load(OBJ_SETTINGS, &set, sizeof set) != (int)sizeof set || !settings_ok())
        settings_defaults();
    engine_from_settings();
}

static void dirty(void)
{
    set_dirty = 1;
    dirty_at = plat_ms();
}

static void settings_save(void)
{
    if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0)
        set_dirty = 0;
}

/* -------------------------------------------- Wünsche an den Audio-Interrupt --- */
static volatile int8_t play_req = -1;    /* 1 Start, 0 Stop */
static volatile uint8_t trig_req;        /* ein Schlag von Hand */
static volatile float trig_pitch, trig_vel;

static void hit(float pitch, float vel)
{
    trig_pitch = pitch;
    trig_vel = vel;
    trig_req = 1;
}

/* ------------------------------------------------------------------- Seiten --- */
enum { PG_OSC1, PG_OSC2, PG_NOISE, PG_ENV, PG_FILTER, PG_OUT, PG_FX, PG_SW, PG_P1, PG_P2, PG_V1, PG_V2, NPAGES };
#define NSOUND PG_P1
static const char *const PAGE_NAME[NPAGES] = {"OSC 1", "OSC 2", "FM / NOISE", "ENVELOPES", "FILTER", "OUTPUT", "FX",
                                              "SWITCHES", "PITCH 1-4", "PITCH 5-8", "VEL 1-4", "VEL 5-8"};
/* die Seiten nach den Feldern am EDGE-Panel */
static const uint8_t PAGE_P[NSOUND][4] = {
    {RP_VCO1_FREQ, RP_VCO1_EG, RP_VCO1_SEQ, RP_VCO1_LVL},        /* HOME: OSCILLATOR (VCO 1) */
    {RP_VCO2_FREQ, RP_VCO2_EG, RP_VCO2_SEQ, RP_VCO2_LVL},        /* HOME nochmal: OSCILLATOR (VCO 2) */
    {RP_FM, RP_NOISE_LVL, RP_NOISE_VCF, RP_VEL_VCF},             /* SEL: 1-2 FM AMT, NOISE */
    {RP_PITCH_DECAY, RP_VCF_DECAY, RP_VCA_DECAY, RP_VCA_ATTACK}, /* ENV: EG (VCO 1&2), VCF, VCA, VCA MODE */
    {RP_CUTOFF, RP_RESO, RP_VCF_EG, RP_FILTER_MODE},             /* LFO: FILTER (VCF) */
    {RP_VEL_VCA, RP_VOLUME, RP_LENGTH, RP_DELAY_TIME},           /* EDIT: OUTPUT (VCA) */
    {RP_REVERB, RP_DELAY, RP_DRIVE, RP_MFILTER},                 /* FX */
    {RP_VCO1_WAVE, RP_VCO2_WAVE, RP_SYNC, RP_NOISE_COLOR},       /* GLO: die Kippschalter */
};

static struct {
    char msg[2][24];
    uint32_t msg_until;
    char big[2][16];                     /* groß in der Mitte: CLOCK gedreht */
    uint32_t big_until;
    uint32_t knob_at[4];
    uint32_t keys, btn;
    uint32_t enc_at[NE];
    uint8_t page;
    int8_t src;                          /* die gehaltene schwarze Taste: ein Ausgang (-1: keiner) */
    uint8_t src_used;                    /* mit ihr gesteckt oder gedreht */
    int8_t in_show;                      /* eine weiße Taste allein: was in diesen Eingang geht */
    uint32_t in_until;
    uint32_t rec_t;
    uint8_t playing;
    int32_t step_drawn;
    int32_t lvl_drawn;
    uint8_t redraw;
    uint32_t load_at, load_shown;
    uint8_t hot;                         /* wie oft hintereinander die Last zu hoch war */
} ui;

static uint32_t txt_len(const char *s)    /* (kein strlen auf dem FM-1) */
{
    uint32_t n = 0;
    while (s[n])
        n++;
    return n;
}

static void text_cat(char *d, const char *s)
{
    while (*d)
        d++;
    while (*s)
        *d++ = *s++;
    *d = 0;
}

static void int_text(int32_t v, char *b)
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do
        t[n++] = (char)('0' + u % 10u);
    while ((u /= 10u) && n < 11);
    if (v < 0)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1200u;
    ui.redraw = 1;
    if (ann_on()) {
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        if (ui.msg[1][0]) {
            text_cat(t, " ");
            text_cat(t, ui.msg[1]);
        }
        ann_say(t, 0, ANN_NOW);
    }
}

static void say_big(const char *a, const char *b)
{
    uint32_t i;
    for (i = 0; i < 15u && a[i]; i++)
        ui.big[0][i] = a[i];
    ui.big[0][i] = 0;
    for (i = 0; i < 15u && b[i]; i++)
        ui.big[1][i] = b[i];
    ui.big[1][i] = 0;
    ui.big_until = plat_ms() + 900u;
    ui.redraw = 1;
    if (ann_on()) {
        char t[40] = "";
        text_cat(t, ui.big[0]);
        text_cat(t, " ");
        text_cat(t, ui.big[1]);
        ann_say(t, 0, ANN_NOW);
    }
}

/* ein Wert in Einheiten: Hz, ms, ±, Schalter, Steps */
static void hz_text(float hz, char *t)
{
    if (hz < 1000.0f) {
        int_text((int32_t)(hz + 0.5f), t);
        text_cat(t, "HZ");
    } else {
        int32_t d = (int32_t)(hz / 100.0f + 0.5f);
        int_text(d / 10, t);
        text_cat(t, ".");
        int_text(d % 10, t + txt_len(t));
        text_cat(t, "K");
    }
}
static void val_text(int id, uint8_t v, char *t)
{
    float n = (float)v / 127.0f;
    t[0] = 0;
    if (RND_PARAMS[id].kind == RK_SWITCH) {
        text_cat(t, v >= 64u ? RND_PARAMS[id].on : RND_PARAMS[id].off);
        return;
    }
    if (RND_PARAMS[id].kind == RK_BIP) {
        int32_t b = (int32_t)v - 64;
        if (b > 0)
            text_cat(t, "+");
        int_text(b, t + txt_len(t));
        return;
    }
    switch (id) {
    case RP_VCO1_FREQ:
    case RP_VCO2_FREQ: hz_text(8.0f * fm_exp2f(n * 10.0f), t); return;
    case RP_CUTOFF: hz_text(20.0f * fm_exp2f(n * 10.0f), t); return;
    case RP_PITCH_DECAY:
    case RP_VCF_DECAY:
    case RP_VCA_DECAY: {
        float ms = 4.0f * fm_exp2f(n * 9.9658f);
        if (ms < 1000.0f) {
            int_text((int32_t)(ms + 0.5f), t);
            text_cat(t, "MS");
        } else {
            int32_t d = (int32_t)(ms / 100.0f + 0.5f);
            int_text(d / 10, t);
            text_cat(t, ".");
            int_text(d % 10, t + txt_len(t));
            text_cat(t, "S");
        }
        return;
    }
    case RP_LENGTH: int_text(1 + (int32_t)(n * 7.0f + 0.5f), t); return;
    default: int_text((int32_t)v * 100 / 127, t); return;
    }
}

/* die Regler der Seite: ein Klang-Parameter, oder (SEQ-Seiten, gehaltener Step) Tonhöhe / Lautstärke eines Steps */
/* die Regler der Seite: ein Klang-Parameter, ein Step (SEQ-Seiten), oder die Stärke der Kabel (Ausgang gehalten) */
typedef struct { int id; int step; int vel; int amt; } knob_t;
static knob_t knob_of(int k)
{
    knob_t kb = {-1, -1, 0, 0};
    if (ui.src >= 0) {
        kb.amt = k == 0;
        return kb;
    }
    if (ui.page < NSOUND) {
        kb.id = PAGE_P[ui.page][k];
        return kb;
    }
    kb.step = (ui.page == PG_P2 || ui.page == PG_V2 ? 4 : 0) + k;   /* wie am DFAM: ein Regler pro Step */
    kb.vel = ui.page >= PG_V1;
    return kb;
}
static int knob_get(knob_t kb)
{
    if (kb.amt)
        return clamp((int32_t)(64.0f + rnd_patch_get_amount(&eng, ui.src) * 63.0f + 0.5f), 1, 127);
    if (kb.id >= 0)
        return set.v[kb.id];
    if (kb.step >= 0)
        return kb.vel ? set.vel[kb.step] : set.pitch[kb.step];
    return -1;
}

static int32_t accel(int k, int32_t e, int32_t mid, int32_t fast)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_at[k];
    ui.enc_at[k] = now;
    if (dt < 35u)
        return e * fast;
    if (dt < 80u)
        return e * mid;
    return e;
}

static void knob(int k, int32_t e)
{
    knob_t kb = knob_of(k);
    int32_t was = knob_get(kb), v;
    if (was < 0)
        return;
    ui.knob_at[k] = plat_ms();
    ui.redraw = 1;
    if (kb.id >= 0 && RND_PARAMS[kb.id].kind == RK_SWITCH)
        v = e > 0 ? 127 : 0;
    else if (kb.id == RP_LENGTH || kb.id == RP_DELAY_TIME) {   /* Rasterwerte: ein Schritt pro Rast */
        int32_t n = kb.id == RP_LENGTH ? 7 : RND_NDIV - 1, st = (was * n + 63) / 127;
        v = (clamp(st + (e > 0 ? 1 : -1), 0, n) * 127 + n / 2) / n;
    } else
        v = clamp(was + accel(EN_K1 + k, e, 3, 6), kb.amt ? 1 : 0, 127);
    if (v == was)
        return;
    if (kb.amt) {
        rnd_patch_amount(&eng, ui.src, ((float)v - 64.0f) / 63.0f);
        cables_from_engine();
        ui.src_used = 1;
    } else if (kb.id >= 0) {
        set.v[kb.id] = (uint8_t)v;
        rnd_set(&eng, kb.id, (float)v / 127.0f);
    } else if (kb.vel) {
        set.vel[kb.step] = (uint8_t)v;
        eng.steps[kb.step].vel = (float)v / 127.0f;
    } else {
        set.pitch[kb.step] = (uint8_t)v;
        eng.steps[kb.step].pitch = (float)v / 127.0f;
    }
    dirty();
}

static void page_set(int pg)
{
    ui.page = (uint8_t)pg;
    ui.redraw = 1;
    app_say(PAGE_NAME[pg], "");
}

/* --------------------------------------------------------------- Transport --- */
static void play_toggle(void)
{
    play_req = (int8_t)!ui.playing;
    ui.playing = (uint8_t)!ui.playing;
    dirty_at = plat_ms();
    ui.redraw = 1;
}

static void preset_apply(uint32_t p)
{
    set.preset = (uint8_t)p;
    rnd_preset_load(&eng, (int)p);
    settings_from_engine();
    if (!ui.playing)
        hit(eng.steps[0].pitch, 1.0f);
    dirty();
}

static void clock_turn(int32_t e)
{
    int idx = clamp(rnd_clock_index(&eng) + (e > 0 ? 1 : -1), 0, RND_NCLOCK - 1);
    set.v[RP_CLOCK] = (uint8_t)((idx * 127 + 5) / 10);
    rnd_set(&eng, RP_CLOCK, (float)set.v[RP_CLOCK] / 127.0f);
    dirty();
    say_big(RND_CLOCK_NAME[idx], idx >= 8 ? "AUDIO RATE" : "CLOCK");
}

static void length_step(int d)
{
    int32_t l = clamp(rnd_length(&eng) + d, 1, RND_STEPS);
    char t[12] = "";
    set.v[RP_LENGTH] = (uint8_t)(((l - 1) * 127 + 3) / 7);
    rnd_set(&eng, RP_LENGTH, (float)set.v[RP_LENGTH] / 127.0f);
    dirty();
    int_text(l, t);
    text_cat(t, l == 1 ? " STEP" : " STEPS");
    app_say("LENGTH", t);
}

/* ---------------------------------------------------------------- Patchbay --- */
static void patch_key(int dst)           /* eine weiße Taste, während ein Ausgang gehalten wird */
{
    int r = rnd_patch_toggle(&eng, ui.src, dst);
    ui.src_used = 1;
    if (r < 0) {
        app_say("PATCH FULL", "8 CABLES");
        return;
    }
    cables_from_engine();
    dirty();
    app_say(r ? "PATCHED" : "PULLED", RND_DST_NAME[dst]);
}

static int cables_into(int dst)
{
    int i, n = 0;
    for (i = 0; i < eng.v.ncab; i++)
        n += eng.v.cab[i].dst == dst;
    return n;
}
static int cables_from(int src)
{
    int i, n = 0;
    for (i = 0; i < eng.v.ncab; i++)
        n += eng.v.cab[i].src == src;
    return n;
}

/* ------------------------------------------------------------------ Eingabe --- */
static void button(uint32_t i)
{
    static const int8_t PAGE_OF[NB] = {PG_FX, PG_NOISE, PG_ENV, PG_FILTER, PG_OUT, PG_SW, PG_OSC1, -1, -1, -1, -1,
                                       -1, -1, -1};
    if (PAGE_OF[i] >= 0) {
        int pg = PAGE_OF[i];
        if (i == B_HOME)                 /* HOME: OSC 1, nochmal OSC 2 */
            pg = ui.page == PG_OSC1 ? PG_OSC2 : PG_OSC1;
        else if (ui.page == pg)
            pg = PG_OSC1;
        page_set(pg);
        return;
    }
    switch (i) {
    case B_SEQ: page_set(ui.page >= PG_P1 && ui.page < PG_V2 ? ui.page + 1 : PG_P1); break;
    case B_PLAY: play_toggle(); break;
    case B_REC:
        if (ui.src >= 0) {               /* ein Ausgang gehalten: seine Kabel weg */
            rnd_patch_clear(&eng, ui.src);
            cables_from_engine();
            ui.src_used = 1;
            dirty();
            app_say("PULLED", RND_SRC_NAME[(int)ui.src]);
        } else {
            ui.rec_t = plat_ms();
        }
        break;
    case B_SAVE: hit(eng.steps[0].pitch, 1.0f); break;   /* TRIGGER */
    case B_ARP:
        rnd_randomize(&eng, plat_ms() * 2654435761u ^ eng.counter);
        settings_from_engine();
        dirty();
        app_say("RAND", "NEW ROWS");
        break;
    case B_OCTDN: length_step(-1); break;
    case B_OCTUP: length_step(1); break;
    default: break;
    }
}

static void held_buttons(uint32_t btn)
{
    if (ui.rec_t) {                      /* REC eine Sekunde: das Preset ganz zurück */
        if (!(btn >> B_REC & 1u))
            ui.rec_t = 0;
        else if (plat_ms() - ui.rec_t >= 1000u) {
            ui.rec_t = 0;
            preset_apply(set.preset);
            app_say("RESET", rnd_preset_name(set.preset));
        }
    }
}

static void input(void)
{
    uint32_t keys = plat_keys(), btn = plat_buttons(), i;
    int32_t e;
    int src = ui.src;
    /* die Ausgänge: eine schwarze Taste gedrückt hält sie (die erste gilt) */
    if (src >= 0 && !(keys >> BK[src] & 1u)) {
        if (!ui.src_used)
            app_say(RND_SRC_NAME[src], cables_from(src) ? "OUT" : "NOT PATCHED");
        src = -1;
    }
    if (src < 0)
        for (i = 0; i < 11u; i++)
            if ((keys >> BK[i] & 1u) && !(ui.keys >> BK[i] & 1u)) {
                src = (int)i;
                ui.src_used = 0;
                break;
            }
    if (src != ui.src) {
        ui.src = (int8_t)src;
        ui.redraw = 1;
    }
    for (i = 0; i < 16u; i++) {          /* die Eingänge */
        uint32_t bit = 1u << WK[i];
        if ((keys & bit) && !(ui.keys & bit)) {
            if (ui.src >= 0) {
                patch_key((int)i);
            } else {
                ui.in_show = (int8_t)i;
                ui.in_until = plat_ms() + 1500u;
                ui.redraw = 1;
            }
        }
    }
    for (i = 0; i < NB; i++)
        if ((btn >> i & 1u) && !(ui.btn >> i & 1u))
            button(i);
    held_buttons(btn);
    for (i = 0; i < 4u; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0)
            knob((int)i, e);
    if ((e = plat_enc(EN_SELECT)) != 0) {
        char t[8];
        set.bpm = (uint16_t)clamp((int32_t)set.bpm + accel(EN_SELECT, e, 2, 5), 40, 280);
        rnd_set(&eng, RP_TEMPO, ((float)set.bpm - 40.0f) / 240.0f);
        dirty();
        int_text(set.bpm, t);
        app_say("BPM", t);
    }
    if ((e = plat_enc(EN_ALGO)) != 0)
        clock_turn(e);
    if ((e = plat_enc(EN_PRESET)) != 0) {
        uint32_t p = (set.preset + RND_NPRESETS + (e > 0 ? 1u : RND_NPRESETS - 1u)) % RND_NPRESETS;
        preset_apply(p);
        app_say("PRESET", rnd_preset_name((int)p));
    }
    if (keys != ui.keys || btn != ui.btn)
        ui.redraw = 1;
    ui.keys = keys;
    ui.btn = btn;
}

void app_input_only(void) { input(); }

/* die LEDs: ein gehaltener Ausgang hell, seine Eingänge hell; sonst gesteckte Buchsen halb, die anderen glimmen */
static void lights(void)
{
    uint32_t lit = 0, mid = 0, dim = 0, btn = 0, dimb, i;
    static const uint8_t PAGE_BTN[NSOUND] = {B_HOME, B_HOME, B_SEL, B_ENV, B_LFO, B_EDIT, B_FX, B_GLO};
    for (i = 0; i < 11u; i++) {
        uint32_t bit = 1u << BK[i];
        if ((int8_t)i == ui.src)
            lit |= bit;
        else if (cables_from((int)i))
            mid |= bit;
        else
            dim |= bit;
    }
    for (i = 0; i < 16u; i++) {
        uint32_t bit = 1u << WK[i];
        if (ui.src >= 0 ? rnd_patch_has(&eng, ui.src, (int)i) : (ui.keys & bit) != 0)
            lit |= bit;
        else if (ui.src < 0 && cables_into((int)i))
            mid |= bit;
        else
            dim |= bit;
    }
    if (ui.playing)
        btn |= LED_PLAY_GREEN;
    if (ui.page < NSOUND)
        btn |= 1u << PAGE_BTN[ui.page];
    else
        btn |= 1u << B_SEQ;
    dimb = (1u << B_PLAY) | (1u << B_REC) | (1u << B_SEQ) | (1u << B_ARP) | (1u << B_FX) | (1u << B_SEL)
           | (1u << B_ENV) | (1u << B_LFO) | (1u << B_EDIT) | (1u << B_GLO) | (1u << B_HOME) | (1u << B_OCTDN)
           | (1u << B_OCTUP) | (1u << B_SAVE);
    plat_lights(btn | (ui.btn & ~(1u << B_PLAY)), lit, dimb, dim, 1);
    plat_lights_mid(0, mid & ~lit);
}

/* ----------------------------------------------------------------- Zeichnen --- */
/* Die Optik der Moog-Eurorack-Kisten (DFAM, Mother-32): schwarzes Panel, weiße Schrift, schwarze Regler mit
 * Silberkappe und Skala, Holzwangen links und rechts, rote LEDs unter den Steps. Unten die zwei Reglerreihen des DFAM:
 * acht für die Tonhöhe, acht für die Lautstärke, die LED des laufenden Steps dazwischen. */
#define C_PANEL RGB(10, 10, 12)
#define C_LABEL RGB(236, 236, 240)
#define C_SUB RGB(140, 140, 148)
#define C_TICK RGB(120, 120, 128)
#define C_KBODY RGB(26, 26, 29)
#define C_KEDGE RGB(78, 78, 84)
#define C_SILVER RGB(176, 176, 182)
#define C_SILVER_HI RGB(232, 232, 236)
#define C_LED RGB(255, 38, 26)
#define C_LED_DIM RGB(64, 12, 10)
#define C_WOOD RGB(112, 60, 32)
#define C_GRAIN RGB(78, 40, 20)

static void ctext(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

static void wood(int32_t h, int32_t y0)  /* die Holzwangen, mit Maserung (fest an der Bildschirmhöhe) */
{
    int32_t y;
    cv_rect(0, 0, 5, h, C_WOOD);
    cv_rect(235, 0, 5, h, C_WOOD);
    for (y = 0; y < h; y++) {
        int32_t g = (y + y0) % 23;
        if (g == 0 || g == 9)
            cv_rect(1 + (g ? 2 : 0), y, 1, 6, C_GRAIN);
        if (g == 4 || g == 15)
            cv_rect(236 + (g == 4 ? 2 : 0), y, 1, 5, C_GRAIN);
    }
}

static void disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t dy;
    for (dy = -r; dy <= r; dy++) {
        int32_t w = 0;
        while ((w + 1) * (w + 1) + dy * dy <= r * r)
            w++;
        cv_rect(cx - w, cy + dy, 2 * w + 1, 1, c);
    }
}

/* ein Regler: Skala (11 Striche über 270°), schwarzer Körper, Silberkappe, weißer Zeiger. v 0..1 */
static void moog_knob(int32_t cx, int32_t cy, int32_t r, float v, int hot, int bip, int ticks)
{
    int32_t i, cap = r * 11 / 20;
    float a, sn, cs;
    if (ticks)
        for (i = 0; i <= 10; i++) {
            a = (-135.0f + 27.0f * (float)i) * 0.017453293f;
            sn = fm_sinf(a);
            cs = fm_cosf(a);
            cv_line(cx + (int32_t)(sn * (float)(r + 2)), cy - (int32_t)(cs * (float)(r + 2)),
                    cx + (int32_t)(sn * (float)(r + (i == 5 && bip ? 5 : 4))),
                    cy - (int32_t)(cs * (float)(r + (i == 5 && bip ? 5 : 4))), i == 5 && bip ? C_LABEL : C_TICK);
        }
    disc(cx, cy, r, hot ? C_SUB : C_KEDGE);
    disc(cx, cy, r - 1, C_KBODY);
    a = (-135.0f + 270.0f * fm_clampf(v, 0.0f, 1.0f)) * 0.017453293f;
    sn = fm_sinf(a);
    cs = fm_cosf(a);
    for (i = cap + 1; i < r; i++) {      /* der Zeiger auf dem Rock, zwei Pixel breit */
        int32_t px = cx + (int32_t)(sn * (float)i), py = cy - (int32_t)(cs * (float)i);
        cv_rect(px, py, 2, 2, C_LABEL);
    }
    disc(cx, cy, cap, C_SILVER);
    disc(cx - cap / 3, cy - cap / 3, cap / 2, C_SILVER_HI);
}

/* ein Kippschalter: Mutter, Hebel nach oben (an) oder unten, die zwei Namen */
static void toggle(int32_t cx, int32_t cy, int on, const char *up, const char *dn, int hot)
{
    ctext(cx, cy - 24, &FONT_XS, up, on ? C_LABEL : C_SUB);
    ctext(cx, cy + 13, &FONT_XS, dn, on ? C_SUB : C_LABEL);
    disc(cx, cy, 6, hot ? C_SUB : C_KEDGE);
    disc(cx, cy, 4, C_SILVER);
    cv_rect(cx - 1, on ? cy - 10 : cy, 3, 11, C_SILVER_HI);
    disc(cx, on ? cy - 11 : cy + 11, 2, C_SILVER_HI);
}

static void led(int32_t cx, int32_t cy, int on)
{
    disc(cx, cy, 3, on ? C_LED : C_LED_DIM);
    if (on)
        cv_rect(cx - 1, cy - 2, 2, 1, RGB(255, 170, 160));
}

/* ein Regler, der gerade nichts zu tun hat, sagt, woran es liegt (statt stumm zu bleiben) */
static int row_flat(void)
{
    uint32_t i;
    for (i = 1; i < RND_STEPS; i++)
        if (set.pitch[i] != set.pitch[0])
            return 0;
    return 1;
}
static const char *needs(knob_t kb)
{
    const uint8_t *v = set.v;
    int vco2_off = v[RP_VCO2_LVL] == 0 && v[RP_FM] == 0;
    if (kb.amt)
        return 0;
    if (kb.step >= 0)
        return !kb.vel && v[RP_VCO1_SEQ] == 64 && v[RP_VCO2_SEQ] == 64 ? "SEQ=0" : 0;
    switch (kb.id) {
    case RP_VCO2_FREQ:
    case RP_VCO2_EG:
    case RP_VCO2_WAVE:
    case RP_SYNC: return vco2_off ? "LVL2=0" : 0;
    case RP_VCO2_SEQ: return vco2_off ? "LVL2=0" : row_flat() ? "FLAT" : 0;
    case RP_VCO1_SEQ: return row_flat() ? "FLAT" : 0;
    case RP_VCO1_WAVE: return v[RP_VCO1_LVL] == 0 && v[RP_FM] == 0 && v[RP_SYNC] < 64 ? "LVL1=0" : 0;
    case RP_DELAY_TIME: return v[RP_DELAY] == 0 ? "DLY=0" : 0;
    case RP_NOISE_COLOR: return v[RP_NOISE_LVL] == 0 && v[RP_NOISE_VCF] == 0 ? "NOISE=0" : 0;
    case RP_PITCH_DECAY: return v[RP_VCO1_EG] == 64 && v[RP_VCO2_EG] == 64 ? "ENV=0" : 0;
    case RP_VCF_DECAY: return v[RP_VCF_EG] == 64 ? "ENV=0" : 0;
    default: return 0;
    }
}

static void draw_knob(int k, int32_t cx, int32_t y, uint32_t now)
{
    knob_t kb = knob_of(k);
    int32_t v = knob_get(kb);
    int hot = ui.knob_at[k] && now - ui.knob_at[k] < 600u, bipolar;
    char t[16], name[12] = "";
    if (v < 0)
        return;
    if (kb.id >= 0) {
        text_cat(name, RND_PARAMS[kb.id].label);
        bipolar = RND_PARAMS[kb.id].kind == RK_BIP;
        val_text(kb.id, (uint8_t)v, t);
        if (RND_PARAMS[kb.id].kind == RK_SWITCH) {
            ctext(cx, y, &FONT_XS, name, hot ? C_LED : C_LABEL);
            toggle(cx, y + 40, v >= 64, RND_PARAMS[kb.id].on, RND_PARAMS[kb.id].off, hot);
            if (needs(kb)) {                 /* wirkt gerade nicht: der Grund statt des oberen Namens */
                cv_rect(cx - 28, y + 15, 57, 13, C_PANEL);
                ctext(cx, y + 16, &FONT_XS, needs(kb), RGB(190, 70, 56));
            }
            return;
        }
    } else {
        if (kb.amt) {
            text_cat(name, "AMOUNT");
        } else {
            text_cat(name, "STEP ");
            int_text(kb.step + 1, name + 5);
        }
        bipolar = !kb.vel || kb.amt;
        t[0] = 0;
        if (bipolar) {
            if (v > 64)
                text_cat(t, "+");
            int_text(v - 64, t + txt_len(t));
        } else {
            int_text(v * 100 / 127, t);
        }
    }
    ctext(cx, y, &FONT_XS, name, hot ? C_LED : C_LABEL);
    moog_knob(cx, y + 34, 14, (float)v / 127.0f, hot, bipolar, 1);
    if (needs(kb) && !hot)               /* wirkt gerade nicht: warum (beim Drehen wieder der Wert) */
        ctext(cx, y + 56, &FONT_XS, needs(kb), RGB(190, 70, 56));
    else
        ctext(cx, y + 56, &FONT_XS, t, hot ? C_LABEL : C_SUB);
}

/* wo man ist: die Seiten als kleine graue Taster, die aktuelle weiß */
static void draw_page_keys(int32_t y)
{
    uint32_t p;
    int32_t x = 236 - (int32_t)NPAGES * 8 - 6;
    for (p = 0; p < NPAGES; p++) {
        if (p == NSOUND)
            x += 6;
        cv_rect(x, y, 6, 6, p == ui.page ? C_LABEL : RGB(60, 60, 64));
        x += 8;
    }
}

/* CLOCK: elf LEDs von /4 bis x128; ab x32 hörbar (AUDIO) */
static void draw_clock(int32_t y)
{
    int idx = eng.clock_override >= 0 ? eng.clock_override : rnd_clock_index(&eng);
    uint32_t i;
    char t[16] = "CLOCK ";
    text_cat(t, RND_CLOCK_NAME[idx]);
    cv_text(10, y, &FONT_XS, t, eng.clock_override >= 0 ? C_LED : C_LABEL);
    for (i = 0; i < RND_NCLOCK; i++)
        led(98 + (int32_t)i * 12, y + 6, (int)i == idx);
    cv_rect(98 + 8 * 12 - 4, y + 12, 3 * 12 - 4, 1, C_SUB);   /* ab hier hörbar: der Takt wird ein Ton */
    cv_rect(98 + 8 * 12 - 4, y + 10, 1, 2, C_SUB);
    cv_rect(98 + 10 * 12 + 4, y + 10, 1, 2, C_SUB);
}

/* eine Buchse: Mutter und Loch, gesteckt mit rotem Ring */
static void jack(int32_t cx, int32_t cy, int on)
{
    disc(cx, cy, 6, on ? C_LED : C_KEDGE);
    disc(cx, cy, 4, C_SILVER);
    disc(cx, cy, 2, C_PANEL);
}

/* die Patchbay: der gehaltene Ausgang, K1 die Stärke, die gesteckten Eingänge */
static void draw_patch(uint32_t now)
{
    int i, n = 0;
    jack(16, 8, 1);
    cv_text(28, 3, &FONT_XS, "OUT", C_SUB);
    cv_text(52, 3, &FONT_XS, RND_SRC_NAME[(int)ui.src], C_LABEL);
    cv_rect(8, 17, 224, 1, RGB(44, 44, 48));
    draw_knob(0, 34, 21, now);
    for (i = 0; i < eng.v.ncab; i++) {
        const rnd_cable *c = &eng.v.cab[i];
        int32_t x, y;
        if (c->src != ui.src)
            continue;
        x = 78 + (n / 4) * 80;
        y = 28 + (n % 4) * 15;
        jack(x, y + 5, 0);
        cv_line(x + 6, y + 5, x + 10, y + 5, C_LED);
        cv_text(x + 12, y, &FONT_XS, RND_DST_NAME[c->dst], C_LABEL);
        n++;
    }
    if (!n)
        cv_text(70, 40, &FONT_XS, "TAP A WHITE KEY: IN", C_SUB);
    cv_text(10, 93, &FONT_XS, "WHITE KEY: PATCH / PULL", C_SUB);
    cv_text(178, 93, &FONT_XS, "REC: ALL", C_SUB);
}

/* eine weiße Taste allein: der Eingang und was in ihn geht */
static void draw_in(void)
{
    int i, n = 0;
    jack(16, 8, cables_into(ui.in_show) > 0);
    cv_text(28, 3, &FONT_XS, "IN", C_SUB);
    cv_text(46, 3, &FONT_XS, RND_DST_NAME[(int)ui.in_show], C_LABEL);
    cv_rect(8, 17, 224, 1, RGB(44, 44, 48));
    for (i = 0; i < eng.v.ncab; i++) {
        const rnd_cable *c = &eng.v.cab[i];
        char t[24] = "";
        if (c->dst != ui.in_show)
            continue;
        text_cat(t, RND_SRC_NAME[c->src]);
        text_cat(t, c->amt < 0.0f ? "  -" : "  +");
        int_text((int32_t)((c->amt < 0.0f ? -c->amt : c->amt) * 100.0f + 0.5f), t + txt_len(t));
        jack(24, 34 + n * 15, 1);
        cv_text(36, 29 + n * 15, &FONT_XS, t, C_LABEL);
        n++;
    }
    if (!n)
        cv_text(10, 40, &FONT_XS, "NOTHING PATCHED. HOLD A BLACK KEY", C_SUB);
    cv_text(10, 93, &FONT_XS, "HOLD A BLACK KEY (OUT), TAP THIS ONE", C_SUB);
}

static void draw_middle(uint32_t now)
{
    uint32_t i;
    wood(110, 30);
    if (ui.src >= 0) {
        draw_patch(now);
        return;
    }
    if (ui.in_show >= 0 && now < ui.in_until) {
        draw_in();
        return;
    }
    {
        int32_t x = cv_text(10, 3, &FONT_XS, PAGE_NAME[ui.page], C_LABEL);
        cv_text(x + 8, 3, &FONT_XS, rnd_preset_name(set.preset), C_SUB);
    }
    draw_page_keys(6);
    cv_rect(8, 17, 224, 1, RGB(44, 44, 48));
    if (now < ui.big_until) {
        ctext(120, 24, &FONT_XS, ui.big[1], C_SUB);
        ctext(120, 40, &APP_BIG_FONT, ui.big[0], C_LABEL);
    } else {
        for (i = 0; i < 4u; i++)
            draw_knob((int)i, 34 + (int32_t)i * 57, 21, now);
    }
    draw_clock(93);
}


/* die zwei Reihen des DFAM: oben PITCH, unten VELOCITY, dazwischen die LED; darunter die Nummer */
static void draw_steps(int32_t s)
{
    uint32_t i;
    int32_t len = rnd_length(&eng);
    wood(100, 140);
    cv_text(9, 1, &FONT_XS, "P", C_SUB);
    cv_text(9, 50, &FONT_XS, "V", C_SUB);
    for (i = 0; i < RND_STEPS; i++) {
        int32_t cx = 31 + (int32_t)i * 26;
        const rnd_step *st = &eng.steps[i];
        int in = (int32_t)i < len, cur = (int32_t)i == s, held = 0, on = st->vel > 0.004f;
        char n[4];
        if (!in) {
            disc(cx, 17, 9, RGB(30, 30, 33));
            disc(cx, 62, 9, RGB(30, 30, 33));
            ctext(cx, 76, &FONT_XS, "-", RGB(60, 60, 64));
            continue;
        }
        if (ui.page >= PG_P1 && (int)(i / 4u) == (ui.page == PG_P2 || ui.page == PG_V2))   /* an K1..K4: Ring */
            disc(cx, ui.page < PG_V1 ? 17 : 62, 11, C_LABEL);
        moog_knob(cx, 17, 9, st->pitch, held, 1, 0);
        led(cx, 40, cur && on);
        if (!on)                         /* Velocity 0: der Step schweigt, die LED durchgestrichen */
            cv_line(cx - 4, 44, cx + 4, 36, C_SUB);
        moog_knob(cx, 62, 9, st->vel, held, 0, 0);
        n[0] = (char)('1' + i);
        n[1] = 0;
        ctext(cx, 76, &FONT_XS, n, held ? C_LED : cur ? C_LABEL : C_SUB);
    }
    {
        char t[20] = "";
        if (eng.v.eco)
            text_cat(t, "ECO ");
        text_cat(t, "CPU ");
        int_text((int32_t)plat_cpu_pct(), t + txt_len(t));
        text_cat(t, "%");
        if (plat_xruns()) {
            text_cat(t, " D");
            int_text((int32_t)plat_xruns(), t + txt_len(t));
        }
        cv_text(230 - text_w(&FONT_XS, t), 88, &FONT_XS, t, plat_cpu_pct() > 80u ? C_LED : RGB(70, 70, 76));
        if (eng.v.ncab) {                /* gesteckte Kabel */
            char c[12] = "";
            int_text(eng.v.ncab, c);
            text_cat(c, eng.v.ncab == 1 ? " CABLE" : " CABLES");
            cv_text(10, 88, &FONT_XS, c, C_LED);
        } else {
            cv_text(10, 88, &FONT_XS, "KEYS: PATCHBAY", RGB(70, 70, 76));
        }
    }
}

static void draw(void)
{
    uint32_t now = plat_ms();
    int32_t s = rnd_step_now(&eng);
    char t[24];
    /* Kopf: der Name wie auf dem Panel, die Trigger-LED, Tempo und Transport; oder eine Meldung */
    cv_begin(240, 30, C_PANEL);
    wood(30, 0);
    if (now < ui.msg_until) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], C_LABEL);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], C_LED);
    } else {
        int32_t x = cv_text(10, 4, &FONT_B, "RAND", C_LABEL);
        cv_rect(x + 5, 5, 1, 18, C_LABEL);
        cv_text(x + 10, 4, &FONT_XS, "PERCUSSION", C_LABEL);
        cv_text(x + 10, 14, &FONT_XS, "SYNTHESIZER", C_LABEL);
        led(158, 14, rnd_level(&eng) > 0.3f);
        t[0] = 0;
        if (eng.ext) {
            text_cat(t, "EXT ");
            int_text((int32_t)(rnd_bpm(&eng) + 0.5f), t + 4);
        } else {
            int_text(set.bpm, t);
        }
        cv_text(214 - text_w(&FONT_XS, t), 9, &FONT_XS, t, eng.ext ? C_LED : C_LABEL);
        if (ui.playing) {
            int32_t i;
            for (i = 0; i < 6; i++)
                cv_rect(219 + i, 9 + i, 1, 12 - 2 * i, C_LED);
        } else {
            cv_rect(218, 9, 10, 10, C_SUB);
        }
    }
    cv_rect(5, 29, 230, 1, RGB(44, 44, 48));
    cv_blit(0, 0);
    cv_begin(240, 110, C_PANEL);
    draw_middle(now);
    cv_blit(0, 30);
    cv_begin(240, 100, C_PANEL);
    draw_steps(s);
    cv_blit(0, 140);
}

/* ------------------------------------------------------------ der Vertrag --- */
static float fx_rev[RND_REV_FLOATS] __attribute__((section(".pool")));
static int16_t fx_dl[RND_DLY_FRAMES] __attribute__((section(".pool")));
static int16_t fx_dr[RND_DLY_FRAMES] __attribute__((section(".pool")));

void app_init(void)
{
    rnd_init(&eng, SR);
    rnd_fx_memory(&eng, fx_rev, fx_dl, fx_dr);
    settings_load();
    ui.src = -1;
    ui.in_show = -1;
}

static void midi_in(void)
{
    uint32_t pkt;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
        switch (st) {
        case 0xF8u: rnd_midi_clock(&eng); break;
        case 0xFAu: rnd_start(&eng); ui.playing = 1; break;
        case 0xFBu: rnd_continue(&eng); ui.playing = 1; break;
        case 0xFCu: rnd_stop(&eng); ui.playing = 0; break;
        default:
            if ((st & 0xF0u) == 0x90u && d2)      /* eine Note: die Stimme, C4 = Grundton, ±2 Oktaven */
                rnd_trigger(&eng, 0.5f + ((float)d1 - 60.0f) / 48.0f, (float)d2 / 127.0f);
            break;
        }
    }
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    static float bl[256], br[256];
    float gain = (float)master_q12 * (1.0f / 4096.0f) * 8300000.0f;
    uint32_t i;
    if (n > 256u)
        n = 256u;
    if (play_req >= 0) {
        if (play_req)
            rnd_start(&eng);
        else
            rnd_stop(&eng);
        play_req = -1;
    }
    if (trig_req) {
        trig_req = 0;
        rnd_trigger(&eng, trig_pitch, trig_vel);
    }
    midi_in();
    rnd_render(&eng, bl, br, (int)n);
    for (i = 0; i < n; i++) {
        lr[2 * i] = (int32_t)(bl[i] * gain);
        lr[2 * i + 1] = (int32_t)(br[i] * gain);
    }
}

void app_panic(void)
{
    ui.playing = 0;
    rnd_stop(&eng);
}

void app_splash(void)                    /* das Panel: der Name, acht rote LEDs, die Holzwangen */
{
    uint32_t i;
    lcd_fill(0, 0, 240, 240, C_PANEL);
    lcd_fill(0, 0, 5, 240, C_WOOD);
    lcd_fill(235, 0, 5, 240, C_WOOD);
    cv_begin(240, 144, C_PANEL);
    wood(144, 48);
    ctext(120, 26, &APP_BIG_FONT, "RAND", C_LABEL);
    cv_rect(60, 64, 120, 1, C_SUB);
    ctext(120, 70, &FONT_XS, "PERCUSSION SYNTHESIZER", C_LABEL);
    for (i = 0; i < 8u; i++)
        led(50 + (int32_t)i * 20, 100, 1);
    ctext(120, 120, &FONT_XS, "by swtdr34mz", C_SUB);
    cv_blit(0, 48);
}

void app_ui_init(void)
{
    lcd_fill(0, 0, 240, 240, C_PANEL);
    ui.src = -1;
    ui.in_show = -1;
    ui.step_drawn = -2;
    ui.redraw = 1;
}

void app_frame(void)
{
    uint32_t now = plat_ms();
    int32_t s, lv;
    input();
    lights();
    if (ann_on() && ann_where()) {
        char t[48] = "RAND. ";
        text_cat(t, PAGE_NAME[ui.page]);
        text_cat(t, ". ");
        text_cat(t, rnd_preset_name(set.preset));
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(now);
    ui.playing = (uint8_t)(rnd_running(&eng) || play_req == 1);
    s = rnd_step_now(&eng);
    lv = (int32_t)(rnd_level(&eng) * 8.0f);
    if (s != ui.step_drawn || lv != ui.lvl_drawn) {
        ui.step_drawn = s;
        ui.lvl_drawn = lv;
        ui.redraw = 1;
    }
    if ((ui.msg_until && now >= ui.msg_until) || (ui.big_until && now >= ui.big_until)) {
        if (now >= ui.msg_until)
            ui.msg_until = 0;
        if (now >= ui.big_until)
            ui.big_until = 0;
        ui.redraw = 1;
    }
    if (now - ui.load_at >= 500u) {
        uint32_t l = plat_cpu_pct() | plat_xruns() << 8;
        ui.load_at = now;
        if (l != ui.load_shown) {
            ui.load_shown = l;
            ui.redraw = 1;
        }
        /* Absicherung: bleibt die Last eine Sekunde über 88 %, rechnet die Stimme ohne Oversampling (ECO) weiter,
         * bevor es Aussetzer gibt. Bis zum Ausschalten. */
        if (plat_cpu_pct() > 88u && !eng.v.eco) {
            if (++ui.hot >= 2) {
                eng.v.eco = 1;
                app_say("ECO MODE", "CPU HIGH");
            }
        } else {
            ui.hot = 0;
        }
    }
    /* gespeichert nur, wenn der Sequencer steht: Flash schreiben hält den Audio-Interrupt an (Aussetzer) */
    if (!ui.playing && set_dirty && now - dirty_at > 1500u)
        settings_save();
    if (ui.redraw) {
        ui.redraw = 0;
        draw();
    }
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "playing"))
        *got = ui.playing;
    else if (!strcmp(what, "step"))
        *got = rnd_step_now(&eng);
    else if (!strcmp(what, "page"))
        *got = ui.page;
    else if (!strcmp(what, "preset"))
        *got = set.preset;
    else if (!strcmp(what, "bpm"))
        *got = set.bpm;
    else if (!strcmp(what, "length"))
        *got = rnd_length(&eng);
    else if (!strcmp(what, "clock"))
        *got = rnd_clock_index(&eng);
    else if (!strcmp(what, "override"))
        *got = eng.clock_override;
    else if (!strcmp(what, "cables"))
        *got = eng.v.ncab;
    else if (!strcmp(what, "eco"))
        *got = eng.v.eco;
    else if (!strcmp(what, "src"))
        *got = ui.src;
    else if (!strcmp(what, "dirty"))
        *got = set_dirty;
    else if (!strcmp(what, "ext"))
        *got = eng.ext;
    else if (what[0] == 'v' && what[1] >= '0' && what[1] <= '9')   /* vN: Regler N (0..127) */
        *got = set.v[atoi(what + 1)];
    else if (!strncmp(what, "pitch", 5))
        *got = set.pitch[atoi(what + 5) - 1];
    else if (!strncmp(what, "vel", 3))
        *got = set.vel[atoi(what + 3) - 1];
    else
        return 0;
    return 1;
}
#endif
