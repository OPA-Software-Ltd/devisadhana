// includes/audio/audio_processor.hpp
#pragma once

#include "audio/audio_capture.hpp"
#include "audio/vad.hpp"
#include "audio/vosk_asr.hpp"
#include "definition/definition.hpp"
#include "phrase_manager.hpp"
#include <functional>
#include <memory>
#include <chrono>
#include <map>

namespace sadhana {

class RitualAudioProcessor {
public:
    struct Config {
        int sampleRate{AudioCapture::DEFAULT_SAMPLE_RATE};
        int framesPerBuffer{AudioCapture::DEFAULT_FRAMES_PER_BUFFER};
        VAD::Config vadConfig;
        VoskASR::Config asrConfig;
    };

    struct ProcessingResult {
        std::string sectionId;
        std::string partId;
        std::string stepId;
        std::string matchedText;
        std::string markerType;  // "step", "iteration", "completion", etc.
        float confidence;
        std::map<std::string, std::string> additionalData;
    };

    struct RitualProgress {
        std::string currentSectionId;
        std::string currentPartId;
        std::string currentStepId;
        int currentRepetition{0};
        int totalRepetitions{0};
        std::map<std::string, int> counts;
    };

    using ProgressCallback = std::function<void(const RitualProgress&)>;
    using ResultCallback = std::function<void(const ProcessingResult&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    using TranscriptionCallback = std::function<void(const std::string&)>;

    explicit RitualAudioProcessor(const RitualDefinition& ritual);
    ~RitualAudioProcessor();

    bool init(const Config& config);
    bool start();
    void stop();

    bool setAudioDevice(int deviceIndex);
    std::vector<AudioDevice> listAudioDevices() const;

    // Callback setters
    void setProgressCallback(ProgressCallback callback) { progressCallback_ = std::move(callback); }
    void setResultCallback(ResultCallback callback) { resultCallback_ = std::move(callback); }
    void setErrorCallback(ErrorCallback callback) { errorCallback_ = std::move(callback); }
    void setTranscriptionCallback(TranscriptionCallback callback) {
        transcriptionCallback_ = std::move(callback);
    }

    // Status queries
    bool isRunning() const { return running_; }
    bool isSpeechActive() const { return speechActive_; }
    const RitualProgress& getCurrentProgress() const { return currentProgress_; }

private:
    const RitualDefinition& ritual_;
    std::unique_ptr<AudioCapture> audioCapture_;
    std::unique_ptr<VAD> vad_;
    std::unique_ptr<VoskASR> asr_;
    std::unique_ptr<PhraseManager> phraseManager_;

    bool running_{false};
    bool speechActive_{false};
    std::vector<float> speechBuffer_;

    ProgressCallback progressCallback_;
    ResultCallback resultCallback_;
    ErrorCallback errorCallback_;
    TranscriptionCallback transcriptionCallback_;

    Config config_;
    RitualProgress currentProgress_;

    // Marker state tracking
    struct MarkerState {
        std::chrono::steady_clock::time_point lastTriggerTime;
        int cooldownMs;
    };
    std::map<std::string, MarkerState> markerStates_;

    void handleAudioData(const float* samples, size_t numSamples);
    void handleSpeechStateChange(bool active);
    void processTranscription(const std::string& text);
    void updateProgress(const ProcessingResult& result);

    bool isInCooldown(const std::string& markerId) const;
    void updateMarkerState(const std::string& markerId, int cooldownMs);
    void notifyError(const std::string& error);
};

} // namespace sadhana