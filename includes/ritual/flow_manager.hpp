#pragma once

#include "definition/definition.hpp"
#include "phrase/phrase_manager.hpp"
#include <string>
#include <optional>
#include <functional>
#include <chrono>
#include <map>
#include <nlohmann/json.hpp>

namespace sadhana {

struct FlowProgress {
    std::string currentSectionId;
    std::string currentPartId;
    std::string currentStepId;
    int currentRepetition{0};
    std::map<std::string, int> counts;
    bool awaitingManualIntervention{false};
    float lastConfidence{0.0f};
};

class FlowManager {
private:
    PhraseManager phraseManager_;  // Add this
public:
    explicit FlowManager(const RitualDefinition& definition);
    ~FlowManager() = default;

    bool loadFlowConfiguration(const std::string& configPath);
    void handleRecognizedPhrase(const std::string& phrase, float confidence);
    void handleManualIntervention();
    bool isComplete() const;

    // Callbacks
    using ProgressCallback = std::function<void(const FlowProgress&)>;
    void setProgressCallback(ProgressCallback callback) { progressCallback_ = callback; }

    // Progress access
    const FlowProgress& getCurrentProgress() const { return progress_; }

private:
    const RitualDefinition& definition_;
    nlohmann::json flowConfig_;
    FlowProgress progress_;
    ProgressCallback progressCallback_;

    struct SectionState {
        bool isComplete{false};
        int failedAttempts{0};
        std::chrono::steady_clock::time_point lastAttempt;
    };
    std::map<std::string, SectionState> sectionStates_;

    bool validateConfiguration() const;
    float getThresholdForSection(const std::string& sectionId) const;
    bool checkSectionCompletion(const std::string& sectionId);
    void advanceSection();
};

} // namespace sadhana