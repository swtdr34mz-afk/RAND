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
 *   SELECT Tempo     ALGORITHM   CLOCK /4 .. x128     OCT- / OCT+   Länge 1..8     ARP   RAND: neue Zufalls-Reihen
 *   PRESET die 64 Patterns (Klang, Reihen, Tempo, Kabel); beim Spielen wechselt es, sobald die Reihe bei Step 1 ist
 *   SAVE   speichert auf den gewählten Platz; gehalten + PRESET: auf einen anderen Platz
 *   REC    kurz: TRIGGER (ein Schlag); 1 s: der Platz wie gespeichert; 3 s (Plätze 1-8): ab Werk
 *   MIDI Program Change 0-63: die Patterns 1-64
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
    return set.magic == SET_MAGIC && set.bpm >= 40u && set.bpm <= 280u && set.preset < 64u;
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

static void text_cat(char *d, const char *s);

/* ------------------------------------------------------------ die 64 Patterns --- */
/* Ein Pattern ist alles, was RAND gerade spielt: Klang (alle Regler), die beiden Reihen, das Tempo und die Kabel.
 * 64 Plätze, die ersten acht ab Werk mit den Presets belegt. Gespeichert in zwei Speicherobjekten zu je 32; auf den
 * Flash geschrieben wird erst, wenn der Sequencer steht (Schreiben hielte den Ton an). */
#define NPAT 64
#define PAT_HALF 32
#define PAT_MAGIC 0x31544150u            /* "PAT1" */
enum { PAT_EMPTY, PAT_FACTORY, PAT_USER };
typedef struct {
    uint8_t kind;                        /* PAT_* */
    uint8_t v[RP_COUNT];
    uint8_t pitch[RND_STEPS], vel[RND_STEPS];
    uint8_t bpm40, ncab;                 /* Tempo - 40 */
    uint8_t cab_src[RND_NCABLES], cab_dst[RND_NCABLES], cab_amt[RND_NCABLES];
} pat_t;
typedef struct {
    uint32_t magic;
    pat_t p[PAT_HALF];
} pat_half_t;
_Static_assert(sizeof(pat_half_t) <= PLAT_STORE_MAX, "32 Patterns passen nicht in ein Speicherobjekt");
static pat_half_t bank[2];
static uint8_t bank_dirty[2];

static pat_t *pat(uint32_t i) { return &bank[i / PAT_HALF].p[i % PAT_HALF]; }

static void pat_from_set(pat_t *p, uint8_t kind)
{
    uint32_t i;
    p->kind = kind;
    for (i = 0; i < RP_COUNT; i++)
        p->v[i] = set.v[i];
    for (i = 0; i < RND_STEPS; i++) {
        p->pitch[i] = set.pitch[i];
        p->vel[i] = set.vel[i];
    }
    p->bpm40 = (uint8_t)(set.bpm - 40u);
    p->ncab = set.ncab;
    for (i = 0; i < RND_NCABLES; i++) {
        p->cab_src[i] = set.cab_src[i];
        p->cab_dst[i] = set.cab_dst[i];
        p->cab_amt[i] = set.cab_amt[i];
    }
}

static void set_from_pat(const pat_t *p)
{
    uint32_t i;
    for (i = 0; i < RP_COUNT; i++)
        set.v[i] = p->v[i];
    for (i = 0; i < RND_STEPS; i++) {
        set.pitch[i] = p->pitch[i];
        set.vel[i] = p->vel[i];
    }
    set.bpm = (uint16_t)(p->bpm40 + 40u);
    set.ncab = p->ncab;
    for (i = 0; i < RND_NCABLES; i++) {
        set.cab_src[i] = p->cab_src[i];
        set.cab_dst[i] = p->cab_dst[i];
        set.cab_amt[i] = p->cab_amt[i];
    }
}

