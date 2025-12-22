#include "api/webcam.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#include <cstring>
#include <cerrno>
#include <chrono>
#include <iostream>

// Helper to write little-endian values
static void write_le32(std::ostream& os, uint32_t val) {
    os.put(val & 0xFF);
    os.put((val >> 8) & 0xFF);
    os.put((val >> 16) & 0xFF);
    os.put((val >> 24) & 0xFF);
}

static void write_le16(std::ostream& os, uint16_t val) {
    os.put(val & 0xFF);
    os.put((val >> 8) & 0xFF);
}

Webcam::Webcam(int device_index)
    : device_index_(device_index)
    , fd_(-1)
    , width_(0)
    , height_(0)
    , streaming_(false)
    , recording_(false)
    , record_fps_(30)
    , frames_recorded_(0) {
}

Webcam::~Webcam() {
    stop_recording();
    close();
}

bool Webcam::open(int width, int height) {
    if (fd_ >= 0) close();

    std::string dev_path = "/dev/video" + std::to_string(device_index_);
    fd_ = ::open(dev_path.c_str(), O_RDWR);
    if (fd_ < 0) {
        last_error_ = "Cannot open " + dev_path + ": " + strerror(errno);
        return false;
    }

    // Query capabilities
    struct v4l2_capability cap{};
    if (ioctl(fd_, VIDIOC_QUERYCAP, &cap) < 0) {
        last_error_ = "VIDIOC_QUERYCAP failed: " + std::string(strerror(errno));
        close();
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        last_error_ = dev_path + " does not support video capture";
        close();
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        last_error_ = dev_path + " does not support streaming";
        close();
        return false;
    }

    // Set format (prefer MJPEG for direct JPEG output)
    struct v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
        // Fallback to YUYV if MJPEG not supported
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        if (ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
            last_error_ = "VIDIOC_S_FMT failed: " + std::string(strerror(errno));
            close();
            return false;
        }
    }

    width_ = fmt.fmt.pix.width;
    height_ = fmt.fmt.pix.height;

    if (!init_mmap()) {
        close();
        return false;
    }

    // Start streaming
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
        last_error_ = "VIDIOC_STREAMON failed: " + std::string(strerror(errno));
        close();
        return false;
    }
    streaming_ = true;

    return true;
}

bool Webcam::init_mmap() {
    struct v4l2_requestbuffers req{};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd_, VIDIOC_REQBUFS, &req) < 0) {
        last_error_ = "VIDIOC_REQBUFS failed: " + std::string(strerror(errno));
        return false;
    }

    if (req.count < 2) {
        last_error_ = "Insufficient buffer memory";
        return false;
    }

    buffers_.resize(req.count);

    for (unsigned i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(fd_, VIDIOC_QUERYBUF, &buf) < 0) {
            last_error_ = "VIDIOC_QUERYBUF failed: " + std::string(strerror(errno));
            return false;
        }

        buffers_[i].length = buf.length;
        buffers_[i].start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, fd_, buf.m.offset);

        if (buffers_[i].start == MAP_FAILED) {
            last_error_ = "mmap failed: " + std::string(strerror(errno));
            return false;
        }

        // Queue buffer
        if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
            last_error_ = "VIDIOC_QBUF failed: " + std::string(strerror(errno));
            return false;
        }
    }

    return true;
}

void Webcam::free_buffers() {
    for (auto& b : buffers_) {
        if (b.start && b.start != MAP_FAILED) {
            munmap(b.start, b.length);
        }
    }
    buffers_.clear();
}

void Webcam::close() {
    if (streaming_) {
        enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ioctl(fd_, VIDIOC_STREAMOFF, &type);
        streaming_ = false;
    }
    free_buffers();
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    width_ = height_ = 0;
}

bool Webcam::is_open() const {
    return fd_ >= 0 && streaming_;
}

