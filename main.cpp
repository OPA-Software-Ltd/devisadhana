#include "audio/audio_capture.hpp"
#include "audio/vad.hpp"
#include "asr/vosk_asr.hpp"
#include "definition/definition.hpp"
#include "ritual/flow_manager.hpp"
#include "ritual/display_manager.hpp"
#include "ritual/keyboard_handler.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <csignal>
#include <regex>
#include <nlohmann/json.hpp>
#include <mutex>
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

int main() {
    signal(SIGINT, signalHandler);

    try {
        // Load ritual definition
        sadhana::RitualDefinition ritual;
        if (!ritual.loadFromFile("rituals/definitions/ganapati/maha_ganapati_caturvrtti_tarpanam.json")) {
            std::cerr << "Failed to load ritual definition\n";
            return 1;
        }

        // Add this after loading the ritual definition
        std::cout << "\nDebug: Initial section structure:\n";
        for (const auto& section : ritual.getSections()) {
            std::cout << "Section: " << section.id << "\n";
            if (section.parts) {
                for (const auto& part : *section.parts) {
                    std::cout << "  Part: " << part.id;
                    if (part.utterance) {
                        std::cout << " (utterance: " << *part.utterance << ")";
                    }
                    std::cout << "\n";
                }
            }
            std::cout << "\n";
        }

        // Display ritual information
        std::cout << "\nRitual Information:\n"
                  << "Title: " << ritual.getTitle() << "\n"
                  << "Version: " << ritual.getVersion() << "\n"
                  << "Source: " << ritual.getSource() << "\n"
                  << "Materials: " << ritual.getMaterials().size() << "\n"
                  << "Mantras: " << ritual.getMantras().size() << "\n"
                  << "Sections: " << ritual.getSections().size() << "\n";

        // Initialize managers
        sadhana::FlowManager flowManager(ritual);
        if (!flowManager.loadFlowConfiguration("rituals/definitions/ganapati/flow.json")) {
            std::cerr << "Failed to load flow configuration\n";
            return 1;
        }

        sadhana::DisplayManager displayManager;
        std::mutex consoleMutex;

        // Setup Phase
        std::cout << "\n=== Setup Phase ===\n";
        std::cout << "1. First, we'll select your audio input device\n";
        std::cout << "2. Then we'll calibrate the audio levels\n";
        std::cout << "3. Finally, you can begin the ritual\n\n";

        // Audio device selection
        sadhana::AudioCapture audio;
        auto devices = audio.listDevices();
        std::cout << "Available input devices:\n";
        std::cout << "------------------------\n";
        for (const auto& device : devices) {
            std::cout << "[" << device.index << "] " << device.name << "\n";
        }
        std::cout << "------------------------\n";
        std::cout << "Enter the number of your preferred input device: " << std::flush;

        int deviceIndex;
        std::cin >> deviceIndex;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (!audio.setDevice(deviceIndex)) {
            std::cerr << "Failed to set device\n";
            return 1;
        }

        // Setup VAD and ASR
        sadhana::VAD::Config vadConfig;
        vadConfig.attackThreshold = 15.0f;
        vadConfig.releaseThreshold = 12.0f;
        vadConfig.hangTimeMs = 2000;        // Increase from 500 to 2000
        vadConfig.calibrationMs = 2000;
        vadConfig.calibrationAttackFactor = 0.05f;
        vadConfig.calibrationReleaseAboveFloor = 10.0f;
        vadConfig.maxSilenceMs = 3000;      // Add this line - max silence before stopping
        vadConfig.maxRecordingMs = 10000;   // Add this line - max total recording time

        sadhana::VAD vad(vadConfig);
        sadhana::VoskASR asr({
            .modelPath = "models/vosk-model-en-in-0.5",
            .sampleRate = sadhana::AudioCapture::DEFAULT_SAMPLE_RATE
        });

        if (!asr.init()) {
            std::cerr << "Failed to initialize ASR\n";
            return 1;
        }

        // Setup keyboard handler - place this BEFORE starting audio processing
        sadhana::KeyboardHandler keyboardHandler;

        // Set up the progress callback for display updates
        flowManager.setProgressCallback([&displayManager, &ritual](const sadhana::FlowProgress& progress) {
            std::cout << "Debug: Progress callback triggered\n" << std::flush;
            displayManager.updateDisplay(progress, ritual, -60.0f);
        });

        // Set up the space key callback
        keyboardHandler.setSpaceCallback([&flowManager, &displayManager, &ritual]() {
            std::cout << "Debug: Space callback triggered\n" << std::flush;
            flowManager.handleManualIntervention();
            const auto& progress = flowManager.getCurrentProgress();
            displayManager.updateDisplay(progress, ritual, -60.0f);
            displayManager.requestUpdate();
        });

        // Start keyboard handling
        std::cout << "Debug: Starting keyboard handler\n" << std::flush;
        keyboardHandler.start();

        // Calibration Phase
        std::cout << "\n=== Calibration Phase ===\n";
        std::cout << "Please remain quiet for 2 seconds while we calibrate background noise levels...\n";

        bool calibrating = true;
        bool recording = false;
        std::vector<float> speechBuffer;
        int calibrationSamplesRemaining = 
            vadConfig.calibrationMs * (sadhana::AudioCapture::DEFAULT_SAMPLE_RATE / 1000);

        // Start audio processing
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
                        std::cout << "\n=== Ritual Phase ===\n";
                        std::cout << "Calibration complete! You can now:\n";
                        std::cout << "- Speak mantras clearly into the microphone\n";
                        std::cout << "- Press SPACE if you need to manually advance\n\n";
                        std::cout << "Current Status:\n";
                        std::cout << "---------------\n";
                    }
                }
                return;
            }

            float currentLevel = calculateRMSdB(samples, numSamples);
            bool wasSpeechActive = vad.isSpeechActive();
            bool isSpeechActive = vad.process(samples, numSamples);

            if (isSpeechActive && !recording) {
                recording = true;
                speechBuffer.clear();
            }

            if (recording) {
                speechBuffer.insert(speechBuffer.end(), samples, samples + numSamples);
            }

            if (recording && (!isSpeechActive && wasSpeechActive)) {
                std::string result = asr.processAudio(speechBuffer.data(), speechBuffer.size());
                try {
                    auto j = nlohmann::json::parse(result);
                    if (j.contains("text")) {
                        std::string text = j["text"];
                        if (!text.empty()) {
                            {
                                std::lock_guard<std::mutex> lock(consoleMutex);
                                std::cout << "Debug: Raw Vosk output: " << result << std::endl;
                                displayManager.showMessage("Recognized: \"" + text + "\"");
                            }
                            
                            // Only try to handle the phrase if we got actual text
                            flowManager.handleRecognizedPhrase(text, 0.8f);
                            displayManager.requestUpdate();  // Request display update after handling
                        }
                    }
                } catch (...) {}
                recording = false;
                speechBuffer.clear();
            }

