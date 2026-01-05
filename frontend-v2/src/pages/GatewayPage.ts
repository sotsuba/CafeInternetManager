/**
 * Gateway Connection Page
 * IP/Port input with connection animation and backend discovery
 * Includes Demo Mode for UI demonstration
 */

import { router } from '../main';
import { MOCK_BACKENDS, enableDemoMode } from '../services/MockData';
import { StateManager } from '../services/StateManager';
import { GatewayCallbacks, GatewayWsClient } from '../services/WebSocketClient';

let wsClient: GatewayWsClient | null = null;

export function GatewayPage(): HTMLElement {
  const page = document.createElement('div');
  page.className = 'gateway-page';

  page.innerHTML = `
    <!-- Connection Card -->
    <div class="connection-card glass-card-strong" id="connection-card">
      <div class="connection-header">
        <div class="connection-icon animate-pulse">🌐</div>
        <h2 class="text-2xl font-semibold mt-4">Kết Nối Gateway</h2>
        <p class="text-secondary mt-2">Nhập địa chỉ IP và cổng của Gateway Server</p>
      </div>

      <form class="connection-form" id="connection-form">
        <input
          type="text"
          class="input"
          id="input-ip"
          placeholder="192.168.1.64"
          value="192.168.1.64"
        />
        <input
          type="text"
          class="input input-port"
          id="input-port"
          placeholder="8888"
          value="8888"
        />
        <button type="submit" class="btn btn-primary" id="btn-connect">
          <span id="connect-icon">⚡</span>
          <span id="connect-text">Kết Nối</span>
        </button>
      </form>

      <div class="connection-hint mt-6">
        <p class="text-xs text-muted">💡 Gateway thường chạy trên cổng 8888. Đảm bảo Gateway đã được khởi động.</p>
      </div>

      <!-- Demo Mode Button -->
      <div class="demo-mode-section mt-6">
        <div class="divider"></div>
        <p class="text-center text-sm text-muted mb-4">Hoặc thử với dữ liệu mẫu</p>
        <button class="btn btn-secondary w-full" id="btn-demo-mode">
          <span>🎭</span>
          <span>Chế Độ Demo</span>
        </button>
      </div>

      <div class="connection-status mt-4" id="connection-status" style="display: none;">
        <div class="status-dot connecting"></div>
        <span id="status-text">Đang kết nối...</span>
      </div>
    </div>

    <!-- Architecture Info (below connection card) -->
    <div class="architecture-info glass-card mt-8 p-6" id="architecture-info">
      <h4 class="font-semibold mb-4 flex items-center gap-2">
        <span>📐</span>
        <span>Kiến Trúc Kết Nối</span>
      </h4>
      <div class="arch-simple">
        <div class="arch-item">
          <span class="arch-emoji">🌐</span>
          <span class="arch-text">Frontend (bạn)</span>
        </div>
        <div class="arch-arrow">→</div>
        <div class="arch-item highlight">
          <span class="arch-emoji">🔌</span>
          <span class="arch-text">Gateway</span>
        </div>
        <div class="arch-arrow">→</div>
        <div class="arch-item">
          <span class="arch-emoji">💻</span>
          <span class="arch-text">Backends</span>
        </div>
      </div>
      <p class="text-xs text-muted mt-4">
        Frontend kết nối đến Gateway qua WebSocket. Gateway sẽ định tuyến tin nhắn đến các Backend đang hoạt động.
      </p>
    </div>

    <!-- Backends Grid (hidden initially) -->
    <div class="backends-container" id="backends-container" style="display: none;">
      <div class="backends-header flex items-center justify-between mb-4">
        <h3 class="text-xl font-semibold">
          <span>💻</span>
          <span>Máy Tính Đang Kết Nối</span>
          <span class="badge badge-success ml-2" id="backend-count">0</span>
          <span class="badge badge-warning ml-2" id="demo-badge" style="display: none;">DEMO</span>
        </h3>
        <button class="btn btn-ghost btn-sm" id="btn-disconnect">
          ⚡ Ngắt kết nối
        </button>
      </div>

      <div class="backends-grid" id="backends-grid"></div>

      <p class="text-xs text-muted text-center mt-4" id="scan-status">
        Đang quét tìm backends...
      </p>
    </div>
  `;

  // Add page-specific styles
  const style = document.createElement('style');
  style.textContent = `
    .connection-header {
      text-align: center;
    }

    .connection-icon {
      font-size: 4rem;
      display: inline-block;
    }

    .connection-form {
      display: flex;
      gap: var(--space-3);
      margin-top: var(--space-6);
    }

    .connection-form .input {
      flex: 1;
    }

    .connection-form .input-port {
      width: 100px;
      flex: none;
      text-align: center;
    }

    .connection-hint {
      text-align: center;
    }

    .demo-mode-section {
      text-align: center;
    }

    .connection-status {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: var(--space-2);
      font-size: var(--text-sm);
    }

    .architecture-info {
      max-width: 500px;
    }

    .arch-simple {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: var(--space-2);
    }

    .arch-item {
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: var(--space-3);
      background: var(--bg-elevated);
      border-radius: var(--radius-md);
      min-width: 80px;
    }

    .arch-item.highlight {
      background: linear-gradient(135deg, var(--aurora-blue), var(--aurora-purple));
      box-shadow: var(--shadow-glow);
    }

    .arch-emoji {
      font-size: 1.5rem;
    }

    .arch-text {
      font-size: var(--text-xs);
      margin-top: var(--space-1);
    }

    .arch-arrow {
      color: var(--aurora-cyan);
      font-size: var(--text-xl);
    }

    .backends-container {
      width: 100%;
      max-width: 1200px;
      margin-top: var(--space-8);
    }

    /* Connected state: shrink connection card */
    .connection-card.minimized {
      max-width: 100%;
      padding: var(--space-4);
    }

    .connection-card.minimized .connection-header,
    .connection-card.minimized .connection-hint,
    .connection-card.minimized .demo-mode-section,
    .connection-card.minimized .architecture-info {
      display: none;
    }

    .connection-card.minimized .connection-form {
      margin-top: 0;
    }

    /* Backend card */
    .backend-card-content {
      display: flex;
      flex-direction: column;
      gap: var(--space-2);
    }

    .backend-os-icon {
      font-size: 2rem;
    }

    .backend-card-ip {
      font-family: var(--font-mono);
      font-size: var(--text-xs);
      color: var(--text-muted);
    }
  `;
  page.appendChild(style);

  // Event handlers
  setTimeout(() => {
    const form = page.querySelector('#connection-form') as HTMLFormElement;
    const inputIp = page.querySelector('#input-ip') as HTMLInputElement;
    const inputPort = page.querySelector('#input-port') as HTMLInputElement;
    const btnConnect = page.querySelector('#btn-connect') as HTMLButtonElement;
    const btnDemoMode = page.querySelector('#btn-demo-mode') as HTMLButtonElement;
    const statusDiv = page.querySelector('#connection-status') as HTMLElement;
    const statusDot = statusDiv.querySelector('.status-dot') as HTMLElement;
    const statusText = page.querySelector('#status-text') as HTMLElement;
    const connectIcon = page.querySelector('#connect-icon') as HTMLElement;
    const connectText = page.querySelector('#connect-text') as HTMLElement;
    const connectionCard = page.querySelector('#connection-card') as HTMLElement;
    const archInfo = page.querySelector('#architecture-info') as HTMLElement;
    const backendsContainer = page.querySelector('#backends-container') as HTMLElement;
    const backendsGrid = page.querySelector('#backends-grid') as HTMLElement;
    const backendCount = page.querySelector('#backend-count') as HTMLElement;
    const demoBadge = page.querySelector('#demo-badge') as HTMLElement;
    const btnDisconnect = page.querySelector('#btn-disconnect') as HTMLElement;
    const scanStatus = page.querySelector('#scan-status') as HTMLElement;

    // Demo Mode Handler
    btnDemoMode.addEventListener('click', () => {
      enableDemoMode();

      // Add mock backends to StateManager
      MOCK_BACKENDS.forEach(mb => {
        StateManager.addBackend(mb.id);
        StateManager.updateBackend(mb.id, {
          name: mb.name,
          os: mb.os,
          status: mb.status,
        });
      });

      // Show connected state
      statusDiv.style.display = 'flex';
      statusDot.className = 'status-dot online';
      statusText.textContent = 'Chế độ Demo';
      connectIcon.textContent = '🎭';
      connectText.textContent = 'Demo Mode';
      btnConnect.classList.remove('btn-primary');
      btnConnect.classList.add('btn-success');
      btnConnect.disabled = true;

      // Show backends
      connectionCard.classList.add('minimized');
      archInfo.style.display = 'none';
      backendsContainer.style.display = 'block';
      demoBadge.style.display = 'inline-flex';
      scanStatus.textContent = '🎭 Chế độ Demo - Dữ liệu mẫu';

      renderBackends(true);
    });

    form.addEventListener('submit', async (e) => {
      e.preventDefault();

      const ip = inputIp.value.trim();
      const port = inputPort.value.trim();

      if (!ip || !port) {
        alert('Vui lòng nhập địa chỉ IP và cổng');
        return;
      }

      // Show connecting status
      statusDiv.style.display = 'flex';
      statusDot.className = 'status-dot connecting';
      statusText.textContent = 'Đang kết nối...';
      btnConnect.disabled = true;
      connectIcon.textContent = '⏳';
      connectText.textContent = 'Đang kết nối...';

      const wsUrl = `ws://${ip}:${port}`;

      // Initialize WebSocket
      const callbacks: GatewayCallbacks = {
        onStatus: (status) => {
          if (status === 'CONNECTED') {
            statusDot.className = 'status-dot online';
            statusText.textContent = 'Đã kết nối!';
            connectIcon.textContent = '✓';
            connectText.textContent = 'Đã kết nối';
            btnConnect.classList.remove('btn-primary');
            btnConnect.classList.add('btn-success');

            // Animate card shrink and show backends
            setTimeout(() => {
              connectionCard.classList.add('minimized');
              archInfo.style.display = 'none';
              backendsContainer.style.display = 'block';
              backendsGrid.classList.add('visible');
            }, 500);
          } else if (status === 'DISCONNECTED') {
            statusDot.className = 'status-dot offline';
            statusText.textContent = 'Đã ngắt kết nối';
            connectIcon.textContent = '⚡';
            connectText.textContent = 'Kết Nối Lại';
            btnConnect.disabled = false;
            btnConnect.classList.remove('btn-success');
            btnConnect.classList.add('btn-primary');
            connectionCard.classList.remove('minimized');
            archInfo.style.display = 'block';
            backendsContainer.style.display = 'none';
          }
        },
        onClientId: (id) => {
          console.log('🆔 Assigned Client ID:', id);
          StateManager.setClientId(id);
          // Start pinging to discover backends
          wsClient?.sendText(0, 'ping');
        },
        onBackendActive: (id) => {
          StateManager.addBackend(id);
          renderBackends(false);
        },
        onBackendFrame: (ev) => {
          // Handle backend data (name info, etc.)
          if (ev.kind === 'text') {
            if (ev.text.startsWith('INFO:NAME=')) {
              const name = ev.text.substring(10).trim();
              StateManager.updateBackend(ev.backendId, { name });
              renderBackends(false);
            } else if (ev.text.startsWith('INFO:OS=')) {
              const os = ev.text.substring(8).trim();
              StateManager.updateBackend(ev.backendId, { os });
              renderBackends(false);
            }
          }
        },
        onClose: () => {
          setTimeout(() => {
            if (wsClient) {
              wsClient.connect(wsUrl);
            }
          }, 3000);
        },
        onLog: () => {},
        onGatewayText: () => {},
      };

      wsClient = new GatewayWsClient(callbacks);
      wsClient.connect(wsUrl);
      StateManager.setWsClient(wsClient);
    });

    btnDisconnect?.addEventListener('click', () => {
      wsClient?.disconnect();
      wsClient = null;
      // Clear backends
      StateManager.getBackends().forEach(b => StateManager.removeBackend(b.id));
      demoBadge.style.display = 'none';
    });

    function renderBackends(isDemo: boolean) {
      const backends = StateManager.getBackends();
      backendCount.textContent = String(backends.length);

      backendsGrid.innerHTML = backends.map((b: any, i: number) => {
        // Get mock data for IP if in demo mode
        const mockData = isDemo ? MOCK_BACKENDS.find(mb => mb.id === b.id) : null;

        return `
        <div class="backend-card glass-card stagger-item" data-id="${b.id}" style="animation-delay: ${i * 50}ms;">
          <div class="backend-card-header">
            <div class="status-dot ${b.status}"></div>
            <div class="backend-os-icon">${getOsIcon(b.os)}</div>
            <div class="backend-card-info">
              <div class="backend-card-name">${b.name || `Backend #${b.id}`}</div>
              <div class="backend-card-id">ID: ${b.id}</div>
              ${mockData ? `<div class="backend-card-ip">${mockData.ip}</div>` : ''}
            </div>
          </div>
          <div class="backend-card-meta flex gap-2 mt-2">
            <span class="badge badge-info">${b.os || 'Unknown OS'}</span>
            ${mockData ? `<span class="badge">${mockData.uptime}</span>` : ''}
          </div>
        </div>
      `}).join('');

      backendsGrid.classList.add('visible');

      // Add click handlers
      backendsGrid.querySelectorAll('.backend-card').forEach(card => {
        card.addEventListener('click', () => {
          const id = Number(card.getAttribute('data-id'));
          StateManager.setActiveBackend(id);
          router.navigate('/dashboard');
        });
      });
    }

    function getOsIcon(os?: string): string {
      if (!os) return '💻';
      const lower = os.toLowerCase();
      if (lower.includes('windows')) return '🪟';
      if (lower.includes('linux') || lower.includes('ubuntu')) return '🐧';
      if (lower.includes('mac')) return '🍎';
      return '💻';
    }

    // Periodic ping to discover backends
    setInterval(() => {
      if (wsClient?.isConnected()) {
        wsClient.sendText(0, 'ping');
      }
    }, 5000);
  }, 0);

  return page;
}
