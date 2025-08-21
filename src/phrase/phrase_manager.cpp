#include "phrase_manager.hpp"
#include <algorithm>
#include <cctype>
#include <regex>

namespace sadhana {

PhraseManager::PhraseManager(const RitualDefinition& ritual)
    : ritual_(ritual) {
    buildMarkerCache();
}

void PhraseManager::buildMarkerCache() {
    markerCache_.clear();

    for (const auto& section : ritual_.getSections()) {
        if (section.iteration_marker) {
            MarkerInfo info{
                .originalMarker = section.iteration_marker->canonical,
                .sectionId = section.id,
                .markerType = "iteration"
            };

            addMarkerToCache(section.iteration_marker->canonical, info);

            for (const auto& variant : section.iteration_marker->variants) {
                addMarkerToCache(variant, info);
            }

            if (section.iteration_marker->with_svaha_variants) {
                generateSvahaVariants(section.iteration_marker->canonical, info);
            }
        }

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

        if (section.parts) {
            for (const auto& part : *section.parts) {
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

    std::string normalized = normalizeText(marker);
    for (const auto& svaha : svaha_variants) {
        if (normalized.ends_with(normalizeText(svaha))) {
            return false;
        }
    }

    for (const auto& svaha : svaha_variants) {
        std::string variant = marker + " " + svaha;
        addMarkerToCache(variant, info);
    }

    return true;
}

std::string PhraseManager::normalizeText(const std::string& text) const {
    std::string normalized;
    normalized.reserve(text.length());

    for (char c : text) {
        if (std::isalnum(c) || c == ' ') {
            normalized += std::tolower(c);
        }
    }

    normalized = std::regex_replace(normalized, std::regex("\\s+"), " ");
    normalized = std::regex_replace(normalized, std::regex("^\\s+|\\s+$"), "");

    return normalized;
}

float PhraseManager::calculatePhraseConfidence(const std::string& source, const std::string& target) const {
    auto sourceWords = splitIntoWords(source);
    auto targetWords = splitIntoWords(target);
    
    if (sourceWords.empty() || targetWords.empty()) {
        return 0.0f;
    }

    // Track best matches for key components
    float bestBeginningScore = 0.0f;  // om/home/etc
    float bestGanapatiScore = 0.0f;   // ganapati/ganapatye
    float bestEndingScore = 0.0f;     // swaha/year/etc
    
    // Count matching word patterns
    int matchedPatterns = 0;
    int requiredPatterns = 3; // We want at least om + ganapati + ending
    
    // Sliding window for phrase matching
    const size_t windowSize = 3;
    for (size_t i = 0; i < sourceWords.size(); i++) {
        // Check beginning sounds (om/home/on)
        if (i == 0 && isBeginningVariant(sourceWords[i])) {
            bestBeginningScore = 1.0f;
            matchedPatterns++;
        }
        
        // Check for Ganapati variations
        if (isGanapatiVariant(sourceWords[i])) {
            bestGanapatiScore = 1.0f;
            matchedPatterns++;
        }
        
        // Check ending patterns (swaha/year/etc)
        if (i >= sourceWords.size() - 3 && isEndingVariant(sourceWords[i])) {
            bestEndingScore = 1.0f;
            matchedPatterns++;
        }
        
        // Look for word groups that match common patterns
        if (i + windowSize <= sourceWords.size()) {
            std::vector<std::string> window(sourceWords.begin() + i, 
                                          sourceWords.begin() + i + windowSize);
            if (isCommonPattern(window)) {
                matchedPatterns++;
            }
        }
    }
    
    // Calculate component scores
    float patternScore = static_cast<float>(matchedPatterns) / requiredPatterns;
    float componentScore = (bestBeginningScore + bestGanapatiScore + bestEndingScore) / 3.0f;
    
    // Weight the final score (pattern matching more important than exact matches)
    return patternScore * 0.7f + componentScore * 0.3f;
}

bool PhraseManager::isBeginningVariant(const std::string& word) const {
    static const std::vector<std::string> beginnings = {
        "om", "home", "on", "from", "aim", "mom"
    };
    return std::find(beginnings.begin(), beginnings.end(), word) != beginnings.end();
}

bool PhraseManager::isGanapatiVariant(const std::string& word) const {
    static const std::vector<std::string> variants = {
        "ganapati", "ganapatye", "ganapathy", "ganapathi"
    };
    return std::find(variants.begin(), variants.end(), word) != variants.end();
}

bool PhraseManager::isEndingVariant(const std::string& word) const {
    static const std::vector<std::string> endings = {
        "swaha", "swaahaa", "swahaa", "year", "years", "her"
    };
    return std::find(endings.begin(), endings.end(), word) != endings.end();
}

bool PhraseManager::isCommonPattern(const std::vector<std::string>& words) const {
    if (words.size() < 3) return false;
    
    // Common patterns from your recognition output
    static const std::vector<std::vector<std::string>> patterns = {
        {"shame", "ram", "claim"},
        {"frame", "him", "claim"},
        {"shame", "him", "claim"},
        {"frame", "frame", "claim"},
        {"shah", "of", "iran"},
        {"server", "run", "mere"},
        {"service", "run", "mere"},
        {"sort", "of", "return"}
    };
    
    for (const auto& pattern : patterns) {
        if (pattern.size() <= words.size()) {
            bool matches = true;
            for (size_t i = 0; i < pattern.size(); i++) {
                if (words[i] != pattern[i]) {
                    matches = false;
                    break;
                }
            }
            if (matches) return true;
        }
    }
    
    return false;
}

float PhraseManager::calculateWordSimilarity(const std::string& word1, const std::string& word2) const {
    if (word1 == word2) return 1.0f;
    
    // Check for common phonetic substitutions first
    if (hasCommonPhoneticSubstitution(word1, word2)) {
        return 0.9f;
    }
    
    // Calculate Levenshtein distance
    int distance = levenshteinDistance(word1, word2);
    int maxLength = std::max(word1.length(), word2.length());
    
    // Convert distance to similarity score (0.0 to 1.0)
    float similarity = 1.0f - static_cast<float>(distance) / maxLength;
    
    // Apply length penalty for very different lengths
    float lengthRatio = static_cast<float>(std::min(word1.length(), word2.length())) /
                       static_cast<float>(std::max(word1.length(), word2.length()));
    
    return similarity * lengthRatio;
}

bool PhraseManager::hasCommonPhoneticSubstitution(const std::string& word1, 
                                                const std::string& word2) const {
    // Common Sanskrit/English phonetic substitutions
    static const std::vector<std::pair<std::string, std::string>> substitutions = {
        {"sreem", "shrim"}, {"sreem", "srim"}, {"sreem", "shree"},
        {"hreem", "hrim"}, {"hreem", "hri"}, {"hreem", "rim"},
        {"kleem", "klim"}, {"kleem", "claim"}, {"kleem", "clean"},
        {"gloum", "glom"}, {"gloum", "glum"}, {"gloum", "glam"},
        {"gum", "gom"}, {"gum", "com"}, {"gum", "gun"},
        {"pati", "pathy"}, {"pati", "pathi"}, {"pati", "pathy"},
        {"vara", "war"}, {"vara", "var"}, {"vara", "wr"},
        {"swaha", "svaha"}, {"swaha", "swa"}, {"swaha", "shah"},
        {"mey", "may"}, {"mey", "me"}, {"mey", "mere"}
    };
    
    std::string w1 = word1;
    std::string w2 = word2;
    std::transform(w1.begin(), w1.end(), w1.begin(), ::tolower);
    std::transform(w2.begin(), w2.end(), w2.begin(), ::tolower);
    
    for (const auto& sub : substitutions) {
        if ((w1.find(sub.first) != std::string::npos && w2.find(sub.second) != std::string::npos) ||
            (w1.find(sub.second) != std::string::npos && w2.find(sub.first) != std::string::npos)) {
            return true;
        }
    }
    
    return false;
}

std::optional<PhraseManager::MarkerInfo> PhraseManager::findBestMatch(
    const std::string& normalizedText) const {
    float bestConfidence = 0.0f;
    std::optional<MarkerInfo> bestMatch;

    for (const auto& [marker, infos] : markerCache_) {
        float confidence = calculatePhraseConfidence(normalizedText, marker);
        if (confidence > bestConfidence && confidence >= 0.6f) {
            bestConfidence = confidence;
            bestMatch = infos[0];
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

std::vector<std::string> PhraseManager::splitIntoWords(const std::string& text) const {
    std::vector<std::string> words;
    std::string word;
    for (char c : text) {
        if (std::isspace(c)) {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else if (std::isalnum(c)) {
            word += std::tolower(c);
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    return words;
}

int PhraseManager::levenshteinDistance(const std::string& s1, const std::string& s2) const {
    std::vector<std::vector<int>> dp(s1.length() + 1, 
                                    std::vector<int>(s2.length() + 1));
    
    for (size_t i = 0; i <= s1.length(); i++) {
        dp[i][0] = i;
    }
    for (size_t j = 0; j <= s2.length(); j++) {
        dp[0][j] = j;
    }
    
    for (size_t i = 1; i <= s1.length(); i++) {
        for (size_t j = 1; j <= s2.length(); j++) {
            if (s1[i-1] == s2[j-1]) {
                dp[i][j] = dp[i-1][j-1];
            } else {
                dp[i][j] = 1 + std::min({dp[i-1][j], dp[i][j-1], dp[i-1][j-1]});
            }
        }
    }
    
    return dp[s1.length()][s2.length()];
}
}