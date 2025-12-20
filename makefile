CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
INCLUDES := -Isrc -Iinclude

SRC_DIR := src
OBJ_DIR := build
BIN := server

# Tìm tất cả file .cpp trong src/
SRC := $(shell find $(SRC_DIR) -name "*.cpp")
OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC))

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(BIN) $(INCLUDES)

# Rule build .o từ .cpp
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all clean
