#pragma once

#include "core/interfaces.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace cafe {

/**
 * @brief Screen capture backend types
 */
enum class ScreenCaptureBackend {
    AUTO,           // Auto-detect best backend
    GRIM,           // Wayland (wlroots compositors)
    GNOME_SCREENSHOT, // GNOME
    SCROT,          // X11
    FFMPEG_X11,     // X11 via ffmpeg
    IMPORT          // ImageMagick
};

/**
 * @brief Desktop session types
 */
enum class SessionType {
    X11,
    WAYLAND_WLROOTS,
    WAYLAND_GNOME,
    UNKNOWN
};

/**
 * @brief Screen capture device implementation
 * Single Responsibility: Captures screen frames using appropriate backend
 * 
 * Note: On GNOME Wayland, screen capture may produce black frames due to
 * security restrictions. Use WebcamCapture for reliable monitoring.
 */
class ScreenCapture : public ICaptureDevice {
public:
    explicit ScreenCapture(ScreenCaptureBackend backend = ScreenCaptureBackend::AUTO)
        : backend_(backend)
        , jpegQuality_(75) {
        if (backend_ == ScreenCaptureBackend::AUTO) {
            detectBackend();
        }
    }

    std::vector<uint8_t> captureFrame() override {
        char tempFile[64];
        snprintf(tempFile, sizeof(tempFile), "/tmp/screen_%d.jpg", getpid());
        
        bool success = false;
        
        switch (backend_) {
            case ScreenCaptureBackend::GRIM:
                success = captureWithGrim(tempFile);
                break;
            case ScreenCaptureBackend::GNOME_SCREENSHOT:
                success = captureWithGnomeScreenshot(tempFile);
                break;
            case ScreenCaptureBackend::SCROT:
                success = captureWithScrot(tempFile);
                break;
            case ScreenCaptureBackend::FFMPEG_X11:
                success = captureWithFfmpegX11(tempFile);
                break;
            case ScreenCaptureBackend::IMPORT:
                success = captureWithImport(tempFile);
                break;
            default:
                success = tryAllBackends(tempFile);
                break;
        }
        
        if (!success) {
            return {};
        }
        
        auto data = readFile(tempFile);
        unlink(tempFile);
        return data;
    }

    bool isAvailable() const override {
        return backend_ != ScreenCaptureBackend::AUTO || detectSessionType() != SessionType::UNKNOWN;
    }

    std::string getName() const override {
        return "Screen Capture (" + getBackendName() + ")";
    }

    void setBackend(ScreenCaptureBackend backend) {
        backend_ = backend;
    }

    void setQuality(int quality) {
        jpegQuality_ = std::max(1, std::min(100, quality));
    }

private:
    ScreenCaptureBackend backend_;
    int jpegQuality_;

    static SessionType detectSessionType() {
        const char* sessionType = std::getenv("XDG_SESSION_TYPE");
        const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
        const char* display = std::getenv("DISPLAY");
        const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");
        
        std::string session = sessionType ? sessionType : "";
        
        if (session.empty() || session == "tty") {
            if (waylandDisplay && strlen(waylandDisplay) > 0) {
                session = "wayland";
            } else if (display && strlen(display) > 0) {
                session = "x11";
            }
        }
        
        if (session == "x11") {
            return SessionType::X11;
        } else if (session == "wayland") {
            if (desktop && strcmp(desktop, "GNOME") == 0) {
                return SessionType::WAYLAND_GNOME;
            }
            return SessionType::WAYLAND_WLROOTS;
        }
        
        return SessionType::UNKNOWN;
    }

    void detectBackend() {
        SessionType session = detectSessionType();
        
        switch (session) {
            case SessionType::X11:
                if (commandExists("scrot")) {
                    backend_ = ScreenCaptureBackend::SCROT;
                } else if (commandExists("import")) {
                    backend_ = ScreenCaptureBackend::IMPORT;
                } else {
                    backend_ = ScreenCaptureBackend::FFMPEG_X11;
                }
                break;
            case SessionType::WAYLAND_WLROOTS:
                backend_ = ScreenCaptureBackend::GRIM;
                break;
            case SessionType::WAYLAND_GNOME:
                backend_ = ScreenCaptureBackend::GNOME_SCREENSHOT;
                break;
            default:
                backend_ = ScreenCaptureBackend::GNOME_SCREENSHOT;
                break;
        }
    }

    static bool commandExists(const char* cmd) {
        char checkCmd[256];
        snprintf(checkCmd, sizeof(checkCmd), "which %s > /dev/null 2>&1", cmd);
        return system(checkCmd) == 0;
    }

    bool captureWithGrim(const char* outFile) const {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "grim -t jpeg -q %d '%s' 2>/dev/null", 
                 jpegQuality_, outFile);
        return system(cmd) == 0;
    }

    bool captureWithGnomeScreenshot(const char* outFile) const {
        char pngTemp[64];
        snprintf(pngTemp, sizeof(pngTemp), "/tmp/screen_%d.png", getpid());
        
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
                 "gnome-screenshot -f '%s' 2>/dev/null && "
                 "ffmpeg -y -i '%s' -update 1 -q:v 2 '%s' 2>/dev/null",
                 pngTemp, pngTemp, outFile);
        
        bool success = system(cmd) == 0;
        unlink(pngTemp);
        return success;
    }

    bool captureWithScrot(const char* outFile) const {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "scrot -o '%s' 2>/dev/null", outFile);
        return system(cmd) == 0;
    }

    bool captureWithFfmpegX11(const char* outFile) const {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "ffmpeg -f x11grab -video_size 1920x1080 -i :0 "
                 "-frames:v 1 -update 1 '%s' -y 2>/dev/null", outFile);
        return system(cmd) == 0;
    }

    bool captureWithImport(const char* outFile) const {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "import -window root -quality %d '%s' 2>/dev/null",
                 jpegQuality_, outFile);
        return system(cmd) == 0;
    }

    bool tryAllBackends(const char* outFile) const {
        if (commandExists("grim") && captureWithGrim(outFile)) return true;
        if (commandExists("gnome-screenshot") && captureWithGnomeScreenshot(outFile)) return true;
        if (commandExists("scrot") && captureWithScrot(outFile)) return true;
        if (commandExists("import") && captureWithImport(outFile)) return true;
        return captureWithFfmpegX11(outFile);
    }

    static std::vector<uint8_t> readFile(const char* path) {
        FILE* f = fopen(path, "rb");
        if (!f) return {};
        
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        rewind(f);
        
        std::vector<uint8_t> data(size);
        fread(data.data(), 1, size, f);
        fclose(f);
        
        return data;
    }

    std::string getBackendName() const {
        switch (backend_) {
            case ScreenCaptureBackend::GRIM: return "grim";
            case ScreenCaptureBackend::GNOME_SCREENSHOT: return "gnome-screenshot";
            case ScreenCaptureBackend::SCROT: return "scrot";
            case ScreenCaptureBackend::FFMPEG_X11: return "ffmpeg-x11";
            case ScreenCaptureBackend::IMPORT: return "import";
            default: return "auto";
        }
    }
};

} // namespace cafe
