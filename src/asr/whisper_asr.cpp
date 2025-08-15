#include "asr/whisper_asr.hpp"  // Changed from "audio/whisper_asr.hpp"
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <iomanip>
#include <filesystem>

namespace sadhana {

WhisperASR::WhisperASR(const Config& config) : config_(config) {}

WhisperASR::~WhisperASR() {
    if (ctx_) {
        whisper_free(ctx_);
    }
}

bool WhisperASR::init() {
    // Try multiple possible locations for the model file
    std::vector<std::filesystem::path> possiblePaths = {
        config_.modelPath,
        std::filesystem::current_path() / config_.modelPath,
        std::filesystem::current_path() / ".." / config_.modelPath,
        std::filesystem::current_path() / "../.." / config_.modelPath
    };

    for (const auto& path : possiblePaths) {
        std::cout << "Trying to load model from: " << path << std::endl;
        if (std::filesystem::exists(path)) {
            ctx_ = whisper_init_from_file(path.string().c_str());
            if (ctx_) {
                std::cout << "Successfully loaded model from: " << path << std::endl;
                return true;
            }
        }
    }

    std::cerr << "Failed to load Whisper model. Tried paths:\n";
    for (const auto& path : possiblePaths) {
        std::cerr << "  " << path << "\n";
    }
    return false;
}

std::string WhisperASR::processAudio(const std::vector<float>& audioBuffer) {
    if (!ctx_) {
        throw std::runtime_error("Whisper model not initialized");
    }

    if (audioBuffer.empty()) {
        return "";
    }

    std::cout << "Processing audio buffer of " << audioBuffer.size() << " samples\n";
    
    // Calculate RMS of the audio buffer to check signal strength
    float sumSquares = 0.0f;
    for (const float& sample : audioBuffer) {
        sumSquares += sample * sample;
    }
    float rms = std::sqrt(sumSquares / audioBuffer.size());
    std::cout << "Audio RMS level: " << rms << std::endl;

    // Set up whisper parameters
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress   = true;
    wparams.print_special   = false;
    wparams.print_realtime  = true;
    wparams.print_timestamps = false;
    wparams.translate       = config_.translateToEnglish;
    wparams.language        = config_.language.c_str();
    wparams.n_threads       = config_.threadCount;
    wparams.offset_ms       = 0;
    wparams.duration_ms     = 0;
    wparams.single_segment  = true;

    std::cout << "Starting Whisper processing...\n";
    
    // Process audio with Whisper
    if (whisper_full(ctx_, wparams, audioBuffer.data(), audioBuffer.size()) != 0) {
        throw std::runtime_error("Failed to process audio with Whisper");
    }

    std::cout << "Whisper processing complete\n";

    // Get the transcription
    std::string result;
    const int n_segments = whisper_full_n_segments(ctx_);
    std::cout << "Number of segments: " << n_segments << std::endl;
    
    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(ctx_, i);
        if (text) {
            if (!result.empty()) {
                result += " ";
            }
            result += text;
        }
    }

    std::cout << "Final transcription: " << result << std::endl;

    if (transcriptionCallback_) {
        transcriptionCallback_(result);
    }

    return result;
}

} // namespace sadhana