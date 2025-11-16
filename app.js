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

function log(message) {
  const time = new Date().toISOString().split("T")[1].split(".")[0];
  logDiv.textContent += `[${time}] ${message}\n`;
  logDiv.scrollTop = logDiv.scrollHeight;
}

function setStatus(text, color) {
  statusSpan.textContent = text;
  statusSpan.style.color = color || "black";
}

connectBtn.addEventListener("click", () => {
  const url = wsUrlInput.value.trim();
  if (!url) {
    alert("Please enter WebSocket URL");
    return;
  }

  if (ws && ws.readyState === WebSocket.OPEN) {
    log("Already connected.");
    return;
  }

  log(`Connecting to ${url}...`);
  ws = new WebSocket(url);

  // nhận binary dạng ArrayBuffer
  ws.binaryType = "arraybuffer";

  ws.onopen = () => {
    log("Connected.");
    setStatus("CONNECTED", "green");
    connectBtn.disabled = true;
    disconnectBtn.disabled = false;
    sendBtn.disabled = false;
  };

  ws.onmessage = (event) => {
    if (typeof event.data === "string") {
      log(`Received (text): ${event.data}`);
      return;
    }

    // RAW frame: [4 bytes w][4 bytes h][pixels...]
    const buffer = event.data;
    const view = new DataView(buffer);

    const width  = view.getUint32(0, false); // big-endian
    const height = view.getUint32(4, false);

    const pixelData = new Uint8ClampedArray(buffer, 8); // phần còn lại

    // Giả sử pixelData là RGBA (4 byte/pixel)
    if (pixelData.length < width * height * 4) {
      log("Binary frame too small for declared size.");
      return;
    }

    const imageData = new ImageData(pixelData, width, height);

    canvas.width = width;
    canvas.height = height;
    ctx.putImageData(imageData, 0, 0);
  };

  ws.onclose = (event) => {
    log(`Disconnected. code=${event.code} reason=${event.reason || "none"}`);
    setStatus("DISCONNECTED", "red");
    connectBtn.disabled = false;
    disconnectBtn.disabled = true;
    sendBtn.disabled = true;
  };

  ws.onerror = (err) => {
    log("WebSocket error.");
    console.error(err);
  };
});

disconnectBtn.addEventListener("click", () => {
  if (ws) {
    log("Closing connection...");
    ws.close(1000, "Client disconnect");
  }
});

sendBtn.addEventListener("click", () => {
  const msg = messageInput.value;
  if (!msg) return;
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    log("Cannot send, not connected.");
    return;
  }
  ws.send(msg);
  log(`Sent: ${msg}`);
  messageInput.value = "";
});

messageInput.addEventListener("keydown", (e) => {
  if (e.key === "Enter") {
    sendBtn.click();
  }
});
