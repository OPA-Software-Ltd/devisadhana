// src/ritual/phrase_manager.cpp
#include "phrase_manager.hpp"
#include <algorithm>
#include <cctype>
#include <regex>
#include <unordered_set>

namespace sadhana {

PhraseManager::PhraseManager(const RitualDefinition& ritual)
    : ritual_(ritual) {
    buildMarkerCache();
}

void PhraseManager::buildMarkerCache() {
    markerCache_.clear();

    // Process all sections
    for (const auto& section : ritual_.getSections()) {
        // Process section iteration markers
        if (section.iteration_marker) {
            MarkerInfo info{
                .originalMarker = section.iteration_marker->canonical,
                .sectionId = section.id,
                .markerType = "iteration"
            };

            // Add canonical form
            addMarkerToCache(section.iteration_marker->canonical, info);

            // Add variants
            for (const auto& variant : section.iteration_marker->variants) {
                addMarkerToCache(variant, info);
            }

            // Generate svaha variants if specified
            if (section.iteration_marker->with_svaha_variants) {
                generateSvahaVariants(section.iteration_marker->canonical, info);
            }
        }

        // Process steps if present
        if (section.steps) {
            for (const auto& step : *section.steps) {
                if (step.marker) {
                    MarkerInfo info{
                        .originalMarker = step.marker->canonical,
                        .sectionId = section.id,
                        .stepId = step.id,
                        .markerType = "step"
                    };

                    addMarkerToCache(step.marker->canonical, info);

                    for (const auto& variant : step.marker->variants) {
                        addMarkerToCache(variant, info);
                    }

                    if (step.marker->with_svaha_variants) {
                        generateSvahaVariants(step.marker->canonical, info);
                    }
                }
            }
        }

        // Process parts if present
        if (section.parts) {
            for (const auto& part : *section.parts) {
                // Add any part-specific markers (e.g., from utterances or sequences)
                if (part.utterance) {
                    MarkerInfo info{
                        .originalMarker = *part.utterance,
                        .sectionId = section.id,
                        .partId = part.id,
                        .markerType = "part"
                    };
                    addMarkerToCache(*part.utterance, info);
                }
            }
        }
    }
}

void PhraseManager::addMarkerToCache(const std::string& marker, const MarkerInfo& info) {
    std::string normalized = normalizeText(marker);
    markerCache_[normalized].push_back(info);
}

bool PhraseManager::generateSvahaVariants(const std::string& marker, const MarkerInfo& info) {
    static const std::vector<std::string> svaha_variants = {
        "svaha", "swaahaa", "swaha", "swaha namaha", "swahaa",
        "svaahaa", "svahaa", "svaha namaha"
    };

    // Check if the marker already ends with any svaha variant
    std::string normalized = normalizeText(marker);
    for (const auto& svaha : svaha_variants) {
        if (normalized.ends_with(normalizeText(svaha))) {
            return false; // Already has svaha
        }
    }

    // Add variants with different svaha forms
    for (const auto& svaha : svaha_variants) {
        std::string variant = marker + " " + svaha;
        addMarkerToCache(variant, info);
    }

    return true;
}

std::string PhraseManager::normalizeText(const std::string& text) const {
    std::string normalized;
    normalized.reserve(text.length());

    // Convert to lowercase and remove diacritics
    for (char c : text) {
        if (std::isalnum(c) || c == ' ') {
            normalized += std::tolower(c);
        }
    }

    // Remove extra spaces
    normalized = std::regex_replace(normalized, std::regex("\\s+"), " ");
    normalized = std::regex_replace(normalized, std::regex("^\\s+|\\s+$"), "");

    return normalized;
}

float PhraseManager::calculatePhraseConfidence(
    const std::string& source, const std::string& target) const {
    if (source == target) return 1.0f;
    if (source.empty() || target.empty()) return 0.0f;

    // Calculate Levenshtein distance
    std::vector<std::vector<int>> distance(
        source.length() + 1,
        std::vector<int>(target.length() + 1));

    for (size_t i = 0; i <= source.length(); i++) {
        distance[i][0] = i;
    }
    for (size_t j = 0; j <= target.length(); j++) {
        distance[0][j] = j;
    }

    for (size_t i = 1; i <= source.length(); i++) {
        for (size_t j = 1; j <= target.length(); j++) {
            int cost = (source[i - 1] == target[j - 1]) ? 0 : 1;
            distance[i][j] = std::min({
                distance[i - 1][j] + 1,      // deletion
                distance[i][j - 1] + 1,      // insertion
                distance[i - 1][j - 1] + cost // substitution
            });
        }
    }

    float maxLength = std::max(source.length(), target.length());
    float similarity = 1.0f - (distance[source.length()][target.length()] / maxLength);

    return similarity;
}

std::optional<PhraseManager::MarkerInfo> PhraseManager::findBestMatch(
    const std::string& normalizedText) const {
    float bestConfidence = 0.0f;
    std::optional<MarkerInfo> bestMatch;

    for (const auto& [marker, infos] : markerCache_) {
        float confidence = calculatePhraseConfidence(normalizedText, marker);
        if (confidence > bestConfidence && confidence >= 0.8f) { // Minimum threshold
            bestConfidence = confidence;
            bestMatch = infos[0]; // Take the first match if multiple exist
        }
    }

    return bestMatch;
}

PhraseManager::MatchResult PhraseManager::matchPhrase(const std::string& text) {
    std::string normalized = normalizeText(text);

    MatchResult result;
    if (auto match = findBestMatch(normalized)) {
        result.sectionId = match->sectionId;
        result.partId = match->partId;
        result.stepId = match->stepId;
        result.matchedText = match->originalMarker;
        result.markerType = match->markerType;
        result.confidence = calculatePhraseConfidence(normalized,
            normalizeText(match->originalMarker));
        result.additionalData = match->metadata;
    }

    return result;
}

} // namespace sadhana