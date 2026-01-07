
#include <deque>
#include <sstream>
#include <thread>
#include <chrono>
#include <ctime>
#include <condition_variable>
#include "core/NetworkDefs.hpp"
#include "core/BackendServer.hpp"
#include "core/StreamSession.hpp"
#include "core/BroadcastBus.hpp"
#include "interfaces/IInputInjector.hpp"

// Conditional Includes
#ifdef PLATFORM_LINUX
    #include "platform/linux/LinuxX11Streamer.hpp"
    #include "platform/linux/LinuxEvdevLogger.hpp"
    #include "platform/linux/LinuxAppManager.hpp"
    #include "platform/linux/LinuxWebcamStreamer.hpp"
    #include "platform/linux/LinuxInputInjectorFactory.hpp"
    #include "platform/linux/LinuxFileTransfer.hpp"
#elif defined(PLATFORM_WINDOWS)
    #include "platform/windows/WindowsScreenStreamer.hpp"
    #include "platform/windows/WindowsWebcamStreamer.hpp"
    #include "platform/windows/WindowsKeylogger.hpp"
    #include "platform/windows/WindowsAppManager.hpp"
    #include "platform/windows/WindowsInputInjector.hpp"
    #include "platform/windows/WindowsFileTransfer.hpp"
#elif defined(PLATFORM_MACOS)
    #include "platform/macos/MacOSScreenStreamer.hpp"
    #include "platform/macos/MacOSWebcamStreamer.hpp"
    #include "platform/macos/MacOSKeylogger.hpp"
    #include "platform/macos/MacOSAppManager.hpp"
    #include "platform/macos/MacOSInputInjector.hpp"
    #include "platform/macos/MacOSFileTransfer.hpp"
#else
    #include "testing/MockStreamer.hpp"
    // Mock classes for dev
    class MockKeylogger : public interfaces::IKeylogger {
        common::EmptyResult start(std::function<void(const interfaces::KeyEvent&)>) override { return common::Result<common::Ok>::success(); }
        void stop() override {}
        bool is_active() const override { return false; }
    };
    class MockAppManager : public interfaces::IAppManager {
        std::vector<interfaces::AppEntry> list_applications(bool) override { return {}; }
        common::Result<uint32_t> launch_app(const std::string&) override { return common::Result<uint32_t>::ok(0); }
        common::EmptyResult kill_process(uint32_t) override { return common::Result<common::Ok>::success(); }
        common::EmptyResult shutdown_system() override { return common::Result<common::Ok>::success(); }
        common::EmptyResult restart_system() override { return common::Result<common::Ok>::success(); }
        std::vector<interfaces::AppEntry> search_apps(const std::string&) override { return {}; }
    };
    class MockInputInjector : public interfaces::IInputInjector {
        common::EmptyResult move_mouse(float, float) override { return common::Result<common::Ok>::success(); }
        common::EmptyResult click_mouse(interfaces::MouseButton, bool) override { return common::Result<common::Ok>::success(); }
        common::EmptyResult scroll_mouse(int) override { return common::Result<common::Ok>::success(); }
        common::EmptyResult press_key(interfaces::KeyCode, bool) override { return common::Result<common::Ok>::success(); }
        common::EmptyResult send_text(const std::string&) override { return common::Result<common::Ok>::success(); }
    };
#endif


#include <iostream>

int main(int argc, char** argv) {
    init_network();
    std::cout << "[Main] Universal Agent Starting..." << std::endl;

    int port = 9091; // Default Backend Server Port

    if(argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "[Main] Invalid port specified, using default 9091" << std::endl;
        }
    }

    std::cout << "[Main] Listening on port: " << port << std::endl;
    std::cout << "[Main] (Discovery will announce this port to any Gateway)" << std::endl;

    // 1. Core Services
    auto bus = std::make_shared<core::BroadcastBus>();

    // 2. Wiring HAL
