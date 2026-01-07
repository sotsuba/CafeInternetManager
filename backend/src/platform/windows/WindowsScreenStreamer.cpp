#include "WindowsScreenStreamer.hpp"
#include <iostream>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <windows.h>

namespace platform {
namespace windows_os {

    WindowsScreenStreamer::WindowsScreenStreamer() {}
    WindowsScreenStreamer::~WindowsScreenStreamer() { stop(); }

    common::EmptyResult WindowsScreenStreamer::stream(
        std::function<void(const common::VideoPacket&)> on_packet,
        common::CancellationToken token
    ) {
        running_ = true;
        current_token_ = token;

        // Use GetDeviceCaps to get true physical resolution, avoiding DPI scaling cropping
        HDC hdc = GetDC(NULL);
        int w = GetDeviceCaps(hdc, DESKTOPHORZRES);
        int h = GetDeviceCaps(hdc, DESKTOPVERTRES);
        ReleaseDC(NULL, hdc);

        std::string res = std::to_string(w) + "x" + std::to_string(h);

        // === MJPEG STREAMING ===
        // Much more reliable than H.264 for real-time remote desktop
        // Each frame is a standalone JPEG - no codec state corruption on drops
        // NOTE: No scaling - capture full screen resolution to avoid cropping
        std::string cmd = "ffmpeg -f gdigrab -framerate 30 -video_size " + res +
                          " -i desktop -c:v mjpeg -q:v 8 -f mjpeg - 2>NUL";

        std::cout << "[Screen] Starting MJPEG stream: " << cmd << std::endl;

        FILE* pipe = _popen(cmd.c_str(), "rb");
        if (!pipe) {
            return common::Result<common::Ok>::err(common::ErrorCode::EncoderError, "Failed to start ffmpeg");
        }

        // JPEG markers
        const uint8_t soi_marker[2] = {0xFF, 0xD8}; // Start Of Image
        const uint8_t eoi_marker[2] = {0xFF, 0xD9}; // End Of Image

        std::vector<uint8_t> read_buffer(65536);
        std::vector<uint8_t> frame_buffer;
        frame_buffer.reserve(256 * 1024); // Pre-allocate for typical JPEG size

        uint64_t pts = 0;
        int frame_count = 0;

        while (!token.is_cancellation_requested()) {
            size_t n = fread(read_buffer.data(), 1, read_buffer.size(), pipe);
            if (n <= 0) break;

            // Append to frame buffer
            frame_buffer.insert(frame_buffer.end(), read_buffer.begin(), read_buffer.begin() + n);

            // Extract complete JPEG frames
            while (true) {
                // Find SOI (Start of Image)
                auto soi_it = std::search(frame_buffer.begin(), frame_buffer.end(), soi_marker, soi_marker + 2);
                if (soi_it == frame_buffer.end()) {
                    frame_buffer.clear(); // No SOI, discard garbage
                    break;
                }

                // Find EOI (End of Image) after SOI
                auto eoi_it = std::search(soi_it + 2, frame_buffer.end(), eoi_marker, eoi_marker + 2);
                if (eoi_it == frame_buffer.end()) {
                    // Incomplete frame, wait for more data
                    // But trim any garbage before SOI
                    if (soi_it != frame_buffer.begin()) {
                        frame_buffer.erase(frame_buffer.begin(), soi_it);
                    }
                    break;
                }

                // Found complete JPEG frame (SOI to EOI inclusive)
                eoi_it += 2; // Include EOI bytes
                std::vector<uint8_t> jpeg_frame(soi_it, eoi_it);

                // Send frame
                auto data_ptr = std::make_shared<const std::vector<uint8_t>>(std::move(jpeg_frame));
                on_packet(common::VideoPacket{data_ptr, pts++, 1, common::PacketKind::KeyFrame});

                frame_count++;
                if (frame_count % 30 == 0) {
                    std::cout << "[Screen] Sent MJPEG frame #" << frame_count << " (" << data_ptr->size() << " bytes)" << std::endl;
                }

                // Remove processed frame from buffer
                frame_buffer.erase(frame_buffer.begin(), eoi_it);
            }

            // Prevent unbounded buffer growth (safety limit)
            if (frame_buffer.size() > 1024 * 1024) {
                std::cerr << "[Screen] WARNING: Frame buffer overflow, clearing" << std::endl;
                frame_buffer.clear();
            }
        }

        _pclose(pipe);
        std::cout << "[Screen] MJPEG stream stopped. Total frames: " << frame_count << std::endl;
        return common::Result<common::Ok>::success();
    }

