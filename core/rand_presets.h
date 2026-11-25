// RAND – Werks-Presets (Klang + 8-Step-Pattern)
#pragma once
#include "rand_engine.h"

namespace rnd {

struct PresetParam { int id; float value; };

struct Preset {
    const char* name;
    const PresetParam* params;   // nur Abweichungen vom Standardwert
    int paramCount;
    float pitch[kSteps];
    float vel[kSteps];
    unsigned char gates;         // Bit 0 = Step 1
};

extern const Preset kPresets[];
extern const int kPresetCount;

void loadPreset(Engine& e, int index);

} // namespace rnd