#ifdef PLATFORM_LINUX
    std::cout << "[Main] Mode: LINUX REAL HARDWARE" << std::endl;
    // 1. Core Services
    auto monitor_bus = std::make_shared<core::BroadcastBus>();
    auto webcam_bus = std::make_shared<core::BroadcastBus>();

    // 2. HAL
    auto screen_streamer = std::make_shared<platform::linux_os::LinuxX11Streamer>();
    auto webcam_streamer = std::make_shared<platform::linux_os::LinuxWebcamStreamer>(0);
    auto keylogger = std::make_shared<platform::linux_os::LinuxEvdevLogger>();
    auto app_manager = std::make_shared<platform::linux_os::LinuxAppManager>();
    // Linux Input Injector using Factory (auto-detects X11 vs Wayland)
    std::shared_ptr<interfaces::IInputInjector> input_injector = platform::linux_os::LinuxInputInjectorFactory::create();
    auto file_transfer = std::make_shared<platform::linux_os::LinuxFileTransfer>();

    // 5. Sessions
    auto session = std::make_shared<core::StreamSession>(screen_streamer, monitor_bus);
    auto webcam_session = std::make_shared<core::StreamSession>(webcam_streamer, webcam_bus);

    // 6. Server
    core::BackendServer server(
        port,
        monitor_bus,
        webcam_bus,
        session,
        webcam_session,
        keylogger,
        app_manager,
        input_injector,
        file_transfer
    );

#elif defined(PLATFORM_WINDOWS)
    std::cout << "[Main] Mode: WINDOWS NATIVE" << std::endl;
    auto monitor_bus = std::make_shared<core::BroadcastBus>();
    auto webcam_bus = std::make_shared<core::BroadcastBus>();

    auto screen_streamer = std::make_shared<platform::windows_os::WindowsScreenStreamer>();
    auto webcam_streamer = std::make_shared<platform::windows_os::WindowsWebcamStreamer>(0);
    auto keylogger = std::make_shared<platform::windows_os::WindowsKeylogger>();
    auto app_manager = std::make_shared<platform::windows_os::WindowsAppManager>();
    auto input_injector = std::make_shared<platform::windows_os::WindowsInputInjector>();
    auto file_transfer = std::make_shared<platform::windows_os::WindowsFileTransfer>();

    auto session = std::make_shared<core::StreamSession>(screen_streamer, monitor_bus);
    auto webcam_session = std::make_shared<core::StreamSession>(webcam_streamer, webcam_bus);

    core::BackendServer server(
        port,
        monitor_bus,
        webcam_bus,
        session,
        webcam_session,
        keylogger,
        app_manager,
        input_injector,
        file_transfer
    );
#elif defined(PLATFORM_MACOS)
    std::cout << "[Main] Mode: MACOS NATIVE" << std::endl;
    auto monitor_bus = std::make_shared<core::BroadcastBus>();
    auto webcam_bus = std::make_shared<core::BroadcastBus>();

    auto screen_streamer = std::make_shared<platform::macos::MacOSScreenStreamer>();
    auto webcam_streamer = std::make_shared<platform::macos::MacOSWebcamStreamer>(0);
    auto keylogger = std::make_shared<platform::macos::MacOSKeylogger>();
    auto app_manager = std::make_shared<platform::macos::MacOSAppManager>();
    auto input_injector = std::make_shared<platform::macos::MacOSInputInjector>();
    auto file_transfer = std::make_shared<platform::macos::MacOSFileTransfer>();

    auto session = std::make_shared<core::StreamSession>(screen_streamer, monitor_bus);
    auto webcam_session = std::make_shared<core::StreamSession>(webcam_streamer, webcam_bus);

    core::BackendServer server(
        port,
        monitor_bus,
        webcam_bus,
        session,
        webcam_session,
        keylogger,
        app_manager,
        input_injector,
        file_transfer
    );
#else
    std::cout << "[Main] Mode: MOCK / DEVELOPMENT" << std::endl;
    auto bus = std::make_shared<core::BroadcastBus>();
    std::shared_ptr<interfaces::IVideoStreamer> streamer = std::make_shared<testing::MockStreamer>();
    std::shared_ptr<interfaces::IKeylogger> keylogger = std::make_shared<MockKeylogger>();
    std::shared_ptr<interfaces::IAppManager> app_manager = std::make_shared<MockAppManager>();
    std::shared_ptr<interfaces::IInputInjector> input_injector = std::make_shared<MockInputInjector>();

    auto session = std::make_shared<core::StreamSession>(bus, streamer);

    core::BackendServer server(port, bus, bus, session, session, keylogger, app_manager, input_injector, nullptr);
#endif

    // 4. Run
    server.run();

    cleanup_network();
    return 0;
}
