/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (RAND) */
/* RAND by swtdr34mz: ein rauer Percussion-Synth für das M-VAVE FM-1 (auf fm1-base), nach dem Konzept des
 * Behringer EDGE / Moog DFAM: eine Stimme, ein 8-Step-Sequencer mit zwei Reihen, ein Takt bis in den Audio-Bereich.
 * Dieser Header ist die App-Hälfte des Vertrags mit der Plattform (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "RAND"
#define APP_SPLASH_RGB RGB(236, 236, 240)
#define APP_SPLASH                       /* app_splash: RANDs eigener Startbildschirm */
#define APP_BLOCK 128u                   /* Frames pro Audioblock: 2.9 ms bei 44.1 kHz (Drums: knapp) */

/* der Speicher: die Einstellungen der App und die Panel-Kalibrierung (die der Plattform) */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u}

void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);
void app_splash(void);

#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
