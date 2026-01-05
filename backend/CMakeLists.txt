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
    src/core/GatewayProtocolV1.cpp
    src/core/GatewayConnection.cpp
    src/core/CommandDispatcher.cpp
    src/core/PlatformRegistry.cpp
    src/core/network/TcpSocket.cpp
    src/core/network/PacketDispatcher.cpp
    src/handlers/MouseCommandHandler.cpp
    src/handlers/StreamCommandHandler.cpp
    src/handlers/KeyloggerCommandHandler.cpp
    src/handlers/AppCommandHandler.cpp
    src/handlers/FileCommandHandler.cpp
)

# 2. Platform HAL
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    add_compile_definitions(PLATFORM_LINUX)
    add_library(backend_hal
        src/platform/linux/LinuxX11Streamer.cpp
        src/platform/linux/LinuxPipeWireStreamer.cpp
        src/platform/linux/LinuxEvdevLogger.cpp
        src/platform/linux/LinuxAppManager.cpp
        src/platform/linux/LinuxWebcamStreamer.cpp
        src/platform/linux/LinuxXTestInjector.cpp
        src/platform/linux/LinuxUInputInjector.cpp
        src/platform/linux/LinuxInputInjectorFactory.cpp
        src/platform/linux/LinuxFileTransfer.cpp
        src/platform/linux/LinuxPlatformFactory.cpp
    )
    find_package(Threads REQUIRED)
    find_package(X11 REQUIRED)
    target_link_libraries(backend_hal PRIVATE Threads::Threads)
    target_link_libraries(backend_hal PRIVATE X11::X11 X11::Xtst)
elseif(WIN32)
    add_compile_definitions(PLATFORM_WINDOWS)

    # Static linking for MinGW to avoid DLL dependencies (libstdc++, libgcc, libwinpthread)
    if(MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static -static-libgcc -static-libstdc++")
    endif()

    add_library(backend_hal
        src/platform/windows/WindowsScreenStreamer.cpp
        src/platform/windows/WindowsWebcamStreamer.cpp
        src/platform/windows/WindowsKeylogger.cpp
        src/platform/windows/WindowsAppManager.cpp
        src/platform/windows/WindowsInputInjector.cpp
        src/platform/windows/WindowsFileTransfer.cpp
        src/platform/windows/WindowsPlatformFactory.cpp
    )
    # Winsock + Shell32 (for SHCreateDirectoryEx)
    target_link_libraries(backend_hal PRIVATE ws2_32 shell32)
elseif(APPLE)
    # macOS Platform HAL
    add_compile_definitions(PLATFORM_MACOS)

    # Enable Objective-C++ for .mm files
    enable_language(OBJCXX)

    add_library(backend_hal
        src/platform/macos/MacOSInputInjector.mm
        src/platform/macos/MacOSScreenStreamer.mm
        src/platform/macos/MacOSWebcamStreamer.mm
        src/platform/macos/MacOSKeylogger.mm
        src/platform/macos/MacOSAppManager.mm
        src/platform/macos/MacOSFileTransfer.cpp
        src/platform/macos/MacOSPlatformFactory.mm
    )

    # Find and link macOS frameworks
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(QUARTZ_LIBRARY Quartz REQUIRED)
    find_library(AVFOUNDATION_LIBRARY AVFoundation REQUIRED)
    find_library(COREGRAPHICS_LIBRARY CoreGraphics REQUIRED)
    find_library(CARBON_LIBRARY Carbon REQUIRED)
    find_library(APPKIT_LIBRARY AppKit REQUIRED)

    target_link_libraries(backend_hal PRIVATE
        ${COCOA_LIBRARY}
        ${QUARTZ_LIBRARY}
        ${AVFOUNDATION_LIBRARY}
        ${COREGRAPHICS_LIBRARY}
        ${CARBON_LIBRARY}
        ${APPKIT_LIBRARY}
    )

    # Set compile options for Objective-C++
    set_source_files_properties(
        src/platform/macos/MacOSInputInjector.mm
        src/platform/macos/MacOSScreenStreamer.mm
        src/platform/macos/MacOSWebcamStreamer.mm
        src/platform/macos/MacOSKeylogger.mm
        src/platform/macos/MacOSAppManager.mm
        src/platform/macos/MacOSPlatformFactory.mm
        PROPERTIES COMPILE_FLAGS "-x objective-c++"
    )
else()
    # Mock / Test Mode
    add_compile_definitions(PLATFORM_MOCK)
    # No HAL target created, main.cpp must use Mocks
endif()

# 3. Executable
add_executable(BackendServer src/main.cpp)

if(TARGET backend_hal)
    if(WIN32)
        target_link_libraries(BackendServer PRIVATE backend_core backend_hal ws2_32 iphlpapi)
    else()
        target_link_libraries(BackendServer PRIVATE backend_core backend_hal pthread)
    endif()
else()
    target_link_libraries(BackendServer PRIVATE backend_core)
endif()

# 4. Test Executable (Windows only)
if(WIN32 AND TARGET backend_hal)
    add_executable(BackendTest src/tests/WindowsPlatformTest.cpp)
    target_include_directories(BackendTest PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/platform/windows
    )
    target_link_libraries(BackendTest PRIVATE backend_hal ws2_32)
endif()

# 5. Test Executable (Linux only)
if(CMAKE_SYSTEM_NAME MATCHES "Linux" AND TARGET backend_hal)
    add_executable(BackendTest src/tests/LinuxPlatformTest.cpp)
    target_include_directories(BackendTest PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/platform/linux
    )
    target_link_libraries(BackendTest PRIVATE backend_hal backend_core pthread X11 Xtst)
endif()

# 6. Test Executable (macOS only)
if(APPLE AND TARGET backend_hal)
    add_executable(BackendTest src/tests/MacOSPlatformTest.mm)
    target_include_directories(BackendTest PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/platform/macos
    )
    target_link_libraries(BackendTest PRIVATE backend_hal backend_core
        ${COCOA_LIBRARY}
        ${QUARTZ_LIBRARY}
        ${AVFOUNDATION_LIBRARY}
        ${COREGRAPHICS_LIBRARY}
        ${CARBON_LIBRARY}
        ${APPKIT_LIBRARY}
    )
endif()
