# Deployment & Testing Manual

**Version:** 1.0.0
**Target OS:** Linux (Ubuntu 20.04/22.04), Windows (Future)

This guide provides step-by-step instructions to deploy, test, and package the Universal Client Agent.

---

## Part 1: Testing on Ubuntu VM (The "Lab" Test)

**Goal:** Verify the new Backend Core and Linux HAL work on a real Linux environment.

### 1. Transfer Code
Copy the `backend/` folder from Windows to your Ubuntu VM.

### 2. Setup Environment
Open a terminal inside `backend/` on Ubuntu:
```bash
chmod +x setup_linux.sh
./setup_linux.sh
```
This script installs CMake, FFmpeg, and builds the project.

### 3. Run and Verify
```bash
./build/BackendServer
```
Open your Web Frontend. It should show the VM's screen.

---

## Part 2: Docker Packaging

**Goal:** Release as a container.

### 1. Build Image
```bash
cd backend
docker build -t cafe-agent:latest .
```

### 2. Run Container (Privileged)
Requires Host Screen Access:
```bash
docker run -d --name cafe-agent \
  --net=host --privileged \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  -v /dev/input:/dev/input \
  cafe-agent:latest
```

---

## Part 3: Full Stack Test
Use `docker-compose.yml` in the root:
```bash
docker-compose up --build
```
This spins up the Gateway and Agent together.
