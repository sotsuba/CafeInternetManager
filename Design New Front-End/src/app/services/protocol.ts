// WebSocket Frame Protocol utilities

// Traffic Class Constants for ACK-based flow control
export const TRAFFIC_CONTROL = 0x01;  // Commands, Status, Info - Never drop
export const TRAFFIC_VIDEO   = 0x02;  // Video frames - Drop if busy
export const TRAFFIC_ACK     = 0x03;  // Frontend -> Gateway - Flow control signal

export interface ParsedFrame {
  payloadLen: number;
  clientId: number;
  backendId: number;
  payload: ArrayBuffer;
}

/**
 * Parse incoming WebSocket binary frame
 * Frame format: [4B payloadLen][4B clientId][4B backendId][payload...]
 */
export function parseBackendFrame(buf: ArrayBuffer): ParsedFrame {
  if (buf.byteLength < 12) throw new Error(`Invalid frame: ${buf.byteLength} bytes`);
  const view = new DataView(buf);
  const payloadLen = view.getUint32(0, false);
  const clientId = view.getUint32(4, false);
  const backendId = view.getUint32(8, false);
  const payload = buf.slice(12);
  return { payloadLen, clientId, backendId, payload };
}

/**
 * Build outgoing WebSocket binary frame
 */
export function buildBackendFrame(backendId: number, clientId: number, payload: Uint8Array): ArrayBuffer {
  const header = new ArrayBuffer(12);
  const view = new DataView(header);
  view.setUint32(0, payload.byteLength, false);
  view.setUint32(4, clientId >>> 0, false);
  view.setUint32(8, backendId >>> 0, false);

  const frame = new Uint8Array(12 + payload.byteLength);
  frame.set(new Uint8Array(header), 0);
  frame.set(payload, 12);
  return frame.buffer;
}

/**
 * Encode text to Uint8Array
 */
export function encodeText(msg: string): Uint8Array {
  return new TextEncoder().encode(msg);
}

/**
 * Check if payload is JPEG image (magic bytes)
 */
export function isJpeg(payload: ArrayBuffer): boolean {
  const u = new Uint8Array(payload);
  return u.length >= 2 && u[0] === 0xff && u[1] === 0xd8;
}

/**
 * Try decode payload as printable UTF-8 text
 */
export function tryDecodePrintableUtf8(payload: ArrayBuffer): string | null {
  try {
    const text = new TextDecoder('utf-8', { fatal: true }).decode(payload);
    if (text.length === 0) return '';
    // Reject binary (null bytes or non-text control chars)
    if (/[\x00-\x08\x0B\x0C\x0E-\x1F]/.test(text)) return null;
    return text;
  } catch {
    return null;
  }
}

/**
 * Get traffic class from payload first byte
 */
export function getTrafficClass(payload: ArrayBuffer): number {
  if (payload.byteLength < 1) return 0;
  return new Uint8Array(payload)[0];
}

/**
 * Build ACK packet for flow control
 */
export function buildAckPacket(clientId: number): ArrayBuffer {
  const header = new ArrayBuffer(12);
  const view = new DataView(header);
  view.setUint32(0, 1, false);  // Length = 1 byte
  view.setUint32(4, clientId >>> 0, false);
  view.setUint32(8, 0, false);  // backendId = 0 (Gateway only)

  const frame = new Uint8Array(13);
  frame.set(new Uint8Array(header), 0);
  frame[12] = TRAFFIC_ACK;
  return frame.buffer;
}

/**
 * Strip traffic class prefix byte from payload
 */
export function stripTrafficClass(payload: ArrayBuffer): ArrayBuffer {
  if (payload.byteLength < 1) return payload;
  return payload.slice(1);
}