// In the audio callback, replace the existing display update logic with:
if (!calibrating) {
    // Only show display updates in two cases:
    // 1. When speech is processed and recognized
    // 2. When space key is pressed (already handled in keyboard callback)
    
    // Initial display after calibration
    static bool initialDisplay = true;
    if (initialDisplay) {
        const auto& progress = flowManager.getCurrentProgress();
        displayManager.updateDisplay(progress, ritual, currentLevel);
        initialDisplay = false;
    }
    
    // Only update display when we have recognized speech
    if (recording && (!isSpeechActive && wasSpeechActive)) {
        std::string result = asr.processAudio(speechBuffer.data(), speechBuffer.size());
        try {
            auto j = nlohmann::json::parse(result);
            if (j.contains("text")) {
                std::string text = j["text"];
                if (!text.empty()) {
                    {
                        std::lock_guard<std::mutex> lock(consoleMutex);
                        std::cout << "Debug: Raw Vosk output: " << result << std::endl;
                        displayManager.showMessage("Recognized: \"" + text + "\"");
                    }
                    
                    // Handle the recognized phrase
                    flowManager.handleRecognizedPhrase(text, 0.8f);
                    // Update display after handling the phrase
                    displayManager.requestUpdate();
                }
            }
        } catch (...) {}
        recording = false;
        speechBuffer.clear();
    }
}
        });

        while (running) {
            std::cout << "." << std::flush;  // Visual indicator that the main loop is running
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (flowManager.isComplete()) {
                displayManager.showMessage("Ritual complete! Press Ctrl+C to exit.");
            }
        }

        keyboardHandler.stop();
        audio.stop();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}