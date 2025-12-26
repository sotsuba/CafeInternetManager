# Stage 1: Build
FROM ubuntu:22.04 as builder

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install Build Tools
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Build
RUN mkdir build && cd build && cmake .. && make -j$(nproc)

# Stage 2: Runtime
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install Runtime Dependencies
# ffmpeg: for screen capture (if running in containerized display or mapped)
# x11-utils: for xrandr
RUN apt-get update && apt-get install -y \
    ffmpeg \
    x11-xserver-utils \
    libevdev2 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy binary from builder
COPY --from=builder /app/build/BackendServer /app/BackendServer
# Copy setup/config scripts if any
COPY setup_linux.sh /app/

# Expose Port
EXPOSE 8882

# Entrypoint
# Note: For screen capture to work, this container must be run with:
# --net=host --privileged -v /tmp/.X11-unix:/tmp/.X11-unix -e DISPLAY=$DISPLAY
CMD ["./BackendServer"]
