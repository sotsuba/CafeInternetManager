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
		if (text.length === 0) return "";
		if (/^[\x20-\x7E\n\r\t]*$/.test(text)) return text;
		return null;
	} catch {
		return null;
	}
}
