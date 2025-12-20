
import { GatewayWsClient } from "../ws/client";

export interface ProcessInfo {
    pid: number;
    ppid: number;
    name: string;
    cmd: string;
}

// Simple parser for standard `ps aux` or custom backend output
// The backend `Process::print_all` outputs: "PID: 123, PPID: 1, Name: systemd, Cmd: /sbin/init"
export function parseProcessList(text: string): ProcessInfo[] {
    const lines = text.split('\n');
    const processes: ProcessInfo[] = [];

    for (const line of lines) {
        if (!line.trim().startsWith("PID:")) continue;

        // Line format: PID: <d>, PPID: <d>, Name: <str>, Cmd: <str>
        // We can use regex
        const match = line.match(/PID: (\d+), PPID: (\d+), Name: (.*?), Cmd: (.*)$/);
        if (match) {
            processes.push({
                pid: parseInt(match[1]),
                ppid: parseInt(match[2]),
                name: match[3],
                cmd: match[4]
            });
        }
    }
    return processes;
}


let currentClient: GatewayWsClient | null = null;
let currentBackendId: number | null = null;

export function initProcessManager(client: GatewayWsClient) {
    currentClient = client;
}

export function showProcessModal(backendId: number) {
    currentBackendId = backendId;
    createModalIfNeeded();

    const modal = document.getElementById("processModal");
    if (modal) {
        modal.classList.add("is-visible");
        const tbody = modal.querySelector("tbody");
        if (tbody) tbody.innerHTML = '<tr><td colspan="5" style="text-align:center; padding: 20px;">Loading processes...</td></tr>';
    }

    // Request process list
    currentClient?.sendText(backendId, "list_process");
}

export function hideProcessModal() {
    const modal = document.getElementById("processModal");
    if (modal) modal.classList.remove("is-visible");
    currentBackendId = null;
}

export function handleProcessListResponse(backendId: number, text: string) {
    // Only update if we are looking at this backend's modal
    const modal = document.getElementById("processModal");
    if (!modal || !modal.classList.contains("is-visible") || currentBackendId !== backendId) return;

    const processes = parseProcessList(text);
    renderProcessTable(processes);
}

function createModalIfNeeded() {
    if (document.getElementById("processModal")) return;

    const modal = document.createElement("div");
    modal.id = "processModal";
    modal.className = "process-modal-overlay";
    modal.innerHTML = `
    <div class="process-modal-card">
      <div class="process-modal-header">
        <div class="modal-title">Process Manager</div>
        <button class="btn-close-modal" id="btnCloseProcessModal"><i class="bi bi-x-lg"></i></button>
      </div>
      <div class="process-modal-body">
        <div class="table-container">
          <table class="process-table">
            <thead>
              <tr>
                <th style="width: 80px">PID</th>
                <th style="width: 80px">PPID</th>
                <th style="width: 150px">Name</th>
                <th>Command</th>
                <th style="width: 80px">Action</th>
              </tr>
            </thead>
            <tbody id="processTableBody">
            </tbody>
          </table>
        </div>
      </div>
      <div class="process-modal-footer">
        <button class="btn-secondary" id="btnRefreshProcess">Refresh</button>
      </div>
    </div>
  `;

    document.body.appendChild(modal);

    modal.querySelector("#btnCloseProcessModal")?.addEventListener("click", hideProcessModal);
    modal.querySelector("#btnRefreshProcess")?.addEventListener("click", () => {
        if (currentBackendId) currentClient?.sendText(currentBackendId, "list_process");
    });

    // Close on click outside
    modal.addEventListener("click", (e) => {
        if (e.target === modal) hideProcessModal();
    });
}

function renderProcessTable(processes: ProcessInfo[]) {
    const tbody = document.getElementById("processTableBody");
    if (!tbody) return;

    if (processes.length === 0) {
        tbody.innerHTML = '<tr><td colspan="5" style="text-align:center; padding: 20px;">No processes found or invalid format.</td></tr>';
        return;
    }

    tbody.innerHTML = "";

    processes.forEach(p => {
        const tr = document.createElement("tr");
        tr.innerHTML = `
      <td>${p.pid}</td>
      <td>${p.ppid}</td>
      <td title="${p.name}">${p.name}</td>
      <td title="${p.cmd}"><div class="cmd-cell">${p.cmd}</div></td>
      <td>
        <button class="btn-kill" data-pid="${p.pid}">Kill</button>
      </td>
    `;

        // Kill Action
        tr.querySelector(".btn-kill")?.addEventListener("click", (e) => {
            const btn = e.currentTarget as HTMLButtonElement;
            const pid = btn.dataset.pid;
            if (confirm(`Are you sure you want to kill process ${pid} (${p.name})?`)) {
                if (currentBackendId && pid) {
                    currentClient?.sendText(currentBackendId, `kill_process ${pid}`);
                    // Optimistic UI or wait for refresh? 
                    // Let's just refresh after a short delay
                    setTimeout(() => currentClient?.sendText(currentBackendId!, "list_process"), 500);
                }
            }
        });

        tbody.appendChild(tr);
    });
}
