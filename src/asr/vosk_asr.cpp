#include "asr/vosk_asr.hpp"
#include <vector>
#include <iostream>
#include <algorithm>

namespace sadhana {

bool VoskASR::init() {
    vosk_set_log_level(-1);

    auto* model = vosk_model_new(config_.modelPath.c_str());
    if (!model) {
        std::cerr << "Failed to create Vosk model\n";
        return false;
    }
    model_.reset(model);

    auto* recognizer = vosk_recognizer_new(model_.get(), config_.sampleRate);
    if (!recognizer) {
        std::cerr << "Failed to create Vosk recognizer\n";
        model_.reset();
        return false;
    }
    recognizer_.reset(recognizer);

    vosk_recognizer_set_partial_words(recognizer_.get(), 1);

    return true;
}

std::string VoskASR::processAudio(const float* samples, size_t numSamples) {
    if (!recognizer_) {
        return "";
    }

    std::vector<int16_t> pcmSamples;
    pcmSamples.reserve(numSamples);
    
    for (size_t i = 0; i < numSamples; ++i) {
        float sample = samples[i];
        sample = std::max(-1.0f, std::min(1.0f, sample));
        pcmSamples.push_back(static_cast<int16_t>(sample * 32767.0f));
    }

    const size_t CHUNK_SIZE = 8192;
    for (size_t offset = 0; offset < pcmSamples.size(); offset += CHUNK_SIZE) {
        size_t chunk = std::min(CHUNK_SIZE, pcmSamples.size() - offset);
        vosk_recognizer_accept_waveform(recognizer_.get(), 
                                      reinterpret_cast<const char*>(pcmSamples.data() + offset), 
                                      chunk * sizeof(int16_t));
    }

    const char* result = vosk_recognizer_final_result(recognizer_.get());
    
    std::cout << "\nDebug: Processed " << numSamples << " samples, PCM size: "
              << pcmSamples.size() << std::endl;
    std::cout << "Debug: Raw Vosk output: " << (result ? result : "null") << std::endl;

    return result ? result : "";
}

}