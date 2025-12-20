#include "api/monitor.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>

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

    if (command_exists("scrot") && std::getenv("DISPLAY") != nullptr) {
        return CaptureBackend::Scrot;
    }

    if (command_exists("import") && std::getenv("DISPLAY") != nullptr) {
        return CaptureBackend::Import;
    }

    return CaptureBackend::Unknown;
}

std::vector<uint8_t> read_file_and_remove(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    std::remove(path.c_str());
    return data;
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
        // Similar behavior to server_enhanced: write to temp file then read.
        const std::string temp_file = "/tmp/screen_capture8.jpg";
        std::string command;

        switch (backend_) {
        case CaptureBackend::Grim:
            if (hypr_output_) {
                command = (std::ostringstream{}
                    << "grim -s " << jpeg_scale_ << " -o " << *hypr_output_
                    << " -t jpeg -q " << jpeg_quality_ << " - > '" << temp_file
                    << "' 2>/dev/null").str();
            } else {
                command = (std::ostringstream{}
                    << "grim -s " << jpeg_scale_ << " -t jpeg -q " << jpeg_quality_
                    << " - > '" << temp_file << "' 2>/dev/null").str();
            }
            break;

        case CaptureBackend::Scrot:
            command = (std::ostringstream{}
                << "scrot -o '" << temp_file << "' 2>/dev/null && "
                << "convert '" << temp_file << "' -quality " << jpeg_quality_
                << " '" << temp_file << "' 2>/dev/null").str();
            break;

        case CaptureBackend::Import:
            command = (std::ostringstream{}
                << "import -window root -quality " << jpeg_quality_
                << " '" << temp_file << "' 2>/dev/null").str();
            break;

        default:
            return {};
        }

        if (std::system(command.c_str()) != 0) {
            return {};
        }

        return read_file_and_remove(temp_file);
    }

    std::string name() const override {
        switch (backend_) {
        case CaptureBackend::Grim: return "grim";
        case CaptureBackend::Scrot: return "scrot";
        case CaptureBackend::Import: return "import";
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
    // Mirror server_enhanced defaults
    constexpr int JPEG_QUALITY = 75;
    constexpr float JPEG_SCALE = 0.25f;

    CaptureBackend backend = detect_capture_backend();
    switch (backend) {
    case CaptureBackend::Grim:
        return std::make_unique<CommandCapturer>(backend, JPEG_SCALE, JPEG_QUALITY);
    case CaptureBackend::Scrot:
        return std::make_unique<CommandCapturer>(backend, JPEG_SCALE, JPEG_QUALITY);
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

std::string Monitor::backend_name() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!capturer_) return "(uninitialized)";
    return capturer_->name();
}

std::string Monitor::get_last_error() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

bool Monitor::start_recording(const std::string& filename, int fps) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (recording_) {
        last_error_ = "Already recording";
        return false;
    }

    // Only support raw MJPEG stream for now (no extra libs)
    auto lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto ends_with = [](const std::string& s, const std::string& suffix) {
        return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    if (!(ends_with(lower, ".mjpeg") || ends_with(lower, ".mjpg"))) {
        last_error_ = "Only .mjpeg/.mjpg recording is supported";
        return false;
    }

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
    if (record_thread_.joinable()) {
        record_thread_.join();
    }
}

bool Monitor::is_recording() const {
    return recording_;
}

size_t Monitor::frames_recorded() const {
    return frames_recorded_;
}

void Monitor::set_frame_callback(FrameCallback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    frame_callback_ = std::move(cb);
}

void Monitor::recording_loop_() {
    // Open output file (binary append)
    std::ofstream out;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        out.open(record_filename_, std::ios::binary | std::ios::trunc);
        if (!out) {
            last_error_ = "Cannot open record file: " + record_filename_;
            recording_ = false;
            return;
        }
    }

    auto frame_duration = std::chrono::microseconds(1000000 / record_fps_);
    auto next_frame = std::chrono::steady_clock::now();

    while (recording_) {
        next_frame += frame_duration;

        std::vector<uint8_t> frame;
        FrameCallback cb;
        {
            // Capture outside holding callback lock is okay; we already serialize capture_frame()
            frame = capture_frame();
            std::lock_guard<std::mutex> lock(mutex_);
            cb = frame_callback_;
        }

        if (!frame.empty()) {
            out.write(reinterpret_cast<const char*>(frame.data()), frame.size());
            out.flush();
            ++frames_recorded_;
            if (cb) cb(frame);
        }

        std::this_thread::sleep_until(next_frame);
    }

    out.close();
}
