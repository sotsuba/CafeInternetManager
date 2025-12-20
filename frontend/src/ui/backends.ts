export type BackendEntry = {
	id: number;
	active: boolean;
	streaming: boolean;
	canvas: HTMLCanvasElement;
	ctx: CanvasRenderingContext2D;
	frameCount: number;
	fps: number;
	lastFrameTime: number;
	lastDataSize: number;

	panel: HTMLDivElement;
	statusEl: HTMLDivElement;
	indicatorEl: HTMLDivElement;
};

export type BackendHandlers = {
	onClick: (id: number) => void;
};

let gridRoot: HTMLElement | null = null;
let emptyState: HTMLElement | null = null;
let handlers: BackendHandlers | null = null;
const backends: Record<number, BackendEntry> = {};

export function initBackendGrid(root: HTMLElement, h: BackendHandlers): Record<number, BackendEntry> {
	gridRoot = root;
	handlers = h;
	emptyState = root.querySelector("#emptyState");
	return backends;
}

export function addBackendPanel(id: number): BackendEntry | null {
	if (!gridRoot || !handlers) return null;
	if (backends[id]) return backends[id];

	// Hide empty state
	emptyState?.classList.add("is-hidden");

	const panel = document.createElement("div");
	panel.className = "backend-panel active";
	panel.id = `backend-panel-${id}`;

	panel.innerHTML = `
		<div class="backend-header">
			<div class="backend-title">Backend #${id}</div>
			<div class="backend-status-container">
				<div class="backend-status active" id="backend-status-${id}">ACTIVE</div>
				<div class="stream-indicator" id="stream-indicator-${id}"></div>
			</div>
		</div>
		<canvas class="backend-canvas" id="backend-canvas-${id}" width="640" height="480"></canvas>
	`;

	gridRoot.appendChild(panel);

	const canvas = panel.querySelector<HTMLCanvasElement>(`#backend-canvas-${id}`)!;
	const ctx = canvas.getContext("2d");
	if (!ctx) throw new Error("Canvas 2D context not available");

	const entry: BackendEntry = {
		id,
		active: true,
		streaming: false,
		canvas,
		ctx,
		frameCount: 0,
		fps: 0,
		lastFrameTime: 0,
		lastDataSize: 0,

		panel,
		statusEl: panel.querySelector<HTMLDivElement>(`#backend-status-${id}`)!,
		indicatorEl: panel.querySelector<HTMLDivElement>(`#stream-indicator-${id}`)!,
	};

	// Click on panel = exclusive session
	panel.addEventListener("click", () => {
		handlers!.onClick(id);
	});

	backends[id] = entry;
	drawNoStream(entry);

	return entry;
}

export function clearAllBackends() {
	Object.keys(backends).forEach(key => {
		const id = Number(key);
		const entry = backends[id];
		entry.panel.remove();
		delete backends[id];
	});
	emptyState?.classList.remove("is-hidden");
}

export function getBackend(id: number): BackendEntry | undefined {
	return backends[id];
}

export function getAllBackends(): BackendEntry[] {
	return Object.values(backends);
}

export function drawNoStream(b: BackendEntry) {
	b.ctx.fillStyle = "#000";
	b.ctx.fillRect(0, 0, b.canvas.width, b.canvas.height);
	b.ctx.fillStyle = "#666";
	b.ctx.font = "16px sans-serif";
	b.ctx.textAlign = "center";
	b.ctx.textBaseline = "middle";
	b.ctx.fillText("Click to view", b.canvas.width / 2, b.canvas.height / 2);
}

export function setBackendActive(b: BackendEntry, isActive: boolean) {
	b.active = isActive;
	if (isActive) {
		b.panel.className = "backend-panel active";
		b.statusEl.className = "backend-status active";
		b.statusEl.textContent = "ACTIVE";
	} else {
		b.panel.className = "backend-panel inactive";
		b.statusEl.className = "backend-status inactive";
		b.statusEl.textContent = "INACTIVE";
		setBackendStreaming(b, false);
	}
}

export function setBackendStreaming(b: BackendEntry, isStreaming: boolean) {
	b.streaming = isStreaming;
	b.indicatorEl.className = isStreaming ? "stream-indicator streaming" : "stream-indicator";
}

export function renderJpegToCanvas(b: BackendEntry, jpegPayload: ArrayBuffer, onError: (msg: string) => void) {
	const blob = new Blob([jpegPayload], { type: "image/jpeg" });
	const url = URL.createObjectURL(blob);
	const img = new Image();

	img.onload = () => {
		b.canvas.width = img.width;
		b.canvas.height = img.height;
		b.ctx.drawImage(img, 0, 0);
		URL.revokeObjectURL(url);
	};

	img.onerror = () => {
		URL.revokeObjectURL(url);
		onError(`❌ Backend #${b.id}: Error loading JPEG frame`);
	};

	img.src = url;
}
