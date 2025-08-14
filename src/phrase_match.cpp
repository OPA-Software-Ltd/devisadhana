#include "phrase_match.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath> // std::floor

static inline bool is_punct_ascii(unsigned char c) {
    return std::ispunct(c) && c != '\''; // keep apostrophes
}

std::string normalize_text(const std::string& in) {
    std::string out; out.reserve(in.size());
    // lowercase + strip punctuation except apostrophes, map any whitespace to ' '
    for (unsigned char c : in) {
        unsigned char d = (unsigned char)std::tolower(c);
        if (std::isspace(d)) { out.push_back(' '); continue; }
        if (d < 128 && is_punct_ascii(d)) continue;
        out.push_back((char)d);
    }
    // collapse spaces
    std::string collapsed; collapsed.reserve(out.size());
    bool in_space = false;
    for (char c : out) {
        if (c == ' ') {
            if (!in_space) { collapsed.push_back(' '); in_space = true; }
        } else {
            collapsed.push_back(c);
            in_space = false;
        }
    }
    // trim
    if (!collapsed.empty() && collapsed.front() == ' ') collapsed.erase(collapsed.begin());
    if (!collapsed.empty() && collapsed.back()  == ' ') collapsed.pop_back();
    return collapsed;
}

static std::vector<std::string> tokenize(const std::string& s) {
    std::vector<std::string> toks;
    std::istringstream iss(s);
    std::string w;
    while (iss >> w) toks.push_back(w);
    return toks;
}

static int lev_distance_tokens(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    const int n = (int)a.size(), m = (int)b.size();
    std::vector<int> prev(m+1), cur(m+1);
    for (int j=0;j<=m;++j) prev[j] = j;
    for (int i=1;i<=n;++i) {
        cur[0] = i;
        for (int j=1;j<=m;++j) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            cur[j] = std::min({ prev[j] + 1, cur[j-1] + 1, prev[j-1] + cost });
        }
        prev.swap(cur);
    }
    return prev[m];
}

std::unordered_map<std::string, PhraseCount>
count_phrase_matches(const std::string& normalized_text,
                     const std::vector<PhraseSpec>& phrases,
                     const MatchParams& params) {

    std::unordered_map<std::string, PhraseCount> counts;
    auto text_tokens = tokenize(normalize_text(normalized_text)); // ensure normalized

    // Pre-tokenize phrases
    struct P { std::string key; std::vector<std::string> toks; };
    std::vector<P> plist;
    plist.reserve(phrases.size());
    for (auto& p : phrases) {
        P item; item.key = normalize_text(p.text); item.toks = tokenize(item.key);
        if (!item.toks.empty()) plist.push_back(std::move(item));
    }

    for (const auto& p : plist) {
        const int L = (int)p.toks.size();
        if (L == 0 || (int)text_tokens.size() < L) { counts[p.key]; continue; }

        int i = 0;
        while (i + L <= (int)text_tokens.size()) {
            std::vector<std::string> window(text_tokens.begin()+i, text_tokens.begin()+i+L);
            int d = lev_distance_tokens(window, p.toks);
            int maxd = std::max(params.max_abs_edit,
                                (int)std::floor(L * params.max_rel_edit + 0.0001));
            if (d <= maxd) {
                counts[p.key].count += 1;
                i += L; // non-overlapping
            } else {
                i += 1;
            }
        }
    }
    return counts;
}

