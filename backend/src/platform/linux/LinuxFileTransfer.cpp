#include "LinuxFileTransfer.hpp"

#include <iostream>
#include <chrono>
#include <cstring>

// POSIX headers
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

namespace platform {
namespace linux_os {

// ============================================================================
// Construction
// ============================================================================

LinuxFileTransfer::LinuxFileTransfer() = default;

LinuxFileTransfer::~LinuxFileTransfer() {
    // Cleanup any active uploads
    std::lock_guard<std::mutex> lock(uploads_mutex_);
    for (auto& [path, state] : active_uploads_) {
        if (state.fd >= 0) {
            close(state.fd);
        }
    }
    active_uploads_.clear();
}

// ============================================================================
// Helper Functions
// ============================================================================

bool LinuxFileTransfer::create_dirs_recursive(const std::string& path) {
    size_t pos = 0;
    std::string current_path;

    while ((pos = path.find('/', pos + 1)) != std::string::npos) {
        current_path = path.substr(0, pos);
        if (!current_path.empty()) {
            struct stat st;
            if (stat(current_path.c_str(), &st) != 0) {
                if (mkdir(current_path.c_str(), 0755) != 0 && errno != EEXIST) {
                    return false;
                }
            }
        }
    }

    // Create the final directory
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
    }
    return true;
}

// ============================================================================
// Directory Operations
// ============================================================================

common::Result<std::vector<interfaces::FileInfo>> LinuxFileTransfer::list_directory(
    const std::string& path
) {
    // ROOT ENUMERATION: If path is empty, list root filesystem
    std::string actual_path = path;
    if (path.empty() || path == "." || path == "~") {
        actual_path = "/";
    }

    DIR* dir = opendir(actual_path.c_str());
    if (!dir) {
        if (errno == ENOENT) {
            return common::Result<std::vector<interfaces::FileInfo>>::err(
                common::ErrorCode::DeviceNotFound,
                "Directory not found: " + actual_path);
        }
        return common::Result<std::vector<interfaces::FileInfo>>::err(
            common::ErrorCode::PermissionDenied,
            "Cannot access directory: " + actual_path);
    }

    std::vector<interfaces::FileInfo> results;
    struct dirent* entry;

    while ((entry = readdir(dir)) != nullptr) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        interfaces::FileInfo info;
        info.name = entry->d_name;

        // Build full path
        std::string full_path = actual_path;
        if (!full_path.empty() && full_path.back() != '/') {
            full_path += '/';
        }
        full_path += entry->d_name;
        info.path = full_path;

        // Get detailed info via lstat (to detect symlinks)
        struct stat st;
        bool is_symlink = false;

        // Use lstat first to check if it's a symlink
        if (lstat(full_path.c_str(), &st) == 0) {
            is_symlink = S_ISLNK(st.st_mode);

            // If symlink, follow it with stat to get target info
            if (is_symlink) {
                struct stat target_st;
                if (stat(full_path.c_str(), &target_st) == 0) {
                    info.size = target_st.st_size;
                    info.modified_time = target_st.st_mtime;
                    info.is_directory = S_ISDIR(target_st.st_mode);
                    info.is_readonly = !(target_st.st_mode & S_IWUSR);
                } else {
                    // Broken symlink
                    info.size = 0;
                    info.modified_time = st.st_mtime;
                    info.is_directory = false;
                    info.is_readonly = true;
                }
            } else {
                info.size = st.st_size;
                info.modified_time = st.st_mtime;
                info.is_directory = S_ISDIR(st.st_mode);
                info.is_readonly = !(st.st_mode & S_IWUSR);
            }
        } else {
            info.size = 0;
            info.modified_time = 0;
            info.is_directory = (entry->d_type == DT_DIR);
            info.is_readonly = false;
        }

        // Check for hidden files (starts with .)
        info.is_hidden = (entry->d_name[0] == '.');

        results.push_back(std::move(info));
    }

    closedir(dir);
    return common::Result<std::vector<interfaces::FileInfo>>::ok(std::move(results));
}

