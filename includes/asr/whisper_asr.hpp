#pragma once

#include <string>
#include <vector>
#include <functional>
#include "whisper.h"

namespace sadhana {

class WhisperASR {
public:
    struct Config {
        std::string modelPath = "models/ggml-base.bin";
        std::string language = "en";
        bool translateToEnglish = false;
        int threadCount = 4;
    };

    explicit WhisperASR(const Config& config);
    ~WhisperASR();

    bool init();
    std::string processAudio(const std::vector<float>& audioBuffer);
    void setTranscriptionCallback(std::function<void(const std::string&)> callback) {
        transcriptionCallback_ = std::move(callback);
    }

private:
    Config config_;
    struct whisper_context* ctx_{nullptr};
    std::function<void(const std::string&)> transcriptionCallback_;
};

} // namespace sadhana