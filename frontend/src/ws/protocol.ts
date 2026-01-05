// ACK-Based Flow Control - Traffic Class Constants
export const TRAFFIC_CONTROL = 0x01;  // Commands, Status, Info - Never drop
export const TRAFFIC_VIDEO   = 0x02;  // Video frames - Drop if busy
export const TRAFFIC_ACK     = 0x03;  // Frontend -> Gateway - Flow control signal

export type ParsedFrame = {
	payloadLen: number;
	clientId: number;
	backendId: number;
	payload: ArrayBuffer;
};

export function parseBackendFrame(buf: ArrayBuffer): ParsedFrame {
	if (buf.byteLength < 12) throw new Error(`Invalid frame: ${buf.byteLength} bytes`);
	const view = new DataView(buf);
	const payloadLen = view.getUint32(0, false);
	const clientId = view.getUint32(4, false);
	const backendId = view.getUint32(8, false);
	const payload = buf.slice(12);
	return { payloadLen, clientId, backendId, payload };
}

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

export function encodeText(msg: string): Uint8Array {
	return new TextEncoder().encode(msg);
}

export function isJpeg(payload: ArrayBuffer): boolean {
	const u = new Uint8Array(payload);
	return u.length >= 2 && u[0] === 0xff && u[1] === 0xd8;
}

export function tryDecodePrintableUtf8(payload: ArrayBuffer): string | null {
	try {
		const text = new TextDecoder("utf-8", { fatal: true }).decode(payload);
		// Allow all valid UTF-8 text (including Unicode chars like Vietnamese, emoji)
		// Only reject if it looks like binary (contains null bytes or control chars except newline/tab)
		if (text.length === 0) return "";
		// Check for null bytes or non-text control characters (0x00-0x08, 0x0B-0x0C, 0x0E-0x1F except tab/newline)
		if (/[\x00-\x08\x0B\x0C\x0E-\x1F]/.test(text)) return null;
		return text;
	} catch {
		return null;
	}
}

// ACK Flow Control: Get Traffic Class from payload first byte
export function getTrafficClass(payload: ArrayBuffer): number {
	if (payload.byteLength < 1) return 0;
	return new Uint8Array(payload)[0];
}

// ACK Flow Control: Build ACK packet to send to Gateway
export function buildAckPacket(clientId: number): ArrayBuffer {
	// ACK is a minimal frame: 12-byte header + 1-byte payload (0x03)
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

// ACK Flow Control: Strip Traffic Class prefix from payload
export function stripTrafficClass(payload: ArrayBuffer): ArrayBuffer {
	if (payload.byteLength < 1) return payload;
	return payload.slice(1);
}
