#include "api/monitor.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <memory>
#include <array>
#include <vector>
#include <thread>
#include <mutex>

namespace {

enum class CaptureBackend {
    Grim,
    Scrot,
    Import,
    Dummy
};

// Dummy JPEG (tiny valid JPEG for fallback)
void make_dummy_jpeg(std::vector<uint8_t>& out, int width_hint) {
    (void)width_hint;
    const uint8_t mini[] = {
        0xFF,0xD8,0xFF,0xE0,0,16,'J','F','I','F',0,1,1,0,0,1,0,1,0,0,
        0xFF,0xDB,0,67,0,8,6,6,7,6,5,8,7,7,7,9,9,8,10,12,20,13,12,11,11,12,25,
        18,19,15,20,29,26,31,30,29,26,28,28,32,36,46,39,32,34,44,35,28,28,40,55,
        41,44,48,49,52,52,52,31,39,57,61,56,50,60,46,51,52,50,0xFF,0xC0,0,11,8,0,
        1,0,1,1,1,17,0,0xFF,0xC4,0,20,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,9,0xFF,
        0xC4,0,20,16,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0xFF,0xDA,0,8,1,1,0,0,63,
        0,85,95,0xFF,0xD9
    };
    out.assign(mini, mini + sizeof(mini));
}

std::string exec_and_get_output(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    // Fix warning: ignoring attributes on template argument
    using PcloseLogin = int(*)(FILE*);
    std::unique_ptr<FILE, PcloseLogin> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

class GrimCapturer : public ICapturer {
public:
    std::vector<uint8_t> capture() override {
        std::string cmd = "grim -t jpeg - 2>/dev/null";
        FILE* fp = popen(cmd.c_str(), "r");
        if (!fp) return {};

        std::vector<uint8_t> buf;
        buf.reserve(1024*1024);
        uint8_t tmp[4096];
        while (true) {
            size_t n = fread(tmp, 1, sizeof(tmp), fp);
            if (n == 0) break;
            buf.insert(buf.end(), tmp, tmp + n);
        }
        pclose(fp);
        return buf;
    }

    std::string name() const override { return "grim"; }

bool is_available() const override {
        std::string result = exec_and_get_output("which grim 2>/dev/null");
        return !result.empty();
    }
};

class ScrotCapturer : public ICapturer {
public:
    std::vector<uint8_t> capture() override {
        std::string tmpfile = "/tmp/scrot_cap.jpg";
        std::string cmd = std::string("scrot -o ") + tmpfile + " 2>/dev/null";
        int ret = system(cmd.c_str());
        if (ret != 0) return {};

        std::ifstream f(tmpfile, std::ios::binary | std::ios::ate);
        if (!f) return {};
        std::streamsize size = f.tellg();
        f.seekg(0, std::ios::beg);

        std::vector<uint8_t> buf(size);
        if (!f.read((char*)buf.data(), size)) return {};
        return buf;
    }

    std::string name() const override { return "scrot"; }

    bool is_available() const override {
        std::string result = exec_and_get_output("which scrot 2>/dev/null");
        return !result.empty();
    }
};

class ImportCapturer : public ICapturer {
public:
    std::vector<uint8_t> capture() override {
        std::string tmpfile = "/tmp/import_cap.jpg";
        std::string cmd = std::string("import -window root -silent ") + tmpfile + " 2>/dev/null";
        int ret = system(cmd.c_str());
        if (ret != 0) return {};

        std::ifstream f(tmpfile, std::ios::binary | std::ios::ate);
        if (!f) return {};
        std::streamsize size = f.tellg();
        f.seekg(0, std::ios::beg);

        std::vector<uint8_t> buf(size);
        if (!f.read((char*)buf.data(), size)) return {};
        return buf;
    }

    std::string name() const override { return "import"; }

    bool is_available() const override {
        std::string result = exec_and_get_output("which import 2>/dev/null");
        return !result.empty();
    }
};

class DummyCapturer : public ICapturer {
public:
    std::vector<uint8_t> capture() override {
        std::vector<uint8_t> dummy;
        make_dummy_jpeg(dummy, 0);
        return dummy;
    }

    std::string name() const override { return "dummy"; }
    bool is_available() const override { return true; }
};

}  // namespace

std::unique_ptr<ICapturer> create_best_capturer() {
    GrimCapturer grim;
    if (grim.is_available())
        return std::make_unique<GrimCapturer>();

    ScrotCapturer scrot;
    if (scrot.is_available())
        return std::make_unique<ScrotCapturer>();

    ImportCapturer import;
    if (import.is_available())
        return std::make_unique<ImportCapturer>();

    return std::make_unique<DummyCapturer>();
}

Monitor::Monitor() : recording_(false), record_fps_(30), frames_recorded_(0) {}

Monitor::~Monitor() {
    stop_recording();
}

void Monitor::ensure_capturer_() {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!capturer_) {
        capturer_ = create_best_capturer();
    }
}

