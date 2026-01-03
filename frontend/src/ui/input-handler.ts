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

        // Special case: Characters for text input?
        // Backend also supports `text_input string`
        if (e.key.length === 1 && !e.ctrlKey && !e.altKey && !e.metaKey) {
            // this.client.sendText(this.backendId, `text_input ${e.key}`);
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

    private mapKeyCode(code: string): number {
        // Partial map based on `backend/include/interfaces/IInputInjector.hpp` (inferred)
        // See `WindowsInputInjector.cpp` logic
        switch(code) {
            case "KeyA": return 65; case "KeyB": return 66; case "KeyC": return 67;
            case "KeyD": return 68; case "KeyE": return 69; case "KeyF": return 70;
            case "KeyG": return 71; case "KeyH": return 72; case "KeyI": return 73;
            case "KeyJ": return 74; case "KeyK": return 75; case "KeyL": return 76;
            case "KeyM": return 77; case "KeyN": return 78; case "KeyO": return 79;
            case "KeyP": return 80; case "KeyQ": return 81; case "KeyR": return 82;
            case "KeyS": return 83; case "KeyT": return 84; case "KeyU": return 85;
            case "KeyV": return 86; case "KeyW": return 87; case "KeyX": return 88;
            case "KeyY": return 89; case "KeyZ": return 90;
            case "Enter": return 13;
            case "Space": return 32;
            case "Backspace": return 8;
            case "Escape": return 27;
            case "ShiftLeft": return 16; case "ShiftRight": return 16;
            case "ControlLeft": return 17; case "ControlRight": return 17;
            case "AltLeft": return 18; case "AltRight": return 18;
            case "ArrowLeft": return 37;
            case "ArrowUp": return 38;
            case "ArrowRight": return 39;
            case "ArrowDown": return 40;
            case "Digit0": return 48; case "Digit1": return 49;
            // ... add more as needed
            default: return 0;
        }
    }
}