static int pat_ok(const pat_t *p)
{
    uint32_t i;
    if (p->kind > PAT_USER || p->ncab > RND_NCABLES || p->bpm40 > 240u)
        return 0;
    for (i = 0; i < RP_COUNT; i++)
        if (p->v[i] > 127u)
            return 0;
    for (i = 0; i < p->ncab; i++)
        if (p->cab_src[i] >= RND_NSRC || p->cab_dst[i] >= RND_NDST)
            return 0;
    return 1;
}

/* Werkszustand eines Platzes: 1-8 die Presets, die anderen leer */
static void pat_factory(uint32_t i)
{
    if (i >= RND_NPRESETS) {
        memset(pat(i), 0, sizeof(pat_t));
        return;
    }
    {
        settings_t keep = set;               /* (die Presets kommen über die Engine; der Klang jetzt bleibt) */
        rnd_preset_load(&eng, (int)i);
        settings_from_engine();
        pat_from_set(pat(i), PAT_FACTORY);
        set = keep;
        engine_from_settings();
    }
}

static void bank_load(void)
{
    uint32_t h, i;
    for (h = 0; h < 2u; h++) {
        int ok = plat_store_load(OBJ_PAT_A + h, &bank[h], sizeof bank[h]) == (int)sizeof bank[h]
                 && bank[h].magic == PAT_MAGIC;
        for (i = 0; ok && i < PAT_HALF; i++)
            if (!pat_ok(&bank[h].p[i]))
                ok = 0;
        if (!ok) {                       /* der erste Start (oder etwas Kaputtes): ab Werk */
            bank[h].magic = PAT_MAGIC;
            for (i = 0; i < PAT_HALF; i++)
                pat_factory(h * PAT_HALF + i);
        }
    }
}

static void bank_save(void)
{
    uint32_t h;
    for (h = 0; h < 2u; h++)
        if (bank_dirty[h] && plat_store_save(OBJ_PAT_A + h, &bank[h], sizeof bank[h]) == 0)
            bank_dirty[h] = 0;
}

static const char *pat_name(uint32_t i)
{
    static char t[24];
    char n[4];
    const pat_t *p = pat(i);
    n[0] = (char)('0' + (i + 1u) / 10u);
    n[1] = (char)('0' + (i + 1u) % 10u);
    n[2] = 0;
    t[0] = 0;
    text_cat(t, n);
    text_cat(t, " ");
    text_cat(t, p->kind == PAT_FACTORY ? rnd_preset_name((int)i) : p->kind == PAT_USER ? "USER" : "EMPTY");
    return t;
}

