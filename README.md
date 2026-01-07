# 🖥️ CafeInternetManager

<div align="center">

**Hệ thống quản lý và giám sát từ xa cho quán net và phòng máy tính**

[![C++17](https://img.shields.io/badge/C++-17-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/17)
[![TypeScript](https://img.shields.io/badge/TypeScript-5.0-3178C6?logo=typescript&logoColor=white)](https://www.typescriptlang.org/)
[![React](https://img.shields.io/badge/React-18-61DAFB?logo=react&logoColor=black)](https://reactjs.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey)](.)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

</div>

---

## 📋 Tổng Quan

CafeInternetManager là giải pháp hoàn chỉnh để quản lý từ xa các máy tính trong quán net hoặc phòng lab. Hệ thống bao gồm 3 thành phần chính:

| Thành phần | Công nghệ | Mô tả |
|:-----------|:----------|:------|
| **Backend** | C++17 | Agent chạy trên máy client, xử lý lệnh và streaming |
| **Gateway** | C | Relay server trung gian, định tuyến WebSocket |
| **Frontend** | React + TypeScript + Vite | Giao diện quản trị web |

---

## ✨ Tính Năng

### 🎥 Streaming & Giám Sát
- **Live Screen Streaming** — Xem màn hình realtime với H.264 encoding
- **Webcam Streaming** — Stream camera trực tiếp
- **Screen Recording** — Ghi lại màn hình và tải về
- **Snapshot Capture** — Chụp ảnh màn hình/webcam

### 💻 Quản Lý Hệ Thống
- **Process Manager** — Xem danh sách tiến trình (CPU, RAM usage), kill process
- **Remote Control** — Input chuột/bàn phím từ xa
- **System Control** — Shutdown, restart, lock máy từ xa
- **File Explorer** — Duyệt và tải file trên máy client

### 🔌 Kết Nối & Tiện Ích
- **Auto-Discovery** — Backend quảng bá UDP, Gateway tự phát hiện
- **Persistent Connection** — Duy trì kết nối WebSocket ổn định
- **Multi-client Support** — Quản lý nhiều máy cùng lúc

---

## 🏛️ Kiến Trúc Hệ Thống

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           FRONTEND (Web UI)                             │
│              React 18 + TypeScript + Vite + TailwindCSS                 │
│                         http://localhost:5173                           │
└────────────────────────────────┬────────────────────────────────────────┘
                                 │ WebSocket (Port 9002)
┌────────────────────────────────▼────────────────────────────────────────┐
│                            GATEWAY (Relay)                              │
│                   C • Multi-threaded • Traffic Routing                  │
│                                                                         │
│   ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌───────────────┐  │
│   │ WS Listener │  │ UDP Listener│  │ Thread Pool │  │ Traffic Class │  │
│   │   :9002     │  │   :9003     │  │             │  │   Router      │  │
│   └─────────────┘  └─────────────┘  └─────────────┘  └───────────────┘  │
└────────────────────────────────┬────────────────────────────────────────┘
                                 │ TCP (Port 9001)
┌────────────────────────────────▼────────────────────────────────────────┐
│                           BACKEND (Agent)                               │
│                        C++17 • Cross-platform                           │
│                                                                         │
│   ┌─────────────────────────────────────────────────────────────────┐   │
│   │                     Command Handlers                            │   │
│   │      Stream • Webcam • Process • File • System • Keylogger      │   │
│   └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 🚀 Cài Đặt & Build

### Yêu Cầu

| Component | Requirements |
|:----------|:-------------|
| **Backend** | C++17 compiler (MinGW/GCC), CMake 3.16+, FFmpeg |
| **Gateway** | C compiler (GCC), CMake |
| **Frontend** | Node.js 18+, npm |

### Build Backend

```bash
cd backend

# Windows (MinGW)
mkdir build_win && cd build_win
cmake -G "MinGW Makefiles" ..
cmake --build . --config Release

# Linux
mkdir build_linux && cd build_linux
cmake ..
make -j$(nproc)
```

### Build Gateway (Linux)

```bash
cd gateway
mkdir build && cd build
cmake ..
make
```

### Setup Frontend

```bash
cd "Design New Front-End"
npm install
npm run dev
```

---

## 📖 Sử Dụng

### 1. Khởi động Gateway (trên server)

```bash
./run_gateway.sh
# Hoặc: ./gateway 9002 9003 9001
```

### 2. Khởi động Backend (trên máy client)

```bash
# Windows
./backend.exe

# Linux (cần sudo cho một số tính năng)
sudo ./backend
```

### 3. Mở Frontend

```bash
cd "Design New Front-End"
npm run dev
# Mở http://localhost:5173
```

### 4. Wake-on-LAN

```bash
python wol.py AA:BB:CC:DD:EE:FF      # Wake một máy
python wol.py --all                   # Wake tất cả từ machines.json
python wol.py --list                  # Liệt kê máy đã cấu hình
```

---

## 📁 Cấu Trúc Dự Án

```
CafeInternetManager/
├── backend/                    # C++17 Backend Agent
│   ├── include/                # Header files
│   ├── src/
│   │   ├── core/               # Server, CommandRegistry, StreamSession
│   │   ├── handlers/           # Command handlers
│   │   ├── platform/
│   │   │   ├── windows/        # Windows implementations
│   │   │   └── linux/          # Linux implementations
│   │   └── main.cpp
│   └── CMakeLists.txt
├── gateway/                    # C Gateway (Linux)
│   ├── include/
│   ├── src/
│   └── CMakeLists.txt
├── Design New Front-End/       # React Frontend
│   ├── src/
│   │   ├── app/
│   │   │   ├── components/     # UI Components
│   │   │   ├── pages/          # Page components
│   │   │   └── services/       # WebSocket client
│   │   └── styles/
│   └── package.json
├── machines.json               # WOL configuration
├── docker-compose.yml          # Docker deployment
├── setup_linux.sh              # Linux setup script
├── run_gateway.sh              # Gateway startup script
├── LICENSE                     # MIT License
└── README.md
```

---

## 🔧 WebSocket Commands

| Command | Mô tả |
|:--------|:------|
| `ping` | Health check |
| `start_screen_stream` | Bắt đầu stream màn hình |
| `start_webcam_stream` | Bắt đầu stream webcam |
| `stop_stream` | Dừng stream |
| `capture_screen` | Chụp ảnh màn hình |
| `capture_webcam` | Chụp ảnh webcam |
| `start_recording` | Bắt đầu ghi hình |
| `stop_recording` | Dừng ghi và gửi file |
| `list_process` | Liệt kê tiến trình |
| `kill_process:<PID>` | Kill process |
| `launch_process:<path>` | Khởi chạy ứng dụng |
| `list_directory:<path>` | Liệt kê file/folder |
| `download_file:<path>` | Tải file về |
| `shutdown` / `restart` / `lock` | Điều khiển hệ thống |

---

## 🔒 Lưu Ý Bảo Mật

> ⚠️ Hệ thống được thiết kế cho **mạng nội bộ (LAN)**. Không nên expose ra Internet mà không có các biện pháp bảo mật bổ sung (VPN, firewall, authentication).

---

## 📄 License

Dự án được phân phối dưới giấy phép **MIT License**. Xem file [LICENSE](LICENSE) để biết thêm chi tiết.

---

## 👥 Đóng Góp

1. Fork repository
2. Tạo feature branch (`git checkout -b feature/new-feature`)
3. Commit changes (`git commit -m 'Add new feature'`)
4. Push to branch (`git push origin feature/new-feature`)
5. Mở Pull Request

---

<div align="center">

**Made with ❤️ for Internet Cafe Management**

</div>
