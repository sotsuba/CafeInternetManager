#include "net/backend_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <sstream>

#include "api/monitor.hpp"
#include "api/process.hpp"
#include "api/webcam.hpp"
#include "api/webcam.hpp"

static const size_t BUFF_SIZE = 8 * 1024 * 1024;
static const int BACKEND_HEADER_SIZE = 12;

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

    // Accept loop in this thread
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

        // Handle client in detached thread
        std::thread t(&BackendServer::handleClient, this, fd);
        t.detach();
    }
}

bool BackendServer::readFrame(int fd, std::vector<uint8_t>& payload, uint32_t& client_id, uint32_t& backend_id) {
    // Read 12-byte header first
    uint8_t header[BACKEND_HEADER_SIZE];
    size_t got = 0;
    while (got < BACKEND_HEADER_SIZE) {
        ssize_t n = recv(fd, header + got, BACKEND_HEADER_SIZE - got, 0);
        if (n <= 0) {
            if (n == 0) return false; // closed
            if (errno == EINTR) continue;
            perror("recv header");
            return false;
        }
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
        if (n <= 0) {
            if (n == 0) return false;
            if (errno == EINTR) continue;
            perror("recv payload");
            return false;
        }
        rcv += n;
    }

    return true;
}

bool BackendServer::sendFrame(int fd, const uint8_t* payload, uint32_t payload_len, uint32_t client_id, uint32_t backend_id) {
    uint32_t net_len = htonl(payload_len);
    uint32_t net_client = htonl(client_id);
    uint32_t net_backend = htonl(backend_id);

    // send header
    uint8_t header[BACKEND_HEADER_SIZE];
    memcpy(header, &net_len, 4);
    memcpy(header + 4, &net_client, 4);
    memcpy(header + 8, &net_backend, 4);

    // send all header
    size_t sent = 0;
    while (sent < BACKEND_HEADER_SIZE) {
        ssize_t n = send(fd, header + sent, BACKEND_HEADER_SIZE - sent, 0);
        if (n <= 0) {
            if (n == 0) return false;
            if (errno == EINTR) continue;
            perror("send header-");
            return false;
        }
        sent += n;
    }

    // send payload
    sent = 0;
    while (sent < payload_len) {
        ssize_t n = send(fd, payload + sent, payload_len - sent, 0);
        if (n <= 0) {
            if (n == 0) return false;
            if (errno == EINTR) continue;
            perror("send payload");
            return false;
        }
        sent += n;
    }

    return true;
}

static void make_dummy_jpeg(std::vector<uint8_t>& out, int size) {
    out.resize(size);
    for (int i = 0; i < size; ++i) out[i] = static_cast<uint8_t>(i % 256);
}

void BackendServer::streamingThread(int fd, uint32_t client_id, uint32_t backend_id, std::atomic_bool* running) {
    const int FPS = 15;
    const useconds_t interval = 1000000 / FPS;
    std::vector<uint8_t> jpeg;
    int count = 0;

    while (running->load()) {
        // Capture screen frame via Monitor API. Fallback to dummy jpeg when capture fails.
        static Monitor monitor;
        jpeg = monitor.capture_frame();
        if (jpeg.empty()) {
            make_dummy_jpeg(jpeg, 1024 + (count % 100));
        }

        if (!sendFrame(fd, jpeg.data(), (uint32_t)jpeg.size(), client_id, backend_id)) {
            std::cerr << "[BackendServer] failed to send frame during streaming" << std::endl;
            break;
        }
        ++count;
        if (count % 30 == 0) std::cout << "[BackendServer] streamed " << count << " frames\n";
        usleep(interval);
    }

    std::cout << "[BackendServer] streaming stopped" << std::endl;
}



