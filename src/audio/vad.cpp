
#include "audio/vad.hpp"
#include <cmath>
#include <algorithm>

namespace sadhana {

VAD::VAD(const Config& config) : config_(config) {
}

void VAD::calibrate(const float* samples, size_t numSamples) {
    if (numSamples == 0) return;

    // Calculate RMS energy
    float sumSquares = 0.0f;
    for (size_t i = 0; i < numSamples; ++i) {
        sumSquares += samples[i] * samples[i];
    }
    float rms = std::sqrt(sumSquares / numSamples);

    // During calibration, we use a slower attack time to establish the noise floor
    if (noiseFloor_ == 0.0f) {
        noiseFloor_ = rms;
    } else {
        // Exponential moving average with very slow attack
        float alpha = config_.calibrationAttackFactor;
        noiseFloor_ = (1.0f - alpha) * noiseFloor_ + alpha * rms;
    }
}

bool VAD::process(const float* samples, size_t numSamples) {
    if (numSamples == 0) return speechActive_;

    // Calculate current RMS energy
    float sumSquares = 0.0f;
    for (size_t i = 0; i < numSamples; ++i) {
        sumSquares += samples[i] * samples[i];
    }
    float rms = std::sqrt(sumSquares / numSamples);

    // Convert to dB
    float dbFS = 20.0f * std::log10(rms + 1e-9f);
    float noiseFloorDB = 20.0f * std::log10(noiseFloor_ + 1e-9f);

    // Absolute minimum threshold to prevent triggering on very quiet sounds
    const float MIN_SPEECH_DB = -50.0f;
    if (dbFS < MIN_SPEECH_DB) {
        if (speechActive_) {
            auto now = std::chrono::steady_clock::now();
            auto timeSinceLastSpeech =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - lastSpeechTime_).count();
            if (timeSinceLastSpeech > config_.hangTimeMs) {
                speechActive_ = false;
            }
        }
        return speechActive_;
    }

    // Apply thresholds with hysteresis
    bool prevSpeechActive = speechActive_;
    auto now = std::chrono::steady_clock::now();

    if (!speechActive_) {
        auto timeSinceLastTrigger =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastTriggerTime_).count();

        // Attack threshold with minimum quiet period
        if (dbFS > (noiseFloorDB + config_.attackThreshold) &&
            timeSinceLastTrigger > 300) { // 300ms minimum gap between triggers
            speechActive_ = true;
            lastSpeechTime_ = now;
            lastTriggerTime_ = now;
        }
    } else {
        // Release threshold with hang time
        if (dbFS < (noiseFloorDB + config_.releaseThreshold)) {
            auto timeSinceLastSpeech =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - lastSpeechTime_).count();

            if (timeSinceLastSpeech > config_.hangTimeMs) {
                speechActive_ = false;
            }
        } else {
            lastSpeechTime_ = now;
        }
    }

    // Notify state changes
    if (prevSpeechActive != speechActive_ && stateChangeCallback_) {
        stateChangeCallback_(speechActive_);
    }

    return speechActive_;
}

} // namespace sadhana