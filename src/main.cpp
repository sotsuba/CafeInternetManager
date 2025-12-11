/**
 * CafeInternetManager Server
 * 
 * A WebSocket-based remote monitoring server following SOLID principles:
 * 
 * - Single Responsibility: Each class has one reason to change
 * - Open/Closed: New commands can be added without modifying existing code
 * - Liskov Substitution: Interfaces can be substituted with any implementation
 * - Interface Segregation: Small, focused interfaces
 * - Dependency Inversion: High-level modules depend on abstractions
 * 
 * Architecture:
 * 
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │                     ApplicationBuilder                      │
 *   │  (Dependency Injection Container)                           │
 *   └──────────────────────────┬──────────────────────────────────┘
 *                              │
 *              ┌───────────────┼───────────────┐
 *              ▼               ▼               ▼
 *   ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
 *   │    Server    │  │CommandRegistry│  │   ILogger    │
 *   │ (TCP Accept) │  │  (Dispatch)   │  │  (Logging)   │
 *   └──────┬───────┘  └──────┬────────┘  └──────────────┘
 *          │                 │
 *          ▼                 │
 *   ┌──────────────┐         │
 *   │WebSocketSession│◄───────┘
 *   │(Per-client)   │
 *   └──────┬────────┘
 *          │
 *          ▼
 *   ┌──────────────────────────────────────────────────────────┐
 *   │                    ICommandHandler                       │
 *   │  ┌─────────────┬─────────────┬─────────────┬──────────┐  │
 *   │  │CaptureWebcam│StartStream  │ Keylogger   │Shutdown  │  │
 *   │  │CaptureScreen│StopStream   │ ListProcess │  ...     │  │
 *   │  └─────────────┴─────────────┴─────────────┴──────────┘  │
 *   └──────────────────────────────────────────────────────────┘
 *          │
 *          ▼
 *   ┌──────────────────────────────────────────────────────────┐
 *   │                      Services                            │
 *   │  ┌─────────────┬─────────────┬─────────────┬──────────┐  │
 *   │  │ICaptureDevice│IStreamable │IKeyboardLis.│IProcessMgr│ │
 *   │  │WebcamCapture │Streaming   │LinuxKeyboard│LinuxProc │  │
 *   │  │ScreenCapture │ Service    │  Listener   │ Manager  │  │
 *   │  └─────────────┴─────────────┴─────────────┴──────────┘  │
 *   └──────────────────────────────────────────────────────────┘
 */

#include "app/application.hpp"
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        std::cerr << "Example: " << argv[0] << " 9004\n";
        return 1;
    }

    try {
        uint16_t port = static_cast<uint16_t>(std::stoi(argv[1]));
        
        // Build application with all dependencies wired up
        cafe::ApplicationBuilder builder;
        auto server = builder.build(port);
        
        // Run the server
        server->run();
        
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
