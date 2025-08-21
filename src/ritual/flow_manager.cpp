#include "ritual/flow_manager.hpp"
#include <fstream>
#include <iostream>

namespace sadhana {

FlowManager::FlowManager(const RitualDefinition& definition)
    : definition_(definition)
    , phraseManager_(definition) {
    // Initialize with the first section (purvangam)
    const auto& sections = definition.getSections();
    if (!sections.empty()) {
        progress_.currentSectionId = sections[0].id;
        progress_.currentPartId = "";
        progress_.currentRepetition = 0;
        
        // Always start with manual intervention in purvangam
        if (progress_.currentSectionId == "purvangam") {
            progress_.awaitingManualIntervention = true;
        }
    }
}

bool FlowManager::loadFlowConfiguration(const std::string& configPath) {
    try {
        std::ifstream file(configPath);
        flowConfig_ = nlohmann::json::parse(file);
        return validateConfiguration();
    } catch (const std::exception& e) {
        return false;
    }
}

bool FlowManager::validateConfiguration() const {
    // Basic validation that required fields exist
    try {
        const auto& exec = flowConfig_["execution"];
        if (exec["recognition_settings"]["default_threshold"].get<float>() < 0 ||
            exec["recognition_settings"]["default_threshold"].get<float>() > 1) {
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

void FlowManager::handleManualIntervention() {
    std::cout << "Debug: Manual intervention handler called\n" << std::flush;
    
    // Get current state once at the beginning
    auto currentState = definition_.getCurrentState(progress_.currentSectionId, progress_.currentPartId);
    
    // If we're in a part that requires repetitions and not awaiting intervention,
    // count this as a successful repetition
    if (!progress_.awaitingManualIntervention && !progress_.currentPartId.empty()) {
        progress_.currentRepetition++;
        std::cout << "Debug: Manual intervention counted as repetition " 
                  << progress_.currentRepetition << "/" << currentState.requiredRepetitions 
                  << "\n" << std::flush;
                  
        // Check if we've reached the required repetitions
        if (progress_.currentRepetition >= currentState.requiredRepetitions) {
            progress_.awaitingManualIntervention = true;
        }
        
        if (progressCallback_) {
            progressCallback_(progress_);
        }
        return;
    }
    
    // Show current state for debugging
    std::cout << "Debug: Current state before intervention:\n"
              << "  Section: " << progress_.currentSectionId << "\n"
              << "  Part: " << progress_.currentPartId << "\n"
              << "  Expected utterance: " << currentState.expectedUtterance << "\n"
              << "  Awaiting intervention: " << progress_.awaitingManualIntervention << "\n" << std::flush;
    
    // Rest of the existing code...
    if (!progress_.awaitingManualIntervention) {
        std::cout << "Debug: Ignoring manual intervention - not awaiting\n" << std::flush;
        return;
    }
    
    // ... (existing section transition code)
    //auto currentState = definition_.getCurrentState(progress_.currentSectionId, progress_.currentPartId);
    std::cout << "Debug: Current state before intervention:\n"
              << "  Section: " << progress_.currentSectionId << "\n"
              << "  Part: " << progress_.currentPartId << "\n"
              << "  Expected utterance: " << currentState.expectedUtterance << "\n"
              << "  Awaiting intervention: " << progress_.awaitingManualIntervention << "\n" << std::flush;
    
    if (!progress_.awaitingManualIntervention) {
        std::cout << "Debug: Ignoring manual intervention - not awaiting\n" << std::flush;
        return;
    }
    
    if (progress_.currentSectionId == "purvangam") {
        std::cout << "Debug: In purvangam section, advancing...\n" << std::flush;
        
        // Mark current section complete and move to tarpanam
        sectionStates_[progress_.currentSectionId].isComplete = true;
        progress_.currentSectionId = "tarpanam";
        progress_.currentPartId = "moola_mantra_tarpanam";
        progress_.currentRepetition = 0;
        progress_.awaitingManualIntervention = false;
        
    } else if (progress_.currentSectionId == "tarpanam") {
        const auto& sections = definition_.getSections();
        auto section = std::find_if(sections.begin(), sections.end(),
            [this](const auto& s) { return s.id == "tarpanam"; });
            
        if (section != sections.end() && section->parts) {
            auto currentPart = std::find_if(section->parts->begin(), section->parts->end(),
                [this](const auto& p) { return p.id == progress_.currentPartId; });
                
            if (currentPart != section->parts->end()) {
                int required = currentPart->repetitions.value_or(1);
                std::cout << "Debug: Current part requires " << required 
                          << " repetitions, current: " << progress_.currentRepetition << "\n" << std::flush;
                          
                if (progress_.currentRepetition >= required) {
                    auto nextPart = std::next(currentPart);
                    if (nextPart != section->parts->end()) {
                        progress_.currentPartId = nextPart->id;
                        progress_.currentRepetition = 0;
                        progress_.awaitingManualIntervention = false;
                        std::cout << "Debug: Advanced to next part: " << nextPart->id << "\n" << std::flush;
                    } else {
                        sectionStates_[progress_.currentSectionId].isComplete = true;
                        progress_.currentSectionId = "uttarangam";
                        progress_.currentPartId = "";
                        progress_.currentRepetition = 0;
                        progress_.awaitingManualIntervention = true;
                        std::cout << "Debug: Advanced to uttarangam section\n" << std::flush;
                    }
                } else {
                    std::cout << "Debug: Need more repetitions for current part\n" << std::flush;
                    progress_.awaitingManualIntervention = false;
                }
            }
        }
    }
    
    // Get and log new state after changes
    auto newState = definition_.getCurrentState(progress_.currentSectionId, progress_.currentPartId);
    std::cout << "Debug: New state after intervention:\n"
              << "  Section: " << progress_.currentSectionId << "\n"
              << "  Part: " << progress_.currentPartId << "\n"
              << "  Expected utterance: " << newState.expectedUtterance << "\n"
              << "  Awaiting intervention: " << progress_.awaitingManualIntervention << "\n" << std::flush;
    
    if (progressCallback_) {
        progressCallback_(progress_);
    }
}

void FlowManager::handleRecognizedPhrase(const std::string& phrase, float confidence) {
    if (progress_.awaitingManualIntervention || phrase.empty()) {
        return;  // Don't process if waiting for manual intervention or empty phrase
    }

    auto result = phraseManager_.matchPhrase(phrase);
    if (!result.matchedText.empty() && result.confidence >= getThresholdForSection(progress_.currentSectionId)) {
        // Valid phrase recognized
        progress_.currentRepetition++;
        
        // Check if we need manual intervention after this repetition
        const auto& sections = definition_.getSections();
        auto section = std::find_if(sections.begin(), sections.end(),
            [this](const auto& s) { return s.id == progress_.currentSectionId; });
            
        if (section != sections.end() && section->parts) {
            auto currentPart = std::find_if(section->parts->begin(), section->parts->end(),
                [this](const auto& p) { return p.id == progress_.currentPartId; });
                
            if (currentPart != section->parts->end()) {
                int required = currentPart->repetitions.value_or(1);
                if (progress_.currentRepetition >= required) {
                    progress_.awaitingManualIntervention = true;
                }
            }
        }
        
        if (progressCallback_) {
            progressCallback_(progress_);
        }
    }
}

float FlowManager::getThresholdForSection(const std::string& sectionId) const {
    try {
        const auto& thresholds = flowConfig_["execution"]["recognition_settings"]["section_specific_thresholds"];
        if (thresholds.contains(sectionId)) {
            return thresholds[sectionId].get<float>();
        }
    } catch (...) {}
    
    return flowConfig_["execution"]["recognition_settings"]["default_threshold"].get<float>();
}

bool FlowManager::checkSectionCompletion(const std::string& sectionId) {
    const auto& sections = definition_.getSections();
    auto it = std::find_if(sections.begin(), sections.end(),
        [&](const auto& section) { return section.id == sectionId; });
    
    if (it == sections.end()) {
        return false;
    }

    if (it->parts) {
        // Check if all required repetitions are completed
        for (const auto& part : *it->parts) {
            if (progress_.counts[part.id] < part.repetitions) {
                return false;
            }
        }
    }

    sectionStates_[sectionId].isComplete = true;
    return true;
}

void FlowManager::advanceSection() {
    const auto& sections = definition_.getSections();
    auto it = std::find_if(sections.begin(), sections.end(),
        [&](const auto& section) { return section.id == progress_.currentSectionId; });
    
    if (it != sections.end() && std::next(it) != sections.end()) {
        auto nextSection = std::next(it);
        progress_.currentSectionId = nextSection->id;
        progress_.currentPartId.clear();
        progress_.currentRepetition = 0;
        
        // If next section has parts, select the first one with an utterance
        if (nextSection->parts) {
            for (const auto& part : *nextSection->parts) {
                if (part.utterance) {
                    progress_.currentPartId = part.id;
                    break;
                }
            }
        }
    }
}

bool FlowManager::isComplete() const {
    const auto& sections = definition_.getSections();
    return std::all_of(sections.begin(), sections.end(),
        [this](const auto& section) {
            auto it = sectionStates_.find(section.id);
            return it != sectionStates_.end() && it->second.isComplete;
        });
}

} // namespace sadhana