common::Result<interfaces::FileInfo> LinuxFileTransfer::get_file_info(
    const std::string& path
) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return common::Result<interfaces::FileInfo>::err(
            common::ErrorCode::DeviceNotFound,
            "File not found: " + path);
    }

    interfaces::FileInfo info;

    // Extract filename from path
    size_t last_sep = path.find_last_of('/');
    info.name = (last_sep != std::string::npos) ? path.substr(last_sep + 1) : path;
    info.path = path;
    info.size = st.st_size;
    info.modified_time = st.st_mtime;
    info.is_directory = S_ISDIR(st.st_mode);
    info.is_readonly = !(st.st_mode & S_IWUSR);
    info.is_hidden = (!info.name.empty() && info.name[0] == '.');

    return common::Result<interfaces::FileInfo>::ok(std::move(info));
}

common::EmptyResult LinuxFileTransfer::create_directory(const std::string& path) {
    if (!create_dirs_recursive(path)) {
        return common::EmptyResult::err(
            common::ErrorCode::PermissionDenied,
            "Failed to create directory: " + path);
    }
    return common::EmptyResult::success();
}

common::EmptyResult LinuxFileTransfer::delete_path(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return common::EmptyResult::err(
            common::ErrorCode::DeviceNotFound,
            "Path not found: " + path);
    }

    int result;
    if (S_ISDIR(st.st_mode)) {
        result = rmdir(path.c_str());
    } else {
        result = unlink(path.c_str());
    }

    if (result != 0) {
        return common::EmptyResult::err(
            common::ErrorCode::PermissionDenied,
            "Failed to delete: " + path);
    }

    return common::EmptyResult::success();
}

// ============================================================================
// Download
// ============================================================================

common::EmptyResult LinuxFileTransfer::download_file(
    const std::string& path,
    interfaces::DataChunkCallback on_chunk,
    interfaces::ProgressCallback on_progress
) {
    // Open file with O_RDONLY
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return common::EmptyResult::err(
            common::ErrorCode::DeviceNotFound,
            "Cannot open file: " + path);
    }

    // Get file size
    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return common::EmptyResult::err(
            common::ErrorCode::Unknown,
            "Cannot get file size: " + path);
    }

    uint64_t total_size = st.st_size;
    uint64_t bytes_read = 0;

    auto start_time = std::chrono::steady_clock::now();

    // Read buffer
    std::vector<uint8_t> buffer(interfaces::FILE_TRANSFER_CHUNK_SIZE);

    while (bytes_read < total_size) {
        ssize_t n = read(fd, buffer.data(), buffer.size());
        if (n < 0) {
            close(fd);
            return common::EmptyResult::err(
                common::ErrorCode::Unknown,
                "Read error at offset " + std::to_string(bytes_read));
        }
        if (n == 0) break;  // EOF

        bytes_read += n;
        bool is_last = (bytes_read >= total_size);

        // Call data callback
        if (on_chunk) {
            on_chunk(buffer.data(), static_cast<size_t>(n), is_last);
        }

        // Call progress callback
        if (on_progress) {
            auto now = std::chrono::steady_clock::now();
            double elapsed_sec = std::chrono::duration<double>(now - start_time).count();
            double speed = elapsed_sec > 0 ? bytes_read / elapsed_sec : 0;

            interfaces::TransferProgress progress;
            progress.file_path = path;
            progress.bytes_transferred = bytes_read;
            progress.total_bytes = total_size;
            progress.speed_bytes_per_sec = speed;
            progress.completed = is_last;
            progress.cancelled = false;

            on_progress(progress);
        }
    }

    close(fd);
    return common::EmptyResult::success();
}

// ============================================================================
// Upload
// ============================================================================

