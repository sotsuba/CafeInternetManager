#include "api/monitor.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
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
    Wsl,
    Unknown,
};

bool is_wsl() {
    FILE *fp = std::fopen("/proc/version", "r");
    if (!fp) return false;

    char buffer[256];
    bool wsl = false;
    if (std::fgets(buffer, sizeof(buffer), fp)) {
        if (std::strstr(buffer, "microsoft") || std::strstr(buffer, "WSL")) {
            wsl = true;
        }
    }
    std::fclose(fp);
    return wsl;
}

bool command_exists(const char *cmd) {
    std::string check = std::string("which ") + cmd + " > /dev/null 2>&1";
    return std::system(check.c_str()) == 0;
}

CaptureBackend detect_capture_backend() {
    if (is_wsl()) {
        return CaptureBackend::Wsl;
    }

    if (command_exists("grim") && std::getenv("WAYLAND_DISPLAY") != nullptr) {
        return CaptureBackend::Grim;
    }

    if (std::getenv("DISPLAY") != nullptr) {
        if (command_exists("scrot")) return CaptureBackend::Scrot;
        if (command_exists("import")) return CaptureBackend::Import;
    }

    return CaptureBackend::Unknown;
}

std::optional<std::string> detect_hyprland_output() {
    FILE *fp = popen(
        "hyprctl monitors -j 2>/dev/null | grep -o '\"name\":\"[^\"]*' | head -1 | cut -d'\"' -f4",
        "r"
    );
    if (!fp) return std::nullopt;

    char output_name[256];
    std::optional<std::string> out;
    if (std::fgets(output_name, sizeof(output_name), fp)) {
        output_name[std::strcspn(output_name, "\n")] = 0;
        if (std::strlen(output_name) > 0) {
            out = std::string(output_name);
        }
    }
    pclose(fp);
    return out;
}

std::vector<uint8_t> exec_popen(const std::string& cmd) {
    std::vector<uint8_t> data;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};

    std::array<uint8_t, 4096> buffer;
    while (true) {
        size_t bytes = fread(buffer.data(), 1, buffer.size(), pipe);
        if (bytes > 0) {
            data.insert(data.end(), buffer.begin(), buffer.begin() + bytes);
        } else {
            break;
        }
    }
    pclose(pipe);
    return data;
}

class DummyCapturer final : public ICapturer {
public:
    std::vector<uint8_t> capture() override {
        std::vector<uint8_t> v(1024);
        for (size_t i = 0; i < v.size(); ++i) v[i] = static_cast<uint8_t>(i & 0xff);
        return v;
    }

    std::string name() const override { return "dummy"; }
    bool is_available() const override { return true; }
};

class CommandCapturer final : public ICapturer {
public:
    CommandCapturer(CaptureBackend backend, float jpeg_scale, int jpeg_quality)
        : backend_(backend), jpeg_scale_(jpeg_scale), jpeg_quality_(jpeg_quality) {
        if (backend_ == CaptureBackend::Grim) {
            hypr_output_ = detect_hyprland_output();
        }
    }

    std::vector<uint8_t> capture() override {
        std::string command;
        int scale_percent = static_cast<int>(jpeg_scale_ * 100);

        switch (backend_) {
        case CaptureBackend::Grim:
            if (hypr_output_) {
                command = (std::ostringstream{}
                    << "grim -s " << jpeg_scale_ << " -o " << *hypr_output_
                    << " -t jpeg -q " << jpeg_quality_ << " -").str();
            } else {
                command = (std::ostringstream{}
                    << "grim -s " << jpeg_scale_ << " -t jpeg -q " << jpeg_quality_
                    << " -").str();
            }
            break;

        case CaptureBackend::Scrot:
            command = (std::ostringstream{}
                << "scrot -z -o - | convert - -resize " << scale_percent << "%"
                << " -quality " << jpeg_quality_ << " jpeg:-").str();
            break;

        case CaptureBackend::Import:
            command = (std::ostringstream{}
                << "import -window root -resize " << scale_percent << "%"
                << " -quality " << jpeg_quality_ << " jpeg:-").str();
            break;

        default:
            return {};
        }

        command += " 2>/dev/null";
        return exec_popen(command);
    }

    std::string name() const override {
        switch (backend_) {
        case CaptureBackend::Grim: return "grim (pipe)";
        case CaptureBackend::Scrot: return "scrot (pipe)";
        case CaptureBackend::Import: return "import (pipe)";
        case CaptureBackend::Wsl: return "wsl";
        case CaptureBackend::Unknown: return "unknown";
        }
        return "unknown";
    }

    bool is_available() const override {
        switch (backend_) {
        case CaptureBackend::Grim: return command_exists("grim") && std::getenv("WAYLAND_DISPLAY") != nullptr;
        case CaptureBackend::Scrot: return command_exists("scrot") && std::getenv("DISPLAY") != nullptr;
        case CaptureBackend::Import: return command_exists("import") && std::getenv("DISPLAY") != nullptr;
        default: return false;
        }
    }

private:
    CaptureBackend backend_;
    float jpeg_scale_;
    int jpeg_quality_;
    std::optional<std::string> hypr_output_;
};

} // namespace

