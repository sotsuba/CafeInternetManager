#include "handlers/FileCommandHandler.hpp"
#include <vector>
#include <algorithm>
#include <iomanip>
#include <thread>
#include <iostream>

namespace handlers {

static const std::string base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

static inline bool is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}

std::vector<uint8_t> base64_decode(std::string const& encoded_string) {
  int in_len = encoded_string.size();
  int i = 0;
  int j = 0;
  int in_ = 0;
  unsigned char char_array_4[4], char_array_3[3];
  std::vector<uint8_t> ret;

  while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
    char_array_4[i++] = encoded_string[in_]; in_++;
    if (i ==4) {
      for (i = 0; i <4; i++)
        char_array_4[i] = base64_chars.find(char_array_4[i]);

      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; (i < 3); i++)
        ret.push_back(char_array_3[i]);
      i = 0;
    }
  }

  if (i) {
    for (j = i; j <4; j++)
      char_array_4[j] = 0;

    for (j = 0; j <4; j++)
      char_array_4[j] = base64_chars.find(char_array_4[j]);

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (j = 0; (j < i - 1); j++) ret.push_back(char_array_3[j]);
  }

  return ret;
}

// ============================================================================
// Base64 Encoding Utility
// ============================================================================
// Used for binary data in text-based protocol

static const char BASE64_CHARS[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string base64_encode(const uint8_t* data, size_t len) {
    std::string result;
    result.reserve((len + 2) / 3 * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = data[i] << 16;
        if (i + 1 < len) n |= data[i + 1] << 8;
        if (i + 2 < len) n |= data[i + 2];

        result += BASE64_CHARS[(n >> 18) & 0x3F];
        result += BASE64_CHARS[(n >> 12) & 0x3F];
        result += (i + 1 < len) ? BASE64_CHARS[(n >> 6) & 0x3F] : '=';
        result += (i + 2 < len) ? BASE64_CHARS[n & 0x3F] : '=';
    }

    return result;
}

// ============================================================================
// Command Implementations
// ============================================================================

// Helper for JSON escaping
static std::string escape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 10);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 32) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

common::EmptyResult FileListCommand::execute() {
    std::cout << "[FileList] Executing for path: " << path_ << std::endl;
    auto result = transfer_.list_directory(path_);

    if (result.is_err()) {
        ctx_.send_error("FILE_LIST_ERROR", result.error().message);
        return common::EmptyResult::success(); // Error sent via protocol
    }

    auto& files = result.unwrap();

    // Format: JSON-like array
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < files.size(); ++i) {
        if (i > 0) ss << ",";
        const auto& f = files[i];
        ss << "{\"name\":\"" << escape_json(f.name)
           << "\",\"path\":\"" << escape_json(f.path)
           << "\",\"size\":" << f.size
           << ",\"time\":" << f.modified_time
           << ",\"dir\":" << (f.is_directory ? "true" : "false")
           << ",\"hidden\":" << (f.is_hidden ? "true" : "false")
           << "}";
    }
    ss << "]";

    std::cout << "[FileList] Sending " << files.size() << " items" << std::endl;
    ctx_.send_data("FILES", ss.str());
    return common::EmptyResult::success();
}

common::EmptyResult FileInfoCommand::execute() {
    auto result = transfer_.get_file_info(path_);

    if (result.is_err()) {
        ctx_.send_error("FILE_INFO_ERROR", result.error().message);
        return common::EmptyResult::success();
    }

    auto& f = result.unwrap();

    std::ostringstream ss;
    ss << "{\"name\":\"" << f.name
       << "\",\"path\":\"" << f.path
       << "\",\"size\":" << f.size
       << ",\"time\":" << f.modified_time
       << ",\"dir\":" << (f.is_directory ? "true" : "false")
       << ",\"hidden\":" << (f.is_hidden ? "true" : "false")
       << ",\"readonly\":" << (f.is_readonly ? "true" : "false")
       << "}";

    ctx_.send_data("FILE_INFO", ss.str());
    return common::EmptyResult::success();
}

