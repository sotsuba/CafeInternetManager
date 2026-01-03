#pragma once
#include "core/ICommand.hpp"
#include "interfaces/IFileTransfer.hpp"
#include <memory>
#include <sstream>

namespace handlers {

using core::command::CommandContext;
using core::command::ICommand;
using core::command::ICommandHandler;

// ============================================================================
// FileCommandHandler - Handles file transfer commands
// ============================================================================
// Commands:
//   file_list <path>              - List directory contents
//   file_info <path>              - Get file/directory info
//   file_download <path>          - Start file download (sends chunks)
//   file_upload_start <path> <size> - Start file upload
//   file_mkdir <path>             - Create directory
//   file_delete <path>            - Delete file/directory
//   file_rename <old> <new>       - Rename/move file
//   file_space <path>             - Get free disk space
// ============================================================================

class FileListCommand : public ICommand {
public:
    FileListCommand(interfaces::IFileTransfer& transfer,
                   std::string path,
                   CommandContext ctx)
        : transfer_(transfer), path_(std::move(path)), ctx_(std::move(ctx)) {}

    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "file_list"; }

private:
    interfaces::IFileTransfer& transfer_;
    std::string path_;
    CommandContext ctx_;
};

class FileInfoCommand : public ICommand {
public:
    FileInfoCommand(interfaces::IFileTransfer& transfer,
                   std::string path,
                   CommandContext ctx)
        : transfer_(transfer), path_(std::move(path)), ctx_(std::move(ctx)) {}

    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "file_info"; }

private:
    interfaces::IFileTransfer& transfer_;
    std::string path_;
    CommandContext ctx_;
};

class FileDownloadCommand : public ICommand {
public:
    FileDownloadCommand(interfaces::IFileTransfer& transfer,
                       std::string path,
                       CommandContext ctx)
        : transfer_(transfer), path_(std::move(path)), ctx_(std::move(ctx)) {}

    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "file_download"; }

private:
    interfaces::IFileTransfer& transfer_;
    std::string path_;
    CommandContext ctx_;
};

class FileUploadStartCommand : public ICommand {
public:
    FileUploadStartCommand(interfaces::IFileTransfer& transfer,
                          std::string path,
                          uint64_t size,
                          CommandContext ctx)
        : transfer_(transfer), path_(std::move(path)),
          size_(size), ctx_(std::move(ctx)) {}

    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "file_upload_start"; }

private:
    interfaces::IFileTransfer& transfer_;
    std::string path_;
    uint64_t size_;
    CommandContext ctx_;
};

class FileUploadChunkCommand : public ICommand {
public:
    FileUploadChunkCommand(interfaces::IFileTransfer& transfer,
                          std::string path,
                          std::vector<uint8_t> data,
                          CommandContext ctx)
        : transfer_(transfer), path_(std::move(path)),
          data_(std::move(data)), ctx_(std::move(ctx)) {}

    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "file_upload_chunk"; }

private:
    interfaces::IFileTransfer& transfer_;
    std::string path_;
    std::vector<uint8_t> data_;
    CommandContext ctx_;
};

class FileUploadEndCommand : public ICommand {
public:
    FileUploadEndCommand(interfaces::IFileTransfer& transfer,
                        std::string path,
                        CommandContext ctx)
        : transfer_(transfer), path_(std::move(path)), ctx_(std::move(ctx)) {}

    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "file_upload_end"; }

private:
    interfaces::IFileTransfer& transfer_;
    std::string path_;
    CommandContext ctx_;
};

class FileUploadCancelCommand : public ICommand {
public:
    FileUploadCancelCommand(interfaces::IFileTransfer& transfer,
                           std::string path,
                           CommandContext ctx)
        : transfer_(transfer), path_(std::move(path)), ctx_(std::move(ctx)) {}

    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "file_upload_cancel"; }

private:
    interfaces::IFileTransfer& transfer_;
    std::string path_;
    CommandContext ctx_;
};

class FileMkdirCommand : public ICommand {
public:
    FileMkdirCommand(interfaces::IFileTransfer& transfer,
                    std::string path,
                    CommandContext ctx)
        : transfer_(transfer), path_(std::move(path)), ctx_(std::move(ctx)) {}

    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "file_mkdir"; }

private:
    interfaces::IFileTransfer& transfer_;
    std::string path_;
    CommandContext ctx_;
};

class FileDeleteCommand : public ICommand {
public:
    FileDeleteCommand(interfaces::IFileTransfer& transfer,
                     std::string path,
                     CommandContext ctx)
        : transfer_(transfer), path_(std::move(path)), ctx_(std::move(ctx)) {}

    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "file_delete"; }

private:
    interfaces::IFileTransfer& transfer_;
    std::string path_;
    CommandContext ctx_;
};

class FileRenameCommand : public ICommand {
public:
    FileRenameCommand(interfaces::IFileTransfer& transfer,
                     std::string old_path,
                     std::string new_path,
                     CommandContext ctx)
        : transfer_(transfer), old_path_(std::move(old_path)),
          new_path_(std::move(new_path)), ctx_(std::move(ctx)) {}

    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "file_rename"; }

private:
    interfaces::IFileTransfer& transfer_;
    std::string old_path_;
    std::string new_path_;
    CommandContext ctx_;
};

class FileSpaceCommand : public ICommand {
public:
    FileSpaceCommand(interfaces::IFileTransfer& transfer,
                    std::string path,
                    CommandContext ctx)
        : transfer_(transfer), path_(std::move(path)), ctx_(std::move(ctx)) {}

    common::EmptyResult execute() override;
    const char* type() const noexcept override { return "file_space"; }

private:
    interfaces::IFileTransfer& transfer_;
    std::string path_;
    CommandContext ctx_;
};

// ============================================================================
// FileCommandHandler - Factory for file commands
// ============================================================================

class FileCommandHandler : public ICommandHandler {
public:
    explicit FileCommandHandler(interfaces::IFileTransfer& transfer)
        : transfer_(transfer) {}

    bool can_handle(const std::string& command) const override;

    std::unique_ptr<ICommand> parse_command(
        const std::string& command,
        const std::string& args,
        const CommandContext& ctx) override;

    const char* category() const noexcept override {
        return "FileCommandHandler";
    }

private:
    interfaces::IFileTransfer& transfer_;

    // Track current upload for chunk handling
    std::string current_upload_path_;
};

} // namespace handlers
