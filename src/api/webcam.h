#ifndef __webcam_h__
#define __webcam_h__

#include <string>
#include <cstdint>
#include <vector>


#include <iostream>

std::vector<uint8_t> capture_webcam_frame(int webcam_index,
                                    int &width,
                                    int &height) {  
    const std::string ffmpeg_cmd = "ffmpeg -f v4l2 -video_size 640x480 -i /dev/video" + std::to_string(webcam_index) + " -frames:v 1 -f image2pipe -vcodec mjpeg -hide_banner -loglevel error -"; 
    FILE *pipe = popen(ffmpeg_cmd.c_str(), "r");
    if (!pipe) {
        fprintf(stderr, "Failed to open webcam pipe\n");
        return {};
    }
    // Read JPEG data from pipe
    std::vector<uint8_t> jpeg_data;
    uint8_t buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
        jpeg_data.insert(jpeg_data.end(), buffer, buffer + n);
    }
    pclose(pipe);
    std::cout << jpeg_data.size() << " bytes captured from webcam\n";

    return jpeg_data;
}

#endif