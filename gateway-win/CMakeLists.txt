cmake_minimum_required(VERSION 3.10)
project(GatewayWin VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Source files
set(SOURCES
    src/main.c
    src/thread.c
    src/websocket.c
    src/mempool.c
    src/queue.c
    src/utils.c
    src/backend.c
    src/discovery.c
)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Windows-specific settings
if(WIN32)
    # Static linking for MinGW to avoid DLL dependencies (libgcc, libwinpthread)
    if(MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static -static-libgcc")
    endif()

    # Link Winsock
    target_link_libraries(${PROJECT_NAME} ws2_32)

    # Disable deprecation warnings
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
    )

    # Enable multi-threaded runtime
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W3)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()
