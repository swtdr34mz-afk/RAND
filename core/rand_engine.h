// RAND – Engine = Stimme + 8-Step-Sequencer mit zwei Reihen (Pitch + Velocity).
// Diese Klasse ist die einzige Schnittstelle, die FM-1-Firmware und VST später benutzen.
#pragma once
#include "rand_voice.h"

namespace rnd {

struct Step {
    float pitch = 0.5f;   // 0..1, 0.5 = Grundton
    float vel = 0.8f;     // 0..1
    bool on = true;
};

class Engine {
public:
    void init(float sampleRate);

    // Parameter (normiert 0..1), Ids aus rand_params.h
    void setParam(int id, float norm);
    float getParam(int id) const { return p_[id]; }

    Step& step(int i) { return steps_[i & 7]; }
    const Step& step(int i) const { return steps_[i & 7]; }

    // Transport
    void start();             // von Step 1
    void stop();
    void cont();              // weiter, wo gestoppt
    bool running() const { return running_; }
    int currentStep() const { return cur_; }     // für Display/LEDs

    // MIDI-Sync (Start/Stop über start()/stop(); clock() bei jedem 0xF8)
    void midiClock();
    void setExternalSync(bool on) { extSync_ = on; }

    // Einzelschlag von Hand (Taste / Trigger-Knopf), Sequencer läuft unabhängig weiter
    void triggerManual(float pitch, float vel) { voice_.trigger(pitch, vel); }

    void process(float* out, int n);             // mono

    float bpm() const;
    int clockMultIndex() const;
    int length() const;

private:
    void advance();
    double samplesPerStep() const;

    float sr_ = 48000.f;
    float p_[P_COUNT] = {};
    Voice voice_;
    Step steps_[kSteps];

    bool running_ = false;
    int cur_ = -1;
    double toNext_ = 0.0;     // Samples bis zum nächsten Step

    // externe Clock
    bool extSync_ = false;
    double sampleCounter_ = 0.0, lastClockAt_ = -1.0;
    double extBpm_ = 0.0;
};

} // namespace rnd
