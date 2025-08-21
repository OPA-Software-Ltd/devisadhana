#pragma once

#include "ritual/flow_manager.hpp"
#include <string>
#include <mutex>
#include <iostream>
#include <chrono>
#include <iomanip>

namespace sadhana {

class DisplayManager {
public:
    DisplayManager() 
        : lastLevel_(0.0f)
        , needsUpdate_(true)
        , lastUpdate_(std::chrono::steady_clock::now())
        , updateIntervalMs_(2000)  // Increase to 2 seconds
        , levelThresholdDb_(10.0f) // Increase to 10dB
    {}

    void requestUpdate() {
        std::lock_guard<std::mutex> lock(mutex_);
        needsUpdate_ = true;
    }

    void updateDisplay(const FlowProgress& progress, 
                      const RitualDefinition& ritual,
                      float currentLevel) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto currentTime = std::chrono::steady_clock::now();
        auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(
            currentTime - lastUpdate_).count();

        // Update only if:
        // 1. Update is requested, or
        // 2. Section or part changed, or
        // 3. Level changed significantly (>5dB), or
        // 4. Update interval has passed
        if (!needsUpdate_ &&
            lastSectionId_ == progress.currentSectionId &&
            lastPartId_ == progress.currentPartId &&
            std::abs(currentLevel - lastLevel_) <= levelThresholdDb_ &&
            timeSinceLastUpdate < updateIntervalMs_) {
            return;
        }

        auto state = ritual.getCurrentState(progress.currentSectionId, progress.currentPartId);
        
        // Clear screen and reset cursor
        std::cout << "\033[2J\033[H";
        
        // Show header
        std::cout << "=== Ritual Progress ===\n";
        std::cout << "Section: " << progress.currentSectionId << "\n";
        std::cout << "Part: " << (progress.currentPartId.empty() ? "-" : progress.currentPartId) << "\n";
        std::cout << "Audio Level: " << std::fixed << std::setprecision(1) 
                 << currentLevel << " dB\n";
        std::cout << "-------------------\n\n";

        // Show current instructions
        if (!state.description.empty()) {
            std::cout << "\033[1mInstructions:\033[0m\n";
            std::cout << state.description << "\n\n";
        }

        // Show expected utterance
        std::cout << "\033[1mExpected Utterance:\033[0m\n";
        if (!state.expectedUtterance.empty()) {
            std::cout << state.expectedUtterance;
            if (state.requiredRepetitions > 1) {
                std::cout << " (" << progress.currentRepetition << "/"
                         << state.requiredRepetitions << " times)";
            }
            std::cout << "\n";
        } else if (progress.awaitingManualIntervention) {
            std::cout << "\033[33mPress SPACE to continue\033[0m\n";
        } else {
            std::cout << "(Waiting for next section)\n";
        }

        std::cout << std::flush;

        // Update state tracking
        lastSectionId_ = progress.currentSectionId;
        lastPartId_ = progress.currentPartId;
        lastLevel_ = currentLevel;
        lastUpdate_ = currentTime;
        needsUpdate_ = false;
    }

    void showMessage(const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        clearLine();
        std::cout << "\r" << message << std::flush;
        needsUpdate_ = true;
    }

private:
    std::mutex mutex_;
    float lastLevel_;
    std::string lastSectionId_;
    std::string lastPartId_;
    std::chrono::steady_clock::time_point lastUpdate_;
    bool needsUpdate_;
    const int updateIntervalMs_;
    const float levelThresholdDb_;  // New threshold member

    void clearLine() {
        std::cout << "\r" << std::string(120, ' ') << "\r";
    }
};

} // namespace sadhana