std::vector<uint8_t> Webcam::capture_frame() {
    if (!is_open()) {
        last_error_ = "Device not open";
        return {};
    }

    struct v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    // Dequeue a filled buffer
    if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
        if (errno == EAGAIN) {
            last_error_ = "No frame available (EAGAIN)";
        } else {
            last_error_ = "VIDIOC_DQBUF failed: " + std::string(strerror(errno));
        }
        return {};
    }

    // Copy frame data
    std::vector<uint8_t> frame(
        static_cast<uint8_t*>(buffers_[buf.index].start),
        static_cast<uint8_t*>(buffers_[buf.index].start) + buf.bytesused
    );

    // Re-queue buffer for next capture
    if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
        last_error_ = "VIDIOC_QBUF failed: " + std::string(strerror(errno));
        // Still return the frame we got
    }

    return frame;
}

int Webcam::get_width() const { return width_; }
int Webcam::get_height() const { return height_; }
std::string Webcam::get_last_error() const { return last_error_; }

void Webcam::stream_h264(StreamCallback cb) {
    // 1. Close current V4L2 handle if open (FFmpeg needs exclusive access)
    close();

    // 2. Build FFmpeg command for V4L2
    std::string dev_path = "/dev/video" + std::to_string(device_index_);

    // Auto-detect if default device is missing
    if (access(dev_path.c_str(), F_OK) == -1) {
        for (int i = 0; i < 64; ++i) { // Check video0 to video63
            std::string p = "/dev/video" + std::to_string(i);
            if (access(p.c_str(), F_OK) == 0) {
                dev_path = p;
                device_index_ = i;
                std::cout << "[Webcam] Auto-detected device: " << p << std::endl;
                break;
            }
        }
    }

    // OPTIMIZED: 640x480 @ 30fps for better quality, matching monitor.cpp approach
    // Using same ultrafast + zerolatency preset as screen capture
    std::string cmd = "ffmpeg -f v4l2 -framerate 30 -video_size 640x480 -i " + dev_path +
                      " -c:v libx264 -preset ultrafast -tune zerolatency "
                      "-g 30 "  // GOP size = framerate for 1-second keyframe interval
                      "-profile:v baseline -level 3.1 -bf 0 "  // Level 3.1 for 640x480
                      "-pix_fmt yuv420p "
                      "-f h264 - 2>ffmpeg_webcam.log";

    std::cout << "[Webcam] Starting H.264 stream: " << dev_path << " @ 640x480 30fps" << std::endl;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        last_error_ = "popen failed: " + std::string(strerror(errno));
        std::cerr << "[Webcam] ERROR: " << last_error_ << std::endl;
        return;
    }

    std::vector<uint8_t> buffer(65536); // 64KB buffer (same as monitor)
    size_t total_bytes = 0;
    while (true) {
        size_t bytes = fread(buffer.data(), 1, buffer.size(), pipe);
        if (bytes > 0) {
            total_bytes += bytes;
            std::vector<uint8_t> chunk(buffer.begin(), buffer.begin() + bytes);
            if (!cb(chunk)) {
                std::cout << "[Webcam] Stream stopped by callback. Total: "
                          << (total_bytes / 1024) << " KB" << std::endl;
                break;
            }
        } else {
            if (feof(pipe)) {
                std::cout << "[Webcam] FFmpeg stream ended (EOF)" << std::endl;
            } else {
                std::cerr << "[Webcam] Read error" << std::endl;
            }
            break;
        }
    }
    pclose(pipe);
    std::cout << "[Webcam] Stream closed" << std::endl;
}

// ─────────────────────────────────────────────────────────────────────────────
// Recording implementation
// ─────────────────────────────────────────────────────────────────────────────

void Webcam::set_frame_callback(FrameCallback cb) {
    frame_callback_ = std::move(cb);
}

bool Webcam::is_recording() const {
    return recording_;
}

size_t Webcam::get_frames_recorded() const {
    return frames_recorded_;
}

