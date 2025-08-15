// src/audio/audio_processor.cpp
#include "audio/audio_processor.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

namespace sadhana {

RitualAudioProcessor::RitualAudioProcessor(const RitualDefinition& ritual)
    : ritual_(ritual),
      audioCapture_(std::make_unique<AudioCapture>()),
      phraseManager_(std::make_unique<PhraseManager>(ritual)) {
}

RitualAudioProcessor::~RitualAudioProcessor() {
    stop();
}

bool RitualAudioProcessor::init(const Config& config) {
    config_ = config;

    // Initialize VAD
    vad_ = std::make_unique<VAD>(config.vadConfig);
    vad_->setStateChangeCallback([this](bool active) {
        handleSpeechStateChange(active);
    });

    // Initialize ASR
    asr_ = std::make_unique<VoskASR>(config.asrConfig);
    if (!asr_->init()) {
        notifyError("Failed to initialize ASR system");
        return false;
    }

    return true;
}

bool RitualAudioProcessor::start() {
    if (running_) return false;

    return audioCapture_->start(
        config_.sampleRate,
        config_.framesPerBuffer,
        [this](const float* samples, size_t numSamples) {
            handleAudioData(samples, numSamples);
        }
    );
}

void RitualAudioProcessor::stop() {
    if (audioCapture_) {
        audioCapture_->stop();
    }
    running_ = false;
}

bool RitualAudioProcessor::setAudioDevice(int deviceIndex) {
    return audioCapture_ && audioCapture_->setDevice(deviceIndex);
}

std::vector<AudioDevice> RitualAudioProcessor::listAudioDevices() const {
    return audioCapture_ ? audioCapture_->listDevices() : std::vector<AudioDevice>();
}

void RitualAudioProcessor::handleAudioData(const float* samples, size_t numSamples) {
    if (!vad_ || !asr_) return;

    bool wasSpeechActive = speechActive_;
    speechActive_ = vad_->process(samples, numSamples);

    // Start recording when speech becomes active
    if (speechActive_ && !wasSpeechActive) {
        speechBuffer_.clear();
    }

    // Collect samples while speech is active
    if (speechActive_) {
        speechBuffer_.insert(speechBuffer_.end(), samples, samples + numSamples);
    }

    // Process completed utterance
    if (!speechActive_ && wasSpeechActive && !speechBuffer_.empty()) {
        std::string result = asr_->processAudio(speechBuffer_.data(), speechBuffer_.size());
        processTranscription(result);
        speechBuffer_.clear();
    }
}

void RitualAudioProcessor::handleSpeechStateChange(bool active) {
    speechActive_ = active;
}

void RitualAudioProcessor::processTranscription(const std::string& text) {
    if (transcriptionCallback_) {
        transcriptionCallback_(text);
    }

    auto result = phraseManager_->matchPhrase(text);
    if (!result.matchedText.empty()) {
        if (!isInCooldown(result.matchedText)) {
            updateMarkerState(result.matchedText,
                ritual_.getCooldownForMarker(result.matchedText).value_or(700));
            updateProgress(result);
            if (resultCallback_) {
                resultCallback_(result);
            }
        }
    }
}

void RitualAudioProcessor::updateProgress(const ProcessingResult& result) {
    // Update progress based on the matched result
    if (result.sectionId != currentProgress_.currentSectionId) {
        currentProgress_.currentSectionId = result.sectionId;
        currentProgress_.currentPartId.clear();
        currentProgress_.currentStepId.clear();
        currentProgress_.currentRepetition = 0;
    }

    if (!result.partId.empty()) {
        currentProgress_.currentPartId = result.partId;
    }
    if (!result.stepId.empty()) {
        currentProgress_.currentStepId = result.stepId;
    }

    // Update counts based on marker type
    if (result.markerType == "iteration") {
        currentProgress_.currentRepetition++;
        currentProgress_.counts[result.partId]++;
    }

    if (progressCallback_) {
        progressCallback_(currentProgress_);
    }
}

bool RitualAudioProcessor::isInCooldown(const std::string& markerId) const {
    auto it = markerStates_.find(markerId);
    if (it == markerStates_.end()) return false;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - it->second.lastTriggerTime).count();

    return elapsed < it->second.cooldownMs;
}

void RitualAudioProcessor::updateMarkerState(const std::string& markerId, int cooldownMs) {
    markerStates_[markerId] = {
        .lastTriggerTime = std::chrono::steady_clock::now(),
        .cooldownMs = cooldownMs
    };
}

void RitualAudioProcessor::notifyError(const std::string& error) {
    if (errorCallback_) {
        errorCallback_(error);
    }
}

} // namespace sadhana