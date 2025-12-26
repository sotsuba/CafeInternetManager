cmake_minimum_required(VERSION 3.10)
project(UniversalAgent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include Directories
include_directories(include src src/core/network)

# 1. Core Library (Business Logic)
add_library(backend_core
    src/core/BackendServer.cpp
    src/core/BroadcastBus.cpp
    src/core/StreamSession.cpp
    src/core/network/TcpSocket.cpp
    src/core/network/PacketDispatcher.cpp
)

# 2. Platform HAL
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_compile_definitions(PLATFORM_LINUX)
    add_library(backend_hal
        src/platform/linux/LinuxX11Streamer.cpp
        src/platform/linux/LinuxEvdevLogger.cpp
        src/platform/linux/LinuxAppManager.cpp
        src/platform/linux/LinuxWebcamStreamer.cpp
        src/platform/linux/LinuxXTestInjector.cpp
        src/platform/linux/LinuxUInputInjector.cpp
        src/platform/linux/LinuxInputInjectorFactory.cpp
    )
    find_package(Threads REQUIRED)
    find_package(X11 REQUIRED)
    target_link_libraries(backend_hal PRIVATE Threads::Threads)
    target_link_libraries(backend_hal PRIVATE X11::X11 X11::Xtst)
elseif(WIN32)
    add_compile_definitions(PLATFORM_WINDOWS)
    add_library(backend_hal
        src/platform/windows/WindowsScreenStreamer.cpp
        src/platform/windows/WindowsWebcamStreamer.cpp
        src/platform/windows/WindowsKeylogger.cpp
        src/platform/windows/WindowsAppManager.cpp
        src/platform/windows/WindowsInputInjector.cpp
    )
    # Winsock
    target_link_libraries(backend_hal PRIVATE ws2_32)
else()
    # Mock / Test Mode
    add_compile_definitions(PLATFORM_MOCK)
    # No HAL target created, main.cpp must use Mocks
endif()

# 3. Executable
add_executable(BackendServer src/main.cpp)

if(TARGET backend_hal)
    if(WIN32)
        target_link_libraries(BackendServer PRIVATE backend_core backend_hal ws2_32)
    else()
        target_link_libraries(BackendServer PRIVATE backend_core backend_hal pthread)
    endif()
else()
    target_link_libraries(BackendServer PRIVATE backend_core)
endif()
