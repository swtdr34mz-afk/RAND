#include "rand_engine.h"

namespace rnd {

void Engine::init(float sampleRate) {
    sr_ = sampleRate;
    voice_.init(sampleRate);
    for (int i = 0; i < P_COUNT; ++i) p_[i] = kParamInfo[i].def;
}

void Engine::setParam(int id, float norm) {
    if (id < 0 || id >= P_COUNT) return;
    p_[id] = clampf(norm, 0.f, 1.f);
    if (id < P_TEMPO) voice_.setParam(id, p_[id]);
}

float Engine::bpm() const {
    if (extSync_ && extBpm_ > 0.0) return (float)extBpm_;
    return 40.f + p_[P_TEMPO] * 240.f;
}

int Engine::clockMultIndex() const {
    int i = (int)(p_[P_CLOCK_MULT] * (kClockMultCount - 1) + 0.5f);
    return i < 0 ? 0 : (i >= kClockMultCount ? kClockMultCount - 1 : i);
}

int Engine::length() const {
    int l = 1 + (int)(p_[P_SEQ_LENGTH] * (kSteps - 1) + 0.5f);
    return l < 1 ? 1 : (l > kSteps ? kSteps : l);
}

double Engine::samplesPerStep() const {
    // 1 Step = 16tel bei x1
    const double stepsPerSec = bpm() / 60.0 * 4.0 * kClockMults[clockMultIndex()];
    double s = sr_ / stepsPerSec;
    return s < 4.0 ? 4.0 : s;
}

void Engine::start() { running_ = true; cur_ = -1; toNext_ = 0.0; }
void Engine::stop() { running_ = false; }
void Engine::cont() { running_ = true; }

void Engine::midiClock() {
    // 24 Pulse pro Viertel -> Tempo aus dem Abstand messen, leicht geglättet
    if (lastClockAt_ >= 0.0) {
        const double d = sampleCounter_ - lastClockAt_;
        if (d > 0.0) {
            const double b = sr_ * 60.0 / (d * 24.0);
            extBpm_ = extBpm_ <= 0.0 ? b : extBpm_ * 0.9 + b * 0.1;
        }
    }
    lastClockAt_ = sampleCounter_;
}

void Engine::advance() {
    cur_ = (cur_ + 1) % length();
    const Step& s = steps_[cur_];
    if (s.on) voice_.trigger(s.pitch, s.vel);
}

void Engine::process(float* out, int n) {
    int done = 0;
    while (done < n) {
        int chunk = n - done;
        if (running_) {
            if (toNext_ <= 0.0) {
                advance();
                toNext_ += samplesPerStep();
            }
            const int untilStep = (int)std::ceil(toNext_);
            if (untilStep < chunk) chunk = untilStep < 1 ? 1 : untilStep;
        }
        voice_.process(out + done, chunk);
        if (running_) toNext_ -= chunk;
        done += chunk;
    }
    sampleCounter_ += n;
}

} // namespace rnd
