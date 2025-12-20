#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <atomic>
#include <thread>
#include <functional>

class Webcam {
public:
    explicit Webcam(int device_index = 0);
    ~Webcam();

    Webcam(const Webcam&) = delete;
    Webcam& operator=(const Webcam&) = delete;

    bool open(int width = 640, int height = 480);
    void close();
    bool is_open() const;

    std::vector<uint8_t> capture_frame();

    
    // Start recording to file. Format depends on extension:
    //   .mjpeg / .mjpg  — raw MJPEG stream (simplest, plays in VLC/ffplay)
    //   .avi            — simple AVI container with MJPEG
    // Returns true if recording started successfully.
    bool start_recording(const std::string& filename, int fps = 30);
    
    // Stop recording and finalize file.
    void stop_recording();
    
    // Check if currently recording.
    bool is_recording() const;
    
    // Get number of frames recorded so far.
    size_t get_frames_recorded() const;

    // Optional: set callback for each captured frame during recording
    using FrameCallback = std::function<void(const std::vector<uint8_t>& frame)>;
    void set_frame_callback(FrameCallback cb);

    // ─────────────────────────────────────────────────────────────
    // Getters
    // ─────────────────────────────────────────────────────────────
    int get_width() const;
    int get_height() const;
    std::string get_last_error() const;

private:
    bool init_mmap();
    void free_buffers();
    void recording_loop();
    void write_avi_header();
    void finalize_avi();

    int device_index_;
    int fd_;
    int width_;
    int height_;
    std::string last_error_;

    struct Buffer {
        void* start;
        size_t length;
    };
    std::vector<Buffer> buffers_;
    bool streaming_;

    // Recording state
    std::ofstream record_file_;
    std::string record_filename_;
    std::atomic<bool> recording_;
    std::thread record_thread_;
    int record_fps_;
    size_t frames_recorded_;
    std::vector<uint32_t> frame_offsets_;  // For AVI index
    FrameCallback frame_callback_;
};

std::vector<uint8_t> capture_webcam_frame(int webcam_index, int& width, int& height);
