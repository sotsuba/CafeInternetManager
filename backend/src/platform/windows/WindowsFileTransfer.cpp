#include "WindowsFileTransfer.hpp"

#include <shlobj.h>
#include <chrono>
#include <iostream>

#pragma comment(lib, "Shell32.lib")

namespace platform {
namespace windows_os {

// ============================================================================
// Construction
// ============================================================================

WindowsFileTransfer::WindowsFileTransfer() = default;

WindowsFileTransfer::~WindowsFileTransfer() {
    // Cleanup any active uploads
    std::lock_guard<std::mutex> lock(uploads_mutex_);
    for (auto& [path, state] : active_uploads_) {
        if (state.file_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(state.file_handle);
        }
    }
    active_uploads_.clear();
}

// ============================================================================
// Helper Functions
// ============================================================================

std::wstring WindowsFileTransfer::to_wide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

std::string WindowsFileTransfer::to_narrow(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

uint64_t WindowsFileTransfer::filetime_to_unix(const FILETIME& ft) {
    // FILETIME is 100-nanosecond intervals since Jan 1, 1601
    // Unix timestamp is seconds since Jan 1, 1970
    ULARGE_INTEGER ull;
    ull.LowPart = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;
    // Subtract difference between 1601 and 1970, convert to seconds
    return (ull.QuadPart - 116444736000000000ULL) / 10000000ULL;
}

interfaces::FileInfo WindowsFileTransfer::make_file_info(
    const std::wstring& dir_path,
    const WIN32_FIND_DATAW& data
) {
    interfaces::FileInfo info;
    info.name = to_narrow(data.cFileName);

    // Build full path
    std::wstring full_path = dir_path;
    if (!full_path.empty() && full_path.back() != L'\\' && full_path.back() != L'/') {
        full_path += L'\\';
    }
    full_path += data.cFileName;
    info.path = to_narrow(full_path);

    // Size (combine high and low parts)
    info.size = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;

    // Modified time
    info.modified_time = filetime_to_unix(data.ftLastWriteTime);

    // Attributes
    info.is_directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    info.is_hidden = (data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
    info.is_readonly = (data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;

    return info;
}

// ============================================================================
// Directory Operations
// ============================================================================

common::Result<std::vector<interfaces::FileInfo>> WindowsFileTransfer::list_directory(
    const std::string& path
) {
    std::wstring wpath = to_wide(path);

    // Ensure path ends with \* for FindFirstFile
    std::wstring search_path = wpath;
    if (!search_path.empty() && search_path.back() != L'\\') {
        search_path += L'\\';
    }
    search_path += L'*';

    WIN32_FIND_DATAW find_data;
    HANDLE h_find = FindFirstFileW(search_path.c_str(), &find_data);

    if (h_find == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_PATH_NOT_FOUND || err == ERROR_FILE_NOT_FOUND) {
            return common::Result<std::vector<interfaces::FileInfo>>::err(
                common::ErrorCode::DeviceNotFound,
                "Directory not found: " + path);
        }
        return common::Result<std::vector<interfaces::FileInfo>>::err(
            common::ErrorCode::PermissionDenied,
            "Cannot access directory: " + path);
    }

    std::vector<interfaces::FileInfo> results;

    do {
        // Skip . and ..
        if (wcscmp(find_data.cFileName, L".") == 0 ||
            wcscmp(find_data.cFileName, L"..") == 0) {
            continue;
        }

        results.push_back(make_file_info(wpath, find_data));

    } while (FindNextFileW(h_find, &find_data));

    FindClose(h_find);

    return common::Result<std::vector<interfaces::FileInfo>>::ok(std::move(results));
}

common::Result<interfaces::FileInfo> WindowsFileTransfer::get_file_info(
    const std::string& path
) {
    std::wstring wpath = to_wide(path);

    WIN32_FIND_DATAW find_data;
    HANDLE h_find = FindFirstFileW(wpath.c_str(), &find_data);

    if (h_find == INVALID_HANDLE_VALUE) {
        return common::Result<interfaces::FileInfo>::err(
            common::ErrorCode::DeviceNotFound,
            "File not found: " + path);
    }

    FindClose(h_find);

    // Extract directory path from full path
    std::wstring dir_path = wpath;
    size_t last_sep = dir_path.find_last_of(L"\\/");
    if (last_sep != std::wstring::npos) {
        dir_path = dir_path.substr(0, last_sep);
    }

    return common::Result<interfaces::FileInfo>::ok(make_file_info(dir_path, find_data));
}

common::EmptyResult WindowsFileTransfer::create_directory(const std::string& path) {
    std::wstring wpath = to_wide(path);

    // SHCreateDirectoryExW creates parent dirs if needed
    int result = SHCreateDirectoryExW(nullptr, wpath.c_str(), nullptr);

    if (result != ERROR_SUCCESS && result != ERROR_ALREADY_EXISTS) {
        return common::EmptyResult::err(
            common::ErrorCode::PermissionDenied,
            "Failed to create directory: " + path);
    }

    return common::EmptyResult::success();
}

common::EmptyResult WindowsFileTransfer::delete_path(const std::string& path) {
    std::wstring wpath = to_wide(path);

    DWORD attrs = GetFileAttributesW(wpath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return common::EmptyResult::err(
            common::ErrorCode::DeviceNotFound,
            "Path not found: " + path);
    }

    BOOL result;
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        result = RemoveDirectoryW(wpath.c_str());
    } else {
        result = DeleteFileW(wpath.c_str());
    }

    if (!result) {
        return common::EmptyResult::err(
            common::ErrorCode::PermissionDenied,
            "Failed to delete: " + path);
    }

    return common::EmptyResult::success();
}

// ============================================================================
// Download
// ============================================================================

common::EmptyResult WindowsFileTransfer::download_file(
    const std::string& path,
    interfaces::DataChunkCallback on_chunk,
    interfaces::ProgressCallback on_progress
) {
    std::wstring wpath = to_wide(path);

    // Open file with sequential scan hint for better read-ahead caching
    HANDLE h_file = CreateFileW(
        wpath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr
    );

    if (h_file == INVALID_HANDLE_VALUE) {
        return common::EmptyResult::err(
            common::ErrorCode::DeviceNotFound,
            "Cannot open file: " + path);
    }

    // Get file size
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(h_file, &file_size)) {
        CloseHandle(h_file);
        return common::EmptyResult::err(
            common::ErrorCode::Unknown,
            "Cannot get file size: " + path);
    }

    uint64_t total_size = static_cast<uint64_t>(file_size.QuadPart);
    uint64_t bytes_read = 0;

    auto start_time = std::chrono::steady_clock::now();

    // Read buffer - using larger buffer for speed
    // OPTIMIZATION: Increase to 256KB or 1MB for very fast disks/networks
    std::vector<uint8_t> buffer(interfaces::FILE_TRANSFER_CHUNK_SIZE);

    while (bytes_read < total_size) {
        DWORD to_read = static_cast<DWORD>((std::min)(
            static_cast<uint64_t>(buffer.size()),
            total_size - bytes_read
        ));

        DWORD actually_read = 0;
        if (!ReadFile(h_file, buffer.data(), to_read, &actually_read, nullptr)) {
            CloseHandle(h_file);
            return common::EmptyResult::err(
                common::ErrorCode::Unknown,
                "Read error at offset " + std::to_string(bytes_read));
        }

        if (actually_read == 0) break;  // EOF

        bytes_read += actually_read;
        bool is_last = (bytes_read >= total_size);

        // Call data callback
        if (on_chunk) {
            on_chunk(buffer.data(), actually_read, is_last);
        }

        // Call progress callback (every chunk or periodically)
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

    CloseHandle(h_file);
    return common::EmptyResult::success();
}

// ============================================================================
// Upload
// ============================================================================

common::EmptyResult WindowsFileTransfer::upload_start(
    const std::string& path,
    uint64_t expected_size
) {
    std::wstring wpath = to_wide(path);

    // Create or truncate file
    HANDLE h_file = CreateFileW(
        wpath.c_str(),
        GENERIC_WRITE,
        0,  // No sharing during upload
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (h_file == INVALID_HANDLE_VALUE) {
        return common::EmptyResult::err(
            common::ErrorCode::PermissionDenied,
            "Cannot create file: " + path);
    }

    // Store upload state
    std::lock_guard<std::mutex> lock(uploads_mutex_);

    // Cancel any existing upload for this path
    auto it = active_uploads_.find(path);
    if (it != active_uploads_.end()) {
        if (it->second.file_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(it->second.file_handle);
        }
        active_uploads_.erase(it);
    }

    UploadState state;
    state.file_handle = h_file;
    state.path = path;
    state.expected_size = expected_size;
    state.bytes_written = 0;
    state.start_time = std::chrono::steady_clock::now();

    active_uploads_[path] = std::move(state);

    return common::EmptyResult::success();
}

common::EmptyResult WindowsFileTransfer::upload_chunk(
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

    DWORD bytes_written = 0;
    if (!WriteFile(state.file_handle, data, static_cast<DWORD>(size),
                   &bytes_written, nullptr)) {
        return common::EmptyResult::err(
            common::ErrorCode::Unknown,
            "Write error at offset " + std::to_string(state.bytes_written));
    }

    state.bytes_written += bytes_written;

    return common::EmptyResult::success();
}

common::EmptyResult WindowsFileTransfer::upload_finish(const std::string& path) {
    std::lock_guard<std::mutex> lock(uploads_mutex_);

    auto it = active_uploads_.find(path);
    if (it == active_uploads_.end()) {
        return common::EmptyResult::err(
            common::ErrorCode::Unknown,
            "No active upload for: " + path);
    }

    UploadState& state = it->second;

    // Flush and close
    FlushFileBuffers(state.file_handle);
    CloseHandle(state.file_handle);

    // Verify size if expected size was specified
    if (state.expected_size > 0 && state.bytes_written != state.expected_size) {
        // Delete incomplete file
        DeleteFileW(to_wide(path).c_str());
        active_uploads_.erase(it);
        return common::EmptyResult::err(
            common::ErrorCode::Unknown,
            "Size mismatch: expected " + std::to_string(state.expected_size) +
            ", got " + std::to_string(state.bytes_written));
    }

    active_uploads_.erase(it);
    return common::EmptyResult::success();
}

common::EmptyResult WindowsFileTransfer::upload_cancel(const std::string& path) {
    std::lock_guard<std::mutex> lock(uploads_mutex_);

    auto it = active_uploads_.find(path);
    if (it == active_uploads_.end()) {
        return common::EmptyResult::success();  // Already cancelled/finished
    }

    UploadState& state = it->second;

    // Close handle and delete partial file
    CloseHandle(state.file_handle);
    DeleteFileW(to_wide(path).c_str());

    active_uploads_.erase(it);
    return common::EmptyResult::success();
}

// ============================================================================
// Utility
// ============================================================================

common::Result<uint64_t> WindowsFileTransfer::get_free_space(const std::string& path) {
    std::wstring wpath = to_wide(path);

    ULARGE_INTEGER free_bytes_available;
    if (!GetDiskFreeSpaceExW(wpath.c_str(), &free_bytes_available, nullptr, nullptr)) {
        return common::Result<uint64_t>::err(
            common::ErrorCode::Unknown,
            "Cannot get free space for: " + path);
    }

    return common::Result<uint64_t>::ok(free_bytes_available.QuadPart);
}

common::EmptyResult WindowsFileTransfer::rename(
    const std::string& old_path,
    const std::string& new_path
) {
    std::wstring wold = to_wide(old_path);
    std::wstring wnew = to_wide(new_path);

    if (!MoveFileW(wold.c_str(), wnew.c_str())) {
        return common::EmptyResult::err(
            common::ErrorCode::PermissionDenied,
            "Failed to rename: " + old_path + " -> " + new_path);
    }

    return common::EmptyResult::success();
}

} // namespace windows_os
} // namespace platform
