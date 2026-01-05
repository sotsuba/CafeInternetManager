/**
 * File Tree Component - macOS Finder Style
 * Expandable directory tree with file operations
 */

import { StateManager } from '../services/StateManager';

interface FileEntry {
  name: string;
  isDir: boolean;
  size?: number;
  children?: FileEntry[];
}

let currentPath = '.';
let fileData: FileEntry[] = [];
let selectedFile: string | null = null;
let expandedDirs = new Set<string>();

export function FileTree(): HTMLElement {
  const container = document.createElement('div');
  container.className = 'file-tree-container';

  container.innerHTML = `
    <div class="flex items-center justify-between mb-4">
      <h3 class="text-2xl font-semibold">Quản Lý Tệp</h3>
      <div class="flex gap-2">
        <button class="btn btn-ghost btn-sm" id="btn-back" disabled>
          <span>←</span>
          <span>Back</span>
        </button>
        <button class="btn btn-ghost btn-sm" id="btn-up">
          <span>↑</span>
          <span>Up</span>
        </button>
        <button class="btn btn-ghost btn-sm" id="btn-home">
          <span>🏠</span>
        </button>
        <button class="btn btn-secondary btn-sm" id="btn-refresh-files">
          <span>🔄</span>
        </button>
      </div>
    </div>

    <!-- Breadcrumb -->
    <div class="breadcrumb mb-4" id="breadcrumb">
      <span class="breadcrumb-item">📂</span>
      <span class="breadcrumb-separator">/</span>
      <span class="breadcrumb-item active" id="current-path">${currentPath}</span>
    </div>

    <!-- File Tree -->
    <div class="file-tree-wrapper glass-card">
      <div class="file-tree" id="file-tree">
        <div class="text-center p-8 text-muted">
          <span class="text-3xl">📁</span>
          <p class="mt-2">Nhấn "Tải danh sách" để xem thư mục</p>
          <button class="btn btn-primary btn-sm mt-4" id="btn-load-files">Tải danh sách</button>
        </div>
      </div>
    </div>

    <!-- Actions -->
    <div class="file-actions mt-4" id="file-actions" style="display: none;">
      <div class="flex items-center justify-between">
        <div class="selected-info">
          <span class="text-sm text-muted">Đã chọn:</span>
          <span class="font-medium ml-2" id="selected-file-name">-</span>
        </div>
        <div class="flex gap-2">
          <button class="btn btn-primary btn-sm" id="btn-download" disabled>
            <span>📥</span>
            <span>Tải về</span>
          </button>
          <button class="btn btn-secondary btn-sm" id="btn-upload">
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

    <!-- Hidden upload input -->
    <input type="file" id="upload-input" style="display: none;" multiple>
  `;

  // Add component-specific styles
  const style = document.createElement('style');
  style.textContent = `
    .file-tree-wrapper {
      max-height: 400px;
      overflow-y: auto;
      padding: var(--space-2);
    }

    .breadcrumb {
      display: flex;
      align-items: center;
      gap: var(--space-1);
      font-size: var(--text-sm);
      padding: var(--space-2) var(--space-3);
      background: var(--bg-elevated);
      border-radius: var(--radius-md);
      overflow-x: auto;
      white-space: nowrap;
    }

    .breadcrumb-separator {
      color: var(--text-muted);
    }

    .breadcrumb-item {
      color: var(--text-secondary);
      cursor: pointer;
    }

    .breadcrumb-item:hover {
      color: var(--aurora-cyan);
    }

    .breadcrumb-item.active {
      color: var(--text-primary);
      font-weight: 500;
    }

    .file-tree .tree-item {
      padding: var(--space-2) var(--space-3);
    }

    .file-actions {
      padding: var(--space-4);
      background: var(--bg-surface);
      border-radius: var(--radius-md);
      border: 1px solid var(--border-subtle);
    }

    .file-item-row {
      display: flex;
      align-items: center;
      width: 100%;
    }

    .file-indent {
      width: 20px;
      flex-shrink: 0;
    }
  `;
  container.appendChild(style);

  // Event handlers
  setTimeout(() => {
    const fileTree = container.querySelector('#file-tree') as HTMLElement;
    const fileActions = container.querySelector('#file-actions') as HTMLElement;
    const selectedFileName = container.querySelector('#selected-file-name') as HTMLElement;
    const btnDownload = container.querySelector('#btn-download') as HTMLButtonElement;
    const btnDelete = container.querySelector('#btn-delete') as HTMLButtonElement;
    const btnUpload = container.querySelector('#btn-upload') as HTMLButtonElement;
    const uploadInput = container.querySelector('#upload-input') as HTMLInputElement;
    const btnLoadFiles = container.querySelector('#btn-load-files') as HTMLButtonElement;
    const btnRefresh = container.querySelector('#btn-refresh-files') as HTMLButtonElement;
    const btnUp = container.querySelector('#btn-up') as HTMLButtonElement;
    const btnHome = container.querySelector('#btn-home') as HTMLButtonElement;
    const currentPathEl = container.querySelector('#current-path') as HTMLElement;

    // Load files
    btnLoadFiles?.addEventListener('click', () => {
      StateManager.sendToActive(`file_list ${currentPath}`);
    });

    btnRefresh?.addEventListener('click', () => {
      StateManager.sendToActive(`file_list ${currentPath}`);
    });

    btnUp?.addEventListener('click', () => {
      if (currentPath === '.' || currentPath === '/') return;
      const parts = currentPath.split('/');
      parts.pop();
      currentPath = parts.join('/') || '.';
      currentPathEl.textContent = currentPath;
      StateManager.sendToActive(`file_list ${currentPath}`);
    });

    btnHome?.addEventListener('click', () => {
      currentPath = '.';
      currentPathEl.textContent = currentPath;
      StateManager.sendToActive(`file_list ${currentPath}`);
    });

    // Download
    btnDownload?.addEventListener('click', () => {
      if (selectedFile) {
        StateManager.sendToActive(`file_download ${selectedFile}`);
      }
    });

    // Upload
    btnUpload?.addEventListener('click', () => {
      uploadInput.click();
    });

    uploadInput?.addEventListener('change', () => {
      const files = uploadInput.files;
      if (files && files.length > 0) {
        // For each file, read and send
        Array.from(files).forEach(file => {
          const reader = new FileReader();
          reader.onload = () => {
            // Send file data (implementation depends on protocol)
            console.log('Uploading:', file.name);
            // StateManager.sendBinary(`file_upload ${currentPath}/${file.name}`, reader.result);
          };
          reader.readAsArrayBuffer(file);
        });
      }
    });

    // Delete
    btnDelete?.addEventListener('click', () => {
      if (selectedFile && confirm(`Xác nhận xóa "${selectedFile}"?`)) {
        StateManager.sendToActive(`file_delete ${selectedFile}`);
        selectedFile = null;
        updateSelection();
        // Refresh after delete
        setTimeout(() => {
          StateManager.sendToActive(`file_list ${currentPath}`);
        }, 500);
      }
    });

    function updateSelection() {
      selectedFileName.textContent = selectedFile || '-';
      btnDownload.disabled = !selectedFile || selectedFile.endsWith('/');
      btnDelete.disabled = !selectedFile;
      fileActions.style.display = selectedFile ? 'block' : 'none';
    }

    // Simulate file data update (would be called from message handler)
    (window as any).updateFileTree = (files: FileEntry[]) => {
      fileData = files;
      renderTree();
    };

    function renderTree() {
      fileTree.innerHTML = renderFileItems(fileData, 0, currentPath);
      attachTreeHandlers();
      fileActions.style.display = 'block';
    }

    function attachTreeHandlers() {
      const items = fileTree.querySelectorAll('.tree-item');
      items.forEach(item => {
        item.addEventListener('click', (e) => {
          e.stopPropagation();
          const path = (item as HTMLElement).dataset.path!;
          const isDir = (item as HTMLElement).dataset.isdir === 'true';

          // Toggle selection
          items.forEach(i => i.classList.remove('selected'));
          item.classList.add('selected');
          selectedFile = path;
          updateSelection();
        });

        item.addEventListener('dblclick', (e) => {
          e.stopPropagation();
          const path = (item as HTMLElement).dataset.path!;
          const isDir = (item as HTMLElement).dataset.isdir === 'true';

          if (isDir) {
            // Toggle expand or navigate
            if (expandedDirs.has(path)) {
              expandedDirs.delete(path);
            } else {
              expandedDirs.add(path);
              // Navigate into directory
              currentPath = path;
              currentPathEl.textContent = currentPath;
              StateManager.sendToActive(`file_list ${currentPath}`);
            }
            renderTree();
          }
        });

        // Toggle button
        const toggle = item.querySelector('.tree-toggle');
        toggle?.addEventListener('click', (e) => {
          e.stopPropagation();
          const path = (item as HTMLElement).dataset.path!;
          if (expandedDirs.has(path)) {
            expandedDirs.delete(path);
          } else {
            expandedDirs.add(path);
          }
          renderTree();
        });
      });
    }
  }, 0);

  return container;
}

