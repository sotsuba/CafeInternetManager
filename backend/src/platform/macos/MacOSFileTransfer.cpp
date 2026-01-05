#include "MacOSFileTransfer.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <cstring>

namespace fs = std::filesystem;

namespace platform {
namespace macos {

// ============================================================================
// Construction
// ============================================================================

MacOSFileTransfer::MacOSFileTransfer() {
    std::cout << "[MacOSFileTransfer] Created" << std::endl;
}

MacOSFileTransfer::~MacOSFileTransfer() {
    std::lock_guard<std::mutex> lock(uploads_mutex_);
    for (auto& [path, state] : active_uploads_) {
        if (state.fd >= 0) {
            close(state.fd);
        }
    }
    active_uploads_.clear();
}

// ============================================================================
// Helpers
// ============================================================================

interfaces::FileInfo MacOSFileTransfer::make_file_info(
    const std::string& dir_path,
    const std::string& name
) {
    interfaces::FileInfo info;
    info.name = name;

    std::string full_path = dir_path;
    if (!full_path.empty() && full_path.back() != '/') {
        full_path += '/';
    }
    full_path += name;
    info.path = full_path;

    struct stat st;
    if (stat(full_path.c_str(), &st) == 0) {
        info.size = static_cast<uint64_t>(st.st_size);
        info.modified_time = static_cast<uint64_t>(st.st_mtime);
        info.is_directory = S_ISDIR(st.st_mode);
        info.is_readonly = !(st.st_mode & S_IWUSR);
    } else {
        info.size = 0;
        info.modified_time = 0;
        info.is_directory = false;
        info.is_readonly = false;
    }

    // Check if hidden (starts with .)
    info.is_hidden = !name.empty() && name[0] == '.';

    return info;
}

// ============================================================================
// Directory Operations
// ============================================================================

common::Result<std::vector<interfaces::FileInfo>> MacOSFileTransfer::list_directory(
    const std::string& path
) {
    std::vector<interfaces::FileInfo> results;

    // ROOT ENUMERATION: List volumes on macOS
    if (path.empty() || path == "/" || path == ".") {
        // List all mounted volumes
        DIR* dir = opendir("/Volumes");
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                    continue;
                }

                interfaces::FileInfo info;
                info.name = entry->d_name;
                info.path = std::string("/Volumes/") + entry->d_name;
                info.is_directory = true;
                info.is_hidden = false;
                info.is_readonly = false;
                info.size = 0;
                info.modified_time = 0;

                // Get free space for size
                struct statvfs svfs;
                if (statvfs(info.path.c_str(), &svfs) == 0) {
                    info.size = static_cast<uint64_t>(svfs.f_blocks) * svfs.f_frsize;
                }

                results.push_back(info);
            }
            closedir(dir);
        }

        // Also add root directory
        interfaces::FileInfo root;
        root.name = "/";
        root.path = "/";
        root.is_directory = true;
        root.is_hidden = false;
        root.is_readonly = false;
        root.size = 0;
        root.modified_time = 0;
        results.insert(results.begin(), root);

        return common::Result<std::vector<interfaces::FileInfo>>::ok(std::move(results));
    }

    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return common::Result<std::vector<interfaces::FileInfo>>::err(
            common::ErrorCode::DeviceNotFound,
            "Cannot open directory: " + path
        );
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        results.push_back(make_file_info(path, entry->d_name));
    }

    closedir(dir);
    return common::Result<std::vector<interfaces::FileInfo>>::ok(std::move(results));
}

common::Result<interfaces::FileInfo> MacOSFileTransfer::get_file_info(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return common::Result<interfaces::FileInfo>::err(
            common::ErrorCode::DeviceNotFound,
            "File not found: " + path
        );
    }

    // Extract name from path
    size_t last_sep = path.find_last_of('/');
    std::string name = (last_sep != std::string::npos) ? path.substr(last_sep + 1) : path;
    std::string dir_path = (last_sep != std::string::npos) ? path.substr(0, last_sep) : ".";

    return common::Result<interfaces::FileInfo>::ok(make_file_info(dir_path, name));
}

common::EmptyResult MacOSFileTransfer::create_directory(const std::string& path) {
    try {
        fs::create_directories(path);
        return common::EmptyResult::success();
    } catch (const std::exception& e) {
        return common::EmptyResult::err(
            common::ErrorCode::PermissionDenied,
            std::string("Failed to create directory: ") + e.what()
        );
    }
}

common::EmptyResult MacOSFileTransfer::delete_path(const std::string& path) {
    try {
        fs::remove_all(path);
        return common::EmptyResult::success();
    } catch (const std::exception& e) {
        return common::EmptyResult::err(
            common::ErrorCode::PermissionDenied,
            std::string("Failed to delete: ") + e.what()
        );
    }
}

