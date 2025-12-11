#include <functional>
#include <iostream>
#include "net/websocket_server.h"

using WSHandler = std::function<void(WebSocketConnection &)>;

class WebSocketRouter {
public:
  void add_route(const std::string &path, WSHandler handler);
  WSHandler match(const std::string &path) const;
};

int main(int argc, char **argv) {
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <port>\n";
		return 1;
	}
  	
	try {
    	WebSocketServer server(std::stoi(argv[1]));
    	server.run();
	} catch (const std::exception &ex) {
		std::cerr << "Fatal error: " << ex.what() << "\n";
		return 1;
  	}
  	return 0;
}