function renderFileItems(items: FileEntry[], depth: number, parentPath: string): string {
  if (!items || items.length === 0) {
    return `<div class="text-center p-4 text-muted text-sm">Thư mục trống</div>`;
  }

  // Sort: directories first, then files
  const sorted = [...items].sort((a, b) => {
    if (a.isDir && !b.isDir) return -1;
    if (!a.isDir && b.isDir) return 1;
    return a.name.localeCompare(b.name);
  });

  return sorted.map(item => {
    const fullPath = parentPath === '.' ? item.name : `${parentPath}/${item.name}`;
    const isExpanded = expandedDirs.has(fullPath);
    const icon = item.isDir ? '📁' : getFileIcon(item.name);
    const size = item.size ? formatSize(item.size) : '';

    return `
      <div class="tree-item ${isExpanded ? 'expanded' : ''}"
           data-path="${fullPath}"
           data-isdir="${item.isDir}"
           style="padding-left: ${depth * 20 + 12}px;">
        <div class="file-item-row">
          ${item.isDir ? `
            <span class="tree-toggle">▶</span>
          ` : `
            <span class="file-indent"></span>
          `}
          <span class="tree-icon">${icon}</span>
          <span class="tree-name">${item.name}</span>
          ${size ? `<span class="tree-size">${size}</span>` : ''}
        </div>
      </div>
      ${item.isDir && isExpanded && item.children ? renderFileItems(item.children, depth + 1, fullPath) : ''}
    `;
  }).join('');
}

