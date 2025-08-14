#include "vad.hpp"
#include <algorithm>
#include <cmath>

static inline float rms_block(const float* x, std::size_t n) {
    if (!x || n == 0) return 0.0f;
    double acc = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        double s = x[i];
        acc += s * s;
    }
    return static_cast<float>(std::sqrt(acc / static_cast<double>(n)));
}

bool EnergyVAD::process_frame(const float* samples, std::size_t frames) {
    // Compute RMS of this callback frame (already short ~10–30ms)
    float r = rms_block(samples, frames);
    float db = (r > 0.0f) ? 20.0f * std::log10(r) : -100.0f;

    // EMA smoothing with faster decay when we are currently in speech
    const float alpha_attack  = 0.30f;
    const float alpha_release = state_ ? 0.20f : 0.05f;
    if (db > ema_) ema_ = alpha_attack * db + (1.0f - alpha_attack) * ema_;
    else           ema_ = alpha_release * db + (1.0f - alpha_release) * ema_;

    const bool enter_speech = (ema_ >= attack_db_);
    const bool keep_speech  = (ema_ >= release_db_);

    if (!state_) {
        if (enter_speech) {
            state_ = true;
            hang_samples_left_ = hangover_samples_total_;
        }
    } else {
        if (keep_speech) {
            hang_samples_left_ = hangover_samples_total_;
        } else {
            // Count down by the number of samples processed in this callback
            hang_samples_left_ -= static_cast<int>(frames);
            if (hang_samples_left_ <= 0) {
                state_ = false;
                hang_samples_left_ = 0;
            }
        }
    }

    return state_;
}

