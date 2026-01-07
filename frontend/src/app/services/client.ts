import {
    buildAckPacket,
    buildBackendFrame,
    encodeText,
    getTrafficClass,
    isJpeg,
    parseBackendFrame,
    stripTrafficClass,
    TRAFFIC_FILE,
    TRAFFIC_VIDEO,
    tryDecodePrintableUtf8,
} from './protocol';
import type { BackendFrameEvent, ConnectionStatus } from './types';

export interface ClientEvents {
  onLog: (msg: string) => void;
  onStatus: (s: ConnectionStatus) => void;
  onClientId: (id: number) => void;
  onGatewayText: (text: string) => void;
  onBackendActive: (backendId: number) => void;
  onBackendFrame: (ev: BackendFrameEvent) => void;
  onClose: (code: number) => void;
}

/**
 * Gateway WebSocket Client
 * Handles connection, message parsing, and command sending
 */
export class GatewayWsClient {
  private ws: WebSocket | null = null;
  private myClientId = 0;
  private events: ClientEvents;

  constructor(events: ClientEvents) {
    this.events = events;
  }

  get clientId(): number {
    return this.myClientId;
  }

  isConnected(): boolean {
    return !!this.ws && this.ws.readyState === WebSocket.OPEN;
  }

  connect(url: string) {
    if (this.ws && (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING)) {
      this.events.onLog('⚠️ Already connected/connecting.');
      return;
    }

    this.events.onStatus('CONNECTING');
    this.events.onLog(`🔄 Connecting to ${url}...`);

    const ws = new WebSocket(url);
    ws.binaryType = 'arraybuffer';
    this.ws = ws;

    ws.onopen = () => {
      this.events.onLog('✅ Connected successfully!');
      this.events.onStatus('CONNECTED');
    };

    ws.onmessage = (event) => {
      if (typeof event.data === 'string') {
        this.events.onLog(`📩 Server (text): ${event.data}`);
        return;
      }

      const buffer = event.data as ArrayBuffer;
      if (buffer.byteLength < 12) {
        this.events.onLog(`❌ Invalid frame: ${buffer.byteLength} bytes`);
        return;
      }

      const frame = parseBackendFrame(buffer);

      // Assign client id (welcome from gateway)
      if (frame.backendId === 0 && this.myClientId === 0) {
        this.myClientId = frame.clientId;
        this.events.onLog(`🆔 Gateway assigned Client ID: ${this.myClientId}`);
        this.events.onClientId(this.myClientId);
        return;
      }

      if (this.myClientId === 0 && frame.clientId > 0) {
        this.myClientId = frame.clientId;
        this.events.onLog(`ℹ️ Client ID: ${this.myClientId}`);
        this.events.onClientId(this.myClientId);
      }

      // backendId==0 => gateway/broadcast
      if (frame.backendId === 0) {
        const text = tryDecodePrintableUtf8(frame.payload);
        if (text !== null) this.events.onGatewayText(text);
        else this.events.onGatewayText(`[binary ${frame.payloadLen} bytes]`);
        return;
      }

      if (frame.backendId < 1 || frame.backendId > 8) {
        this.events.onLog(`⚠️ Invalid backend_id: ${frame.backendId}`);
        return;
      }

      this.events.onBackendActive(frame.backendId);

      const trafficClass = getTrafficClass(frame.payload);
      const strippedPayload = stripTrafficClass(frame.payload);

      // Handle binary file transfer (High Performance)
      if (trafficClass === TRAFFIC_FILE) {
        this.events.onBackendFrame({
          kind: 'file',
          backendId: frame.backendId,
          payload: strippedPayload,
        });
        return;
      }

      // Check if this is a video frame
      const isVideoFrame = trafficClass === TRAFFIC_VIDEO || isJpeg(strippedPayload);

      if (isVideoFrame) {
        const realKind = isJpeg(strippedPayload) ? 'jpeg' : 'binary';
        this.events.onBackendFrame({
          kind: realKind,
          backendId: frame.backendId,
          payload: strippedPayload,
        });

        // Send ACK for flow control
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
          const ackPacket = buildAckPacket(this.myClientId);
          this.ws.send(ackPacket);
        }
        return;
      }

      // Control messages - process as text
      const text = tryDecodePrintableUtf8(strippedPayload);
      if (text !== null) {
        this.events.onBackendFrame({
          kind: 'text',
          backendId: frame.backendId,
          text,
        });
      } else {
        this.events.onBackendFrame({
          kind: 'binary',
          backendId: frame.backendId,
          payload: strippedPayload,
        });
      }
    };

    ws.onclose = (event) => {
      this.ws = null;
      this.myClientId = 0;
      this.events.onLog(`🔌 Disconnected. Code: ${event.code}`);
      this.events.onStatus('DISCONNECTED');
      this.events.onClose(event.code);
    };

    ws.onerror = (err) => {
      this.events.onLog('❌ WebSocket error');
      console.error(err);
    };
  }

  disconnect() {
    if (!this.ws) return;
    this.events.onLog('🔌 Closing connection...');
    this.ws.close(1000, 'Client disconnect');
  }

  /**
   * Send text command to a specific backend
   */
  sendText(backendId: number, message: string): number {
    console.log(`[GatewayWsClient] sendText: to backend ${backendId}, message: "${message}"`);
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      this.events.onLog('⚠️ Cannot send, not connected.');
      return 0;
    }
    const payload = encodeText(message);
    const frame = buildBackendFrame(backendId, this.myClientId, payload);
    this.ws.send(frame);
    return frame.byteLength;
  }

  /**
   * Send binary data to a specific backend
   */
  sendBinary(backendId: number, data: ArrayBuffer): number {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      this.events.onLog('⚠️ Cannot send, not connected.');
      return 0;
    }
    const payload = new Uint8Array(data);
    const frame = buildBackendFrame(backendId, this.myClientId, payload);
    this.ws.send(frame);
    return frame.byteLength;
  }
}
