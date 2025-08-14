#include "phrase_spec.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>
#include <stdexcept>
#include <algorithm> // <-- needed for std::find_if

static inline void trim(std::string& s) {
    auto notspace = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
}

static bool parse_csv_line(const std::string& line, std::string& phrase, int& target, bool& has_target) {
    // Very small CSV: <phrase>[,<target>]
    // phrase may be "quoted, with commas".
    has_target = false;
    phrase.clear();
    target = 0;

    std::string s = line;
    trim(s);
    if (s.empty() || s[0] == '#') return false;

    if (s.front() == '"' || s.front() == '\'') {
        char q = s.front();
        size_t endq = std::string::npos;
        for (size_t i = 1; i < s.size(); ++i) {
            if (s[i] == q) { endq = i; break; }
        }
        if (endq == std::string::npos) return false; // bad quote
        phrase = s.substr(1, endq - 1);
        size_t pos = endq + 1;
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) ++pos;
        if (pos < s.size() && s[pos] == ',') {
            ++pos;
            std::string t = s.substr(pos);
            trim(t);
            if (!t.empty()) {
                try { target = std::stoi(t); has_target = true; } catch (...) { has_target = false; }
            }
        }
    } else {
        // split on first comma
        size_t comma = s.find(',');
        if (comma == std::string::npos) {
            phrase = s;
        } else {
            phrase = s.substr(0, comma);
            std::string t = s.substr(comma + 1);
            trim(t);
            if (!t.empty()) {
                try { target = std::stoi(t); has_target = true; } catch (...) { has_target = false; }
            }
        }
        trim(phrase);
    }
    return !phrase.empty();
}

std::vector<PhraseSpec> load_phrases_csv(const std::string& path) {
    std::ifstream f(path);
    if (!f.good()) {
        throw std::runtime_error("Cannot open phrases file: " + path);
    }
    std::vector<PhraseSpec> out;
    std::string line;
    while (std::getline(f, line)) {
        std::string p; int tgt = 0; bool ht = false;
        if (!parse_csv_line(line, p, tgt, ht)) continue;
        PhraseSpec ps; ps.text = p; ps.target = ht ? tgt : 0;
        out.push_back(std::move(ps));
    }
    return out;
}