common::EmptyResult LinuxFileTransfer::upload_start(
    const std::string& path,
    uint64_t expected_size
) {
    // Create parent directories if needed
    size_t last_sep = path.find_last_of('/');
    if (last_sep != std::string::npos) {
        std::string parent = path.substr(0, last_sep);
        create_dirs_recursive(parent);
    }

    // Create or truncate file
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return common::EmptyResult::err(
            common::ErrorCode::PermissionDenied,
            "Cannot create file: " + path);
    }

    // Store upload state
    std::lock_guard<std::mutex> lock(uploads_mutex_);

    // Cancel any existing upload for this path
    auto it = active_uploads_.find(path);
    if (it != active_uploads_.end()) {
        if (it->second.fd >= 0) {
            close(it->second.fd);
        }
        active_uploads_.erase(it);
    }

    UploadState state;
    state.fd = fd;
    state.path = path;
    state.expected_size = expected_size;
    state.bytes_written = 0;
    state.start_time = std::chrono::steady_clock::now();

    active_uploads_[path] = std::move(state);

    return common::EmptyResult::success();
}

common::EmptyResult LinuxFileTransfer::upload_chunk(
    const std::string& path,
    const uint8_t* data,
    size_t size
) {
    std::lock_guard<std::mutex> lock(uploads_mutex_);

    auto it = active_uploads_.find(path);
    if (it == active_uploads_.end()) {
        return common::EmptyResult::err(
            common::ErrorCode::Unknown,
            "No active upload for: " + path);
    }

    UploadState& state = it->second;

    ssize_t bytes_written = write(state.fd, data, size);
    if (bytes_written < 0) {
        return common::EmptyResult::err(
            common::ErrorCode::Unknown,
            "Write error at offset " + std::to_string(state.bytes_written));
    }

    state.bytes_written += bytes_written;

    return common::EmptyResult::success();
}

common::EmptyResult LinuxFileTransfer::upload_finish(const std::string& path) {
    std::lock_guard<std::mutex> lock(uploads_mutex_);

    auto it = active_uploads_.find(path);
    if (it == active_uploads_.end()) {
        return common::EmptyResult::err(
            common::ErrorCode::Unknown,
            "No active upload for: " + path);
    }

    UploadState& state = it->second;

    // Sync and close
    fsync(state.fd);
    close(state.fd);

    // Verify size if expected size was specified
    if (state.expected_size > 0 && state.bytes_written != state.expected_size) {
        // Delete incomplete file
        unlink(path.c_str());
        active_uploads_.erase(it);
        return common::EmptyResult::err(
            common::ErrorCode::Unknown,
            "Size mismatch: expected " + std::to_string(state.expected_size) +
            ", got " + std::to_string(state.bytes_written));
    }

    active_uploads_.erase(it);
    return common::EmptyResult::success();
}

common::EmptyResult LinuxFileTransfer::upload_cancel(const std::string& path) {
    std::lock_guard<std::mutex> lock(uploads_mutex_);

    auto it = active_uploads_.find(path);
    if (it == active_uploads_.end()) {
        return common::EmptyResult::success();  // Already cancelled/finished
    }

    UploadState& state = it->second;

    // Close handle and delete partial file
    close(state.fd);
    unlink(path.c_str());

    active_uploads_.erase(it);
    return common::EmptyResult::success();
}

// ============================================================================
// Utility
// ============================================================================

common::Result<uint64_t> LinuxFileTransfer::get_free_space(const std::string& path) {
    struct statvfs st;
    if (statvfs(path.c_str(), &st) != 0) {
        return common::Result<uint64_t>::err(
            common::ErrorCode::Unknown,
            "Cannot get free space for: " + path);
    }

    uint64_t free_bytes = static_cast<uint64_t>(st.f_bavail) * st.f_bsize;
    return common::Result<uint64_t>::ok(free_bytes);
}

common::EmptyResult LinuxFileTransfer::rename(
    const std::string& old_path,
    const std::string& new_path
) {
    if (::rename(old_path.c_str(), new_path.c_str()) != 0) {
        return common::EmptyResult::err(
            common::ErrorCode::PermissionDenied,
            "Failed to rename: " + old_path + " -> " + new_path);
    }

    return common::EmptyResult::success();
}

} // namespace linux_os
} // namespace platform
