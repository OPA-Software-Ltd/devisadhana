#pragma once
#include <portaudio.h>
#include <functional>
#include <string>
#include <vector>

namespace sadhana {

struct AudioDevice {
    int index;
    std::string name;
    int maxInputChannels;
    double defaultSampleRate;
};

class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();

    std::vector<AudioDevice> listDevices();
    bool setDevice(int deviceIndex);
    bool start(int sampleRate, int framesPerBuffer,
              std::function<void(const float*, size_t)> callback);
    void stop();

    static constexpr int DEFAULT_SAMPLE_RATE = 48000;
    static constexpr int DEFAULT_FRAMES_PER_BUFFER = 480 * 3;

private:
    static int paCallback(const void* input, void* output,
                         unsigned long frameCount,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void* userData);

    PaStream* stream_{nullptr};
    int selectedDevice_{-1};
    std::function<void(const float*, size_t)> dataCallback_;
};

}