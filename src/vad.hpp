#pragma once
#include <cstddef>

// Energy-based VAD with hysteresis + hangover.
// Hangover is now tracked in **samples**, decremented by processed samples per callback.
class EnergyVAD {
public:
    explicit EnergyVAD(double sample_rate,
                       float attack_db = -45.0f,
                       float release_db = -55.0f,
                       int hangover_ms = 200,
                       int window_ms = 30)
        : sr_(sample_rate),
          attack_db_(attack_db),
          release_db_(release_db),
          hangover_samples_total_(static_cast<int>(hangover_ms * 1e-3 * sample_rate)),
          win_samples_(static_cast<int>(window_ms * 1e-3 * sample_rate)) {}

    bool process_frame(const float* samples, std::size_t frames);

    void set_thresholds(float attack_db, float release_db) {
        attack_db_ = attack_db;
        release_db_ = release_db;
    }
    void reset_ema(float db) { ema_ = db; }
    float ema_db() const { return ema_; }

private:
    double sr_;
    float attack_db_;
    float release_db_;
    int   hangover_samples_total_;
    int   win_samples_;

    int   hang_samples_left_ = 0; // counts down in **samples**
    bool  state_ = false;
    float ema_   = -100.0f;
};