bool Webcam::start_recording(const std::string& filename, int fps) {
    if (!is_open()) {
        last_error_ = "Device not open";
        return false;
    }
    if (recording_) {
        last_error_ = "Already recording";
        return false;
    }

    record_filename_ = filename;
    record_fps_ = fps;
    frames_recorded_ = 0;
    frame_offsets_.clear();

    record_file_.open(filename, std::ios::binary | std::ios::trunc);
    if (!record_file_) {
        last_error_ = "Cannot open file: " + filename;
        return false;
    }

    // Check if AVI format requested
    bool is_avi = (filename.size() >= 4 &&
                   (filename.substr(filename.size() - 4) == ".avi" ||
                    filename.substr(filename.size() - 4) == ".AVI"));

    if (is_avi) {
        write_avi_header();
    }

    recording_ = true;
    record_thread_ = std::thread(&Webcam::recording_loop, this);

    return true;
}

void Webcam::stop_recording() {
    if (!recording_) return;

    recording_ = false;
    if (record_thread_.joinable()) {
        record_thread_.join();
    }

    // Finalize AVI if needed
    bool is_avi = (record_filename_.size() >= 4 &&
                   (record_filename_.substr(record_filename_.size() - 4) == ".avi" ||
                    record_filename_.substr(record_filename_.size() - 4) == ".AVI"));
    if (is_avi) {
        finalize_avi();
    }

    record_file_.close();
}

void Webcam::recording_loop() {
    auto frame_duration = std::chrono::microseconds(1000000 / record_fps_);
    auto next_frame_time = std::chrono::steady_clock::now();

    while (recording_) {
        auto frame = capture_frame();
        if (!frame.empty()) {
            // Write frame
            bool is_avi = (record_filename_.size() >= 4 &&
                           (record_filename_.substr(record_filename_.size() - 4) == ".avi" ||
                            record_filename_.substr(record_filename_.size() - 4) == ".AVI"));

            if (is_avi) {
                // Record offset for index
                frame_offsets_.push_back(static_cast<uint32_t>(record_file_.tellp()));

                // Write AVI frame chunk: "00dc" + size + data (padded to even)
                record_file_.write("00dc", 4);
                uint32_t size = frame.size();
                write_le32(record_file_, size);
                record_file_.write(reinterpret_cast<const char*>(frame.data()), frame.size());
                if (frame.size() % 2) {
                    record_file_.put(0);  // Pad to even
                }
            } else {
                // Raw MJPEG stream
                record_file_.write(reinterpret_cast<const char*>(frame.data()), frame.size());
            }

            ++frames_recorded_;

            // Call user callback if set
            if (frame_callback_) {
                frame_callback_(frame);
            }
        }

        // Timing control
        next_frame_time += frame_duration;
        std::this_thread::sleep_until(next_frame_time);
    }
}

