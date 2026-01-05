#pragma once
#include "interfaces/IFileTransfer.hpp"
#include <unordered_map>
#include <mutex>

namespace platform {
namespace linux_os {

// ============================================================================
// LinuxFileTransfer - High-performance file transfer for Linux
// ============================================================================
// Uses POSIX APIs for optimal performance:
// - opendir/readdir/stat for directory listing
// - open/read/write with buffering for file I/O
// - statvfs for disk space queries
//
// Memory Optimization Notes:
// - Uses 64KB buffers (FILE_TRANSFER_CHUNK_SIZE)
// - Increase to 256KB+ for faster large file transfers
// - Upload state kept in-memory for active transfers
// ============================================================================

class LinuxFileTransfer final : public interfaces::IFileTransfer {
public:
    LinuxFileTransfer();
    ~LinuxFileTransfer() override;

    // ========== Directory Operations ==========

    common::Result<std::vector<interfaces::FileInfo>> list_directory(
        const std::string& path) override;

    common::Result<interfaces::FileInfo> get_file_info(
        const std::string& path) override;

    common::EmptyResult create_directory(
        const std::string& path) override;

    common::EmptyResult delete_path(
        const std::string& path) override;

    // ========== Download ==========

    common::EmptyResult download_file(
        const std::string& path,
        interfaces::DataChunkCallback on_chunk,
        interfaces::ProgressCallback on_progress = nullptr) override;

    // ========== Upload ==========

    common::EmptyResult upload_start(
        const std::string& path,
        uint64_t expected_size) override;

    common::EmptyResult upload_chunk(
        const std::string& path,
        const uint8_t* data,
        size_t size) override;

    common::EmptyResult upload_finish(
        const std::string& path) override;

    common::EmptyResult upload_cancel(
        const std::string& path) override;

    // ========== Utility ==========

    common::Result<uint64_t> get_free_space(
        const std::string& path) override;

    common::EmptyResult rename(
        const std::string& old_path,
        const std::string& new_path) override;

private:
    // Upload state tracking
    struct UploadState {
        int fd = -1;
        std::string path;
        uint64_t expected_size = 0;
        uint64_t bytes_written = 0;
        std::chrono::steady_clock::time_point start_time;
    };

    std::mutex uploads_mutex_;
    std::unordered_map<std::string, UploadState> active_uploads_;

    // Helper: Create recursive directories
    static bool create_dirs_recursive(const std::string& path);
};

} // namespace linux_os
} // namespace platform
