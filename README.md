# CafeInternetManager

A remote monitoring and management system for internet cafes and computer labs. Features a **C++17 WebSocket server** running on client machines and a **TypeScript/Vite frontend** for administrators.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C.svg)
![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)

## Features

- 📹 **Live Webcam Streaming** — Real-time video from client machines
- 🖥️ **Screen Capture** — Remote screen monitoring with multiple backend support
- ⌨️ **Keyboard Logger** — Monitor keystrokes (requires root)
- 📊 **Process Manager** — List and kill processes remotely
- ⚡ **System Control** — Remote shutdown/restart
- 🌐 **Wake-on-LAN** — Power on machines remotely
- 🔌 **Persistent Connections** — Maintain WebSocket connections for thumbnails

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      Frontend (Web UI)                          │
│            TypeScript/Vite • WebSocket Client                   │
└────────────────────────────┬────────────────────────────────────┘
                             │ ws://host:9004
┌────────────────────────────▼────────────────────────────────────┐
│                    C++ WebSocket Server                         │
│  ┌──────────────┐  ┌─────────────────┐  ┌───────────────────┐  │
│  │  TcpListener │→ │ WebSocketSession│→ │ CommandRegistry   │  │
│  └──────────────┘  └─────────────────┘  └─────────┬─────────┘  │
│                                                    │            │
│  ┌─────────────────────────────────────────────────▼─────────┐ │
│  │                   Command Handlers                         │ │
│  │  Capture • Stream • Keylogger • Process • System          │ │
│  └───────────────────────────────────────────────────────────┘ │
│  ┌───────────────────────────────────────────────────────────┐ │
│  │                      Services                              │ │
│  │  WebcamCapture • ScreenCapture • KeyboardListener         │ │
│  └───────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### SOLID Principles

The backend follows **SOLID OOP principles**:

| Principle | Implementation |
|-----------|----------------|
| **S**ingle Responsibility | Each class has one job (e.g., `WebcamCapture` only captures) |
| **O**pen/Closed | New commands via `CommandRegistry` without modifying code |
| **L**iskov Substitution | All implementations are substitutable for interfaces |
| **I**nterface Segregation | Small interfaces (`IMessageSender`, `ICaptureDevice`) |
| **D**ependency Inversion | `ApplicationBuilder` injects all dependencies |

## Requirements

### Server (C++)

- Linux (uses `epoll`, `/dev/input`, v4l2)
- C++17 compiler (g++)
- pthread
- ffmpeg (for capture)

```bash
# Fedora
sudo dnf install gcc-c++ ffmpeg

# Ubuntu/Debian
sudo apt install g++ ffmpeg
```

### Frontend

- Node.js 18+
- npm or pnpm

### Optional (Screen Capture Backends)

```bash
# X11
sudo dnf install scrot ImageMagick

# Wayland (wlroots)
sudo dnf install grim

# GNOME Wayland (limited support)
# gnome-screenshot is usually pre-installed
```

## Installation

### Build Server

```bash
# Clone repository
git clone https://github.com/yourusername/CafeInternetManager.git
cd CafeInternetManager

# Build
make

# Or with debug symbols
make debug
```

### Setup Frontend

```bash
cd frontend
npm install
```

## Usage

### Start Server

```bash
# Requires sudo for keylogger and system control
sudo -E ./server 9004
```

Or use the make target:

```bash
make run
```

### Start Frontend

```bash
cd frontend
npm run dev
```

Open `http://localhost:5173` in your browser.

### Wake-on-LAN

```bash
# Wake a single machine
./wol.py AA:BB:CC:DD:EE:FF

# Wake all machines from machines.json
./wol.py --all

# List configured machines
./wol.py --list
```

## WebSocket Commands

| Command | Description |
|---------|-------------|
| `capture_webcam` | Capture single webcam frame (JPEG) |
| `capture_screen` | Capture single screen frame (PNG) |
| `start_webcam_stream` | Start webcam stream at 30 FPS |
| `start_screen_stream` | Start screen stream at 30 FPS |
| `stop_stream` | Stop active stream |
| `start_keylogger` | Start keyboard monitoring |
| `stop_keylogger` | Stop keyboard monitoring |
| `list_process` | List top 50 processes by memory |
| `kill_process:<PID>` | Kill process by PID |
| `shutdown` | Shutdown remote machine |
| `restart` | Restart remote machine |

## Project Structure

```
CafeInternetManager/
├── src/
│   ├── main.cpp                 # Entry point
│   ├── app/
│   │   └── application.hpp      # DI container
│   ├── core/
│   │   ├── interfaces.hpp       # Abstract interfaces
│   │   └── logger.hpp           # Logger implementations
│   ├── capture/
│   │   ├── webcam_capture.hpp   # Webcam via ffmpeg
│   │   └── screen_capture.hpp   # Multi-backend screen capture
│   ├── commands/
│   │   ├── command_registry.hpp # Command routing
│   │   └── handlers.hpp         # Command handlers
│   ├── services/
│   │   ├── streaming_service.hpp
│   │   ├── keyboard_service.hpp
│   │   └── system_service.hpp
│   └── net/
│       ├── server.hpp           # TCP server
│       ├── websocket_session.hpp
│       └── websocket_protocol.hpp
├── frontend/                    # TypeScript/Vite frontend
├── Makefile
├── machines.json               # WOL configuration
└── wol.py                      # Wake-on-LAN script
```

## Screen Capture Backends

The server auto-detects the best available backend:

| Environment | Backend | Status |
|-------------|---------|--------|
| X11 | `scrot`, `import`, `ffmpeg` | ✅ Reliable |
| Wayland (wlroots) | `grim` | ✅ Reliable |
| Wayland (GNOME) | `gnome-screenshot` | ⚠️ May produce black frames |

> **Note**: GNOME Wayland restricts screen capture for security. Webcam streaming works reliably as an alternative.

## Configuration

### machines.json

```json
{
  "machines": [
    {
      "name": "PC-01",
      "mac": "AA:BB:CC:DD:EE:01",
      "ip": "192.168.1.101",
      "port": 9004
    }
  ]
}
```

## Known Limitations

1. **GNOME Wayland**: Screen capture may fail due to security restrictions
2. **Keylogger**: Requires root privileges to read `/dev/input` devices
3. **Linux-only**: Uses Linux-specific APIs (epoll, v4l2, proc filesystem)

## Development

```bash
# Rebuild
make rebuild

# Clean
make clean

# Show project structure
make tree

# Format code (requires clang-format)
make format
```

## License

MIT License - See [LICENSE](LICENSE) for details.

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing`)
5. Open a Pull Request