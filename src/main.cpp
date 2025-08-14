#include "audio_input.hpp"
#include "rms.hpp"
#include "vad.hpp"
#include "sqlite_logger.hpp"
#include "config.hpp"
#include "phrase_spec.hpp"
#include "phrase_match.hpp"
#include <atomic>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

static std::atomic<bool> g_stop{false};
static void on_sigint(int) { g_stop.store(true); }

struct Args {
    bool list_devices = false;
    std::optional<int> device_index{};
    double        sample_rate       = 48000.0;
    unsigned long frames_per_buffer = 480; // 10ms @ 48k
    std::string   db_path;

    // VAD / calibration controls
    float vad_attack    = -45.0f;
    float vad_release   = -55.0f;
    int   vad_hang_ms   = 200;
    int   calibrate_ms  = 0;        // 0 = off; e.g. 2000 for 2s
    float calib_attack  = 18.0f;    // dB above floor to ENTER speech
    float calib_rel_above_floor = 6.0f; // dB ABOVE floor to KEEP speech
    bool  show_thresholds = false;

    // config path used (for info)
    std::string config_path;

    // Phrase detection (phase 1: from transcript file)
    std::string phrases_csv;        // --phrases <file>
    std::string transcript_path;    // --from-transcript <file>
};

static const char* scan_flag_value(int argc, char** argv, const char* longflag, const char* shortflag) {
    for (int i=1;i<argc;++i) {
        std::string s = argv[i];
        if ((longflag && s == longflag) || (shortflag && s == shortflag)) {
            if (i+1 < argc) return argv[i+1];
        }
    }
    return nullptr;
}

static void parse_args_into(Args& a, int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        if (s == "--list-devices" || s == "-l") a.list_devices = true;
        else if ((s == "--device" || s == "-d") && i + 1 < argc) a.device_index = std::stoi(argv[++i]);
        else if (s == "--sr" && i + 1 < argc) a.sample_rate = std::stod(argv[++i]);
        else if ((s == "--fpb" || s == "--frames") && i + 1 < argc) a.frames_per_buffer = static_cast<unsigned long>(std::stoul(argv[++i]));
        else if (s == "--db" && i + 1 < argc) a.db_path = argv[++i];
        else if (s == "--vad-attack" && i + 1 < argc) a.vad_attack = std::stof(argv[++i]);
        else if (s == "--vad-release" && i + 1 < argc) a.vad_release = std::stof(argv[++i]);
        else if (s == "--vad-hang" && i + 1 < argc) a.vad_hang_ms = std::stoi(argv[++i]);
        else if (s == "--calibrate" && i + 1 < argc) a.calibrate_ms = std::stoi(argv[++i]);
        else if (s == "--calib-attack" && i + 1 < argc) a.calib_attack = std::stof(argv[++i]);
        else if (s == "--calib-rel-above-floor" && i + 1 < argc) a.calib_rel_above_floor = std::stof(argv[++i]);
        else if (s == "--show-thresholds") a.show_thresholds = true;
        else if ((s == "--config" || s == "-c") && i + 1 < argc) a.config_path = expand_path(argv[++i]);
        else if (s == "--phrases" && i + 1 < argc) a.phrases_csv = argv[++i];
        else if (s == "--from-transcript" && i + 1 < argc) a.transcript_path = argv[++i];
        else if (s == "--help" || s == "-h") {
            std::cout << "Sadhana — mic capture + VAD + calibration + config + phrase counting (from transcript)\n"
                      << "Audio / device:\n"
                      << "  -l, --list-devices                 List input devices\n"
                      << "  -d, --device <index>               Use specific input device index\n"
                      << "      --sr <Hz>                      Sample rate (default 48000)\n"
                      << "      --fpb <frames>                 Frames per buffer (default 480)\n"
                      << "      --db <path>                    SQLite DB path (default XDG)\n"
                      << "VAD:\n"
                      << "      --vad-attack <dB>              Absolute attack threshold (default -45)\n"
                      << "      --vad-release <dB>             Absolute release threshold (default -55)\n"
                      << "      --vad-hang <ms>                Hangover duration (default 200)\n"
                      << "Calibration:\n"
                      << "      --calibrate <ms>               Measure noise floor for N ms, then set thresholds\n"
                      << "      --calib-attack <dB>            Attack margin above floor (default 18)\n"
                      << "      --calib-rel-above-floor <dB>   Release margin ABOVE floor (default 6)\n"
                      << "      --show-thresholds              Print final thresholds and EMA (debug)\n"
                      << "Config:\n"
                      << "  -c, --config <path>                Load config file (default: ~/.config/sadhana/sadhana.toml)\n"
                      << "Phrase counting (phase 1: from transcript):\n"
                      << "      --phrases <csv>                CSV file: \"phrase\",target\n"
                      << "      --from-transcript <txt>        Transcript text file to analyze\n";
            std::exit(0);
        }
    }
}

static float percentile(std::vector<float>& v, float p) {
    if (v.empty()) return -100.0f;
    std::sort(v.begin(), v.end());
    size_t idx = static_cast<size_t>(std::clamp(p, 0.0f, 1.0f) * (v.size() - 1));
    return v[idx];
}