// ============================================================================
// Download
// ============================================================================

common::EmptyResult MacOSFileTransfer::download_file(
    const std::string& path,
    interfaces::DataChunkCallback on_chunk,
    interfaces::ProgressCallback on_progress
) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return common::EmptyResult::err(
            common::ErrorCode::DeviceNotFound,
            "Cannot open file: " + path
        );
    }

    // Get file size
    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return common::EmptyResult::err(
            common::ErrorCode::Unknown,
            "Cannot stat file: " + path
        );
    }

    uint64_t total_size = static_cast<uint64_t>(st.st_size);
    uint64_t bytes_read = 0;

    auto start_time = std::chrono::steady_clock::now();

    std::vector<uint8_t> buffer(interfaces::FILE_TRANSFER_CHUNK_SIZE);

    while (bytes_read < total_size) {
        ssize_t n = read(fd, buffer.data(), buffer.size());
        if (n <= 0) break;

        bytes_read += static_cast<uint64_t>(n);
        bool is_last = (bytes_read >= total_size);

        if (on_chunk) {
            on_chunk(buffer.data(), static_cast<size_t>(n), is_last);
        }

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

common::EmptyResult MacOSFileTransfer::upload_start(
    const std::string& path,
    uint64_t expected_size
) {
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return common::EmptyResult::err(
            common::ErrorCode::PermissionDenied,
            "Cannot create file: " + path
        );
    }

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

common::EmptyResult MacOSFileTransfer::upload_chunk(
    const std::string& path,
    const uint8_t* data,
    size_t size
) {
    std::lock_guard<std::mutex> lock(uploads_mutex_);

    auto it = active_uploads_.find(path);
    if (it == active_uploads_.end()) {
        return common::EmptyResult::err(
            common::ErrorCode::Unknown,
            "No active upload for: " + path
        );
    }

    UploadState& state = it->second;

    ssize_t written = write(state.fd, data, size);
    if (written < 0 || static_cast<size_t>(written) != size) {
        return common::EmptyResult::err(
            common::ErrorCode::Unknown,
            "Write error at offset " + std::to_string(state.bytes_written)
        );
    }

    state.bytes_written += static_cast<uint64_t>(written);

    return common::EmptyResult::success();
}

common::EmptyResult MacOSFileTransfer::upload_finish(const std::string& path) {
    std::lock_guard<std::mutex> lock(uploads_mutex_);

    auto it = active_uploads_.find(path);
    if (it == active_uploads_.end()) {
        return common::EmptyResult::err(
            common::ErrorCode::Unknown,
            "No active upload for: " + path
        );
    }

    UploadState& state = it->second;

    fsync(state.fd);
    close(state.fd);

    if (state.expected_size > 0 && state.bytes_written != state.expected_size) {
        unlink(path.c_str());
        active_uploads_.erase(it);
        return common::EmptyResult::err(
            common::ErrorCode::Unknown,
            "Size mismatch: expected " + std::to_string(state.expected_size) +
            ", got " + std::to_string(state.bytes_written)
        );
    }

    active_uploads_.erase(it);
    return common::EmptyResult::success();
}

common::EmptyResult MacOSFileTransfer::upload_cancel(const std::string& path) {
    std::lock_guard<std::mutex> lock(uploads_mutex_);

    auto it = active_uploads_.find(path);
    if (it == active_uploads_.end()) {
        return common::EmptyResult::success();
    }

    close(it->second.fd);
    unlink(path.c_str());

    active_uploads_.erase(it);
    return common::EmptyResult::success();
}

// ============================================================================
// Utility
// ============================================================================

common::Result<uint64_t> MacOSFileTransfer::get_free_space(const std::string& path) {
    struct statvfs svfs;
    if (statvfs(path.c_str(), &svfs) != 0) {
        return common::Result<uint64_t>::err(
            common::ErrorCode::Unknown,
            "Cannot get free space for: " + path
        );
    }

    uint64_t free_space = static_cast<uint64_t>(svfs.f_bavail) * svfs.f_frsize;
    return common::Result<uint64_t>::ok(free_space);
}

common::EmptyResult MacOSFileTransfer::rename(
    const std::string& old_path,
    const std::string& new_path
) {
    if (::rename(old_path.c_str(), new_path.c_str()) != 0) {
        return common::EmptyResult::err(
            common::ErrorCode::PermissionDenied,
            "Failed to rename: " + old_path + " -> " + new_path
        );
    }

    return common::EmptyResult::success();
}

} // namespace macos
} // namespace platform
