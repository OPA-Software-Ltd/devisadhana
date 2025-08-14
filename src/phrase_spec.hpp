#pragma once
#include <string>
#include <vector>
#include <optional>

// One phrase we want to count, with an optional target.
struct PhraseSpec {
    std::string text;   // canonical phrase text
    int target = 0;     // 0 means "no target"
};

// Load phrases from a simple CSV file:  phrase,target
// - Lines starting with # are comments.
// - phrase may be quoted. target is optional. Example:
//     "om namo narayanaya",108
//     om,108
//     "lokah samastah sukhino bhavantu",
std::vector<PhraseSpec> load_phrases_csv(const std::string& path);