static int pat_edited(void)              /* der Klang jetzt weicht vom gewählten Platz ab */
{
    pat_t now;
    const pat_t *p = pat(set.preset);
    if (p->kind == PAT_EMPTY)
        return 0;
    memset(&now, 0, sizeof now);
    pat_from_set(&now, p->kind);
    return memcmp(&now, p, sizeof now) != 0;
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
    uint32_t rec_t, save_t;
    uint8_t rec_stage;                   /* REC gehalten: 1 neu geladen, 2 ab Werk */
    uint8_t save_to;                     /* SAVE gehalten + PRESET: der Zielplatz */
    int8_t next, prev_step;              /* das nächste Pattern, sobald die Reihe wieder vorn ist (-1: keins) */
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

static volatile int8_t pc_req = -1;      /* MIDI Program Change 0..63 (aus dem Audio-Interrupt) */

/* ein Platz wird gespielt: sein Klang, seine Reihen, sein Tempo, seine Kabel (ein leerer: nur gewählt) */
static void pattern_apply(uint32_t p)
{
    set.preset = (uint8_t)p;
    ui.next = -1;
    if (pat(p)->kind != PAT_EMPTY) {
        set_from_pat(pat(p));
        engine_from_settings();
        if (!ui.playing)
            hit(eng.steps[0].pitch, 1.0f);
    }
    dirty();
    ui.redraw = 1;
}

/* gewählt: gestoppt sofort, beim Spielen, sobald die Reihe wieder bei Step 1 ist (im Takt) */
static void pattern_select(uint32_t p)
{
    if (ui.playing) {
        ui.next = (int8_t)p;
        app_say("NEXT", pat_name(p));
    } else {
        pattern_apply(p);
        app_say("PATTERN", pat(p)->kind == PAT_EMPTY ? "EMPTY: SAVE HERE" : pat_name(p));
    }
}

static void pattern_save(uint32_t to)
{
    pat_from_set(pat(to), PAT_USER);
    bank_dirty[to / PAT_HALF] = 1;
    set.preset = (uint8_t)to;
    dirty();
    app_say("SAVED", pat_name(to));
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
            ui.rec_t = plat_ms();        /* kurz: TRIGGER; 1 s: der Platz neu geladen; 3 s: ab Werk */
            ui.rec_stage = 0;
        }
        break;
    case B_SAVE:                         /* loslassen: gespeichert; gehalten + PRESET: auf einen anderen Platz */
        ui.save_t = plat_ms();
        ui.save_to = set.preset;
        break;
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
    if (ui.rec_t) {
        uint32_t held = plat_ms() - ui.rec_t;
        if (!(btn >> B_REC & 1u)) {      /* losgelassen: kurz war es TRIGGER */
            if (!ui.rec_stage)
                hit(eng.steps[0].pitch, 1.0f);
            ui.rec_t = 0;
        } else if (held >= 1000u && ui.rec_stage == 0) {   /* 1 s: der Platz, wie er gespeichert ist */
            ui.rec_stage = 1;
            if (pat(set.preset)->kind != PAT_EMPTY) {
                pattern_apply(set.preset);
                app_say("RELOADED", pat_name(set.preset));
            }
        } else if (held >= 3000u && ui.rec_stage == 1 && set.preset < RND_NPRESETS) {   /* 3 s: ab Werk */
            ui.rec_stage = 2;
            pat_factory(set.preset);
            bank_dirty[set.preset / PAT_HALF] = 1;
            pattern_apply(set.preset);
            app_say("FACTORY", pat_name(set.preset));
        }
    }
    if (ui.save_t && !(btn >> B_SAVE & 1u)) {
        ui.save_t = 0;
        pattern_save(ui.save_to);
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
        int32_t d = accel(EN_PRESET, e, 2, 4);
        if (ui.save_t) {                 /* SAVE gehalten: der Zielplatz */
            ui.save_to = (uint8_t)(((int32_t)ui.save_to + d % NPAT + NPAT) % NPAT);
            app_say("SAVE TO", pat_name(ui.save_to));
        } else {
            int32_t from = ui.next >= 0 ? ui.next : (int32_t)set.preset;
            pattern_select((uint32_t)((from + d % NPAT + NPAT) % NPAT));
        }
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

/* ---- geglättetes Zeichnen: das Display hat feste 240 x 240 Pixel, also bekommen Kanten Zwischentöne statt
 *      Treppen (jedes Randpixel mischt die Farbe nach dem Anteil, den die Form von ihm bedeckt) ---- */
static inline void aa_px(int32_t x, int32_t y, uint16_t c, int32_t a)   /* a 0..256 */
{
    uint32_t i, d, r, g, b;
    if (a <= 0)
        return;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1)
        return;
    i = (uint32_t)y * cv_w + (uint32_t)x;
    if (a >= 256) {
        cv_px[i] = swap16(c);
        return;
    }
    d = swap16(cv_px[i]);
    r = (((c >> 11) & 31u) * (uint32_t)a + ((d >> 11) & 31u) * (uint32_t)(256 - a)) >> 8;
    g = (((c >> 5) & 63u) * (uint32_t)a + ((d >> 5) & 63u) * (uint32_t)(256 - a)) >> 8;
    b = ((c & 31u) * (uint32_t)a + (d & 31u) * (uint32_t)(256 - a)) >> 8;
    cv_px[i] = swap16((r << 11) | (g << 5) | b);
}

/* eine Kreisfläche (Mittelpunkt und Radius in Pixeln, auch krumm), alpha 0..256 */
static void aa_disc_a(float cx, float cy, float r, uint16_t c, int32_t alpha)
{
    int32_t x, y, x0 = (int32_t)(cx - r - 1.0f), x1 = (int32_t)(cx + r + 2.0f);
    int32_t y0 = (int32_t)(cy - r - 1.0f), y1 = (int32_t)(cy + r + 2.0f);
    float in2 = (r - 0.5f) * (r - 0.5f), out2 = (r + 0.5f) * (r + 0.5f);
    if (r < 0.5f)
        in2 = 0.0f;
    for (y = y0; y <= y1; y++) {
        float dy = (float)y - cy;
        for (x = x0; x <= x1; x++) {
            float dx = (float)x - cx, d2 = dx * dx + dy * dy;
            if (d2 >= out2)
                continue;
            if (d2 <= in2)
                aa_px(x, y, c, alpha);
            else
                aa_px(x, y, c, (int32_t)((r + 0.5f - fm_sqrtf(d2)) * (float)alpha));
        }
    }
}
static void aa_disc(float cx, float cy, float r, uint16_t c) { aa_disc_a(cx, cy, r, c, 256); }

/* eine Linie mit runden Enden, w Pixel breit */
static void aa_line(float x0, float y0, float x1, float y1, float w, uint16_t c)
{
    float dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy, h = w * 0.5f;
    int32_t x, y, bx0 = (int32_t)(fm_minf(x0, x1) - h - 1.0f), bx1 = (int32_t)(fm_maxf(x0, x1) + h + 2.0f);
    int32_t by0 = (int32_t)(fm_minf(y0, y1) - h - 1.0f), by1 = (int32_t)(fm_maxf(y0, y1) + h + 2.0f);
    for (y = by0; y <= by1; y++)
        for (x = bx0; x <= bx1; x++) {
            float px = (float)x - x0, py = (float)y - y0, t = l2 > 0.0f ? (px * dx + py * dy) / l2 : 0.0f, ex, ey, d;
            t = fm_clampf(t, 0.0f, 1.0f);
            ex = px - t * dx;
            ey = py - t * dy;
            d = fm_sqrtf(ex * ex + ey * ey);
            if (d < h + 0.5f)
                aa_px(x, y, c, (int32_t)(fm_clampf(h + 0.5f - d, 0.0f, 1.0f) * 256.0f));
        }
}

static void disc(int32_t cx, int32_t cy, int32_t r, uint16_t c) { aa_disc((float)cx, (float)cy, (float)r + 0.3f, c); }

/* ein Regler: Skala (11 Striche über 270°), schwarzer Körper mit Rand, Silberkappe mit Glanz, weißer Zeiger. v 0..1 */
static void moog_knob(int32_t cx, int32_t cy, int32_t r, float v, int hot, int bip, int ticks)
{
    int32_t i;
    float a, sn, cs, fx = (float)cx, fy = (float)cy, fr = (float)r + 0.3f, cap = fr * 0.55f;
    if (ticks)
        for (i = 0; i <= 10; i++) {
            float r0 = fr + 2.2f, r1 = fr + (i == 5 && bip ? 5.6f : 4.6f);
            a = (-135.0f + 27.0f * (float)i) * 0.017453293f;
            sn = fm_sinf(a);
            cs = fm_cosf(a);
            aa_line(fx + sn * r0, fy - cs * r0, fx + sn * r1, fy - cs * r1, 1.1f, i == 5 && bip ? C_LABEL : C_TICK);
        }
    aa_disc(fx, fy, fr, hot ? C_SUB : C_KEDGE);
    aa_disc(fx, fy, fr - 1.2f, C_KBODY);
    aa_disc_a(fx - fr * 0.2f, fy - fr * 0.25f, fr * 0.75f, RGB(60, 60, 66), 70);   /* ein Hauch Licht auf dem Rock */
    a = (-135.0f + 270.0f * fm_clampf(v, 0.0f, 1.0f)) * 0.017453293f;
    sn = fm_sinf(a);
    cs = fm_cosf(a);
    aa_line(fx + sn * (cap + 1.0f), fy - cs * (cap + 1.0f), fx + sn * (fr - 1.6f), fy - cs * (fr - 1.6f),
            r >= 12 ? 2.4f : 1.8f, C_LABEL);   /* der Zeiger auf dem Rock */
    aa_disc(fx, fy, cap, C_SILVER);
    aa_disc_a(fx - cap * 0.25f, fy - cap * 0.3f, cap * 0.62f, C_SILVER_HI, 200);
}

/* ein Kippschalter: Mutter, Hebel nach oben (an) oder unten, die zwei Namen */
static void toggle(int32_t cx, int32_t cy, int on, const char *up, const char *dn, int hot)
{
    float fx = (float)cx, fy = (float)cy, tip = on ? fy - 11.0f : fy + 11.0f;
    ctext(cx, cy - 24, &FONT_XS, up, on ? C_LABEL : C_SUB);
    ctext(cx, cy + 13, &FONT_XS, dn, on ? C_SUB : C_LABEL);
    aa_disc(fx, fy, 6.3f, hot ? C_SUB : C_KEDGE);
    aa_disc(fx, fy, 4.3f, C_SILVER);
    aa_line(fx, fy, fx, tip, 3.0f, C_SILVER_HI);
    aa_disc(fx, tip, 2.4f, C_SILVER_HI);
}

/* eine LED: aus dunkelrot, an hell mit einem Hof und einem Glanzpunkt */
static void led(int32_t cx, int32_t cy, int on)
{
    float fx = (float)cx, fy = (float)cy;
    if (on) {
        aa_disc_a(fx, fy, 5.5f, C_LED, 60);
        aa_disc(fx, fy, 3.3f, C_LED);
        aa_disc_a(fx - 0.8f, fy - 1.0f, 1.2f, RGB(255, 200, 190), 220);
    } else {
        aa_disc(fx, fy, 3.3f, C_LED_DIM);
    }
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
    int32_t x = 232 - (int32_t)NPAGES * 6 - 4;
    for (p = 0; p < NPAGES; p++) {
        if (p == NSOUND)
            x += 4;
        cv_rect(x, y + 1, 4, 4, p == ui.page ? C_LABEL : RGB(60, 60, 64));
        x += 6;
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
        aa_line((float)x + 6.0f, (float)y + 5.0f, (float)x + 10.0f, (float)y + 5.0f, 1.6f, C_LED);
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
        {   /* der Platz: Nummer und Name; zu lang für die Zeile: nur die Nummer */
            const char *nm = pat_name(set.preset);
            char num[4] = {nm[0], nm[1], 0, 0};
            x = cv_text(x + 8, 3, &FONT_XS, x + 8 + text_w(&FONT_XS, nm) > 146 ? num : nm, C_SUB);
        }
        if (pat_edited())
            x = cv_text(x + 2, 3, &FONT_XS, "*", C_LABEL);
        if (ui.next >= 0) {              /* wartet auf Step 1 */
            char t[12] = "> ";
            t[2] = (char)('0' + (ui.next + 1) / 10);
            t[3] = (char)('0' + (ui.next + 1) % 10);
            t[4] = 0;
            cv_text(x + 6, 3, &FONT_XS, t, C_LED);
        }
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
            aa_line((float)cx - 4.0f, 44.0f, (float)cx + 4.0f, 36.0f, 1.2f, C_SUB);
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
        cv_text(230 - text_w(&FONT_XS, t), 84, &FONT_XS, t, plat_cpu_pct() > 80u ? C_LED : RGB(70, 70, 76));
        if (eng.v.ncab) {                /* gesteckte Kabel */
            char c[12] = "";
            int_text(eng.v.ncab, c);
            text_cat(c, eng.v.ncab == 1 ? " CABLE" : " CABLES");
            cv_text(10, 84, &FONT_XS, c, C_LED);
        } else {
            cv_text(10, 84, &FONT_XS, "KEYS: PATCHBAY", RGB(70, 70, 76));
        }
    }
}

/* ------------------------------------------------------------------ der Akku --- */
/* Prozent aus der Spannung: die Messung des Akku-Teilers (fm1-base: Stufen bei 531 / 561 / 591 Zählern, voll um
 * 620 = 4.2 V) in Volt, dann die Entladekurve einer Li-Ionen-Zelle (unter Last flach in der Mitte, steil an den
 * Enden). Geglättet und nur in ganzen Schritten bewegt, damit die Zahl nicht flackert. Am FM-1 noch nicht
 * nachgemessen: ein Schätzwert, die Richtung stimmt. */
enum { BAT_NONE, BAT_CELL, BAT_CHARGING, BAT_USB };
static const float LI_V[9] = {3.40f, 3.50f, 3.60f, 3.70f, 3.80f, 3.90f, 4.00f, 4.10f, 4.20f};
static const float LI_P[9] = {0.0f, 6.0f, 15.0f, 30.0f, 48.0f, 64.0f, 78.0f, 90.0f, 100.0f};
static struct { float pct; int shown, mode; } bat = {-1.0f, -1, BAT_NONE};

static void battery_read(void)
{
    int32_t raw = plat_battery_raw(), ch = plat_battery_charging(), bars = plat_battery(), i;
    float v, p;
#ifdef PLAT_HOST
    {   /* der Simulator: RAND_BATT="Zähler,Laden" für die Bildschirmfotos (z.B. "548,0" oder "575,2") */
        const char *e = getenv("RAND_BATT");
        if (e) {
            raw = atoi(e);
            ch = strchr(e, ',') ? atoi(strchr(e, ',') + 1) : 0;
        }
    }
#endif
    if (raw <= 0 && bars < 0) {
        bat.mode = BAT_NONE;             /* (noch nicht gelesen, oder der Browser-Emulator) */
        return;
    }
    bat.mode = ch == 2 ? BAT_CHARGING : (bars == 4 || ch == 1) ? BAT_USB : BAT_CELL;
    if (raw > 0) {
        v = (float)raw * (4.2f / 620.0f);
        if (v <= LI_V[0])
            p = 0.0f;
        else if (v >= LI_V[8])
            p = 100.0f;
        else {
            for (i = 0; v > LI_V[i + 1]; i++)
                ;
            p = LI_P[i] + (LI_P[i + 1] - LI_P[i]) * (v - LI_V[i]) / (LI_V[i + 1] - LI_V[i]);
        }
    } else {
        p = bars >= 3 ? 100.0f : (float)bars * 33.0f;   /* ohne Messwert: die Stufen */
    }
    bat.pct = bat.pct < 0.0f ? p : bat.pct + (p - bat.pct) * 0.15f;
    i = (int32_t)(bat.pct + 0.5f);
    if (bat.shown < 0 || i > bat.shown + 1 || i < bat.shown - 1 || i == 0 || i == 100)
        bat.shown = (int)i;
}

/* der Akku im Kopf: ein klares Symbol (Rahmen, Pol, Füllung in Ampelfarben, beim Laden ein Blitz); unter 10 %
 * blinkt er */
static void draw_battery(int32_t x, int32_t y, uint32_t now)
{
    int32_t w = 30, h = 15, fill, pct = bat.shown;
    uint16_t c;
    if (bat.mode == BAT_NONE)
        return;
    cv_rect(x - 2, y - 3, 240 - 5 - (x - 2), h + 6, C_PANEL);   /* (über einer langen Meldung) */
    if (pct < 0)
        pct = 0;
    c = pct >= 40 ? RGB(70, 220, 110) : pct >= 15 ? RGB(255, 190, 40) : C_LED;
    if (bat.mode == BAT_CHARGING || bat.mode == BAT_USB)
        c = RGB(70, 220, 110);
    /* der Rahmen, 2 px, mit 1 px Luft zur Füllung; der Pol rechts */
    cv_rect(x, y, w, h, C_LABEL);
    cv_rect(x + 2, y + 2, w - 4, h - 4, C_PANEL);
    cv_rect(x + w, y + 4, 3, h - 8, C_LABEL);
    fill = (pct * (w - 6) + 50) / 100;
    if (pct > 0 && fill < 2)
        fill = 2;
    if (!(pct < 10 && bat.mode == BAT_CELL && (now / 500u) & 1u))
        cv_rect(x + 3, y + 3, fill, h - 6, c);
    if (bat.mode != BAT_CELL) {          /* ein Blitz: am Netz (8 x 10 Pixel, dunkel umrandet) */
        static const char *const BOLT[10] = {"....##..", "...##...", "..##....", ".##.....", "#######.",
                                             "....##..", "...##...", "..##....", ".##.....", "##......"};
        int32_t bx = x + w / 2 - 4, by = y + 2, r, q;
        for (r = 0; r < 10; r++)
            for (q = 0; q < 8; q++)
                if (BOLT[r][q] == '#')
                    cv_rect(bx + q - 1, by + r, 3, 1, C_PANEL);
        for (r = 0; r < 10; r++)
            for (q = 0; q < 8; q++)
                if (BOLT[r][q] == '#')
                    cv_rect(bx + q, by + r, 1, 1, C_LABEL);
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
        led(130, 14, rnd_level(&eng) > 0.3f);
        t[0] = 0;
        int_text((int32_t)((eng.ext ? rnd_bpm(&eng) : (float)set.bpm) + 0.5f), t);
        cv_text(176 - text_w(&FONT_B, t), 6, &FONT_B, t, eng.ext ? C_LED : C_LABEL);   /* rot: im Takt von außen */
        if (ui.playing) {
            int32_t i;
            for (i = 0; i < 6; i++)
                cv_rect(181 + i, 9 + i, 1, 12 - 2 * i, C_LED);
        } else {
            cv_rect(180, 10, 9, 9, C_SUB);
        }
    }
    draw_battery(199, 7, now);
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
    bank_load();
    ui.next = -1;
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
            if ((st & 0xF0u) == 0xC0u && d1 < NPAT)   /* Program Change: Pattern 1-64 */
                pc_req = (int8_t)d1;
            else if ((st & 0xF0u) == 0x90u && d2)      /* eine Note: die Stimme, C4 = Grundton, ±2 Oktaven */
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
        text_cat(t, pat_name(set.preset));
        ann_say(t, 0, ANN_NOW);
    }
    ann_tick(now);
    ui.playing = (uint8_t)(rnd_running(&eng) || play_req == 1);
    s = rnd_step_now(&eng);
    if (pc_req >= 0) {                   /* MIDI Program Change: ein Pattern */
        int8_t p = pc_req;
        pc_req = -1;
        pattern_select((uint32_t)p);
    }
    if (ui.next >= 0 && (!ui.playing || (s == 0 && ui.prev_step != 0)))
        pattern_apply((uint32_t)ui.next);
    ui.prev_step = (int8_t)s;
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
        uint32_t l;
        battery_read();
        l = plat_cpu_pct() | plat_xruns() << 8 | (uint32_t)(bat.shown & 0x7F) << 16 | (uint32_t)bat.mode << 23
            | ((bat.shown < 10 && bat.mode == BAT_CELL) ? (now / 500u & 1u) << 25 : 0u);
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
    if (!ui.playing && now - dirty_at > 1500u) {
        if (set_dirty)
            settings_save();
        if (bank_dirty[0] || bank_dirty[1])
            bank_save();
    }
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
    else if (!strcmp(what, "next"))
        *got = ui.next;
    else if (!strncmp(what, "kind", 4))  /* kindN: Platz N (1..64) leer / Werk / eigen */
        *got = pat((uint32_t)atoi(what + 4) - 1u)->kind;
    else if (!strcmp(what, "bankdirty"))
        *got = bank_dirty[0] | bank_dirty[1] << 1;
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
