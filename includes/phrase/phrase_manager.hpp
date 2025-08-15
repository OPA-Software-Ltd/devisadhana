// includes/phrase_manager.hpp
#pragma once

#include "definition/definition.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace sadhana {

class PhraseManager {
public:
    struct MatchResult {
        std::string sectionId;
        std::string partId;
        std::string stepId;
        std::string matchedText;
        std::string markerType;  // "step", "iteration", "completion", etc.
        float confidence;
        std::map<std::string, std::string> additionalData;
    };

    explicit PhraseManager(const RitualDefinition& ritual);

    MatchResult matchPhrase(const std::string& text);

private:
    const RitualDefinition& ritual_;

    // Cache for normalized markers and their metadata
    struct MarkerInfo {
        std::string originalMarker;
        std::string sectionId;
        std::string partId;
        std::string stepId;
        std::string markerType;
        std::map<std::string, std::string> metadata;
    };

    std::unordered_map<std::string, std::vector<MarkerInfo>> markerCache_;

    void buildMarkerCache();
    std::string normalizeText(const std::string& text) const;
    float calculatePhraseConfidence(const std::string& source, const std::string& target) const;
    std::optional<MarkerInfo> findBestMatch(const std::string& normalizedText) const;
    void addMarkerToCache(const std::string& marker, const MarkerInfo& info);
    bool generateSvahaVariants(const std::string& marker, const MarkerInfo& info);
};

} // namespace sadhana