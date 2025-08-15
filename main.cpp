#include "audio/audio_capture.hpp"
#include "audio/vad.hpp"
#include "asr/vosk_asr.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <csignal>
#include <regex>
#include <nlohmann/json.hpp>
#include <mutex>

static volatile bool running = true;

void signalHandler(int) {
    running = false;
}

float calculateRMSdB(const float* samples, size_t numSamples) {
    float sumSquares = 0.0f;
    for (size_t i = 0; i < numSamples; ++i) {
        sumSquares += samples[i] * samples[i];
    }
    float rms = std::sqrt(sumSquares / numSamples);
    return 20.0f * std::log10(rms + 1e-9f);
}

std::string extractText(const std::string& jsonStr) {
    try {
        auto j = nlohmann::json::parse(jsonStr);
        if (j.contains("alternatives") && !j["alternatives"].empty()) {
            return j["alternatives"][0]["text"];
        }
    } catch (...) {
        return jsonStr;
    }
    return "";
}

int main() {
    signal(SIGINT, signalHandler);
    
    try {
        sadhana::AudioCapture audio;
        auto devices = audio.listDevices();
        std::cout << "Available input devices:\n";
        for (const auto& device : devices) {
            std::cout << "[" << device.index << "] " << device.name << "\n";
        }
        
        std::cout << "Select input device: ";
        int deviceIndex;
        std::cin >> deviceIndex;
        if (!audio.setDevice(deviceIndex)) {
            std::cerr << "Failed to set device\n";
            return 1;
        }

        // Adjusted VAD settings for better speech detection
        sadhana::VAD::Config vadConfig;
        vadConfig.attackThreshold = 15.0f;       // Increased threshold
        vadConfig.releaseThreshold = 12.0f;      // Increased threshold
        vadConfig.hangTimeMs = 500;              // Longer hang time
        vadConfig.calibrationMs = 2000;          // Longer calibration
        vadConfig.calibrationAttackFactor = 0.05f; // Slower calibration
        vadConfig.calibrationReleaseAboveFloor = 10.0f;

        sadhana::VAD vad(vadConfig);
        sadhana::VoskASR asr({
            .modelPath = "models/vosk-model-small-en-us-0.15",  // Use specific model version
            .sampleRate = sadhana::AudioCapture::DEFAULT_SAMPLE_RATE
        });
        
        if (!asr.init()) {
            std::cerr << "Failed to initialize ASR\n";
            return 1;
        }

        bool calibrating = true;
        bool recording = false;
        std::vector<float> speechBuffer;
        int calibrationSamplesRemaining = 
            vadConfig.calibrationMs * (sadhana::AudioCapture::DEFAULT_SAMPLE_RATE / 1000);

        std::mutex consoleMutex;
        
        audio.start(sadhana::AudioCapture::DEFAULT_SAMPLE_RATE,
                   sadhana::AudioCapture::DEFAULT_FRAMES_PER_BUFFER,
                   [&](const float* samples, size_t numSamples) {
            if (calibrating) {
                vad.calibrate(samples, numSamples);
                calibrationSamplesRemaining -= numSamples;
                if (calibrationSamplesRemaining <= 0) {
                    calibrating = false;
                    {
                        std::lock_guard<std::mutex> lock(consoleMutex);
                        std::cout << "\nCalibration complete. Ready!\n"
                                 << "Speak into the microphone...\n" << std::flush;
                    }
                }
                return;
            }

            float currentLevel = calculateRMSdB(samples, numSamples);
            bool wasSpeechActive = vad.isSpeechActive();
            bool isSpeechActive = vad.process(samples, numSamples);

            // Start recording when speech becomes active
            if (isSpeechActive && !recording) {
                recording = true;
                speechBuffer.clear();
                {
                    std::lock_guard<std::mutex> lock(consoleMutex);
                    std::cout << "\rLevel: " << std::fixed << std::setprecision(1) 
                             << currentLevel << " dB | Recording" 
                             << std::string(20, ' ') << std::flush;
                }
            }

            // Collect samples while recording
            if (recording) {
                speechBuffer.insert(speechBuffer.end(), samples, samples + numSamples);
            }

            // Inside the audio callback, replace the speech processing section with:
            if (recording && (!isSpeechActive && wasSpeechActive)) {
                // Debug output for buffer size
                {
                    std::lock_guard<std::mutex> lock(consoleMutex);
                    std::cout << "\nDebug: Speech buffer size: " << speechBuffer.size() 
                              << " samples (" << (float)speechBuffer.size() / sadhana::AudioCapture::DEFAULT_SAMPLE_RATE 
                              << " seconds)\n" << std::flush;
                }

                // Only process if we have enough audio (300ms minimum)
                if (speechBuffer.size() >= sadhana::AudioCapture::DEFAULT_SAMPLE_RATE * 0.3) {
                    std::string result = asr.processAudio(speechBuffer.data(), speechBuffer.size());
                    
                    {
                        std::lock_guard<std::mutex> lock(consoleMutex);
                        std::cout << "\nDebug: Raw ASR result: " << result << std::endl;
                    }

                    try {
                        auto j = nlohmann::json::parse(result);
                        if (j.contains("text")) {
                            std::string text = j["text"];
                            if (!text.empty()) {
                                std::lock_guard<std::mutex> lock(consoleMutex);
                                std::cout << "\nTranscribed: \"" << text << "\"\n" << std::flush;
                            } else {
                                std::lock_guard<std::mutex> lock(consoleMutex);
                                std::cout << "\nDebug: Empty text in JSON\n" << std::flush;
                            }
                        } else {
                            std::lock_guard<std::mutex> lock(consoleMutex);
                            std::cout << "\nDebug: No 'text' field in JSON\n" << std::flush;
                        }
                    } catch (const std::exception& e) {
                        std::lock_guard<std::mutex> lock(consoleMutex);
                        std::cout << "\nDebug: JSON parsing failed: " << e.what() << "\n";
                        if (!result.empty()) {
                            std::cout << "Using raw result: \"" << result << "\"\n" << std::flush;
                        }
                    }
                } else {
                    std::lock_guard<std::mutex> lock(consoleMutex);
                    std::cout << "\nDebug: Speech too short, ignoring\n" << std::flush;
                }
                
                recording = false;
                speechBuffer.clear();
            }

            // Update status when not recording
            if (!recording) {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cout << "\rLevel: " << std::fixed << std::setprecision(1) 
                         << currentLevel << " dB | Waiting" 
                         << std::string(20, ' ') << std::flush;
            }
        });

        std::cout << "Recording... Press Ctrl+C to stop\n";
        
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        audio.stop();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}