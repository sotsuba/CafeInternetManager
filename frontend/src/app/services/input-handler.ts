import type { GatewayWsClient } from './client';

/**
 * Handles mouse and keyboard input events on an element and sends them to the backend
 */
export class InputHandler {
  private element: HTMLElement;
  private client: GatewayWsClient;
  private backendId: number;
  private bound: boolean = false;

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
    this.element.addEventListener('mousemove', this.onMouseMove);
    this.element.addEventListener('mousedown', this.onMouseDown);
    this.element.addEventListener('mouseup', this.onMouseUp);
    this.element.addEventListener('wheel', this.onWheel, { passive: false });

    window.addEventListener('keydown', this.onKeyDown);
    window.addEventListener('keyup', this.onKeyUp);

    this.element.addEventListener('contextmenu', (e) => e.preventDefault());

    this.bound = true;
    console.log('🎮 Input Handler Attached to Backend:', this.backendId);
  }

  public detach() {
    if (!this.bound) return;
    this.element.removeEventListener('mousemove', this.onMouseMove);
    this.element.removeEventListener('mousedown', this.onMouseDown);
    this.element.removeEventListener('mouseup', this.onMouseUp);
    this.element.removeEventListener('wheel', this.onWheel);
    window.removeEventListener('keydown', this.onKeyDown);
    window.removeEventListener('keyup', this.onKeyUp);

    this.bound = false;
    console.log('🎮 Input Handler Detached');
  }

  private onWheel = (e: WheelEvent) => {
    if (!this.element.matches(':hover')) return;
    e.preventDefault();

    const dy = e.deltaY;
    const dx = e.deltaX;

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
    if (!this.element.matches(':hover')) return;
    e.preventDefault();

    const code = this.mapKeyCode(e.code);
    if (code !== 0) {
      this.client.sendText(this.backendId, `key_down ${code}`);
    }

    if (e.key.length === 1 && !e.ctrlKey && !e.altKey && !e.metaKey) {
      this.client.sendText(this.backendId, `text_input ${e.key}`);
    }
  };

  private onKeyUp = (e: KeyboardEvent) => {
    if (!this.element.matches(':hover')) return;
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
    switch(code) {
      case 'KeyA': return 1;  case 'KeyB': return 2;  case 'KeyC': return 3;
      case 'KeyD': return 4;  case 'KeyE': return 5;  case 'KeyF': return 6;
      case 'KeyG': return 7;  case 'KeyH': return 8;  case 'KeyI': return 9;
      case 'KeyJ': return 10; case 'KeyK': return 11; case 'KeyL': return 12;
      case 'KeyM': return 13; case 'KeyN': return 14; case 'KeyO': return 15;
      case 'KeyP': return 16; case 'KeyQ': return 17; case 'KeyR': return 18;
      case 'KeyS': return 19; case 'KeyT': return 20; case 'KeyU': return 21;
      case 'KeyV': return 22; case 'KeyW': return 23; case 'KeyX': return 24;
      case 'KeyY': return 25; case 'KeyZ': return 26;
      case 'Digit0': return 27; case 'Digit1': return 28; case 'Digit2': return 29;
      case 'Digit3': return 30; case 'Digit4': return 31; case 'Digit5': return 32;
      case 'Digit6': return 33; case 'Digit7': return 34; case 'Digit8': return 35;
      case 'Digit9': return 36;
      case 'Enter': return 37;
      case 'Space': return 38;
      case 'Backspace': return 39;
      case 'Tab': return 40;
      case 'Escape': return 41;
      case 'ShiftLeft': case 'ShiftRight': return 42;
      case 'ControlLeft': case 'ControlRight': return 43;
      case 'AltLeft': case 'AltRight': return 44;
      case 'MetaLeft': case 'MetaRight': return 45;
      case 'ArrowLeft': return 46;
      case 'ArrowRight': return 47;
      case 'ArrowUp': return 48;
      case 'ArrowDown': return 49;
      case 'Home': return 50;
      case 'End': return 51;
      case 'PageUp': return 52;
      case 'PageDown': return 53;
      case 'Insert': return 54;
      case 'Delete': return 55;
      case 'F1': return 56;  case 'F2': return 57;  case 'F3': return 58;
      case 'F4': return 59;  case 'F5': return 60;  case 'F6': return 61;
      case 'F7': return 62;  case 'F8': return 63;  case 'F9': return 64;
      case 'F10': return 65; case 'F11': return 66; case 'F12': return 67;
      case 'CapsLock': return 68;
      case 'NumLock': return 69;
      case 'ScrollLock': return 70;
      case 'Comma': return 71;
      case 'Period': return 72;
      case 'Slash': return 73;
      case 'Semicolon': return 74;
      case 'Quote': return 75;
      case 'BracketLeft': return 76;
      case 'BracketRight': return 77;
      case 'Backslash': return 78;
      case 'Minus': return 79;
      case 'Equal': return 80;
      case 'Backquote': return 81;
      default: return 0;
    }
  }
}
