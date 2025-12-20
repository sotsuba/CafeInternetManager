#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class ICapturer {
public:
    virtual ~ICapturer() = default;

    // Capture screen to menory (JPEG format)
    virtual std::vector<uint8_t> capture() = 0;
    
    // Get backend name for logging
    virtual std::string name() const = 0;

    // Check if the backend is available on this system.
    virtual bool is_available() const = 0;
};

// Create the best capturer available on this system.
// Order: grim (Wayland) -> scrot (X11) -> import (X11) -> dummy.
std::unique_ptr<ICapturer> create_best_capturer();

// Monitor API: capture screen frames and optionally record them.
// No extra libraries required (uses system tools when available).
class Monitor {
public:
    Monitor();
    ~Monitor();

    Monitor(const Monitor&) = delete;
    Monitor& operator=(const Monitor&) = delete;

    // Capture a single JPEG frame. Returns empty vector on failure.
    std::vector<uint8_t> capture_frame();

    // Recording: writes a raw MJPEG stream (concatenated JPEG frames) to filename.
    // Recommended extension: .mjpeg or .mjpg
    bool start_recording(const std::string& filename, int fps = 30);
    void stop_recording();
    bool is_recording() const;
    size_t frames_recorded() const;

    // Optional callback invoked for each captured frame while recording.
    using FrameCallback = std::function<void(const std::vector<uint8_t>&)>;
    void set_frame_callback(FrameCallback cb);

    std::string get_last_error() const;
    std::string backend_name() const;

private:
    void ensure_capturer_();
    void recording_loop_();

    mutable std::mutex mutex_;
    std::unique_ptr<ICapturer> capturer_;
    std::string last_error_;

    std::atomic<bool> recording_;
    std::thread record_thread_;
    int record_fps_;
    size_t frames_recorded_;
    std::string record_filename_;
    FrameCallback frame_callback_;
};