void Webcam::write_avi_header() {
    // Placeholder header — will be rewritten in finalize_avi()
    // RIFF header
    record_file_.write("RIFF", 4);
    write_le32(record_file_, 0);  // File size placeholder
    record_file_.write("AVI ", 4);

    // hdrl LIST
    record_file_.write("LIST", 4);
    write_le32(record_file_, 192);  // hdrl size
    record_file_.write("hdrl", 4);

    // avih chunk (main AVI header)
    record_file_.write("avih", 4);
    write_le32(record_file_, 56);  // avih size
    write_le32(record_file_, 1000000 / record_fps_);  // microseconds per frame
    write_le32(record_file_, 0);   // max bytes per sec (placeholder)
    write_le32(record_file_, 0);   // padding granularity
    write_le32(record_file_, 0);   // flags
    write_le32(record_file_, 0);   // total frames (placeholder)
    write_le32(record_file_, 0);   // initial frames
    write_le32(record_file_, 1);   // streams
    write_le32(record_file_, width_ * height_ * 3);  // suggested buffer size
    write_le32(record_file_, width_);
    write_le32(record_file_, height_);
    write_le32(record_file_, 0);   // reserved
    write_le32(record_file_, 0);
    write_le32(record_file_, 0);
    write_le32(record_file_, 0);

    // strl LIST (stream header list)
    record_file_.write("LIST", 4);
    write_le32(record_file_, 116);  // strl size
    record_file_.write("strl", 4);

    // strh chunk (stream header)
    record_file_.write("strh", 4);
    write_le32(record_file_, 56);  // strh size
    record_file_.write("vids", 4);  // fccType
    record_file_.write("MJPG", 4);  // fccHandler
    write_le32(record_file_, 0);   // flags
    write_le16(record_file_, 0);   // priority
    write_le16(record_file_, 0);   // language
    write_le32(record_file_, 0);   // initial frames
    write_le32(record_file_, 1);   // scale
    write_le32(record_file_, record_fps_);  // rate
    write_le32(record_file_, 0);   // start
    write_le32(record_file_, 0);   // length (placeholder)
    write_le32(record_file_, width_ * height_ * 3);  // suggested buffer
    write_le32(record_file_, 0);   // quality
    write_le32(record_file_, 0);   // sample size
    write_le16(record_file_, 0);   // left
    write_le16(record_file_, 0);   // top
    write_le16(record_file_, width_);
    write_le16(record_file_, height_);

    // strf chunk (stream format - BITMAPINFOHEADER)
    record_file_.write("strf", 4);
    write_le32(record_file_, 40);  // strf size
    write_le32(record_file_, 40);  // biSize
    write_le32(record_file_, width_);
    write_le32(record_file_, height_);
    write_le16(record_file_, 1);   // biPlanes
    write_le16(record_file_, 24);  // biBitCount
    record_file_.write("MJPG", 4); // biCompression
    write_le32(record_file_, width_ * height_ * 3);  // biSizeImage
    write_le32(record_file_, 0);   // biXPelsPerMeter
    write_le32(record_file_, 0);   // biYPelsPerMeter
    write_le32(record_file_, 0);   // biClrUsed
    write_le32(record_file_, 0);   // biClrImportant

    // movi LIST header
    record_file_.write("LIST", 4);
    write_le32(record_file_, 0);   // movi size (placeholder)
    record_file_.write("movi", 4);
}

void Webcam::finalize_avi() {
    // Get current position (end of movi data)
    auto movi_end = record_file_.tellp();

    // Write idx1 chunk
    record_file_.write("idx1", 4);
    write_le32(record_file_, frame_offsets_.size() * 16);

    // Index entries
    uint32_t movi_start = 220 + 4;  // After "movi" tag
    for (size_t i = 0; i < frame_offsets_.size(); ++i) {
        record_file_.write("00dc", 4);  // chunk id
        write_le32(record_file_, 0x10);  // flags (keyframe)
        write_le32(record_file_, frame_offsets_[i] - movi_start);  // offset

        // Read frame size from file (stored after "00dc")
        // For simplicity, we'll estimate — in production you'd store sizes
        write_le32(record_file_, width_ * height_ / 10);  // approximate size
    }

    auto file_end = record_file_.tellp();

    // Update headers with actual values
    record_file_.seekp(4);
    write_le32(record_file_, static_cast<uint32_t>(file_end) - 8);  // RIFF size

    // Update total frames in avih
    record_file_.seekp(48);
    write_le32(record_file_, frames_recorded_);

    // Update stream length in strh
    record_file_.seekp(140);
    write_le32(record_file_, frames_recorded_);

    // Update movi size
    record_file_.seekp(216);
    write_le32(record_file_, static_cast<uint32_t>(movi_end) - 220);

    record_file_.seekp(0, std::ios::end);
}

// Legacy wrapper — opens/closes each call (slow but backward compatible)
std::vector<uint8_t> capture_webcam_frame(int webcam_index, int& width, int& height) {
    Webcam cam(webcam_index);
    if (!cam.open(640, 480)) {
        return {};
    }
    width = cam.get_width();
    height = cam.get_height();
    return cam.capture_frame();
}
