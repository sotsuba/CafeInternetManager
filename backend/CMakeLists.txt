cmake_minimum_required(VERSION 3.10)
project(CafeInternetManagerBackend)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(.)

# Source files
file(GLOB_RECURSE SOURCES
    "main.cpp"
    "api/*.cpp"
    "net/*.cpp"
    "util/*.cpp"
)

# Create executable
add_executable(BackendServer ${SOURCES})

# Link libraries
# - pthread is essential for std::thread support on Linux
target_link_libraries(BackendServer pthread stdc++fs)

# Optional: Add compiler warnings
if(MSVC)
    target_compile_options(BackendServer PRIVATE /W4)
else()
    target_compile_options(BackendServer PRIVATE -Wall -Wextra)
endif()
