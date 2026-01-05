#import "MacOSScreenStreamer.hpp"
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#include <iostream>
#include <vector>
#include <algorithm>

namespace platform {
namespace macos {

MacOSScreenStreamer::MacOSScreenStreamer() {
    @autoreleasepool {
        NSScreen* mainScreen = [NSScreen mainScreen];
        if (mainScreen) {
            NSRect frame = [mainScreen frame];
            // Account for Retina scaling
            CGFloat scale = [mainScreen backingScaleFactor];
            screen_width_ = static_cast<int>(frame.size.width * scale);
            screen_height_ = static_cast<int>(frame.size.height * scale);
        }
    }

    if (screen_width_ <= 0 || screen_height_ <= 0) {
        CGDirectDisplayID displayID = CGMainDisplayID();
        screen_width_ = static_cast<int>(CGDisplayPixelsWide(displayID));
        screen_height_ = static_cast<int>(CGDisplayPixelsHigh(displayID));
    }

    std::cout << "[MacOSScreenStreamer] Screen: " << screen_width_ << "x" << screen_height_ << std::endl;
}

MacOSScreenStreamer::~MacOSScreenStreamer() {
    stop();
}

common::EmptyResult MacOSScreenStreamer::stream(
    std::function<void(const common::VideoPacket&)> on_packet,
    common::CancellationToken token
) {
    running_ = true;
    current_token_ = token;

    // Use ffmpeg with AVFoundation input for screen capture
    // avfoundation device "1" is typically the screen (device 0 is camera)
    std::string cmd = "ffmpeg -f avfoundation -framerate 30 -capture_cursor 1 "
                      "-i \"1:none\" -vf scale=1280:-2 -c:v mjpeg -q:v 8 "
                      "-f mjpeg - 2>/dev/null";

    std::cout << "[MacOSScreenStreamer] Starting MJPEG stream: " << cmd << std::endl;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return common::Result<common::Ok>::err(
            common::ErrorCode::EncoderError,
            "Failed to start ffmpeg (ensure ffmpeg is installed: brew install ffmpeg)"
        );
    }

    // JPEG markers
    const uint8_t soi_marker[2] = {0xFF, 0xD8}; // Start Of Image
    const uint8_t eoi_marker[2] = {0xFF, 0xD9}; // End Of Image

    std::vector<uint8_t> read_buffer(65536);
    std::vector<uint8_t> frame_buffer;
    frame_buffer.reserve(256 * 1024);

    uint64_t pts = 0;
    int frame_count = 0;

    while (!token.is_cancellation_requested()) {
        size_t n = fread(read_buffer.data(), 1, read_buffer.size(), pipe);
        if (n <= 0) break;

        frame_buffer.insert(frame_buffer.end(), read_buffer.begin(), read_buffer.begin() + n);

        // Extract complete JPEG frames
        while (true) {
            auto soi_it = std::search(frame_buffer.begin(), frame_buffer.end(),
                                      soi_marker, soi_marker + 2);
            if (soi_it == frame_buffer.end()) {
                frame_buffer.clear();
                break;
            }

            auto eoi_it = std::search(soi_it + 2, frame_buffer.end(),
                                      eoi_marker, eoi_marker + 2);
            if (eoi_it == frame_buffer.end()) {
                if (soi_it != frame_buffer.begin()) {
                    frame_buffer.erase(frame_buffer.begin(), soi_it);
                }
                break;
            }

            eoi_it += 2;
            std::vector<uint8_t> jpeg_frame(soi_it, eoi_it);

            auto data_ptr = std::make_shared<const std::vector<uint8_t>>(std::move(jpeg_frame));
            on_packet(common::VideoPacket{data_ptr, pts++, 1, common::PacketKind::KeyFrame});

            frame_count++;
            if (frame_count % 30 == 0) {
                std::cout << "[MacOSScreenStreamer] Sent MJPEG frame #" << frame_count
                          << " (" << data_ptr->size() << " bytes)" << std::endl;
            }

            frame_buffer.erase(frame_buffer.begin(), eoi_it);
        }

        if (frame_buffer.size() > 1024 * 1024) {
            std::cerr << "[MacOSScreenStreamer] WARNING: Frame buffer overflow, clearing" << std::endl;
            frame_buffer.clear();
        }
    }

    pclose(pipe);
    std::cout << "[MacOSScreenStreamer] Stream stopped. Total frames: " << frame_count << std::endl;
    return common::Result<common::Ok>::success();
}

void MacOSScreenStreamer::stop() {
    running_ = false;
}

common::Result<common::RawFrame> MacOSScreenStreamer::capture_snapshot() {
    @autoreleasepool {
        // Capture the main display
        CGDirectDisplayID displayID = CGMainDisplayID();
        CGImageRef screenshot = CGDisplayCreateImage(displayID);

        if (!screenshot) {
            return common::Result<common::RawFrame>::err(
                common::ErrorCode::PermissionDenied,
                "Failed to capture screen (check Screen Recording permission)"
            );
        }

        size_t width = CGImageGetWidth(screenshot);
        size_t height = CGImageGetHeight(screenshot);
        size_t bitsPerComponent = CGImageGetBitsPerComponent(screenshot);
        size_t bytesPerRow = CGImageGetBytesPerRow(screenshot);

        // Create bitmap context to get raw pixels
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

        // Allocate buffer for RGB data
        size_t stride = ((width * 3) + 3) & ~3;  // DWORD-aligned for consistency
        std::vector<uint8_t> pixels(stride * height);

        // Create context in RGB format (24-bit)
        size_t bitmapBytesPerRow = width * 4;  // RGBA
        std::vector<uint8_t> tempBuffer(bitmapBytesPerRow * height);

        CGContextRef context = CGBitmapContextCreate(
            tempBuffer.data(),
            width, height,
            8,
            bitmapBytesPerRow,
            colorSpace,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
        );

        if (!context) {
            CGColorSpaceRelease(colorSpace);
            CGImageRelease(screenshot);
            return common::Result<common::RawFrame>::err(
                common::ErrorCode::Unknown,
                "Failed to create bitmap context"
            );
        }

        // Draw image into context
        CGContextDrawImage(context, CGRectMake(0, 0, width, height), screenshot);

        // Convert RGBA to RGB
        for (size_t y = 0; y < height; ++y) {
            uint8_t* srcRow = tempBuffer.data() + y * bitmapBytesPerRow;
            uint8_t* dstRow = pixels.data() + y * stride;
            for (size_t x = 0; x < width; ++x) {
                dstRow[x * 3 + 0] = srcRow[x * 4 + 0];  // R
                dstRow[x * 3 + 1] = srcRow[x * 4 + 1];  // G
                dstRow[x * 3 + 2] = srcRow[x * 4 + 2];  // B
            }
        }

        CGContextRelease(context);
        CGColorSpaceRelease(colorSpace);
        CGImageRelease(screenshot);

        common::RawFrame frame;
        frame.pixels = std::move(pixels);
        frame.width = static_cast<uint32_t>(width);
        frame.height = static_cast<uint32_t>(height);
        frame.stride = static_cast<uint32_t>(stride);
        frame.format = "rgb";

        return common::Result<common::RawFrame>::ok(std::move(frame));
    }
}

} // namespace macos
} // namespace platform
