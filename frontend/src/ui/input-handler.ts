import { GatewayWsClient } from "../ws/client";

export class InputHandler {
    private element: HTMLElement;
    private client: GatewayWsClient;
    private backendId: number;
    private bound: boolean = false;

    // Throttle move events
    private lastMoveTime: number = 0;
    private readonly MOVE_INTERVAL = 30; // ~33fps

    constructor(videoElement: HTMLElement, wsClient: GatewayWsClient, backendId: number) {
        this.element = videoElement;
        this.client = wsClient;
        this.backendId = backendId;
    }

    public setBackendId(id: number) {
        this.backendId = id;
    }

    public attach() {
        if (this.bound) return;
        this.element.addEventListener("mousemove", this.onMouseMove);
        this.element.addEventListener("mousedown", this.onMouseDown);
        this.element.addEventListener("mouseup", this.onMouseUp);
        this.element.addEventListener("wheel", this.onWheel, { passive: false });

        // Keyboard needs global or tabindex focus.
        // For now, let's attach keydown to window ONLY when mouse is over video
        window.addEventListener("keydown", this.onKeyDown);
        window.addEventListener("keyup", this.onKeyUp);

        // Right click menu disable
        this.element.addEventListener("contextmenu", (e) => e.preventDefault());

        this.bound = true;
        console.log("🎮 Input Handler Attached");
    }

    public detach() {
        if (!this.bound) return;
        this.element.removeEventListener("mousemove", this.onMouseMove);
        this.element.removeEventListener("mousedown", this.onMouseDown);
        this.element.removeEventListener("mouseup", this.onMouseUp);
        this.element.removeEventListener("wheel", this.onWheel);
        window.removeEventListener("keydown", this.onKeyDown);
        window.removeEventListener("keyup", this.onKeyUp);

        this.bound = false;
        console.log("🎮 Input Handler Detached");
    }

    private onWheel = (e: WheelEvent) => {
        if (!this.element.matches(":hover")) return;
        e.preventDefault();

        // Throttle? Maybe not for scroll
        const dy = e.deltaY;
        const dx = e.deltaX;

        // Backend expects 'mouse_scroll dy dx'
        // Note: Windows SendInput uses + for up, but typical deltaY is + for down.
        // We might need to invert dy depending on Backend implementation.
        // Usually, users scroll down (positive deltaY) -> Move page down (negative content) -> Scroll bar goes down.
        // Windows 'WHEEL_DELTA' is 120. Positive value indicates wheel rotated forward, away from user (Up).
        // Browser deltaY: Down is positive.
        // So we should invert dy.

        this.client.sendText(this.backendId, `mouse_scroll ${-dy} ${dx}`);
    };

    private onMouseMove = (e: MouseEvent) => {
        const now = Date.now();
        if (now - this.lastMoveTime < this.MOVE_INTERVAL) return;
        this.lastMoveTime = now;

        const { x, y } = this.getNormCoords(e);
        this.client.sendText(this.backendId, `mouse_move ${x.toFixed(4)} ${y.toFixed(4)}`);
    };

    private onMouseDown = (e: MouseEvent) => {
        // Backend expects: 0=Left, 1=Right, 2=Middle
        let btn = 0;
        if (e.button === 2) btn = 1;
        else if (e.button === 1) btn = 2;

        this.client.sendText(this.backendId, `mouse_down ${btn}`);
    };

    private onMouseUp = (e: MouseEvent) => {
        let btn = 0;
        if (e.button === 2) btn = 1;
        else if (e.button === 1) btn = 2;

        this.client.sendText(this.backendId, `mouse_up ${btn}`);
    };

    private onKeyDown = (e: KeyboardEvent) => {
        // Only if mouse is over video to prevent accidental typing while doing other things
        if (!this.element.matches(":hover")) return;

        e.preventDefault();

        // Mapping JS KeyCodes to Custom or transmitting raw
        // Backend `WindowsInputInjector` expects custom `KeyCode` enum values or we need a map.
        // For simplicity, let's assume we implement a basic map here or send raw key names if backend supports text_input
        // The backend `BackendServer.cpp` handles `key_down int`.
        // We need a mapping from `e.code` to `interfaces::KeyCode` integers.
        const code = this.mapKeyCode(e.code);
        if (code !== 0) {
            this.client.sendText(this.backendId, `key_down ${code}`);
        }

        // Special case: Send printable characters via text_input for better Unicode support
        if (e.key.length === 1 && !e.ctrlKey && !e.altKey && !e.metaKey) {
            this.client.sendText(this.backendId, `text_input ${e.key}`);
        }
    };

