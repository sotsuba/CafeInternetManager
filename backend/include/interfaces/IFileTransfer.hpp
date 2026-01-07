#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "common/Result.hpp"

namespace interfaces {

// ============================================================================
// File Transfer Interface
// ============================================================================
// High-performance file transfer abstraction optimized for speed and accuracy.
// Uses chunked I/O for large files and streaming transfers.
//
// Design Decisions:
// - CHUNK_SIZE = 64KB default (tunable for optimization)
// - Uses callbacks for progress reporting without blocking
// - Memory-prioritized: prefers larger buffers for speed
// ============================================================================

// Default chunk size for file transfers
// OPTIMIZATION NOTE: 64KB provides good balance.
// Increase to 256KB-1MB for high-latency networks.
// Decrease to 16KB-32KB for low-memory environments.
constexpr size_t FILE_TRANSFER_CHUNK_SIZE = 250 * 1024;  // 250 KB - Fits in Gateway TIER 3 (256KB)

// ============================================================================
// Data Structures
// ============================================================================

struct FileInfo {
    std::string name;           // File/directory name only
    std::string path;           // Full absolute path
    uint64_t size;              // Size in bytes (0 for directories)
    uint64_t modified_time;     // Unix timestamp (seconds since epoch)
    bool is_directory;
    bool is_hidden;
    bool is_readonly;

    // Helper for serialization
    std::string to_string() const {
        return name + "|" + path + "|" + std::to_string(size) + "|" +
               std::to_string(modified_time) + "|" +
               (is_directory ? "D" : "F") + "|" +
               (is_hidden ? "H" : "-") + (is_readonly ? "R" : "-");
    }
};

struct TransferProgress {
    std::string file_path;
    uint64_t bytes_transferred;
    uint64_t total_bytes;
    double speed_bytes_per_sec;     // Transfer speed
    bool completed;
    bool cancelled;
    std::string error;              // Empty if no error

    double progress_percent() const {
        return total_bytes > 0
            ? (100.0 * bytes_transferred / total_bytes)
            : 0.0;
    }
};

// Callbacks
using ProgressCallback = std::function<void(const TransferProgress&)>;
using DataChunkCallback = std::function<void(const uint8_t* data, size_t size, bool is_last)>;

// ============================================================================
// IFileTransfer Interface
// ============================================================================

class IFileTransfer {
public:
    virtual ~IFileTransfer() = default;

    // ========== Directory Operations ==========

    // List directory contents
    // Returns: Vector of FileInfo for all items in directory
    virtual common::Result<std::vector<FileInfo>> list_directory(
        const std::string& path) = 0;

    // Get info for a single file/directory
    virtual common::Result<FileInfo> get_file_info(
        const std::string& path) = 0;

    // Create directory (recursive - creates parent dirs if needed)
    virtual common::EmptyResult create_directory(
        const std::string& path) = 0;

    // Delete file or empty directory
    virtual common::EmptyResult delete_path(
        const std::string& path) = 0;

    // ========== Download (Server → Client) ==========

    // Read file and call callback with chunks
    // Optimized for streaming large files
    // on_chunk: Called for each chunk (data pointer valid only during callback)
    // on_progress: Called periodically with transfer progress
    virtual common::EmptyResult download_file(
        const std::string& path,
        DataChunkCallback on_chunk,
        ProgressCallback on_progress = nullptr) = 0;

    // ========== Upload (Client → Server) ==========

    // Start upload session
    // Creates/truncates file and prepares for receiving chunks
    virtual common::EmptyResult upload_start(
        const std::string& path,
        uint64_t expected_size) = 0;

    // Write chunk to ongoing upload
    // Data is moved to avoid copying large buffers
    virtual common::EmptyResult upload_chunk(
        const std::string& path,
        const uint8_t* data,
        size_t size) = 0;

    // Finalize upload (flush buffers, verify size)
    virtual common::EmptyResult upload_finish(
        const std::string& path) = 0;

    // Cancel ongoing upload (delete partial file)
    virtual common::EmptyResult upload_cancel(
        const std::string& path) = 0;

    // ========== Utility ==========

    // Get available disk space at path
    virtual common::Result<uint64_t> get_free_space(
        const std::string& path) = 0;

    // Rename/move file or directory
    virtual common::EmptyResult rename(
        const std::string& old_path,
        const std::string& new_path) = 0;
};

} // namespace interfaces
