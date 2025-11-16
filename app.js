let ws = null;
const wsUrlInput     = document.getElementById("wsUrl");
const connectBtn     = document.getElementById("connectBtn");
const disconnectBtn  = document.getElementById("disconnectBtn");
const sendBtn        = document.getElementById("sendBtn");
const messageInput   = document.getElementById("messageInput");
const statusSpan     = document.getElementById("status");
const logDiv         = document.getElementById("log");
const canvas         = document.getElementById("screenCanvas");
const ctx            = canvas.getContext("2d");

// Stats elements
const fpsValue = document.getElementById("fpsValue");
const frameValue = document.getElementById("frameValue");
const dataValue = document.getElementById("dataValue");

// Stats tracking
let frameCount = 0;
let totalBytes = 0;
let lastFrameTime = 0;
let fps = 0;

function log(message) {
  const time = new Date().toISOString().split("T")[1].split(".")[0];
  logDiv.textContent += `[${time}] ${message}\n`;
  logDiv.scrollTop = logDiv.scrollHeight;
}

function setStatus(text, isConnected) {
  statusSpan.textContent = text;
  if (isConnected) {
    statusSpan.className = "status-connected";
  } else {
    statusSpan.className = "status-disconnected";
  }
}

function updateStats() {
  fpsValue.textContent = fps;
  frameValue.textContent = frameCount;
  dataValue.textContent = (totalBytes / 1024 / 1024).toFixed(2) + " MB";
}

connectBtn.addEventListener("click", () => {
  const url = wsUrlInput.value.trim();
  if (!url) {
    alert("Please enter WebSocket URL");
    return;
  }
  if (ws && ws.readyState === WebSocket.OPEN) {
    log("⚠️ Already connected.");
    return;
  }

  log(`🔄 Connecting to ${url}...`);
  ws = new WebSocket(url);

  // Receive binary as ArrayBuffer
  ws.binaryType = "arraybuffer";

  ws.onopen = () => {
    log("✅ Connected successfully!");
    setStatus("CONNECTED", true);
    connectBtn.disabled = true;
    disconnectBtn.disabled = false;
    sendBtn.disabled = false;

    // Reset stats
    frameCount = 0;
    totalBytes = 0;
    lastFrameTime = 0;
    fps = 0;
    updateStats();
  };

  ws.onmessage = (event) => {
    if (typeof event.data === "string") {
      log(`📩 Server: ${event.data}`);
      return;
    }

    // Binary data - this is JPEG compressed frame
    const buffer = event.data;

    // Calculate FPS
    const now = performance.now();
    if (lastFrameTime > 0) {
      const timeDiff = (now - lastFrameTime) / 1000;
      fps = Math.round(1 / timeDiff);
    }
    lastFrameTime = now;

    // Update stats
    frameCount++;
    totalBytes += buffer.byteLength;
    updateStats();

    log(`📸 Frame #${frameCount}: ${(buffer.byteLength / 1024).toFixed(2)} KB | ${fps} FPS`);

    // Create blob from JPEG data
    const blob = new Blob([buffer], { type: 'image/jpeg' });
    const url = URL.createObjectURL(blob);

    // Load image and draw to canvas
    const img = new Image();
    img.onload = () => {
      // Resize canvas to match image
      canvas.width = img.width;
      canvas.height = img.height;

      // Draw image
      ctx.drawImage(img, 0, 0);

      // Clean up
      URL.revokeObjectURL(url);
    };
    img.onerror = () => {
      log("❌ Error loading JPEG frame");
      URL.revokeObjectURL(url);
    };
    img.src = url;
  };

  ws.onclose = (event) => {
    log(`🔌 Disconnected. Code: ${event.code}, Reason: ${event.reason || "none"}`);
    setStatus("DISCONNECTED", false);
    connectBtn.disabled = false;
    disconnectBtn.disabled = true;
    sendBtn.disabled = true;
  };

  ws.onerror = (err) => {
    log("❌ WebSocket error occurred");
    console.error(err);
  };
});

disconnectBtn.addEventListener("click", () => {
  if (ws) {
    log("🔌 Closing connection...");
    ws.close(1000, "Client disconnect");
  }
});

sendBtn.addEventListener("click", () => {
  const msg = messageInput.value.trim();
  if (!msg) return;
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    log("⚠️ Cannot send, not connected.");
    return;
  }
  ws.send(msg);
  log(`📤 Sent: ${msg}`);
  messageInput.value = "";
});

messageInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter") {
    sendBtn.click();
  }
});

// Helper functions for common commands
function startStream() {
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    log("⚠️ Not connected!");
    alert("Please connect to the server first!");
    return;
  }
  ws.send("start_stream");
  log("▶️ Started streaming");
}

function stopStream() {
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    log("⚠️ Not connected!");
    alert("Please connect to the server first!");
    return;
  }
  ws.send("stop_stream");
  log("⏹️ Stopped streaming");
}

function captureFrame() {
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    log("⚠️ Not connected!");
    alert("Please connect to the server first!");
    return;
  }
  ws.send("frame_capture");
  log("📸 Requested single frame");
}

function listProcesses() {
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    log("⚠️ Not connected!");
    alert("Please connect to the server first!");
    return;
  }
  ws.send("list_process");
  log("📋 Requested process list");
}

// Expose functions globally for buttons
window.startStream = startStream;
window.stopStream = stopStream;
window.captureFrame = captureFrame;
window.listProcesses = listProcesses;
