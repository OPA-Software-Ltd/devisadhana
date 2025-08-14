#pragma once
#include <optional>
#include <string>

struct AppConfig {
    std::optional<int> device;                 // -d / --device
    std::optional<double> sample_rate;         // --sr
    std::optional<unsigned long> frames_per_buffer; // --fpb / --frames

    std::optional<int> calibrate_ms;           // --calibrate
    std::optional<float> calib_attack;         // --calib-attack
    std::optional<float> calib_rel_above_floor;// --calib-rel-above-floor

    std::optional<float> vad_attack;           // --vad-attack
    std::optional<float> vad_release;          // --vad-release
    std::optional<int> vad_hang_ms;            // --vad-hang

    std::optional<bool> show_thresholds;       // --show-thresholds

    std::optional<std::string> db_path;        // --db
};

// Returns $XDG_CONFIG_HOME/sadhana/sadhana.toml or ~/.config/sadhana/sadhana.toml
std::string default_config_path();

// Load config file if it exists. Simple TOML/INI-like: key = value
// Supports comments starting with '#' or ';'. Strings may be quoted.
// Missing file returns an empty AppConfig (all optionals disengaged).
AppConfig load_config_file(const std::string& path);

// Expand leading '~/' in paths using $HOME.
std::string expand_path(const std::string& p);
