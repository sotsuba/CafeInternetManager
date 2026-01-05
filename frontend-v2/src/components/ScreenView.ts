/**
 * Screen/Webcam View Component
 * Video streaming with controls (record, pause, snapshot, fullscreen)
 */

import { StateManager } from '../services/StateManager';

export function ScreenView(type: 'monitor' | 'webcam', title: string): HTMLElement {
  const container = document.createElement('div');
  container.className = 'screen-view';

  const backendId = StateManager.getActiveBackend();
  const clientState = backendId ? StateManager.getClientState(backendId) : null;
  const currentState = type === 'monitor' ? clientState?.monitor : clientState?.webcam;
  const isActive = currentState === 'active';
  const isStarting = currentState === 'starting';

  container.innerHTML = `
    <div class="flex items-center justify-between mb-4">
      <h3 class="text-2xl font-semibold">${title}</h3>
      <span class="badge ${isActive ? 'badge-success' : 'badge-info'}" id="stream-status">
        ${isActive ? '● LIVE' : isStarting ? '◐ Starting...' : '○ Stopped'}
      </span>
    </div>

    <!-- Video Container -->
    <div class="video-container" id="video-container">
      <video id="stream-video" class="video-frame" autoplay muted playsinline></video>
      <div class="video-overlay" id="video-overlay" style="display: ${isActive ? 'none' : 'flex'};">
        <div class="video-placeholder">
          <span class="text-4xl">${type === 'monitor' ? '🖥️' : '📹'}</span>
          <p class="text-muted mt-2">Stream chưa bắt đầu</p>
        </div>
      </div>
    </div>

    <!-- Controls -->
    <div class="video-controls">
      <button class="btn ${isActive ? 'btn-danger' : 'btn-primary'}" id="btn-toggle" ${isStarting ? 'disabled' : ''}>
        <span id="toggle-icon">${isActive ? '⏹' : '▶'}</span>
        <span id="toggle-text">${isActive ? 'Dừng' : isStarting ? 'Đang khởi động...' : 'Bắt đầu'}</span>
      </button>

      <button class="btn btn-secondary" id="btn-record" ${!isActive ? 'disabled' : ''}>
        <span>⏺</span>
        <span>Ghi hình</span>
      </button>

      <button class="btn btn-secondary" id="btn-snapshot" ${!isActive ? 'disabled' : ''}>
        <span>📷</span>
        <span>Chụp ảnh</span>
      </button>

      <button class="btn btn-secondary" id="btn-fullscreen" ${!isActive ? 'disabled' : ''}>
        <span>⛶</span>
        <span>Toàn màn hình</span>
      </button>
    </div>

    <!-- Recording Timer (hidden by default) -->
    <div class="recording-info" id="recording-info" style="display: none;">
      <span class="status-dot online animate-recording"></span>
      <span class="font-mono" id="recording-timer">00:00:00</span>
    </div>
  `;

  // Add component-specific styles
  const style = document.createElement('style');
  style.textContent = `
    .video-overlay {
      position: absolute;
      inset: 0;
      display: flex;
      align-items: center;
      justify-content: center;
      background: var(--bg-base);
    }

    .video-placeholder {
      text-align: center;
    }

    .recording-info {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: var(--space-2);
      margin-top: var(--space-4);
      padding: var(--space-2) var(--space-4);
      background: rgba(239, 68, 68, 0.1);
      border-radius: var(--radius-md);
    }

    .video-controls .btn {
      min-width: 120px;
    }
  `;
  container.appendChild(style);

  // Event handlers
  setTimeout(() => {
    const btnToggle = container.querySelector('#btn-toggle') as HTMLButtonElement;
    const btnRecord = container.querySelector('#btn-record') as HTMLButtonElement;
    const btnSnapshot = container.querySelector('#btn-snapshot') as HTMLButtonElement;
    const btnFullscreen = container.querySelector('#btn-fullscreen') as HTMLButtonElement;
    const videoContainer = container.querySelector('#video-container') as HTMLElement;
    const toggleIcon = container.querySelector('#toggle-icon') as HTMLElement;
    const toggleText = container.querySelector('#toggle-text') as HTMLElement;
    const streamStatus = container.querySelector('#stream-status') as HTMLElement;
    const videoOverlay = container.querySelector('#video-overlay') as HTMLElement;

    let isRecording = false;

    btnToggle.addEventListener('click', () => {
      const backendId = StateManager.getActiveBackend();
      if (!backendId) return;

      const clientState = StateManager.getClientState(backendId);
      const currentState = type === 'monitor' ? clientState.monitor : clientState.webcam;

      if (currentState === 'active') {
        // STOP - Instant, no waiting
        StateManager.setClientState(backendId, type === 'monitor' ? 'monitor' : 'webcam', 'idle');
        StateManager.sendToActive(`${type}_stop`);

        // Update UI immediately
        btnToggle.className = 'btn btn-primary';
        toggleIcon.textContent = '▶';
        toggleText.textContent = 'Bắt đầu';
        streamStatus.className = 'badge badge-info';
        streamStatus.textContent = '○ Stopped';
        videoOverlay.style.display = 'flex';

        // Disable other buttons
        btnRecord.disabled = true;
        btnSnapshot.disabled = true;
        btnFullscreen.disabled = true;
      } else {
        // START - With loading state
        StateManager.setClientState(backendId, type === 'monitor' ? 'monitor' : 'webcam', 'starting');
        StateManager.sendToActive(`${type}_start`);

        // Update UI to starting
        btnToggle.disabled = true;
        toggleIcon.textContent = '◐';
        toggleText.textContent = 'Đang khởi động...';
        streamStatus.className = 'badge badge-warning';
        streamStatus.textContent = '◐ Starting...';
      }
    });

    btnRecord.addEventListener('click', () => {
      isRecording = !isRecording;
      const recordingInfo = container.querySelector('#recording-info') as HTMLElement;

      if (isRecording) {
        btnRecord.classList.add('btn-danger');
        btnRecord.classList.remove('btn-secondary');
        btnRecord.innerHTML = '<span>⏹</span><span>Dừng ghi</span>';
        recordingInfo.style.display = 'flex';
        // Start recording timer
        startRecordingTimer(container.querySelector('#recording-timer') as HTMLElement);
      } else {
        btnRecord.classList.remove('btn-danger');
        btnRecord.classList.add('btn-secondary');
        btnRecord.innerHTML = '<span>⏺</span><span>Ghi hình</span>';
        recordingInfo.style.display = 'none';
        // Stop recording
        StateManager.sendToActive(`${type}_record_stop`);
      }
    });

    btnSnapshot.addEventListener('click', () => {
      StateManager.sendToActive(`${type}_snapshot`);

      // Flash effect
      const flash = document.createElement('div');
      flash.style.cssText = `
        position: absolute;
        inset: 0;
        background: white;
        opacity: 0;
        animation: flash 0.3s ease;
        pointer-events: none;
      `;
      videoContainer.appendChild(flash);
      setTimeout(() => flash.remove(), 300);
    });

    btnFullscreen.addEventListener('click', () => {
      const video = container.querySelector('#stream-video') as HTMLVideoElement;
      if (video.requestFullscreen) {
        video.requestFullscreen();
      }
    });
  }, 0);

  return container;
}

let recordingInterval: number | null = null;

function startRecordingTimer(element: HTMLElement): void {
  let seconds = 0;

  if (recordingInterval) {
    clearInterval(recordingInterval);
  }

  recordingInterval = window.setInterval(() => {
    seconds++;
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = seconds % 60;
    element.textContent = `${pad(h)}:${pad(m)}:${pad(s)}`;
  }, 1000);
}

function pad(n: number): string {
  return n.toString().padStart(2, '0');
}