    void WindowsScreenStreamer::stop() {
        running_ = false;
        // _popen doesn't support easy non-blocking kill unless we use Win32 API
        // But closing the pipe from read side usually terminates the writer (ffmpeg) eventually (Broken Pipe)
    }

    common::Result<common::RawFrame> WindowsScreenStreamer::capture_snapshot() {
        // Use GetDeviceCaps to get true physical resolution
        HDC hTempDC = GetDC(NULL);
        int width = GetDeviceCaps(hTempDC, DESKTOPHORZRES);
        int height = GetDeviceCaps(hTempDC, DESKTOPVERTRES);
        ReleaseDC(NULL, hTempDC);

        if (width <= 0 || height <= 0) {
            return common::Result<common::RawFrame>::err(
                common::ErrorCode::DeviceNotFound, "Failed to get screen dimensions");
        }

        // Get device contexts
        HDC hScreenDC = GetDC(NULL);  // NULL = entire screen
        if (!hScreenDC) {
            return common::Result<common::RawFrame>::err(
                common::ErrorCode::DeviceNotFound, "Failed to get screen DC");
        }

        HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
        if (!hMemoryDC) {
            ReleaseDC(NULL, hScreenDC);
            return common::Result<common::RawFrame>::err(
                common::ErrorCode::Unknown, "Failed to create memory DC");
        }

        // Create bitmap
        HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
        if (!hBitmap) {
            DeleteDC(hMemoryDC);
            ReleaseDC(NULL, hScreenDC);
            return common::Result<common::RawFrame>::err(
                common::ErrorCode::Unknown, "Failed to create bitmap");
        }

        // Select bitmap into memory DC
        HGDIOBJ hOldBitmap = SelectObject(hMemoryDC, hBitmap);

        // Copy screen to bitmap
        BOOL result = BitBlt(hMemoryDC, 0, 0, width, height,
                            hScreenDC, 0, 0, SRCCOPY);

        if (!result) {
            SelectObject(hMemoryDC, hOldBitmap);
            DeleteObject(hBitmap);
            DeleteDC(hMemoryDC);
            ReleaseDC(NULL, hScreenDC);
            return common::Result<common::RawFrame>::err(
                common::ErrorCode::Unknown, "BitBlt failed");
        }

        // Prepare bitmap info for GetDIBits (24-bit RGB)
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;  // Negative = top-down (normal orientation)
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 24;  // RGB
        bmi.bmiHeader.biCompression = BI_RGB;

        // Calculate stride (rows must be DWORD-aligned)
        // Stride formula for 24-bit: ((width * 3) + 3) & ~3
        uint32_t stride = ((width * 3) + 3) & ~3;
        size_t dataSize = stride * height;

        // Allocate buffer
        std::vector<uint8_t> pixels(dataSize);

        // Copy bitmap data
        int lines = GetDIBits(hMemoryDC, hBitmap, 0, height,
                             pixels.data(), &bmi, DIB_RGB_COLORS);

        // Cleanup GDI objects
        SelectObject(hMemoryDC, hOldBitmap);
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(NULL, hScreenDC);

        if (lines != height) {
            return common::Result<common::RawFrame>::err(
                common::ErrorCode::Unknown,
                "GetDIBits failed: got " + std::to_string(lines) + " lines");
        }

        // Convert BGR to RGB (GDI returns BGR)
        // OPTIMIZATION NOTE: If performance is critical, consider keeping BGR
        // or using SIMD for conversion. For snapshots, this is acceptable.
        for (size_t y = 0; y < static_cast<size_t>(height); ++y) {
            uint8_t* row = pixels.data() + y * stride;
            for (int x = 0; x < width; ++x) {
                std::swap(row[x * 3], row[x * 3 + 2]);  // Swap B and R
            }
        }

        // Create result
        common::RawFrame frame;
        frame.pixels = std::move(pixels);
        frame.width = static_cast<uint32_t>(width);
        frame.height = static_cast<uint32_t>(height);
        frame.stride = stride;
        frame.format = "rgb";

        return common::Result<common::RawFrame>::ok(std::move(frame));
    }

