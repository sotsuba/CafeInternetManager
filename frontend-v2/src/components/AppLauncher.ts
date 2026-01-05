/**
 * App Launcher Component
 * Grid of installed applications with search and quick launch
 */

import { StateManager } from '../services/StateManager';

interface App {
  id: string;
  name: string;
  icon: string;
  exec: string;
}

export function AppLauncher(apps: App[]): HTMLElement {
  const container = document.createElement('div');
  container.className = 'app-launcher';

  container.innerHTML = `
    <div class="flex items-center justify-between mb-4">
      <h3 class="text-2xl font-semibold">Ứng Dụng</h3>
      <button class="btn btn-secondary btn-sm" id="btn-refresh-apps">
        <span>🔄</span>
        <span>Làm mới</span>
      </button>
    </div>

    <!-- Search -->
    <div class="input-group mb-6">
      <span class="input-icon">🔍</span>
      <input type="text" class="input" id="app-search" placeholder="Tìm kiếm ứng dụng... (giống Windows search)">
    </div>

    <!-- App Grid -->
    <div class="app-grid" id="app-grid">
      ${apps.length > 0 ? renderApps(apps) : `
        <div class="text-center p-8 text-muted" style="grid-column: 1/-1;">
          <span class="text-3xl">🚀</span>
          <p class="mt-2">Chưa có danh sách ứng dụng</p>
          <button class="btn btn-primary btn-sm mt-4" id="btn-load-apps">Tải danh sách</button>
        </div>
      `}
    </div>

    <!-- Quick Launch -->
    <div class="quick-launch mt-8">
      <h4 class="font-semibold mb-3">🚀 Quick Launch</h4>
      <p class="text-sm text-muted mb-3">Nhập tên hoặc đường dẫn ứng dụng (giống Windows Run)</p>
      <div class="flex gap-3">
        <input type="text" class="input flex-1" id="quick-launch-input" placeholder="notepad.exe, calc, cmd...">
        <button class="btn btn-primary" id="btn-quick-launch">
          <span>▶</span>
          <span>Chạy</span>
        </button>
      </div>
    </div>
  `;

  // Add component-specific styles
  const style = document.createElement('style');
  style.textContent = `
    .app-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(100px, 1fr));
      gap: var(--space-4);
      max-height: 350px;
      overflow-y: auto;
      padding: var(--space-2);
    }

    .app-item {
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: var(--space-2);
      padding: var(--space-4);
      border-radius: var(--radius-md);
      cursor: pointer;
      transition: all var(--transition-fast);
      text-align: center;
    }

    .app-item:hover {
      background: var(--bg-glass);
      transform: translateY(-2px);
    }

    .app-item:active {
      transform: translateY(0);
    }

    .app-icon {
      width: 48px;
      height: 48px;
      border-radius: var(--radius-md);
      background: linear-gradient(135deg, var(--aurora-blue), var(--aurora-purple));
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 1.5rem;
    }

    .app-icon img {
      width: 32px;
      height: 32px;
      object-fit: contain;
    }

    .app-name {
      font-size: var(--text-xs);
      color: var(--text-secondary);
      max-width: 100%;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap;
    }

    .quick-launch {
      padding: var(--space-4);
      background: var(--bg-surface);
      border-radius: var(--radius-md);
      border: 1px solid var(--border-subtle);
    }
  `;
  container.appendChild(style);

  // Event handlers
  setTimeout(() => {
    const appGrid = container.querySelector('#app-grid') as HTMLElement;
    const searchInput = container.querySelector('#app-search') as HTMLInputElement;
    const quickLaunchInput = container.querySelector('#quick-launch-input') as HTMLInputElement;
    const btnQuickLaunch = container.querySelector('#btn-quick-launch') as HTMLButtonElement;
    const btnRefresh = container.querySelector('#btn-refresh-apps') as HTMLButtonElement;
    const btnLoad = container.querySelector('#btn-load-apps') as HTMLButtonElement;

    let filteredApps = [...apps];

    // Search
    searchInput?.addEventListener('input', () => {
      const term = searchInput.value.toLowerCase();
      filteredApps = apps.filter(app =>
        app.name.toLowerCase().includes(term) ||
        app.exec.toLowerCase().includes(term)
      );
      appGrid.innerHTML = renderApps(filteredApps);
      attachAppHandlers();
    });

    // Quick Launch
    btnQuickLaunch?.addEventListener('click', () => {
      const command = quickLaunchInput.value.trim();
      if (command) {
        StateManager.sendToActive(`launch ${command}`);
        quickLaunchInput.value = '';
      }
    });

    quickLaunchInput?.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        btnQuickLaunch.click();
      }
    });

    // Refresh
    btnRefresh?.addEventListener('click', () => {
      StateManager.sendToActive('list_apps');
    });

    // Load
    btnLoad?.addEventListener('click', () => {
      StateManager.sendToActive('list_apps');
    });

    function attachAppHandlers() {
      const items = appGrid.querySelectorAll('.app-item');
      items.forEach(item => {
        item.addEventListener('click', () => {
          const exec = (item as HTMLElement).dataset.exec;
          if (exec) {
            StateManager.sendToActive(`launch ${exec}`);

            // Visual feedback
            item.classList.add('animate-pulse');
            setTimeout(() => item.classList.remove('animate-pulse'), 500);
          }
        });
      });
    }

    attachAppHandlers();
  }, 0);

  return container;
}

function renderApps(apps: App[]): string {
  if (apps.length === 0) {
    return `
      <div class="text-center p-4 text-muted" style="grid-column: 1/-1;">
        Không tìm thấy ứng dụng
      </div>
    `;
  }

  return apps.map(app => `
    <div class="app-item" data-exec="${escapeAttr(app.exec)}" title="${escapeHtml(app.name)}">
      <div class="app-icon">
        ${app.icon ? `<img src="data:image/png;base64,${app.icon}" alt="">` : getAppEmoji(app.name)}
      </div>
      <span class="app-name">${escapeHtml(app.name)}</span>
    </div>
  `).join('');
}

function getAppEmoji(name: string): string {
  const lower = name.toLowerCase();
  if (lower.includes('chrome')) return '🌐';
  if (lower.includes('firefox')) return '🦊';
  if (lower.includes('edge')) return '🌊';
  if (lower.includes('notepad')) return '📝';
  if (lower.includes('calculator') || lower.includes('calc')) return '🧮';
  if (lower.includes('explorer')) return '📁';
  if (lower.includes('paint')) return '🎨';
  if (lower.includes('word')) return '📘';
  if (lower.includes('excel')) return '📗';
  if (lower.includes('powerpoint')) return '📙';
  if (lower.includes('outlook')) return '📧';
  if (lower.includes('teams')) return '💬';
  if (lower.includes('code') || lower.includes('vscode')) return '💻';
  if (lower.includes('terminal') || lower.includes('cmd') || lower.includes('powershell')) return '🖥️';
  if (lower.includes('spotify')) return '🎵';
  if (lower.includes('discord')) return '🎮';
  if (lower.includes('steam')) return '🎮';
  if (lower.includes('settings')) return '⚙️';
  return '📱';
}

function escapeHtml(text: string): string {
  const div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}

function escapeAttr(text: string): string {
  return text.replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}
