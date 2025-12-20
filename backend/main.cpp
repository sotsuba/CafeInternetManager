#include <functional>
#include <iostream>

#include "net/websocket_server.h"
#include "net/backend_server.h"

using WSHandler = std::function<void(WebSocketConnection &)>;

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <port>\n";
		return 1;
	}
	int port = std::stoi(argv[1]);
	try {
		BackendServer backend(port);
		std::thread backend_thread([&]() { backend.run(); });
		if (backend_thread.joinable()) backend_thread.join();
	} catch (const std::exception &ex) {
		std::cerr << "Fatal error: " << ex.what() << "\n";
		return 1;
	}
	return 0;
}
