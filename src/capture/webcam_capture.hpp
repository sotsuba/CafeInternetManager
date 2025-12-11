#pragma once

#include "core/interfaces.hpp"
#include <cstdio>
#include <string>
#include <vector>

namespace cafe {

/**
 * @brief Webcam capture device implementation
 * Single Responsibility: Captures frames from webcam using ffmpeg
 */
class WebcamCapture : public ICaptureDevice {
public:
    explicit WebcamCapture(int deviceIndex = 0, int width = 640, int height = 480)
        : deviceIndex_(deviceIndex)
        , width_(width)
        , height_(height) {}

    std::vector<uint8_t> captureFrame() override {
        std::string cmd = buildCommand();
        
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            return {};
        }
        
        std::vector<uint8_t> jpegData;
        uint8_t buffer[4096];
        size_t bytesRead;
        
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
            jpegData.insert(jpegData.end(), buffer, buffer + bytesRead);
        }
        
        pclose(pipe);
        return jpegData;
    }

    bool isAvailable() const override {
        std::string checkCmd = "test -e /dev/video" + std::to_string(deviceIndex_);
        return system(checkCmd.c_str()) == 0;
    }

    std::string getName() const override {
        return "Webcam /dev/video" + std::to_string(deviceIndex_);
    }

    // Configuration setters
    void setResolution(int width, int height) {
        width_ = width;
        height_ = height;
    }

    void setDeviceIndex(int index) {
        deviceIndex_ = index;
    }

private:
    int deviceIndex_;
    int width_;
    int height_;

    std::string buildCommand() const {
        return "ffmpeg -f v4l2 -video_size " + std::to_string(width_) + "x" + 
               std::to_string(height_) + " -i /dev/video" + 
               std::to_string(deviceIndex_) + 
               " -frames:v 1 -f image2pipe -vcodec mjpeg -hide_banner -loglevel error -";
    }
};

} // namespace cafe
