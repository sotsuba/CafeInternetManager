# CafeInternetManager Server - Makefile
# SOLID-compliant C++17 WebSocket server

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
INCLUDES := -I src

# Source files
SRCS := src/main.cpp
OBJS := $(SRCS:.cpp=.o)

# Output
TARGET := server

# Default target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET) $(INCLUDES) -lpthread

# Compile source files
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET)

# Rebuild
rebuild: clean all

# Run the server (default port 9004)
run: $(TARGET)
	sudo -E ./$(TARGET) 9004

# Development mode with debug symbols
debug: CXXFLAGS += -g -DDEBUG
debug: rebuild

# Format code (requires clang-format)
format:
	find src -name "*.hpp" -o -name "*.cpp" | xargs clang-format -i

# Show project structure
tree:
	@echo "Project Structure:"
	@echo "src/"
	@echo "├── main.cpp              # Application entry point"
	@echo "├── app/"
	@echo "│   └── application.hpp   # DI container & app builder"
	@echo "├── core/"
	@echo "│   ├── interfaces.hpp    # All interface definitions"
	@echo "│   └── logger.hpp        # Logger implementations"
	@echo "├── capture/"
	@echo "│   ├── webcam_capture.hpp"
	@echo "│   └── screen_capture.hpp"
	@echo "├── commands/"
	@echo "│   ├── command_registry.hpp"
	@echo "│   └── handlers.hpp      # All command handlers"
	@echo "├── services/"
	@echo "│   ├── streaming_service.hpp"
	@echo "│   ├── keyboard_service.hpp"
	@echo "│   └── system_service.hpp"
	@echo "└── net/"
	@echo "    ├── server.hpp        # TCP server"
	@echo "    ├── websocket_session.hpp"
	@echo "    └── websocket_protocol.hpp"

.PHONY: all clean rebuild run debug format tree
