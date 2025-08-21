#pragma once

#include <functional>
#include <thread>
#include <atomic>
#include <termios.h>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>

namespace sadhana {

class KeyboardHandler {
public:
    using KeyCallback = std::function<void()>;
    
    KeyboardHandler() : running_(false), spaceCallback_(nullptr) {
        // Save terminal settings
        if (tcgetattr(STDIN_FILENO, &oldSettings_) < 0) {
            std::cerr << "Failed to get terminal attributes\n";
            return;
        }
        
        newSettings_ = oldSettings_;
        newSettings_.c_lflag &= ~(ICANON | ECHO | ISIG);  // Also disable signals
        newSettings_.c_cc[VMIN] = 1;    // Wait for at least one character
        newSettings_.c_cc[VTIME] = 0;   // No timeout
        
        // Set non-blocking mode
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        if (flags < 0 || fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) < 0) {
            std::cerr << "Failed to set non-blocking mode\n";
            return;
        }
    }
    
    ~KeyboardHandler() {
        stop();
        // Restore terminal settings
        tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings_);
    }
    
    void start() {
        if (running_) return;
        
        std::cout << "Debug: KeyboardHandler starting...\n" << std::flush;
        
        // Apply new terminal settings
        if (tcsetattr(STDIN_FILENO, TCSANOW, &newSettings_) < 0) {
            std::cerr << "Failed to set terminal attributes\n";
            return;
        }
        
        running_ = true;
        thread_ = std::thread([this]() {
            char c;
            auto lastSpaceTime = std::chrono::steady_clock::now();
            const auto debounceTime = std::chrono::milliseconds(250);  // Reduced debounce time
            const auto pollInterval = std::chrono::milliseconds(1);    // More frequent polling
            
            while (running_) {
                if (read(STDIN_FILENO, &c, 1) > 0) {
                    if (c == ' ' || c == '\n') {  // Also accept Enter key
                        auto now = std::chrono::steady_clock::now();
                        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - lastSpaceTime) >= debounceTime) {
                            std::cout << "Debug: Space/Enter key detected\n" << std::flush;
                            if (spaceCallback_) {
                                try {
                                    spaceCallback_();
                                } catch (const std::exception& e) {
                                    std::cerr << "Error in space callback: " << e.what() << std::endl;
                                }
                            }
                            lastSpaceTime = now;
                        }
                    }
                }
                std::this_thread::sleep_for(pollInterval);
            }
        });
    }
    
    void stop() {
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
    }
    
    void setSpaceCallback(KeyCallback callback) {
        spaceCallback_ = std::move(callback);
    }

private:
    std::atomic<bool> running_;
    std::thread thread_;
    KeyCallback spaceCallback_;
    struct termios oldSettings_;
    struct termios newSettings_;
};

} // namespace sadhana