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

        int w = GetSystemMetrics(SM_CXSCREEN);
        int h = GetSystemMetrics(SM_CYSCREEN);
        std::string res = std::to_string(w) + "x" + std::to_string(h);

        // Command to record desktop using gdigrab
        // -f h264 - outputs raw h264 to stdout
        // -g 30 ensures keyframe every 1s (30fps) for fast reconnect
        std::string cmd = "ffmpeg -f gdigrab -framerate 30 -video_size " + res +
                          " -i desktop -c:v libx264 -preset ultrafast -tune zerolatency " +
                          "-g 30 -profile:v baseline -level 3.0 -bf 0 " +
                          "-pix_fmt yuv420p -f h264 - 2>NUL";

        FILE* pipe = _popen(cmd.c_str(), "rb"); // Binary mode on Windows is CRITICAL
        if (!pipe) {
            return common::Result<common::Ok>::err(common::ErrorCode::EncoderError, "Failed to start ffmpeg");
        }

        std::vector<uint8_t> buffer(65536);
        while (!token.is_cancellation_requested()) {
            size_t n = fread(buffer.data(), 1, buffer.size(), pipe);
            if (n <= 0) break;

            std::vector<uint8_t> chunk(buffer.begin(), buffer.begin() + n);

            // Basic NALU Kind detection (Optional but good for latency)
            common::PacketKind kind = common::PacketKind::InterFrame;
            // Simple check for start codes 00 00 01 or 00 00 00 01
            // ... (Simplified for brevity, assuming standard streaming)

            static uint64_t pts = 0;
            // TODO: Better PTS generation

            auto data_ptr = std::make_shared<const std::vector<uint8_t>>(std::move(chunk));
            on_packet(common::VideoPacket{data_ptr, pts++, 1, kind});
        }

        _pclose(pipe);
        return common::Result<common::Ok>::success();
    }

    void WindowsScreenStreamer::stop() {
        running_ = false;
        // _popen doesn't support easy non-blocking kill unless we use Win32 API
        // But closing the pipe from read side usually terminates the writer (ffmpeg) eventually (Broken Pipe)
    }

    common::Result<common::RawFrame> WindowsScreenStreamer::capture_snapshot() {
        // Get screen dimensions
        int width = GetSystemMetrics(SM_CXSCREEN);
        int height = GetSystemMetrics(SM_CYSCREEN);

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

}
}
