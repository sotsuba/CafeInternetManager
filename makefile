
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
INCLUDES := -Ibackend -Iinclude

SRC_DIR := backend
OBJ_DIR := build
BIN := server

# Tìm tất cả file .cpp trong src/
SRC := $(shell find $(SRC_DIR) -name "*.cpp")
OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC))

all: $(BIN)

$(BIN): $(OBJ_DIR)/api/monitor.o \
	                   $(OBJ_DIR)/api/keylogger.o \
	                   $(OBJ_DIR)/api/keylogger_find.o \
	                   $(OBJ_DIR)/api/keylogger_lifecycle.o \
	                   $(OBJ_DIR)/api/keylogger_utils.o \
	                   $(OBJ_DIR)/api/webcam.o \
	                   $(OBJ_DIR)/api/process.o \
	                   $(OBJ_DIR)/api/application.o \
	                   $(OBJ_DIR)/net/backend_server.o \
	                   $(OBJ_DIR)/main.o
	$(CXX) $(CXXFLAGS) $(OBJ_DIR)/api/monitor.o \
	                   $(OBJ_DIR)/api/keylogger.o \
	                   $(OBJ_DIR)/api/keylogger_find.o \
	                   $(OBJ_DIR)/api/keylogger_lifecycle.o \
	                   $(OBJ_DIR)/api/keylogger_utils.o \
	                   $(OBJ_DIR)/api/webcam.o \
	                   $(OBJ_DIR)/api/process.o \
	                   $(OBJ_DIR)/api/application.o \
	                   $(OBJ_DIR)/net/backend_server.o \
	                   $(OBJ_DIR)/main.o \
	                   -o $(BIN) $(INCLUDES) -lpthread -lX11 -lXtst

# Rule build .o từ .cpp
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all clean
