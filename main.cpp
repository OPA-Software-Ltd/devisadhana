#include "audio/audio_capture.hpp"
#include "audio/vad.hpp"
#include "asr/vosk_asr.hpp"
#include "definition/definition.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <csignal>
#include <regex>
#include <nlohmann/json.hpp>
#include <mutex>
#include "phrase/phrase_manager.hpp"

// Add these includes at the top
#include <filesystem>
#include <fstream>

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
        // In main(), before loading the ritual:
        const std::string ritualPath = "rituals/definitions/ganapati/maha_ganapati_caturvrtti_tarpanam.json";

        // Check if the file exists and is readable
        if (std::filesystem::exists(ritualPath)) {
            std::cout << "File exists at: " << std::filesystem::absolute(ritualPath) << std::endl;
            
            // Try to read the file content
            std::ifstream file(ritualPath);
            if (file.is_open()) {
                std::string content((std::istreambuf_iterator<char>(file)),
                                  std::istreambuf_iterator<char>());
                std::cout << "Successfully read " << content.length() << " bytes from file" << std::endl;
                std::cout << "First 100 characters of file:\n" << content.substr(0, 100) << std::endl;
            } else {
                std::cerr << "File exists but cannot be opened" << std::endl;
            }
        } else {
            std::cerr << "File does not exist at: " << std::filesystem::absolute(ritualPath) << std::endl;
            
            // Try to list the contents of the rituals directory
            try {
                std::cout << "Contents of rituals directory:" << std::endl;
                for (const auto& entry : std::filesystem::recursive_directory_iterator("rituals")) {
                    std::cout << entry.path() << std::endl;
                }
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "Error listing rituals directory: " << e.what() << std::endl;
            }
        }

        // Then proceed with loading the ritual
        sadhana::RitualDefinition ritual;

        // Test ritual definition loading
        std::cout << "Loading ritual definition...\n";
        if (!ritual.loadFromFile("rituals/definitions/ganapati/maha_ganapati_caturvrtti_tarpanam.json")) {
            std::cerr << "Failed to load ritual definition\n";
            return 1;
        }

        std::cout << "\nRitual Information:\n"
                  << "Title: " << ritual.getTitle() << "\n"
                  << "Version: " << ritual.getVersion() << "\n"
                  << "Source: " << ritual.getSource() << "\n"
                  << "Materials: " << ritual.getMaterials().size() << "\n"
                  << "Mantras: " << ritual.getMantras().size() << "\n"
                  << "Sections: " << ritual.getSections().size() << "\n\n";

        // Create PhraseManager with loaded ritual
        sadhana::PhraseManager phraseManager(ritual);

        // Rest of the existing audio setup code
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
        vadConfig.attackThreshold = 15.0f;
        vadConfig.releaseThreshold = 12.0f;
        vadConfig.hangTimeMs = 500;
        vadConfig.calibrationMs = 2000;
        vadConfig.calibrationAttackFactor = 0.05f;
        vadConfig.calibrationReleaseAboveFloor = 10.0f;

        sadhana::VAD vad(vadConfig);
        sadhana::VoskASR asr({
            .modelPath = "models/vosk-model-small-en-us-0.15",
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

            // Process speech when it ends
            if (recording && (!isSpeechActive && wasSpeechActive)) {
                if (speechBuffer.size() >= sadhana::AudioCapture::DEFAULT_SAMPLE_RATE * 0.3) {
                    std::string result = asr.processAudio(speechBuffer.data(), speechBuffer.size());

                    try {
                        auto j = nlohmann::json::parse(result);
                        if (j.contains("text")) {
                            std::string text = j["text"];
                            if (!text.empty()) {
                                std::lock_guard<std::mutex> lock(consoleMutex);
                                std::cout << "\nTranscribed: \"" << text << "\"\n";

                                // Try to match the phrase with ritual markers
                                auto match = phraseManager.matchPhrase(text);
                                if (!match.matchedText.empty()) {
                                    std::cout << "Matched: " << match.matchedText << "\n"
                                              << "Section: " << match.sectionId << "\n"
                                              << "Type: " << match.markerType << "\n"
                                              << "Confidence: " << match.confidence << "\n"
                                              << std::flush;
                                }
                            }
                        }
                    } catch (const std::exception& e) {
                        std::lock_guard<std::mutex> lock(consoleMutex);
                        std::cout << "\nDebug: JSON parsing failed: " << e.what() << "\n";
                    }
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