int main(int argc, char** argv) {
    std::signal(SIGINT, on_sigint);
    std::signal(SIGTERM, on_sigint);

    // Defaults and DB path
    Args args{};
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg) args.db_path = std::string(xdg) + "/sadhana/sadhana.db";
    else {
        const char* home = std::getenv("HOME");
        args.db_path = std::string(home ? home : ".") + "/.local/share/sadhana/sadhana.db";
    }

    // Config path
    if (const char* cp = scan_flag_value(argc, argv, "--config", "-c")) {
        args.config_path = expand_path(cp);
    } else {
        args.config_path = default_config_path();
    }

    // Load config defaults then override with CLI
    AppConfig cfg = load_config_file(args.config_path);
    if (cfg.device)                args.device_index = cfg.device;
    if (cfg.sample_rate)           args.sample_rate = *cfg.sample_rate;
    if (cfg.frames_per_buffer)     args.frames_per_buffer = *cfg.frames_per_buffer;
    if (cfg.calibrate_ms)          args.calibrate_ms = *cfg.calibrate_ms;
    if (cfg.calib_attack)          args.calib_attack = *cfg.calib_attack;
    if (cfg.calib_rel_above_floor) args.calib_rel_above_floor = *cfg.calib_rel_above_floor;
    if (cfg.vad_attack)            args.vad_attack = *cfg.vad_attack;
    if (cfg.vad_release)           args.vad_release = *cfg.vad_release;
    if (cfg.vad_hang_ms)           args.vad_hang_ms = *cfg.vad_hang_ms;
    if (cfg.show_thresholds)       args.show_thresholds = *cfg.show_thresholds;
    if (cfg.db_path)               args.db_path = *cfg.db_path;

    parse_args_into(args, argc, argv);

    // ---- Phrase counting from transcript (no audio needed) ----
    if (!args.phrases_csv.empty() && !args.transcript_path.empty()) {
        auto phrases = load_phrases_csv(args.phrases_csv);

        std::ifstream tf(args.transcript_path);
        if (!tf.good()) {
            std::cerr << "Error: cannot open transcript: " << args.transcript_path << "\n";
            return 1;
        }
        std::string txt((std::istreambuf_iterator<char>(tf)), std::istreambuf_iterator<char>());
        auto norm = normalize_text(txt);
        auto counts = count_phrase_matches(norm, phrases);

        std::cout << "=== Phrase counts (" << args.phrases_csv << ") over transcript (" << args.transcript_path << ") ===\n";
        for (const auto& p : phrases) {
            std::string key = normalize_text(p.text);
            int got = counts[key].count;
            if (p.target > 0) {
                std::cout << p.text << " : " << got << " / " << p.target << "\n";
            } else {
                std::cout << p.text << " : " << got << "\n";
            }
        }
        return 0;
    }

    // ---- Normal device listing ----
    if (args.list_devices) {
        auto devs = AudioInput::list_input_devices();
        for (const auto& d : devs) std::cout << d << "\n";
        return 0;
    }

    // ---- Live audio path (unchanged from before) ----
    int resolved_device = args.device_index.value_or(Pa_GetDefaultInputDevice());
    if (resolved_device == paNoDevice) {
        std::cerr << "Error: no default input device available.\n";
        return 1;
    }
    std::cout << "Using input device: " << AudioInput::device_summary(resolved_device) << "\n";

    AudioParams params;
    params.sample_rate       = args.sample_rate;
    params.frames_per_buffer = args.frames_per_buffer;
    params.channels          = 1;
    params.device_index      = resolved_device;

    std::atomic<float> last_rms{0.0f};
    std::atomic<float> last_db{-100.0f};

    EnergyVAD      vad(args.sample_rate, args.vad_attack, args.vad_release, args.vad_hang_ms);
    SessionLogger  logger(args.db_path);
    const auto     session_id = logger.start_session();

    bool last_voiced = false;

    try {
        AudioInput ai;
        ai.open(params, [&](const float* data, std::size_t frames) {
            float r  = compute_rms(data, frames);
            last_rms.store(r, std::memory_order_relaxed);
            float db = (r > 0.0f) ? 20.0f * std::log10(r) : -100.0f;
            last_db.store(db, std::memory_order_relaxed);

            bool voiced = vad.process_frame(data, frames);
            if (voiced != last_voiced) {
                last_voiced = voiced;
                logger.log_vad_event(session_id, voiced);
            }
        });
        ai.start();

        if (args.calibrate_ms > 0) {
            std::cout << "Calibrating noise floor for " << args.calibrate_ms << " ms… stay quiet.\n";
            std::vector<float> samples;
            samples.reserve(static_cast<size_t>(args.calibrate_ms / 10) + 4);
            auto t0 = std::chrono::steady_clock::now();
            while (std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - t0).count() < args.calibrate_ms) {
                samples.push_back(last_db.load(std::memory_order_relaxed));
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            float floor_db = percentile(samples, 0.10f);
            float attack  = floor_db + args.calib_attack;
            float release = floor_db + args.calib_rel_above_floor;
            if (release >= attack - 8.0f) release = attack - 8.0f;
            attack  = std::clamp(attack,  -30.0f, -1.0f);
            release = std::clamp(release, -90.0f, attack - 6.0f);
            vad.reset_ema(floor_db);
            vad.set_thresholds(attack, release);
            std::cout << "Calibrated floor ≈ " << floor_db
                      << " dBFS → attack " << attack
                      << " dB, release " << release << " dB\n";
        }

        std::cout << "Capturing mic + VAD… (Ctrl+C to stop)\n";
        auto last_debug = std::chrono::steady_clock::now();
        while (!g_stop.load()) {
            float r  = last_rms.load(std::memory_order_relaxed);
            float db = last_db.load(std::memory_order_relaxed);
            std::cout << (last_voiced ? "[SPEECH] " : "[silence] ")
                      << "RMS: " << r << "\t(" << db << " dBFS)        \r" << std::flush;

            if (args.show_thresholds) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_debug).count() > 1000) {
                    std::cout << "\n[debug] EMA " << vad.ema_db() << " dB\n";
                    last_debug = now;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "\nStopping…\n";
        ai.stop();
        logger.end_session(session_id);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        logger.end_session(session_id);
        return 1;
    }
}

