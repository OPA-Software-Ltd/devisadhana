#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <portaudio.h>
#include <string>
#include <vector>

struct AudioParams {
    double sample_rate = 16000.0; // default
    unsigned int channels = 1;    // mono
    unsigned long frames_per_buffer = 512; // ~32 ms @ 16k
    std::optional<int> device_index;       // if not set, use default input
};

class AudioInput {
public:
    using Callback = std::function<void(const float* data, std::size_t frames)>;

    AudioInput();
    ~AudioInput();
    AudioInput(const AudioInput&) = delete;
    AudioInput& operator=(const AudioInput&) = delete;

    static std::vector<std::string> list_input_devices();

    // Safe: initializes PortAudio if needed, fetches a one-line summary, and terminates if it initialized.
    static std::string device_summary(int device_index);

    void open(const AudioParams& params, Callback cb);
    void start();
    void stop();
    bool is_running() const { return running_.load(); }

private:
    static int pa_callback(const void* input,
                           void* output,
                           unsigned long frame_count,
                           const PaStreamCallbackTimeInfo* time_info,
                           PaStreamCallbackFlags status_flags,
                           void* user_data);

    PaStream* stream_ = nullptr;
    Callback callback_;
    std::atomic<bool> running_{false};
};