    common::Result<uint32_t> WindowsScreenStreamer::start_recording(const std::string& output_path) {
        std::lock_guard<std::mutex> lock(recording_mutex_);

        if (recording_.load()) {
            return common::Result<uint32_t>::err(
                common::ErrorCode::Unknown, "Already recording");
        }

        HDC hdc = GetDC(NULL);
        int w = GetDeviceCaps(hdc, DESKTOPHORZRES);
        int h = GetDeviceCaps(hdc, DESKTOPVERTRES);
        ReleaseDC(NULL, hdc);

        std::string res = std::to_string(w) + "x" + std::to_string(h);

        recording_path_ = output_path;

        // Record to MP4 file using ffmpeg
        std::string cmd = "ffmpeg -y -f gdigrab -framerate 30 -video_size " + res +
                          " -i desktop -c:v libx264 -preset ultrafast -crf 23 " +
                          "-pix_fmt yuv420p \"" + output_path + "\" 2>NUL";

        std::cout << "[Screen] Starting recording: " << cmd << std::endl;

        recording_pipe_ = _popen(cmd.c_str(), "w");
        if (!recording_pipe_) {
            std::cerr << "[WindowsScreenStreamer] CRITICAL: Failed to open ffmpeg pipe" << std::endl;
            return common::Result<uint32_t>::err(
                common::ErrorCode::EncoderError, "Failed to start ffmpeg recording");
        }

        recording_.store(true);
        paused_.store(false);
        std::cout << "[WindowsScreenStreamer] Record process spawned (Target: " << output_path << ")" << std::endl;
        return common::Result<uint32_t>::ok(0); // Dummy ID
    }

    common::EmptyResult WindowsScreenStreamer::stop_recording() {
        std::lock_guard<std::mutex> lock(recording_mutex_);

        if (!recording_.load()) {
            return common::Result<common::Ok>::err(
                common::ErrorCode::Unknown, "Not recording");
        }

        if (recording_pipe_) {
            std::cout << "[WindowsScreenStreamer] Closing ffmpeg pipe..." << std::endl;
            // Send 'q' to ffmpeg to quit gracefully
            fwrite("q", 1, 1, recording_pipe_);
            fflush(recording_pipe_);
            _pclose(recording_pipe_);
            recording_pipe_ = nullptr;
        }

        recording_.store(false);
        paused_.store(false);
        std::cout << "[WindowsScreenStreamer] Recording stopped successfully. Final file: " << recording_path_ << std::endl;
        return common::Result<common::Ok>::success();
    }

    common::EmptyResult WindowsScreenStreamer::pause_recording() {
        if (!recording_.load()) {
            return common::Result<common::Ok>::err(
                common::ErrorCode::Unknown, "Not recording");
        }

        // Toggle pause state
        // Note: ffmpeg gdigrab doesn't support true pause, this is a placeholder
        // For true pause, we'd need to stop/restart ffmpeg or use a different approach
        bool was_paused = paused_.load();
        paused_.store(!was_paused);
        std::cout << "[Screen] Recording " << (paused_.load() ? "paused" : "resumed") << std::endl;
        return common::Result<common::Ok>::success();
    }

}
}
