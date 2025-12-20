import { buildBackendFrame, encodeText, isJpeg, parseBackendFrame, tryDecodePrintableUtf8 } from "./protocol";

export type BackendFrameEvent =
	| { kind: "jpeg"; backendId: number; payloadLen: number; frameBytes: number; payload: ArrayBuffer }
	| { kind: "text"; backendId: number; payloadLen: number; frameBytes: number; text: string }
	| { kind: "binary"; backendId: number; payloadLen: number; frameBytes: number };

export type ClientEvents = {
	onLog: (msg: string) => void;
	onStatus: (s: "CONNECTING" | "CONNECTED" | "DISCONNECTED") => void;
	onClientId: (id: number) => void;
	onGatewayText: (text: string) => void;
	onBackendActive: (backendId: number) => void;
	onBackendFrame: (ev: BackendFrameEvent) => void;
	onClose: (code: number) => void;
};

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
			this.events.onLog("⚠️ Already connected/connecting.");
			return;
		}

		this.events.onStatus("CONNECTING");
		this.events.onLog(`🔄 Connecting to ${url}...`);

		const ws = new WebSocket(url);
		ws.binaryType = "arraybuffer";
		this.ws = ws;

		ws.onopen = () => {
			this.events.onLog("✅ Connected successfully!");
			this.events.onStatus("CONNECTED");
		};

		ws.onmessage = (event) => {
			if (typeof event.data === "string") {
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

			if (isJpeg(frame.payload)) {
				this.events.onBackendFrame({
					kind: "jpeg",
					backendId: frame.backendId,
					payloadLen: frame.payloadLen,
					frameBytes: buffer.byteLength,
					payload: frame.payload,
				});
				return;
			}

			const text = tryDecodePrintableUtf8(frame.payload);
			if (text !== null) {
				this.events.onBackendFrame({
					kind: "text",
					backendId: frame.backendId,
					payloadLen: frame.payloadLen,
					frameBytes: buffer.byteLength,
					text,
				});
			} else {
				this.events.onBackendFrame({
					kind: "binary",
					backendId: frame.backendId,
					payloadLen: frame.payloadLen,
					frameBytes: buffer.byteLength,
				});
			}
		};

		ws.onclose = (event) => {
			this.ws = null;
			this.myClientId = 0;
			this.events.onLog(`🔌 Disconnected. Code: ${event.code}`);
			this.events.onStatus("DISCONNECTED");
			this.events.onClose(event.code);
		};

		ws.onerror = (err) => {
			this.events.onLog("❌ WebSocket error");
			// eslint-disable-next-line no-console
			console.error(err);
		};
	}

	disconnect() {
		if (!this.ws) return;
		this.events.onLog("🔌 Closing connection...");
		this.ws.close(1000, "Client disconnect");
	}

	sendText(backendId: number, message: string): number {
		if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
			this.events.onLog("⚠️ Cannot send, not connected.");
			return 0;
		}
		const payload = encodeText(message);
		const frame = buildBackendFrame(backendId, this.myClientId, payload);
		this.ws.send(frame);
		return frame.byteLength;
	}
}
