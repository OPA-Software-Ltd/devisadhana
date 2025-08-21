#pragma once

#include "definition/definition.hpp"
#include <string>
#include <vector>
#include <map>
#include <optional>

namespace sadhana {

class PhraseManager {
public:
    struct MarkerInfo {
        std::string originalMarker;
        std::string sectionId;
        std::string partId;
        std::string stepId;
        std::string markerType;
        std::map<std::string, std::string> metadata;
    };

    struct MatchResult {
        std::string sectionId;
        std::string partId;
        std::string stepId;
        std::string matchedText;
        std::string markerType;
        float confidence{0.0f};
        std::map<std::string, std::string> additionalData;
    };

    explicit PhraseManager(const RitualDefinition& ritual);

    MatchResult matchPhrase(const std::string& text);
    
private:
    const RitualDefinition& ritual_;
    std::map<std::string, std::vector<MarkerInfo>> markerCache_;

    void buildMarkerCache();
    void addMarkerToCache(const std::string& marker, const MarkerInfo& info);
    bool generateSvahaVariants(const std::string& marker, const MarkerInfo& info);
    std::string normalizeText(const std::string& text) const;
    float calculatePhraseConfidence(const std::string& source, const std::string& target) const;
    std::optional<MarkerInfo> findBestMatch(const std::string& normalizedText) const;
    std::vector<std::string> splitIntoWords(const std::string& text) const;
    int levenshteinDistance(const std::string& s1, const std::string& s2) const;
    
    // New helper methods for pattern matching
    bool isBeginningVariant(const std::string& word) const;
    bool isGanapatiVariant(const std::string& word) const;
    bool isEndingVariant(const std::string& word) const;
    bool isCommonPattern(const std::vector<std::string>& words) const;
    float calculateWordSimilarity(const std::string& word1, const std::string& word2) const;
    bool hasCommonPhoneticSubstitution(const std::string& word1, const std::string& word2) const;
};

} // namespace sadhana