void BackendServer::handleClient(int fd) {
    std::vector<uint8_t> payload;
    uint32_t client_id = 0, backend_id = 0;

    std::atomic_bool alive(true);

    // per-connection send lock: tránh interleave header/payload giữa threads
    std::mutex send_mu;
    auto send_text = [&](const std::string& s, uint32_t cid, uint32_t bid) -> bool {
        std::lock_guard<std::mutex> lk(send_mu);
        return sendFrame(fd, (const uint8_t*)s.data(), (uint32_t)s.size(), cid, bid);
    };
    auto send_bytes = [&](const std::vector<uint8_t>& b, uint32_t cid, uint32_t bid) -> bool {
        std::lock_guard<std::mutex> lk(send_mu);
        return sendFrame(fd, b.data(), (uint32_t)b.size(), cid, bid);
    };

    // screen streaming
    std::atomic_bool screen_streaming(false);
    std::thread screen_thread;

    // webcam streaming
    std::atomic_bool webcam_streaming(false);
    std::thread webcam_thread;
    std::unique_ptr<Webcam> webcam_cam;
    int webcam_index = 0;

    auto stop_screen = [&]() {
        screen_streaming.store(false);
        if (screen_thread.joinable()) screen_thread.join();
    };
    auto stop_webcam = [&]() {
        webcam_streaming.store(false);
        if (webcam_thread.joinable()) webcam_thread.join();
        if (webcam_cam) { webcam_cam->close(); webcam_cam.reset(); }
    };

    auto split_ws = [](const std::string& s) {
        std::istringstream iss(s);
        std::vector<std::string> out;
        for (std::string tok; iss >> tok;) out.push_back(tok);
        return out;
    };
    auto to_int = [](const std::string& s, int def) {
        try { return std::stoi(s); } catch (...) { return def; }
    };

    using H = std::function<void(const std::vector<std::string>&, uint32_t, uint32_t)>;
    std::unordered_map<std::string, H> handlers;

    handlers["start_monitor_stream"] = [&](const auto&, uint32_t cid, uint32_t bid) {
        if (screen_streaming.exchange(true)) return (void)send_text("ERROR: Already streaming", cid, bid);

        // Stop webcam if active
        if (webcam_streaming.load()) stop_webcam();

        send_text("OK: Streaming started", cid, bid);

        stop_screen();                 // ensure no previous thread
        screen_streaming.store(true);

    screen_thread = std::thread([&, cid, bid]() {
            Monitor monitor;

            // Continuous H.264 Stream (using FFmpeg pipe)
            // Blocks here until callback returns false or stream ends
            monitor.stream_h264([&](const std::vector<uint8_t>& chunk) {
                if (!alive.load() || !screen_streaming.load()) return false; // Stop stream
                return send_bytes(chunk, cid, bid);
            });

            screen_streaming.store(false);
            send_text("INFO: Streaming stopped", cid, bid);
        });
    };

    handlers["stop_monitor_stream"] = [&](const auto&, uint32_t cid, uint32_t bid) {
        if (!screen_streaming.load()) return (void)send_text("ERROR: Not streaming", cid, bid);
        stop_screen();
        send_text("OK: Streaming stopped", cid, bid);
    };

    handlers["frame_capture"] = [&](const auto&, uint32_t cid, uint32_t bid) {
        Monitor monitor;
        auto jpeg = monitor.capture_frame();
        if (jpeg.empty()) make_dummy_jpeg(jpeg, 2048);
        send_bytes(jpeg, cid, bid);
    };

    handlers["webcam_capture"] = [&](const auto& a, uint32_t cid, uint32_t bid) {
        int idx = (a.size() >= 2) ? to_int(a[1], webcam_index) : webcam_index;
        int w=0,h=0;
        auto jpeg = capture_webcam_frame(idx, w, h);
        if (jpeg.empty()) {
            Monitor m;
            jpeg = m.capture_frame();
            if (jpeg.empty()) make_dummy_jpeg(jpeg, 2048);
        }
        send_bytes(jpeg, cid, bid);
    };

    handlers["start_webcam_stream"] = [&](const auto& a, uint32_t cid, uint32_t bid) {
        if (webcam_streaming.exchange(true)) return (void)send_text("ERROR: Webcam already streaming", cid, bid);

        // Stop screen stream if active (exclusive mode)
        if (screen_streaming.load()) stop_screen();

        int idx = (a.size() >= 2) ? to_int(a[1], webcam_index) : webcam_index;
        webcam_index = idx;

        stop_webcam();
        webcam_cam = std::make_unique<Webcam>(webcam_index);

        // We don't explicit open() because stream_h264 uses ffmpeg which handles it.
        // But we can check if device node exists? For now just try stream.

        send_text("OK: Webcam streaming started", cid, bid);
        webcam_streaming.store(true);

        webcam_thread = std::thread([&, cid, bid]() {
            if (!webcam_cam) webcam_cam = std::make_unique<Webcam>(webcam_index);

            webcam_cam->stream_h264([&](const std::vector<uint8_t>& chunk) {
                if (!alive.load() || !webcam_streaming.load()) return false;
                return send_bytes(chunk, cid, bid);
            });

            webcam_streaming.store(false);
            send_text("INFO: Webcam stopped", cid, bid);
        });
    };

    handlers["stop_webcam_stream"] = [&](const auto&, uint32_t cid, uint32_t bid) {
        if (!webcam_streaming.load()) return (void)send_text("ERROR: Webcam not streaming", cid, bid);
        stop_webcam();
        send_text("OK: Webcam streaming stopped", cid, bid);
    };

    handlers["list_process"] = [&](const auto&, uint32_t cid, uint32_t bid) {
        std::ostringstream oss;
        Process::print_all(oss);
        auto out = oss.str();
        if (out.empty()) send_text("ERROR: Failed to list processes", cid, bid);
        else send_text(out, cid, bid);
    };

    handlers["ping"] = [&](const auto&, uint32_t cid, uint32_t bid) { send_text("PONG", cid, bid); };
    handlers["shutdown"] = [&](const auto&, uint32_t cid, uint32_t bid) { send_text("OK: Shutdown requested", cid, bid); };

    handlers["kill_process"] = [&](const auto& a, uint32_t cid, uint32_t bid) {
        if (a.size() < 2) return (void)send_text("ERROR: Missing PID", cid, bid);
        int pid = to_int(a[1], -1);
        if (pid <= 0) return (void)send_text("ERROR: Invalid PID", cid, bid);

        Process p(pid);
        if (p.destroy() == 0) send_text("OK: Process killed " + std::to_string(pid), cid, bid);
        else {
            std::string err = "ERROR: Failed to kill process " + std::to_string(pid) + " (" + strerror(errno) + ")";
            send_text(err, cid, bid);
        }
    };

    while (running_.load()) {
        payload.clear();
        if (!readFrame(fd, payload, client_id, backend_id)) break;

        std::string cmd;
        if (!payload.empty()) cmd.assign((char*)payload.data(), payload.size());
        auto z = cmd.find('\0');
        if (z != std::string::npos) cmd.resize(z);

        auto parts = split_ws(cmd);
        if (parts.empty()) continue;

        auto it = handlers.find(parts[0]);
        if (it == handlers.end()) {
            send_text("Unknown command", client_id, backend_id);
            continue;
        }
        it->second(parts, client_id, backend_id);
    }

    alive.store(false);
    stop_screen();
    stop_webcam();
    close(fd);
    std::cout << "[BackendServer] Connection closed fd=" << fd << std::endl;
}
