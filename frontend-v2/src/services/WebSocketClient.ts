/**
 * WebSocket Client for Gateway Communication
 * Simplified version for v2
 */

export interface GatewayCallbacks {
  onLog: (msg: string) => void;
  onStatus: (status: 'CONNECTED' | 'CONNECTING' | 'DISCONNECTED') => void;
  onClientId: (id: number) => void;
  onGatewayText: (text: string) => void;
  onBackendActive: (id: number) => void;
  onBackendFrame: (ev: BackendFrame) => void;
  onClose: () => void;
}

export interface BackendFrame {
  backendId: number;
  kind: 'text' | 'binary' | 'jpeg';
  text: string;
  payload?: ArrayBuffer;
}

export class GatewayWsClient {
  private ws: WebSocket | null = null;
  private callbacks: GatewayCallbacks;
  private clientId: number = 0;
  private url: string = '';

  constructor(callbacks: GatewayCallbacks) {
    this.callbacks = callbacks;
  }

  connect(url: string): void {
    this.url = url;
    this.callbacks.onStatus('CONNECTING');
    this.callbacks.onLog(`Connecting to ${url}...`);

    try {
      this.ws = new WebSocket(url);
      this.ws.binaryType = 'arraybuffer';

      this.ws.onopen = () => {
        this.callbacks.onLog('WebSocket connected');
        this.callbacks.onStatus('CONNECTED');
      };

      this.ws.onclose = () => {
        this.callbacks.onLog('WebSocket closed');
        this.callbacks.onStatus('DISCONNECTED');
        this.callbacks.onClose();
      };

      this.ws.onerror = (e) => {
        this.callbacks.onLog(`WebSocket error: ${e}`);
      };

      this.ws.onmessage = (event) => {
        this.handleMessage(event.data);
      };
    } catch (e) {
      this.callbacks.onLog(`Connection failed: ${e}`);
      this.callbacks.onStatus('DISCONNECTED');
    }
  }

  disconnect(): void {
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
  }

  isConnected(): boolean {
    return this.ws?.readyState === WebSocket.OPEN;
  }

  sendText(targetId: number, message: string): void {
    if (!this.isConnected()) return;

    // Format: [1 byte targetId][text]
    const encoder = new TextEncoder();
    const textBytes = encoder.encode(message);
    const data = new Uint8Array(1 + textBytes.length);
    data[0] = targetId;
    data.set(textBytes, 1);

    this.ws?.send(data.buffer);
  }

  sendBinary(targetId: number, data: ArrayBuffer): void {
    if (!this.isConnected()) return;

    const payload = new Uint8Array(1 + data.byteLength);
    payload[0] = targetId;
    payload.set(new Uint8Array(data), 1);

    this.ws?.send(payload.buffer);
  }

  private handleMessage(data: string | ArrayBuffer): void {
    if (typeof data === 'string') {
      this.handleTextMessage(data);
    } else {
      this.handleBinaryMessage(data);
    }
  }

  private handleTextMessage(text: string): void {
    // Gateway messages
    if (text.startsWith('YOUR_ID:')) {
      this.clientId = parseInt(text.substring(8), 10);
      this.callbacks.onClientId(this.clientId);
      return;
    }

    if (text.startsWith('GATEWAY:')) {
      this.callbacks.onGatewayText(text.substring(8));
      return;
    }

    if (text.startsWith('ACTIVE:')) {
      const id = parseInt(text.substring(7), 10);
      this.callbacks.onBackendActive(id);
      return;
    }

    // Backend text message: [ID]:text
    const colonIdx = text.indexOf(':');
    if (colonIdx > 0) {
      const id = parseInt(text.substring(0, colonIdx), 10);
      if (!isNaN(id)) {
        this.callbacks.onBackendFrame({
          backendId: id,
          kind: 'text',
          text: text.substring(colonIdx + 1),
        });
      }
    }
  }

  private handleBinaryMessage(data: ArrayBuffer): void {
    if (data.byteLength < 2) return;

    const view = new DataView(data);
    const backendId = view.getUint8(0);
    const payload = data.slice(1);

    // Detect message type
    const payloadView = new DataView(payload);
    const firstByte = payloadView.getUint8(0);

    if (firstByte === 0x01 || firstByte === 0x02) {
      // MJPEG frame (Screen=0x01, Webcam=0x02)
      this.callbacks.onBackendFrame({
        backendId,
        kind: 'jpeg',
        text: '',
        payload,
      });
    } else if (firstByte === 0x03) {
      // File download
      this.callbacks.onBackendFrame({
        backendId,
        kind: 'binary',
        text: '',
        payload,
      });
    } else {
      // Generic binary
      this.callbacks.onBackendFrame({
        backendId,
        kind: 'binary',
        text: '',
        payload,
      });
    }
  }

  getClientId(): number {
    return this.clientId;
  }
}
