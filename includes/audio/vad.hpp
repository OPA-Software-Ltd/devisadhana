
#pragma once
#include <chrono>
#include <functional>

namespace sadhana {

class VAD {
public:
    struct Config {
        float attackThreshold = 15.0f;
        float releaseThreshold = 10.0f;
        int hangTimeMs = 500;
        int calibrationMs = 2000;
        float calibrationAttackFactor = 0.05f;
        float calibrationReleaseAboveFloor = 8.0f;
    };

    explicit VAD(const Config& config);

    void calibrate(const float* samples, size_t numSamples);
    bool process(const float* samples, size_t numSamples);
    float getNoiseFloor() const { return noiseFloor_; }
    bool isSpeechActive() const { return speechActive_; }

    void setStateChangeCallback(std::function<void(bool)> callback) {
        stateChangeCallback_ = std::move(callback);
    }

private:
    Config config_;
    float noiseFloor_{0.0f};
    bool speechActive_{false};
    std::chrono::steady_clock::time_point lastSpeechTime_;
    static inline std::chrono::steady_clock::time_point lastTriggerTime_;
    std::function<void(bool)> stateChangeCallback_;
};

} // namespace sadhana