std::unique_ptr<ICapturer> create_best_capturer() {
    constexpr int JPEG_QUALITY = 85;
    constexpr float JPEG_SCALE = 0.85f;

    CaptureBackend backend = detect_capture_backend();
    switch (backend) {
    case CaptureBackend::Grim:
    case CaptureBackend::Scrot:
    case CaptureBackend::Import:
         return std::make_unique<CommandCapturer>(backend, JPEG_SCALE, JPEG_QUALITY);
    case CaptureBackend::Wsl:
    case CaptureBackend::Unknown:
    default:
        return std::make_unique<DummyCapturer>();
    }
}

Monitor::Monitor()
    : recording_(false)
    , record_fps_(30)
    , frames_recorded_(0) {
}

Monitor::~Monitor() {
    stop_recording();
}

void Monitor::ensure_capturer_() {
    if (!capturer_) {
        capturer_ = create_best_capturer();
    }
}

std::vector<uint8_t> Monitor::capture_frame() {
    std::lock_guard<std::mutex> lock(mutex_);
    ensure_capturer_();

    auto frame = capturer_->capture();
    if (frame.empty()) {
        last_error_ = "Failed to capture screen";
    }
    return frame;
}

// Helper to get X11 screen resolution
std::string detect_screen_resolution() {
    FILE *fp = popen("xdpyinfo | grep dimensions | awk '{print $2}'", "r");
    if (!fp) return "1366x768";

    char buffer[128];
    std::string res = "1366x768";
    if (std::fgets(buffer, sizeof(buffer), fp)) {
        buffer[std::strcspn(buffer, "\n")] = 0;
        if (std::strlen(buffer) > 0) {
            // Ensure dimensions are even (libx264 requirement)
            int w, h;
            if (sscanf(buffer, "%dx%d", &w, &h) == 2) {
                if (w % 2 != 0) w--;
                if (h % 2 != 0) h--;
                res = std::to_string(w) + "x" + std::to_string(h);
            } else {
                 res = std::string(buffer);
            }
        }
    }
    pclose(fp);
    return res;
}

// === NEW FFmpeg H.264 Implementation ===
void Monitor::stream_h264(StreamCallback cb) {
    // Detect actual screen resolution to prevent FFmpeg errors
    std::string resolution = detect_screen_resolution();

    // We use -f mpegts to output a raw stream that jmuxer can read.
    std::string cmd = "ffmpeg -f x11grab -draw_mouse 1 -framerate 30 "
                      "-video_size " + resolution + " -i :0.0 "
                      "-c:v libx264 -preset ultrafast -tune zerolatency -g 30 "
                      "-profile:v baseline -level 3.0 -bf 0 "
                      "-pix_fmt yuv420p "
                      "-f h264 - 2>ffmpeg.log";

    // Attempt open pipe
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        // Fallback: Try smaller resolution or auto detection?
        // For now just fail.
        return;
    }

    std::vector<uint8_t> buffer(65536); // Reverted to 64KB (User verified this was stable)
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

std::string Monitor::backend_name() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!capturer_) return "(uninitialized)";
    return capturer_->name();
}

std::string Monitor::get_last_error() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

// ... Recording methods (kept same) ...
bool Monitor::start_recording(const std::string& filename, int fps) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (recording_) return false;

    ensure_capturer_();
    record_filename_ = filename;
    record_fps_ = fps > 0 ? fps : 30;
    frames_recorded_ = 0;
    recording_ = true;
    record_thread_ = std::thread(&Monitor::recording_loop_, this);
    return true;
}

void Monitor::stop_recording() {
    recording_ = false;
    if (record_thread_.joinable()) record_thread_.join();
}

bool Monitor::is_recording() const { return recording_; }
size_t Monitor::frames_recorded() const { return frames_recorded_; }

void Monitor::set_frame_callback(FrameCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    frame_callback_ = std::move(cb);
}

void Monitor::recording_loop_() {
    std::ofstream out;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        out.open(record_filename_, std::ios::binary | std::ios::trunc);
        if (!out) { recording_ = false; return; }
    }

    auto frame_duration = std::chrono::microseconds(1000000 / record_fps_);
    auto next_frame = std::chrono::steady_clock::now();

    while (recording_) {
        next_frame += frame_duration;
        std::vector<uint8_t> frame;
        FrameCallback cb;
        {
            frame = capture_frame();
            std::lock_guard<std::mutex> lock(mutex_);
            cb = frame_callback_;
        }
        if (!frame.empty()) {
            out.write(reinterpret_cast<const char*>(frame.data()), frame.size());
            out.flush();
            if (cb) cb(frame);
        }
        std::this_thread::sleep_until(next_frame);
    }
    out.close();
}
