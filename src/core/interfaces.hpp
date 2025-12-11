#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cafe {

// ============================================================================
// Core Interfaces - Following Interface Segregation Principle (ISP)
// ============================================================================

/**
 * @brief Interface for sending messages over a connection
 * Single responsibility: Message transmission
 */
class IMessageSender {
public:
    virtual ~IMessageSender() = default;
    virtual void sendText(const std::string& message) = 0;
    virtual void sendBinary(const std::vector<uint8_t>& data) = 0;
};

/**
 * @brief Interface for receiving messages from a connection
 * Single responsibility: Message reception
 */
class IMessageReceiver {
public:
    virtual ~IMessageReceiver() = default;
    virtual bool receiveText(std::string& outMessage) = 0;
    virtual bool hasData(int timeoutMs = 0) = 0;
};

/**
 * @brief Interface for logging
 * Single responsibility: Logging operations
 */
class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void info(const std::string& message) = 0;
    virtual void error(const std::string& message) = 0;
    virtual void debug(const std::string& message) = 0;
};

/**
 * @brief Interface for image/video capture devices
 * Single responsibility: Frame capture
 */
class ICaptureDevice {
public:
    virtual ~ICaptureDevice() = default;
    virtual std::vector<uint8_t> captureFrame() = 0;
    virtual bool isAvailable() const = 0;
    virtual std::string getName() const = 0;
};

/**
 * @brief Interface for streaming services
 * Single responsibility: Continuous frame streaming
 */
class IStreamable {
public:
    virtual ~IStreamable() = default;
    virtual void startStream(IMessageSender& sender, int fps) = 0;
    virtual void stopStream() = 0;
    virtual bool isStreaming() const = 0;
};

/**
 * @brief Context passed to command handlers
 * Contains all dependencies needed to execute commands
 */
struct CommandContext {
    IMessageSender& sender;
    ILogger& logger;
    std::function<void()> stopStreaming;
    std::function<bool()> isStreaming;
};

/**
 * @brief Interface for command handlers
 * Single responsibility: Handle a specific command
 * Open/Closed: New commands can be added without modifying existing code
 */
class ICommandHandler {
public:
    virtual ~ICommandHandler() = default;
    virtual std::string getCommand() const = 0;
    virtual bool canHandle(const std::string& command) const = 0;
    virtual void execute(const std::string& command, CommandContext& ctx) = 0;
};

/**
 * @brief Interface for keyboard event listening
 * Single responsibility: Capture keyboard events
 */
class IKeyboardListener {
public:
    using KeyCallback = std::function<void(int code, int value, const char* keyName)>;
    
    virtual ~IKeyboardListener() = default;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void setCallback(KeyCallback callback) = 0;
    virtual bool isRunning() const = 0;
};

/**
 * @brief Interface for process management
 * Single responsibility: Process operations
 */
class IProcessManager {
public:
    virtual ~IProcessManager() = default;
    virtual std::string listProcesses() = 0;
    virtual bool killProcess(int pid) = 0;
};

/**
 * @brief Interface for system operations
 * Single responsibility: System-level operations
 */
class ISystemController {
public:
    virtual ~ISystemController() = default;
    virtual void shutdown() = 0;
    virtual void restart() = 0;
};

} // namespace cafe
