/**
 * Process Manager Component
 * Professional process list with search, sort, and kill functionality
 */

import { StateManager } from '../services/StateManager';

interface Process {
  pid: string;
  name: string;
  user: string;
  cmd: string;
}

export function ProcessManager(processes: Process[]): HTMLElement {
  const container = document.createElement('div');
  container.className = 'process-manager';

  container.innerHTML = `
    <div class="flex items-center justify-between mb-4">
      <h3 class="text-2xl font-semibold">Quản Lý Tiến Trình</h3>
      <button class="btn btn-secondary btn-sm" id="btn-refresh">
        <span>🔄</span>
        <span>Làm mới</span>
      </button>
    </div>

    <!-- Search & Actions -->
    <div class="flex items-center gap-3 mb-4">
      <div class="input-group flex-1">
        <span class="input-icon">🔍</span>
        <input type="text" class="input" id="search-input" placeholder="Tìm kiếm tiến trình...">
      </div>
      <button class="btn btn-danger btn-sm" id="btn-kill-selected" disabled>
        <span>☠️</span>
        <span>Kill Selected (<span id="selected-count">0</span>)</span>
      </button>
    </div>

    <!-- Process Table -->
    <div class="process-list card">
      <table class="table" id="process-table">
        <thead>
          <tr>
            <th style="width: 40px;">
              <input type="checkbox" class="checkbox" id="select-all">
            </th>
            <th style="width: 80px;" class="sortable" data-sort="pid">PID ↕</th>
            <th class="sortable" data-sort="name">Tên ↕</th>
            <th style="width: 120px;">User</th>
            <th style="width: 80px;">Action</th>
          </tr>
        </thead>
        <tbody id="process-tbody">
          ${renderRows(processes)}
        </tbody>
      </table>

      ${processes.length === 0 ? `
        <div class="text-center p-8 text-muted">
          <span class="text-3xl">📋</span>
          <p class="mt-2">Không có dữ liệu tiến trình</p>
          <button class="btn btn-primary btn-sm mt-4" id="btn-load">Tải danh sách</button>
        </div>
      ` : ''}
    </div>

    <p class="text-xs text-muted mt-4">
      💡 Tổng số: <strong id="total-count">${processes.length}</strong> tiến trình
    </p>
  `;

  // Add component-specific styles
  const style = document.createElement('style');
  style.textContent = `
    .process-list {
      max-height: 400px;
      overflow-y: auto;
      padding: 0;
    }

    .sortable {
      cursor: pointer;
      user-select: none;
    }

    .sortable:hover {
      color: var(--aurora-cyan);
    }

    .process-name {
      display: flex;
      align-items: center;
      gap: var(--space-2);
    }

    .process-icon {
      width: 20px;
      height: 20px;
      background: var(--bg-elevated);
      border-radius: var(--radius-sm);
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 12px;
    }
  `;
  container.appendChild(style);

  // Event handlers
  setTimeout(() => {
    const searchInput = container.querySelector('#search-input') as HTMLInputElement;
    const selectAll = container.querySelector('#select-all') as HTMLInputElement;
    const tbody = container.querySelector('#process-tbody') as HTMLElement;
    const btnKillSelected = container.querySelector('#btn-kill-selected') as HTMLButtonElement;
    const selectedCount = container.querySelector('#selected-count') as HTMLElement;
    const btnRefresh = container.querySelector('#btn-refresh') as HTMLButtonElement;
    const btnLoad = container.querySelector('#btn-load') as HTMLButtonElement;

    let filteredProcesses = [...processes];
    let selectedPids = new Set<string>();

    // Search
    searchInput?.addEventListener('input', () => {
      const term = searchInput.value.toLowerCase();
      filteredProcesses = processes.filter(p =>
        p.name.toLowerCase().includes(term) ||
        p.pid.includes(term) ||
        p.cmd.toLowerCase().includes(term)
      );
      tbody.innerHTML = renderRows(filteredProcesses);
      attachRowHandlers();
    });

    // Select all
    selectAll?.addEventListener('change', () => {
      const checkboxes = tbody.querySelectorAll('.row-checkbox') as NodeListOf<HTMLInputElement>;
      checkboxes.forEach(cb => {
        cb.checked = selectAll.checked;
        const pid = cb.dataset.pid!;
        if (selectAll.checked) {
          selectedPids.add(pid);
        } else {
          selectedPids.delete(pid);
        }
      });
      updateSelectedCount();
    });

    // Refresh
    btnRefresh?.addEventListener('click', () => {
      StateManager.sendToActive('list_process');
    });

    // Load (for empty state)
    btnLoad?.addEventListener('click', () => {
      StateManager.sendToActive('list_process');
    });

    // Kill selected
    btnKillSelected?.addEventListener('click', () => {
      if (selectedPids.size === 0) return;

      if (confirm(`Bạn có chắc muốn kill ${selectedPids.size} tiến trình?`)) {
        selectedPids.forEach(pid => {
          StateManager.sendToActive(`kill ${pid}`);
        });
        selectedPids.clear();
        updateSelectedCount();

        // Refresh after kill
        setTimeout(() => {
          StateManager.sendToActive('list_process');
        }, 500);
      }
    });

    function attachRowHandlers() {
      const checkboxes = tbody.querySelectorAll('.row-checkbox') as NodeListOf<HTMLInputElement>;
      const killButtons = tbody.querySelectorAll('.btn-kill-single');

      checkboxes.forEach(cb => {
        cb.addEventListener('change', () => {
          const pid = cb.dataset.pid!;
          if (cb.checked) {
            selectedPids.add(pid);
          } else {
            selectedPids.delete(pid);
          }
          updateSelectedCount();
        });
      });

      killButtons.forEach(btn => {
        btn.addEventListener('click', () => {
          const pid = (btn as HTMLElement).dataset.pid!;
          if (confirm(`Kill tiến trình PID ${pid}?`)) {
            StateManager.sendToActive(`kill ${pid}`);
            setTimeout(() => {
              StateManager.sendToActive('list_process');
            }, 500);
          }
        });
      });
    }

    function updateSelectedCount() {
      selectedCount.textContent = String(selectedPids.size);
      btnKillSelected.disabled = selectedPids.size === 0;
    }

    attachRowHandlers();
  }, 0);

  return container;
}

function renderRows(processes: Array<{pid: string; name: string; user: string; cmd: string}>): string {
  if (processes.length === 0) return '';

  return processes.map(p => `
    <tr>
      <td>
        <input type="checkbox" class="checkbox row-checkbox" data-pid="${p.pid}">
      </td>
      <td class="font-mono text-xs">${p.pid}</td>
      <td>
        <div class="process-name">
          <span class="process-icon">${getProcessIcon(p.name)}</span>
          <span class="truncate" style="max-width: 200px;" title="${p.cmd || p.name}">${p.name}</span>
        </div>
      </td>
      <td class="text-muted text-sm">${p.user || 'System'}</td>
      <td>
        <button class="btn btn-danger btn-sm btn-kill-single" data-pid="${p.pid}">Kill</button>
      </td>
    </tr>
  `).join('');
}

function getProcessIcon(name: string): string {
  const lower = name.toLowerCase();
  if (lower.includes('chrome')) return '🌐';
  if (lower.includes('firefox')) return '🦊';
  if (lower.includes('code') || lower.includes('vscode')) return '💻';
  if (lower.includes('explorer')) return '📁';
  if (lower.includes('python')) return '🐍';
  if (lower.includes('node')) return '🟢';
  if (lower.includes('java')) return '☕';
  return '⚙️';
}
