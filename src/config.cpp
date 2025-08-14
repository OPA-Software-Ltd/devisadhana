#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using std::string;

static inline void trim_inplace(string& s) {
    auto not_space = [](unsigned char ch){ return !std::isspace(ch); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
}

static inline string unquote(const string& s) {
    if (s.size() >= 2 && ((s.front()=='"' && s.back()=='"') || (s.front()=='\'' && s.back()=='\''))) {
        return s.substr(1, s.size()-2);
    }
    return s;
}

std::string expand_path(const std::string& p) {
    if (p.size() >= 2 && p[0] == '~' && p[1] == '/') {
        const char* home = std::getenv("HOME");
        if (home && *home) return string(home) + p.substr(1);
    }
    return p;
}

std::string default_config_path() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) return string(xdg) + "/sadhana/sadhana.toml";
    const char* home = std::getenv("HOME");
    string base = home ? string(home) + "/.config" : string(".config");
    return base + "/sadhana/sadhana.toml";
}

static inline bool ieq(const string& a, const string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i=0;i<a.size();++i) if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}

AppConfig load_config_file(const std::string& path) {
    AppConfig cfg;
    std::ifstream f(path);
    if (!f.good()) return cfg; // missing is fine

    string line;
    while (std::getline(f, line)) {
        // strip comments
        auto pos_hash = line.find('#');
        auto pos_sc   = line.find(';');
        auto pos_cmt  = std::min(pos_hash == string::npos ? line.size() : pos_hash,
                                  pos_sc   == string::npos ? line.size() : pos_sc);
        line = line.substr(0, pos_cmt);
        trim_inplace(line);
        if (line.empty()) continue;

        // allow 'key = value' or 'key: value'
        size_t sep = line.find('=');
        if (sep == string::npos) sep = line.find(':');
        if (sep == string::npos) continue;

        string key = line.substr(0, sep);
        string val = line.substr(sep+1);
        trim_inplace(key);
        trim_inplace(val);
        if (key.empty() || val.empty()) continue;

        // unquote strings
        if ((val.front()=='"' && val.back()=='"') || (val.front()=='\'' && val.back()=='\'')) {
            val = unquote(val);
        }

        auto as_int = [&](const string& s)->std::optional<int>{
            try { return std::stoi(s); } catch (...) { return std::nullopt; }
        };
        auto as_ulong = [&](const string& s)->std::optional<unsigned long>{
            try { return static_cast<unsigned long>(std::stoul(s)); } catch (...) { return std::nullopt; }
        };
        auto as_double = [&](const string& s)->std::optional<double>{
            try { return std::stod(s); } catch (...) { return std::nullopt; }
        };
        auto as_float = [&](const string& s)->std::optional<float>{
            try { return std::stof(s); } catch (...) { return std::nullopt; }
        };
        auto as_bool = [&](const string& s)->std::optional<bool>{
            if (ieq(s, "true") || ieq(s,"yes") || s=="1") return true;
            if (ieq(s, "false")|| ieq(s,"no")  || s=="0") return false;
            return std::nullopt;
        };

        if (ieq(key, "device")) cfg.device = as_int(val);
        else if (ieq(key, "sample_rate")) cfg.sample_rate = as_double(val);
        else if (ieq(key, "frames_per_buffer") || ieq(key, "fpb")) cfg.frames_per_buffer = as_ulong(val);
        else if (ieq(key, "calibrate_ms")) cfg.calibrate_ms = as_int(val);
        else if (ieq(key, "calib_attack")) cfg.calib_attack = as_float(val);
        else if (ieq(key, "calib_rel_above_floor") || ieq(key, "calib_rel_above")) cfg.calib_rel_above_floor = as_float(val);
        else if (ieq(key, "vad_attack")) cfg.vad_attack = as_float(val);
        else if (ieq(key, "vad_release")) cfg.vad_release = as_float(val);
        else if (ieq(key, "vad_hang_ms") || ieq(key, "vad_hang")) cfg.vad_hang_ms = as_int(val);
        else if (ieq(key, "show_thresholds")) cfg.show_thresholds = as_bool(val);
        else if (ieq(key, "db_path")) cfg.db_path = expand_path(val);
    }
    return cfg;
}