common::EmptyResult FileDownloadCommand::execute() {
    // Launch detached thread to handle download asynchronously
    // Capturing context by value is essential as Command object will be destroyed
    std::thread([transfer = &transfer_, path = path_, ctx = ctx_]() mutable {

        // Get file info first (inside thread to avoid blocking main thread even for stat)
        auto info_result = transfer->get_file_info(path);
        if (info_result.is_err()) {
            ctx.send_error("FILE_DOWNLOAD_ERROR", info_result.error().message);
            return;
        }

        auto& info = info_result.unwrap();
        if (info.is_directory) {
            ctx.send_error("FILE_DOWNLOAD_ERROR", "Cannot download directory");
            return;
        }

        // Send download start notification
        std::ostringstream header;
        header << path << "|" << info.size;
        ctx.send_data("FILE_DOWNLOAD_START", header.str());

        // Stream file chunks
        size_t chunk_num = 0;
        auto download_result = transfer->download_file(
            path,
            [&ctx, &chunk_num](const uint8_t* data, size_t size, bool is_last) {
                // Encode chunk as base64 for text protocol
                // Note: In high-performance scenarios, base64 overhead is significant.
                // Future optimization: Use separate binary channel or raw socket mode.
                std::string encoded = base64_encode(data, size);

                std::ostringstream chunk_msg;
                chunk_msg << chunk_num++ << "|" << size << "|"
                          << (is_last ? "1" : "0") << "|" << encoded;

                ctx.send_data("FILE_CHUNK", chunk_msg.str(), false); // Low Priority
            },
            nullptr  // No progress callback needed
        );

        if (download_result.is_err()) {
            ctx.send_error("FILE_DOWNLOAD_ERROR", download_result.error().message);
            return;
        }

        ctx.send_data("FILE_DOWNLOAD_END", path);

    }).detach();

    return common::EmptyResult::success();
}

common::EmptyResult FileUploadStartCommand::execute() {
    auto result = transfer_.upload_start(path_, size_);

    if (result.is_err()) {
        ctx_.send_error("FILE_UPLOAD_ERROR", result.error().message);
        return common::EmptyResult::success();
    }

    ctx_.send_status("FILE_UPLOAD_READY", path_);
    return common::EmptyResult::success();
}

common::EmptyResult FileMkdirCommand::execute() {
    auto result = transfer_.create_directory(path_);

    if (result.is_err()) {
        ctx_.send_error("FILE_MKDIR_ERROR", result.error().message);
        return common::EmptyResult::success();
    }

    ctx_.send_status("FILE_MKDIR_OK", path_);
    return common::EmptyResult::success();
}

common::EmptyResult FileDeleteCommand::execute() {
    auto result = transfer_.delete_path(path_);

    if (result.is_err()) {
        ctx_.send_error("FILE_DELETE_ERROR", result.error().message);
        return common::EmptyResult::success();
    }

    ctx_.send_status("FILE_DELETE_OK", path_);
    return common::EmptyResult::success();
}

common::EmptyResult FileRenameCommand::execute() {
    auto result = transfer_.rename(old_path_, new_path_);

    if (result.is_err()) {
        ctx_.send_error("FILE_RENAME_ERROR", result.error().message);
        return common::EmptyResult::success();
    }

    ctx_.send_status("FILE_RENAME_OK", new_path_);
    return common::EmptyResult::success();
}

common::EmptyResult FileSpaceCommand::execute() {
    auto result = transfer_.get_free_space(path_);

    if (result.is_err()) {
        ctx_.send_error("FILE_SPACE_ERROR", result.error().message);
        return common::EmptyResult::success();
    }

    uint64_t bytes = result.unwrap();

    std::ostringstream ss;
    ss << bytes;

    ctx_.send_data("FILE_SPACE", ss.str());
    return common::EmptyResult::success();
}

common::EmptyResult FileUploadChunkCommand::execute() {
    auto result = transfer_.upload_chunk(path_, data_.data(), data_.size());
    if (result.is_err()) {
        ctx_.send_error("FILE_UPLOAD_ERROR", result.error().message);
        return common::EmptyResult::success();
    }
    // No response per chunk for speed
    return common::EmptyResult::success();
}

common::EmptyResult FileUploadEndCommand::execute() {
    auto result = transfer_.upload_finish(path_);
    if (result.is_err()) {
        ctx_.send_error("FILE_UPLOAD_ERROR", result.error().message);
        return common::EmptyResult::success();
    }
    ctx_.send_status("FILE_UPLOAD_COMPLETE", path_);
    return common::EmptyResult::success();
}

common::EmptyResult FileUploadCancelCommand::execute() {
    auto result = transfer_.upload_cancel(path_);
    ctx_.send_status("FILE_UPLOAD_CANCELLED", path_);
    return common::EmptyResult::success();
}

// ============================================================================
// FileCommandHandler
// ============================================================================

bool FileCommandHandler::can_handle(const std::string& command) const {
    static const std::vector<std::string> commands = {
        "file_list", "file_info", "file_download",
        "file_upload_start", "file_upload_chunk", "file_upload_end", "file_upload_cancel",
        "file_mkdir", "file_delete", "file_rename", "file_space"
    };

    return std::find(commands.begin(), commands.end(), command) != commands.end();
}

