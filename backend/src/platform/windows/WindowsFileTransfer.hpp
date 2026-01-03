#pragma once
#include "interfaces/IFileTransfer.hpp"
#include <unordered_map>
#include <mutex>
#include <fstream>
#include <windows.h>

namespace platform {
namespace windows_os {

// ============================================================================
// WindowsFileTransfer - High-performance file transfer for Windows
// ============================================================================
// Uses Win32 API for optimal performance:
// - FindFirstFile/FindNextFile for directory listing
// - CreateFile with FILE_FLAG_SEQUENTIAL_SCAN for download
// - Buffered writes for upload
//
// Memory Optimization Notes:
// - Uses 64KB buffers (FILE_TRANSFER_CHUNK_SIZE)
// - Increase to 256KB+ for faster large file transfers
// - Upload state kept in-memory for active transfers
// ============================================================================

class WindowsFileTransfer final : public interfaces::IFileTransfer {
public:
    WindowsFileTransfer();
    ~WindowsFileTransfer() override;

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
        HANDLE file_handle = INVALID_HANDLE_VALUE;
        std::string path;
        uint64_t expected_size = 0;
        uint64_t bytes_written = 0;
        std::chrono::steady_clock::time_point start_time;
    };

    std::mutex uploads_mutex_;
    std::unordered_map<std::string, UploadState> active_uploads_;

    // Helper: Convert std::string to std::wstring (UTF-8 → UTF-16)
    static std::wstring to_wide(const std::string& str);

    // Helper: Convert std::wstring to std::string (UTF-16 → UTF-8)
    static std::string to_narrow(const std::wstring& wstr);

    // Helper: Convert FILETIME to Unix timestamp
    static uint64_t filetime_to_unix(const FILETIME& ft);

    // Helper: Create FileInfo from WIN32_FIND_DATA
    static interfaces::FileInfo make_file_info(
        const std::wstring& dir_path,
        const WIN32_FIND_DATAW& data);
};

} // namespace windows_os
} // namespace platform
