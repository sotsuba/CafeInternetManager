// =============================================================================
// Protocol.ts - Frontend commands matching BackendServer.cpp
// =============================================================================

// --- Stream Commands ---
export enum StreamCommand {
    START_MONITOR = "start_monitor_stream",
    STOP_MONITOR = "stop_monitor_stream",
    START_WEBCAM = "start_webcam_stream",
    STOP_WEBCAM = "stop_webcam_stream",
}

// --- Keylogger Commands ---
export enum KeyloggerCommand {
    START = "start_keylog",
    STOP = "stop_keylog",
}

// --- Input Commands ---
export enum InputCommand {
    MOUSE_MOVE = "mouse_move",       // mouse_move x y
    MOUSE_DOWN = "mouse_down",       // mouse_down btn
    MOUSE_UP = "mouse_up",           // mouse_up btn
    MOUSE_CLICK = "mouse_click",     // mouse_click btn
    MOUSE_SCROLL = "mouse_scroll",   // mouse_scroll delta
    KEY_DOWN = "key_down",           // key_down code
    KEY_UP = "key_up",               // key_up code
    TEXT_INPUT = "text_input",       // text_input string
}

// --- App/Process Commands ---
export enum AppCommand {
    LIST_APPS = "list_apps",
    SEARCH_APPS = "search_apps",
    LAUNCH_APP = "launch_app",
    LIST_PROCESS = "list_process",
    KILL_PROCESS = "kill_process",
}

// --- System Commands ---
export enum SystemCommand {
    PING = "ping",
    GET_STATE = "get_state",
    SHUTDOWN = "shutdown",
    RESTART = "restart",
}

// --- File Transfer Commands ---
export enum FileCommand {
    LIST = "file_list",               // file_list path
    INFO = "file_info",               // file_info path
    MKDIR = "file_mkdir",             // file_mkdir path
    DELETE = "file_delete",           // file_delete path
    RENAME = "file_rename",           // file_rename old new
    DOWNLOAD = "file_download",       // file_download path
    UPLOAD_START = "file_upload_start", // file_upload_start path size
    UPLOAD_CHUNK = "file_upload_chunk", // file_upload_chunk path [binary]
    UPLOAD_FINISH = "file_upload_finish", // file_upload_finish path
    FREE_SPACE = "file_free_space",   // file_free_space path
}

// =============================================================================
// KeyCode Enum - Must match backend IInputInjector.hpp
// =============================================================================
export enum KeyCode {
    Unknown = 0,
    // Alphanumeric (1-26)
    A = 1, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    // Numbers (27-36)
    Num0 = 27, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    // Control keys (37-41)
    Enter = 37, Space, Backspace, Tab, Escape,
    // Modifiers (42-45)
    Shift = 42, Control, Alt, Meta,
    // Navigation (46-55)
    Left = 46, Right, Up, Down,
    Home = 50, End, PageUp, PageDown, Insert, Delete,
    // Function keys (56-67)
    F1 = 56, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    // Lock keys (68-70)
    CapsLock = 68, NumLock, ScrollLock,
    // Symbols (71-81)
    Comma = 71, Period, Slash, Semicolon, Quote,
    BracketLeft = 76, BracketRight, Backslash, Minus, Equal, Tilde,
}

// --- Mouse Button Enum ---
export enum MouseButton {
    Left = 0,
    Right = 1,
    Middle = 2,
}

// =============================================================================
// Helper Functions
// =============================================================================

export function killProcessCommand(pid: number): string {
    return `${AppCommand.KILL_PROCESS} ${pid}`;
}

export function launchAppCommand(exec: string): string {
    return `${AppCommand.LAUNCH_APP} ${exec}`;
}

export function searchAppsCommand(query: string): string {
    return `${AppCommand.SEARCH_APPS} ${query}`;
}

// File helpers
export function fileListCommand(path: string): string {
    return `${FileCommand.LIST} ${path}`;
}

export function fileDownloadCommand(path: string): string {
    return `${FileCommand.DOWNLOAD} ${path}`;
}

export function fileMkdirCommand(path: string): string {
    return `${FileCommand.MKDIR} ${path}`;
}

export function fileDeleteCommand(path: string): string {
    return `${FileCommand.DELETE} ${path}`;
}

export function fileRenameCommand(oldPath: string, newPath: string): string {
    return `${FileCommand.RENAME} ${oldPath} ${newPath}`;
}

// =============================================================================
// Status Types
// =============================================================================

export type ConnectionStatus = 'DISCONNECTED' | 'CONNECTING' | 'CONNECTED';

export interface StreamStats {
    fps: number;
    frameCount: number;
    totalBytes: number;
}

export interface FileEntry {
    name: string;
    path: string;
    size: number;
    modified_time: number;
    is_directory: boolean;
    is_readonly: boolean;
    is_hidden: boolean;
}
