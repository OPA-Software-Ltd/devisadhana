#include "audio/audio_capture.hpp"
#include <stdexcept>
#include <iostream>

namespace sadhana {

AudioCapture::AudioCapture() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        throw std::runtime_error("Failed to initialize PortAudio: " + 
                               std::string(Pa_GetErrorText(err)));
    }
}

AudioCapture::~AudioCapture() {
    stop();
    Pa_Terminate();
}

std::vector<AudioDevice> AudioCapture::listDevices() {
    std::vector<AudioDevice> devices;
    int numDevices = Pa_GetDeviceCount();
    
    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo && deviceInfo->maxInputChannels > 0) {
            AudioDevice device;
            device.index = i;
            device.name = deviceInfo->name;
            device.maxInputChannels = deviceInfo->maxInputChannels;
            device.defaultSampleRate = deviceInfo->defaultSampleRate;
            devices.push_back(device);
        }
    }
    
    return devices;
}

bool AudioCapture::setDevice(int deviceIndex) {
    if (stream_) {
        stop();
    }
    
    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(deviceIndex);
    if (deviceInfo && deviceInfo->maxInputChannels > 0) {
        selectedDevice_ = deviceIndex;
        return true;
    }
    return false;
}

bool AudioCapture::start(int sampleRate, int framesPerBuffer,
                        std::function<void(const float*, size_t)> callback) {
    if (stream_) {
        return false;
    }

    dataCallback_ = std::move(callback);

    PaStreamParameters inputParams = {};

    inputParams.device = selectedDevice_ >= 0 ? selectedDevice_ : Pa_GetDefaultInputDevice();
    
    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(inputParams.device);
    if (!deviceInfo) {
        std::cerr << "Error: Could not get device info for device " << inputParams.device << std::endl;
        return false;
    }

    inputParams.channelCount = deviceInfo->maxInputChannels >= 1 ? 1 : 2;
    inputParams.sampleFormat = paFloat32;
    inputParams.suggestedLatency = deviceInfo->defaultLowInputLatency;

    PaError err = Pa_IsFormatSupported(&inputParams, nullptr, sampleRate);
    if (err != paFormatIsSupported) {
        std::cerr << "Error: Sample format or rate not supported: " << Pa_GetErrorText(err) << std::endl;
        std::cerr << "Device: " << deviceInfo->name << std::endl;
        std::cerr << "Requested format: " << inputParams.channelCount << " channels, " 
                  << sampleRate << " Hz" << std::endl;
        return false;
    }
    
    err = Pa_OpenStream(&stream_,
                       &inputParams,
                       nullptr,
                       sampleRate,
                       framesPerBuffer,
                       paClipOff,
                       AudioCapture::paCallback,
                       this);
    
    if (err != paNoError) {
        std::cerr << "Error opening stream: " << Pa_GetErrorText(err) << std::endl;
        stream_ = nullptr;
        return false;
    }
    
    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        std::cerr << "Error starting stream: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(stream_);
        stream_ = nullptr;
        return false;
    }
    
    return true;
}

void AudioCapture::stop() {
    if (stream_) {
        Pa_StopStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
}

int AudioCapture::paCallback(const void* input, void* output,
                           unsigned long frameCount,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void* userData) {
    (void)output;
    (void)timeInfo;
    (void)statusFlags;
    
    auto* self = static_cast<AudioCapture*>(userData);
    const float* inputBuffer = static_cast<const float*>(input);
    
    if (self->dataCallback_) {
        self->dataCallback_(inputBuffer, frameCount);
    }
    
    return paContinue;
}

}