// @ts-ignore
import JMuxer from "jmuxer";
import { GatewayWsClient } from "../ws/client";
import { showProcessModal } from "./process-manager";
import { switchView } from "./view-manager";

// DOM Elements
const stageTitle = document.getElementById("stageTitle") as HTMLDivElement;
const statusText = document.getElementById("statusText") as HTMLDivElement;
const videoEl = document.getElementById("videoEl") as HTMLVideoElement;
const placeholder = document.getElementById("videoPlaceholder") as HTMLDivElement;
const menuSheet = document.getElementById("menuSheet") as HTMLDivElement;
const btnCloseMenu = document.getElementById("btnCloseMenu") as HTMLButtonElement;
const menuBackendInfo = document.getElementById("menuBackendInfo") as HTMLDivElement;

const dock = {
    home: document.getElementById("btnHome") as HTMLButtonElement,
    share: document.getElementById("btnShare") as HTMLButtonElement,
    camera: document.getElementById("btnCam") as HTMLButtonElement,
    menu: document.getElementById("btnMenu") as HTMLButtonElement,
};

const menuActions = {
    start: document.getElementById("menuStartStream") as HTMLDivElement,
    stop: document.getElementById("menuStopStream") as HTMLDivElement,
};

type Mode = "home" | "share" | "camera" | "menu";

let currentBackendId: number | null = null;
let wsClient: GatewayWsClient | null = null;
let isCameraStreaming = false;
let isScreenStreaming = false;
let jmuxer: any = null;

export function initExclusiveSession(client: GatewayWsClient) {
    wsClient = client;

    // Initialize JMuxer attached to the main video element
    if (!jmuxer) {
        jmuxer = new JMuxer({
            node: 'videoEl',
            mode: 'video',
            flushingTime: 0,
            fps: 30,
            debug: true,
            onError: function(data: any) {
                console.warn('JMuxer error:', data);
            }
        });
    }

    // Dock Events
    dock.home.addEventListener("click", () => {
        stopActiveStreams();
        exitSession();
    });

    dock.share.addEventListener("click", () => {
        setActiveMode("share");
        void tryStartScreenShare();
    });

    dock.camera.addEventListener("click", () => {
        setActiveMode("camera");
        void tryStartCamera();
    });

    dock.menu.addEventListener("click", () => {
        if (currentBackendId) showProcessModal(currentBackendId);
    });

    // Menu Events
    btnCloseMenu.addEventListener("click", () => openMenu(false));
    menuSheet.addEventListener("click", (e) => {
        if (e.target === menuSheet) openMenu(false);
    });

    menuActions.start.addEventListener("click", () => {
        if (currentBackendId) wsClient?.sendText(currentBackendId, "start_stream");
        openMenu(false);
    });

    menuActions.stop.addEventListener("click", () => {
        if (currentBackendId) wsClient?.sendText(currentBackendId, "stop_stream");
        openMenu(false);
    });

    const menuProcesses = document.getElementById("menuProcesses")!;
    menuProcesses.addEventListener("click", () => {
        if (currentBackendId) showProcessModal(currentBackendId);
        openMenu(false);
    });
}

export function enterSession(backendId: number) {
    currentBackendId = backendId;
    stageTitle.textContent = `Live Stage - Backend #${backendId}`;
    menuBackendInfo.textContent = `Backend #${backendId}`;
    setActiveMode("home");

    // Reset video
    placeholder.style.display = "flex";
    if (jmuxer) jmuxer.reset();

    switchView({ view: "exclusive", backendId });
}

export function exitSession() {
    stopActiveStreams();
    currentBackendId = null;
    switchView({ view: "dashboard" });
}

export function handleExclusiveFrame(payload: ArrayBuffer) {
    // If we receive data, hide placeholder
    if (placeholder.style.display !== "none") {
        placeholder.style.display = "none";
    }

    // Feed raw H.264 stream to JMuxer
    if (jmuxer) {
        try {
            jmuxer.feed({
                video: new Uint8Array(payload)
            });
        } catch (e) {
            console.error("JMuxer feed error", e);
        }
    }
}

function setActiveMode(mode: Mode) {
    (Object.keys(dock) as Mode[]).forEach((k) => {
        dock[k].classList.toggle("is-active", k === mode);
    });
    if (mode !== "menu") openMenu(false);
}

export function updateBackendStatusDisplay(isActive: boolean) {
    statusText.textContent = isActive ? "ACTIVE" : "INACTIVE";
    statusText.className = isActive ? "stage-status active" : "stage-status inactive";
}

function openMenu(open: boolean) {
    menuSheet.classList.toggle("is-open", open);
    menuSheet.setAttribute("aria-hidden", String(!open));
}

async function tryStartScreenShare() {
    if (!currentBackendId || !wsClient?.isConnected()) return;
    wsClient.sendText(currentBackendId, "start_monitor_stream");
    isScreenStreaming = true;
}

async function tryStartCamera() {
    if (!currentBackendId || !wsClient?.isConnected()) return;
    wsClient.sendText(currentBackendId, "start_webcam_stream");
    isCameraStreaming = true;
}

function stopActiveStreams() {
    if (!currentBackendId || !wsClient?.isConnected()) return;

    if (isCameraStreaming) {
        wsClient.sendText(currentBackendId, "stop_webcam_stream");
        isCameraStreaming = false;
    }
    if (isScreenStreaming) {
        wsClient.sendText(currentBackendId, "stop_monitor_stream");
        isScreenStreaming = false;
    }

    placeholder.style.display = "flex";
    videoEl.srcObject = null;
}