std::vector<uint8_t> Monitor::capture_frame() {
    ensure_capturer_();
    std::lock_guard<std::mutex> lk(mutex_);
    auto frame = capturer_->capture();
    if (frame.empty()) {
        last_error_ = "Failed to capture frame with " + capturer_->name();
    }
    return frame;
}

bool Monitor::start_recording(const std::string& filename, int fps) {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (recording_.load()) {
            last_error_ = "Already recording";
            return false;
        }
        record_filename_ = filename;
        record_fps_ = fps;
        frames_recorded_ = 0;
    }

    recording_.store(true);
    record_thread_ = std::thread(&Monitor::recording_loop_, this);
    return true;
}

void Monitor::stop_recording() {
    if (!recording_.load()) return;
    recording_.store(false);
    if (record_thread_.joinable()) {
        record_thread_.join();
    }
}

bool Monitor::is_recording() const {
    return recording_.load();
}

size_t Monitor::frames_recorded() const {
    return frames_recorded_;
}

void Monitor::set_frame_callback(FrameCallback cb) {
    std::lock_guard<std::mutex> lk(mutex_);
    frame_callback_ = std::move(cb);
}

std::string Monitor::get_last_error() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return last_error_;
}

std::string Monitor::backend_name() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return capturer_ ? capturer_->name() : "none";
}

void Monitor::recording_loop_() {
    ensure_capturer_();

    std::ofstream out(record_filename_, std::ios::binary);
    if (!out) {
        last_error_ = "Failed to open " + record_filename_;
        recording_.store(false);
        return;
    }

    auto interval = std::chrono::milliseconds(1000 / record_fps_);

    while (recording_.load()) {
        auto t0 = std::chrono::steady_clock::now();

        auto frame = capture_frame();
        if (!frame.empty()) {
            out.write((const char*)frame.data(), frame.size());
            frames_recorded_++;

            std::lock_guard<std::mutex> lk(mutex_);
            if (frame_callback_) {
                frame_callback_(frame);
            }
        }

        auto t1 = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
        auto sleep_time = interval - elapsed;
        if (sleep_time.count() > 0) {
            std::this_thread::sleep_for(sleep_time);
        }
    }
}

// Detect screen resolution
std::string detect_screen_resolution() {
    // Try xrandr (Standard X11)
    std::string cmd = "xrandr 2>/dev/null | grep '*' | awk '{print $1}' | head -n1";
    FILE* fp = popen(cmd.c_str(), "r");
    if (fp) {
        char buffer[256];
        std::string res;
        if (fgets(buffer, sizeof(buffer), fp)) {
            res = buffer;
            if (!res.empty() && res.back() == '\n') res.pop_back();
        }
        pclose(fp);
        if (!res.empty()) return res;
    }

    // Try reading sysfs (works on some embedded/framebuffer devices)
    // /sys/class/graphics/fb0/virtual_size usually "1920,1080"
    std::ifstream fb("/sys/class/graphics/fb0/virtual_size");
    if (fb) {
        std::string line;
        if (std::getline(fb, line)) {
            std::replace(line.begin(), line.end(), ',', 'x');
            return line;
        }
    }

    // Fallback safe default
    std::cerr << "[Monitor] Warning: Could not detect resolution. Defaulting to 1920x1080." << std::endl;
    return "1920x1080";
}

// === NEW FFmpeg H.264 Implementation ===
void Monitor::stream_h264(StreamCallback cb) {
    // Detect actual screen resolution to prevent FFmpeg errors
    std::string resolution = detect_screen_resolution();

    // We use -f h264 to output raw H.264 stream
    std::string cmd = "ffmpeg -f x11grab -draw_mouse 1 -framerate 30 "
                      "-video_size " + resolution + " -i :0.0 "
                      "-c:v libx264 -preset ultrafast -tune zerolatency -g 30 "
                      "-profile:v baseline -level 3.0 -bf 0 "
                      "-pix_fmt yuv420p "
                      "-f h264 - 2>ffmpeg.log";

    // Attempt open pipe
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return;
    }

    std::vector<uint8_t> buffer(65536); // 64KB buffer
    while (true) {
        size_t bytes = fread(buffer.data(), 1, buffer.size(), pipe);
        if (bytes > 0) {
            // Resize vector to actual bytes read for the callback
            std::vector<uint8_t> chunk(buffer.begin(), buffer.begin() + bytes);
            if (!cb(chunk)) {
                break; // Callback requested stop
            }
        } else {
            break; // EOF or error
        }
    }
    pclose(pipe);
}
