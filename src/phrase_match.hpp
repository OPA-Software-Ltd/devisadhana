#pragma once
#include "phrase_spec.hpp"
#include <string>
#include <unordered_map>
#include <vector>

// Normalizes text (lowercase, strip punctuation, collapse spaces)
std::string normalize_text(const std::string& in);

// Counts occurrences of each phrase in 'normalized_text' using fuzzy matching.
//
// Approach:
// - Tokenize both the text and each phrase by spaces (after normalization).
// - Slide a window over the text tokens matching the phrase token-length,
//   compute token-level Levenshtein distance (insert/delete/substitute).
// - Accept a match if distance <= floor(len * max_rel_edit) or <= max_abs_edit.
// - Non-overlapping: after a match, advance by phrase length; else advance by 1.
//
struct MatchParams {
    float max_rel_edit = 0.25f; // up to 25% token edits allowed
    int   max_abs_edit = 1;     // or allow 1 token edit at minimum
};

struct PhraseCount {
    int count = 0;
};

std::unordered_map<std::string, PhraseCount>
count_phrase_matches(const std::string& normalized_text,
                     const std::vector<PhraseSpec>& phrases,
                     const MatchParams& params = {});

