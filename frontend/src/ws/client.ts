import { buildAckPacket, buildBackendFrame, encodeText, getTrafficClass, isJpeg, parseBackendFrame, stripTrafficClass, TRAFFIC_VIDEO, tryDecodePrintableUtf8 } from "./protocol";

export type BackendFrameEvent =
	| { kind: "jpeg"; backendId: number; payloadLen: number; frameBytes: number; payload: ArrayBuffer }
	| { kind: "text"; backendId: number; payloadLen: number; frameBytes: number; text: string }
	| { kind: "binary"; backendId: number; payloadLen: number; frameBytes: number; payload: ArrayBuffer };

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

			// ACK Flow Control: Get traffic class and strip from payload
			const trafficClass = getTrafficClass(frame.payload);
			const strippedPayload = stripTrafficClass(frame.payload);

			// Check if this is a video frame (TRAFFIC_VIDEO or JPEG)
			const isVideoFrame = trafficClass === TRAFFIC_VIDEO || isJpeg(strippedPayload);
            // DEBUG: Log video frame receipt
            // if (isVideoFrame) console.log(`[FE-DEBUG] Video Frame Recv: ${strippedPayload.byteLength} bytes`);

			if (isVideoFrame) {
                // Determine actual content type: JPEG or Binary (H.264)
                const realKind = isJpeg(strippedPayload) ? "jpeg" : "binary";

				this.events.onBackendFrame({
					kind: realKind,
					backendId: frame.backendId,
					payloadLen: strippedPayload.byteLength,
					frameBytes: buffer.byteLength,
					payload: strippedPayload,
				});

				// ACK Flow Control: Send ACK after rendering video frame
				// This tells Gateway we're ready for the next frame
				// ACK Flow Control: Send ACK after rendering video frame
				// This tells Gateway we're ready for the next frame
				if (this.ws && this.ws.readyState === WebSocket.OPEN) {
					const ackPacket = buildAckPacket(this.myClientId);
					this.ws.send(ackPacket);
					// console.log('[WS] ACK sent for video frame');
				}
				return;
			}

			// Control messages (TRAFFIC_CONTROL) - process as text
			const text = tryDecodePrintableUtf8(strippedPayload);
			if (text !== null) {
				this.events.onBackendFrame({
					kind: "text",
					backendId: frame.backendId,
					payloadLen: strippedPayload.byteLength,
					frameBytes: buffer.byteLength,
					text,
				});
			} else {
				this.events.onBackendFrame({
					kind: "binary",
					backendId: frame.backendId,
					payloadLen: strippedPayload.byteLength,
					frameBytes: buffer.byteLength,
					payload: strippedPayload,
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
