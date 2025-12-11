#pragma once

#include "core/interfaces.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace cafe {

/**
 * @brief Generic streaming service for any capture device
 * Single Responsibility: Manages streaming loop for any ICaptureDevice
 * Open/Closed: Works with any capture device without modification
 * Dependency Inversion: Depends on ICaptureDevice abstraction
 */
class StreamingService : public IStreamable {
public:
    explicit StreamingService(std::shared_ptr<ICaptureDevice> captureDevice)
        : captureDevice_(std::move(captureDevice))
        , streaming_(false)
        , fps_(30) {}

    void startStream(IMessageSender& sender, int fps) override {
        if (streaming_) return;
        
        fps_ = fps;
        streaming_ = true;
        
        streamThread_ = std::thread([this, &sender]() {
            runStreamLoop(sender);
        });
    }

    void stopStream() override {
        streaming_ = false;
        if (streamThread_.joinable()) {
            streamThread_.join();
        }
    }

    bool isStreaming() const override {
        return streaming_;
    }

    void setCaptureDevice(std::shared_ptr<ICaptureDevice> device) {
        if (!streaming_) {
            captureDevice_ = std::move(device);
        }
    }

private:
    std::shared_ptr<ICaptureDevice> captureDevice_;
    std::atomic<bool> streaming_;
    int fps_;
    std::thread streamThread_;

    void runStreamLoop(IMessageSender& sender) {
        const auto frameInterval = std::chrono::milliseconds(1000 / fps_);
        
        while (streaming_) {
            auto start = std::chrono::steady_clock::now();
            
            auto frame = captureDevice_->captureFrame();
            if (!frame.empty()) {
                sender.sendBinary(frame);
            }
            
            auto elapsed = std::chrono::steady_clock::now() - start;
            auto sleepTime = frameInterval - 
                std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
            
            if (sleepTime.count() > 0) {
                std::this_thread::sleep_for(sleepTime);
            }
        }
    }
};

} // namespace cafe