    private onKeyUp = (e: KeyboardEvent) => {
        if (!this.element.matches(":hover")) return;
        e.preventDefault();

        const code = this.mapKeyCode(e.code);
        if (code !== 0) {
            this.client.sendText(this.backendId, `key_up ${code}`);
        }
    };

    private getNormCoords(e: MouseEvent) {
        const rect = this.element.getBoundingClientRect();
        const x = (e.clientX - rect.left) / rect.width;
        const y = (e.clientY - rect.top) / rect.height;
        return { x, y };
    }

    /**
     * Maps JavaScript KeyboardEvent.code to backend KeyCode enum values.
     * Backend KeyCode enum (from IInputInjector.hpp):
     * Unknown=0, A=1, B=2, ..., Z=26, Num0=27, ..., Num9=36,
     * Enter=37, Space=38, Backspace=39, Tab=40, Escape=41,
     * Shift=42, Control=43, Alt=44, Meta=45,
     * Left=46, Right=47, Up=48, Down=49,
     * Home=50, End=51, PageUp=52, PageDown=53, Insert=54, Delete=55,
     * F1=56, ..., F12=67, CapsLock=68, NumLock=69, ScrollLock=70,
     * Comma=71, Period=72, Slash=73, Semicolon=74, Quote=75,
     * BracketLeft=76, BracketRight=77, Backslash=78, Minus=79, Equal=80, Tilde=81
     */
    private mapKeyCode(code: string): number {
        switch(code) {
            // Alphanumeric A-Z (enum 1-26)
            case "KeyA": return 1;  case "KeyB": return 2;  case "KeyC": return 3;
            case "KeyD": return 4;  case "KeyE": return 5;  case "KeyF": return 6;
            case "KeyG": return 7;  case "KeyH": return 8;  case "KeyI": return 9;
            case "KeyJ": return 10; case "KeyK": return 11; case "KeyL": return 12;
            case "KeyM": return 13; case "KeyN": return 14; case "KeyO": return 15;
            case "KeyP": return 16; case "KeyQ": return 17; case "KeyR": return 18;
            case "KeyS": return 19; case "KeyT": return 20; case "KeyU": return 21;
            case "KeyV": return 22; case "KeyW": return 23; case "KeyX": return 24;
            case "KeyY": return 25; case "KeyZ": return 26;

            // Numbers 0-9 (enum 27-36)
            case "Digit0": return 27; case "Digit1": return 28; case "Digit2": return 29;
            case "Digit3": return 30; case "Digit4": return 31; case "Digit5": return 32;
            case "Digit6": return 33; case "Digit7": return 34; case "Digit8": return 35;
            case "Digit9": return 36;

            // Control keys (enum 37-41)
            case "Enter": return 37;
            case "Space": return 38;
            case "Backspace": return 39;
            case "Tab": return 40;
            case "Escape": return 41;

            // Modifiers (enum 42-45)
            case "ShiftLeft": case "ShiftRight": return 42;
            case "ControlLeft": case "ControlRight": return 43;
            case "AltLeft": case "AltRight": return 44;
            case "MetaLeft": case "MetaRight": return 45;

            // Navigation (enum 46-55)
            case "ArrowLeft": return 46;
            case "ArrowRight": return 47;
            case "ArrowUp": return 48;
            case "ArrowDown": return 49;
            case "Home": return 50;
            case "End": return 51;
            case "PageUp": return 52;
            case "PageDown": return 53;
            case "Insert": return 54;
            case "Delete": return 55;

            // Function keys F1-F12 (enum 56-67)
            case "F1": return 56;  case "F2": return 57;  case "F3": return 58;
            case "F4": return 59;  case "F5": return 60;  case "F6": return 61;
            case "F7": return 62;  case "F8": return 63;  case "F9": return 64;
            case "F10": return 65; case "F11": return 66; case "F12": return 67;

            // Lock keys (enum 68-70)
            case "CapsLock": return 68;
            case "NumLock": return 69;
            case "ScrollLock": return 70;

            // Symbols (enum 71-81)
            case "Comma": return 71;
            case "Period": return 72;
            case "Slash": return 73;
            case "Semicolon": return 74;
            case "Quote": return 75;
            case "BracketLeft": return 76;
            case "BracketRight": return 77;
            case "Backslash": return 78;
            case "Minus": return 79;
            case "Equal": return 80;
            case "Backquote": return 81;  // Tilde/Grave

            default: return 0;  // Unknown
        }
    }
}
