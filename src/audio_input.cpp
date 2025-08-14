#include "audio_input.hpp"
#include <stdexcept>
#include <sstream>

AudioInput::AudioInput() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        throw std::runtime_error(std::string("PortAudio init failed: ") + Pa_GetErrorText(err));
    }
}

AudioInput::~AudioInput() {
    if (stream_) {
        Pa_AbortStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
    Pa_Terminate();
}

std::vector<std::string> AudioInput::list_input_devices() {
    std::vector<std::string> out;
    PaError err = Pa_Initialize();
    if (err != paNoError) return out;

    int num = Pa_GetDeviceCount();
    for (int i = 0; i < num; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (!info) continue;
        if (info->maxInputChannels > 0) {
            std::ostringstream oss;
            oss << "[" << i << "] " << info->name
                << " — API: " << Pa_GetHostApiInfo(info->hostApi)->name
                << ", inCh: " << info->maxInputChannels
                << ", defaultSR: " << info->defaultSampleRate;
            out.push_back(oss.str());
        }
    }
    Pa_Terminate();
    return out;
}

std::string AudioInput::device_summary(int device_index) {
    // Ensure PortAudio is initialized for this query.
    bool inited_here = false;
    if (Pa_GetDeviceCount() < 0) { // not initialized
        if (Pa_Initialize() == paNoError) inited_here = true;
    }
    const PaDeviceInfo* info = Pa_GetDeviceInfo(device_index);
    std::ostringstream oss;
    if (!info) {
        oss << "[" << device_index << "] <invalid device index>";
    } else {
        const PaHostApiInfo* api = Pa_GetHostApiInfo(info->hostApi);
        oss << "[" << device_index << "] " << info->name
            << " — API: " << (api ? api->name : "?")
            << ", inCh: " << info->maxInputChannels
            << ", defaultSR: " << info->defaultSampleRate;
    }
    if (inited_here) Pa_Terminate();
    return oss.str();
}

void AudioInput::open(const AudioParams& params, Callback cb) {
    if (stream_) {
        throw std::runtime_error("Stream already open");
    }
    callback_ = std::move(cb);

    int device = params.device_index.value_or(Pa_GetDefaultInputDevice());
    if (device == paNoDevice) {
        throw std::runtime_error("No default input device available");
    }

    PaStreamParameters in_params{};
    in_params.device = device;
    in_params.channelCount = static_cast<int>(params.channels);
    in_params.sampleFormat = paFloat32; // [-1,1]
    in_params.suggestedLatency = Pa_GetDeviceInfo(device)->defaultLowInputLatency;
    in_params.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(
        &stream_,
        &in_params,
        nullptr,
        params.sample_rate,
        params.frames_per_buffer,
        paNoFlag,
        &AudioInput::pa_callback,
        this);

    if (err != paNoError) {
        stream_ = nullptr;
        throw std::runtime_error(std::string("Pa_OpenStream failed: ") + Pa_GetErrorText(err));
    }
}

void AudioInput::start() {
    if (!stream_) throw std::runtime_error("Stream not open");
    PaError err = Pa_StartStream(stream_);
    if (err != paNoError) {
        throw std::runtime_error(std::string("Pa_StartStream failed: ") + Pa_GetErrorText(err));
    }
    running_.store(true);
}

void AudioInput::stop() {
    if (stream_) {
        Pa_StopStream(stream_);
        running_.store(false);
    }
}

int AudioInput::pa_callback(const void* input,
                            void* /*output*/,
                            unsigned long frame_count,
                            const PaStreamCallbackTimeInfo* /*time_info*/,
                            PaStreamCallbackFlags /*status_flags*/,
                            void* user_data) {
    auto* self = static_cast<AudioInput*>(user_data);
    if (!self || !self->callback_) return paContinue;
    const float* in = static_cast<const float*>(input);
    if (in) {
        self->callback_(in, static_cast<std::size_t>(frame_count));
    }
    return paContinue;
}

