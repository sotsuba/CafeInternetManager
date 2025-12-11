export type ClientCommand = 'start_stream' | 'stop_stream' | 'frame_capture';

export interface StreamStats {
    fps: number;
    bytes: number;
    frameId: number;
}