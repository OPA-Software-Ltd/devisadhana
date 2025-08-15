#pragma once

#include <string>
#include <memory>
#include <vosk_api.h>

namespace sadhana {

struct VoskASR {
    struct Config {
        std::string modelPath;
        float sampleRate;
    };

    explicit VoskASR(const Config& config) : config_(config) {}

    bool init();
    std::string processAudio(const float* samples, size_t numSamples);

private:
    Config config_;
    struct VoskModelDeleter {
        void operator()(VoskModel* p) { if (p) vosk_model_free(p); }
    };
    struct VoskRecognizerDeleter {
        void operator()(VoskRecognizer* p) { if (p) vosk_recognizer_free(p); }
    };

    std::unique_ptr<VoskModel, VoskModelDeleter> model_;
    std::unique_ptr<VoskRecognizer, VoskRecognizerDeleter> recognizer_;
};

}