std::unique_ptr<ICommand> FileCommandHandler::parse_command(
    const std::string& command,
    const std::string& args,
    const CommandContext& ctx
) {
    std::istringstream iss(args);

    // Make a copy of context that can be moved
    CommandContext ctx_copy = ctx;

    if (command == "file_list") {
        std::string path;
        if (std::getline(iss, path)) {
            // Trim whitespace
            path.erase(0, path.find_first_not_of(" \t"));
            if (!path.empty()) path.erase(path.find_last_not_of(" \t") + 1);
            return std::make_unique<FileListCommand>(transfer_, path, std::move(ctx_copy));
        }
        return nullptr;
    }

    if (command == "file_info") {
        std::string path;
        if (std::getline(iss, path)) {
            path.erase(0, path.find_first_not_of(" \t"));
            if (!path.empty()) path.erase(path.find_last_not_of(" \t") + 1);
            return std::make_unique<FileInfoCommand>(transfer_, path, std::move(ctx_copy));
        }
        return nullptr;
    }

    if (command == "file_download") {
        std::string path;
        if (std::getline(iss, path)) {
            path.erase(0, path.find_first_not_of(" \t"));
            if (!path.empty()) path.erase(path.find_last_not_of(" \t") + 1);
            return std::make_unique<FileDownloadCommand>(transfer_, path, std::move(ctx_copy));
        }
        return nullptr;
    }

    if (command == "file_upload_start") {
        std::string path;
        uint64_t size = 0;
        if (iss >> path >> size) {
            // Stateless: Don't store path in handler
            return std::make_unique<FileUploadStartCommand>(transfer_, path, size, std::move(ctx_copy));
        }
        return nullptr;
    }

    if (command == "file_upload_chunk") {
        // Format: path data_base64
        size_t last_space = args.find_last_of(' ');
        if (last_space != std::string::npos) {
            std::string path = args.substr(0, last_space);
            // Trim path
            path.erase(0, path.find_first_not_of(" \t"));
            path.erase(path.find_last_not_of(" \t") + 1);

            std::string b64 = args.substr(last_space + 1);
            auto data = base64_decode(b64);
            return std::make_unique<FileUploadChunkCommand>(
                transfer_, path, std::move(data), std::move(ctx_copy));
        }
        return nullptr;
    }

    if (command == "file_upload_end") {
        std::string path;
        if (std::getline(iss, path)) {
            path.erase(0, path.find_first_not_of(" \t"));
            if (!path.empty()) path.erase(path.find_last_not_of(" \t") + 1);
            return std::make_unique<FileUploadEndCommand>(transfer_, path, std::move(ctx_copy));
        }
        return nullptr;
    }

    if (command == "file_upload_cancel") {
        std::string path;
        if (std::getline(iss, path)) {
            path.erase(0, path.find_first_not_of(" \t"));
            if (!path.empty()) path.erase(path.find_last_not_of(" \t") + 1);
            return std::make_unique<FileUploadCancelCommand>(transfer_, path, std::move(ctx_copy));
        }
        return nullptr;
    }

    if (command == "file_mkdir") {
        std::string path;
        if (std::getline(iss, path)) {
            path.erase(0, path.find_first_not_of(" \t"));
            if (!path.empty()) path.erase(path.find_last_not_of(" \t") + 1);
            return std::make_unique<FileMkdirCommand>(transfer_, path, std::move(ctx_copy));
        }
        return nullptr;
    }

    if (command == "file_delete") {
        std::string path;
        if (std::getline(iss, path)) {
            path.erase(0, path.find_first_not_of(" \t"));
            if (!path.empty()) path.erase(path.find_last_not_of(" \t") + 1);
            return std::make_unique<FileDeleteCommand>(transfer_, path, std::move(ctx_copy));
        }
        return nullptr;
    }

    if (command == "file_rename") {
        std::string old_path, new_path;
        if (iss >> old_path >> new_path) {
            return std::make_unique<FileRenameCommand>(
                transfer_, old_path, new_path, std::move(ctx_copy));
        }
        return nullptr;
    }

    if (command == "file_space") {
        std::string path;
        if (std::getline(iss, path)) {
            path.erase(0, path.find_first_not_of(" \t"));
            if (!path.empty()) path.erase(path.find_last_not_of(" \t") + 1);
            return std::make_unique<FileSpaceCommand>(transfer_, path, std::move(ctx_copy));
        }
        return nullptr;
    }

    return nullptr;
}

} // namespace handlers
