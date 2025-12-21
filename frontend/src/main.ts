import { addBackendPanel, clearAllBackends, getBackend, initBackendGrid, renderJpegToCanvas, setBackendActive, setBackendStreaming } from "./ui/backends";
import { enterSession, handleExclusiveFrame, initExclusiveSession, updateBackendStatusDisplay } from "./ui/exclusive-session";
import { handleProcessListResponse, initProcessManager } from "./ui/process-manager";
import { GatewayWsClient } from "./ws/client";

// State
let wsClient: GatewayWsClient;
let activeExclusiveBackendId: number | null = null;
let pingIntervalId: ReturnType<typeof setInterval> | null = null;
let reconnectTimeoutId: ReturnType<typeof setTimeout> | null = null;

// DOM Elements
const statusSpan = document.getElementById("status") as HTMLSpanElement;
const backendsGrid = document.getElementById("backendsGrid") as HTMLElement;

// Config
const WS_URL = "ws://192.168.1.10:8080";
const RECONNECT_DELAY = 2000; // 2 seconds
const PING_INTERVAL = 5000;   // 5 seconds

// WS Initialization
wsClient = new GatewayWsClient({
	onLog: (msg) => console.log(msg),
	onStatus: (s) => {
		statusSpan.textContent = s;
		statusSpan.className = s === "CONNECTED" ? "status-badge connected" : "status-badge disconnected";
	},
	onClientId: (id) => {
		// Client ID assigned by gateway - start discovery
		console.log(`🆔 Client ID assigned: ${id}`);
		discoverBackends();
		startPingInterval();
	},
	onGatewayText: (text) => console.log(`📢 ${text}`),
	onBackendActive: (id) => {
		// Dynamically add backend panel if not exists
		const existing = getBackend(id);
		const backend = existing ?? addBackendPanel(id);
		const isNew = !existing;

		if (backend && !backend.active) {
			setBackendActive(backend, true);
			if (activeExclusiveBackendId === id) updateBackendStatusDisplay(true);
		}

		// Auto-preview for 10 seconds if newly discovered
		if (isNew && backend) {
			console.log(`🎥 Auto-previewing backend #${id} for 10s...`);
			wsClient.sendText(id, "start_monitor_stream");
			setTimeout(() => {
				wsClient.sendText(id, "stop_monitor_stream");
			}, 10000);
		}
	},
	onBackendFrame: (ev) => {
		// Dynamically add backend panel if not exists
		const b = getBackend(ev.backendId) ?? addBackendPanel(ev.backendId);
		if (!b) return;

		b.frameCount++;
		b.lastDataSize = ev.payloadLen;

		const now = performance.now();
		if (b.lastFrameTime > 0) {
			const dt = (now - b.lastFrameTime) / 1000;
			if (dt > 0) b.fps = Math.round(1 / dt);
		}
		b.lastFrameTime = now;

			if (ev.kind === "jpeg") {
				setBackendStreaming(b, true);
				renderJpegToCanvas(b, ev.payload, (err) => console.error(err));

				// If this backend is in exclusive mode, pipe the frame there too
				if (activeExclusiveBackendId === ev.backendId) {
					handleExclusiveFrame(ev.payload);
				}
			} else if (ev.kind === "binary") {
				// H.264 Stream Data
				setBackendStreaming(b, true);
				if (activeExclusiveBackendId === ev.backendId) {
					handleExclusiveFrame(ev.payload || new ArrayBuffer(0));
				}
			} else if (ev.kind === "text") {
			console.log(`💬 #${ev.backendId}: ${ev.text}`);
			if (ev.text.includes("Streaming started")) {
				setBackendStreaming(b, true);
			}
			if (ev.text.includes("Streaming stopped")) {
				setBackendStreaming(b, false);
			}

			// Handle process list response
			if (ev.text.includes("PID:") && ev.text.includes("Cmd:")) {
				handleProcessListResponse(ev.backendId, ev.text);
			}

			// Handle simple command responses
			if (ev.text.startsWith("OK: Process killed") || ev.text.startsWith("ERROR: Failed to kill")) {
				// Maybe refresh list?
				// process-manager handles refresh itself via timeout, but we could trigger it here.
				// For now let's just log or notify?
				// The modal will likely stay open.
				console.log(ev.text);
			}
			if (ev.text.startsWith("OK: Shutdown")) {
				alert(`Backend #${ev.backendId} is shutting down.`);
			}
		}
	},
	onClose: () => {
		// Clear all backend panels on disconnect
		clearAllBackends();
		stopPingInterval();
		// Schedule reconnect
		scheduleReconnect();
	}
});

// Initialize backend grid (dynamic mode)
initBackendGrid(backendsGrid, {
	onClick: (id) => {
		activeExclusiveBackendId = id;
		const b = getBackend(id);
		enterSession(id);
		if (b) updateBackendStatusDisplay(b.active);
	}
});

initExclusiveSession(wsClient);
initProcessManager(wsClient);

// Discover backends by broadcasting ping
function discoverBackends() {
	if (!wsClient.isConnected()) return;
	wsClient.sendText(0, "ping");
	console.log("📡 Broadcast ping sent");
}

// Auto-ping every 5 seconds
function startPingInterval() {
	stopPingInterval();
	pingIntervalId = setInterval(() => {
		discoverBackends();
	}, PING_INTERVAL);
}

function stopPingInterval() {
	if (pingIntervalId) {
		clearInterval(pingIntervalId);
		pingIntervalId = null;
	}
}

// Auto-connect with retry
function connect() {
	if (wsClient.isConnected()) return;
	statusSpan.textContent = "CONNECTING...";
	statusSpan.className = "status-badge disconnected";
	wsClient.connect(WS_URL);
}

function scheduleReconnect() {
	if (reconnectTimeoutId) return;
	console.log(`⏳ Reconnecting in ${RECONNECT_DELAY / 1000}s...`);
	reconnectTimeoutId = setTimeout(() => {
		reconnectTimeoutId = null;
		connect();
	}, RECONNECT_DELAY);
}

// Handle 'Home' button click to reset the exclusive tracking
document.getElementById("btnHome")?.addEventListener("click", () => {
	activeExclusiveBackendId = null;
});

// Theme toggle
document.getElementById("themeToggle")?.addEventListener("click", () => {
	document.body.classList.toggle("dark-theme");
});

// Auto-detect system theme
if (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) {
	document.body.classList.add("dark-theme");
}

// Auto-connect on page load
console.log("🚀 Backend Monitor Initialized");
connect();