function getFileIcon(name: string): string {
  const ext = name.split('.').pop()?.toLowerCase();
  const icons: Record<string, string> = {
    'txt': '📄',
    'md': '📝',
    'pdf': '📕',
    'doc': '📘', 'docx': '📘',
    'xls': '📗', 'xlsx': '📗',
    'ppt': '📙', 'pptx': '📙',
    'jpg': '🖼️', 'jpeg': '🖼️', 'png': '🖼️', 'gif': '🖼️', 'webp': '🖼️',
    'mp3': '🎵', 'wav': '🎵', 'flac': '🎵',
    'mp4': '🎬', 'mkv': '🎬', 'avi': '🎬', 'mov': '🎬',
    'zip': '📦', 'rar': '📦', '7z': '📦', 'tar': '📦', 'gz': '📦',
    'exe': '⚙️', 'msi': '⚙️',
    'js': '🟨', 'ts': '🔷',
    'py': '🐍',
    'html': '🌐', 'css': '🎨',
    'json': '📋', 'xml': '📋', 'yaml': '📋', 'yml': '📋',
  };
  return icons[ext || ''] || '📄';
}

function formatSize(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  if (bytes < 1024 * 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
  return `${(bytes / (1024 * 1024 * 1024)).toFixed(1)} GB`;
}
