CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
INCLUDES := -Isrc -Iinclude

SRC_DIR := src
BIN := server

# Tìm tất cả file .cpp trong src/
SRC := $(shell find $(SRC_DIR) -name "*.cpp")
OBJ := $(SRC:.cpp=.o)

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(BIN) $(INCLUDES)

# Rule build .o từ .cpp
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all clean
