import { Square } from 'lucide-react';
import { motion } from 'motion/react';
import { useEffect, useRef, useState } from 'react';
import { useGateway } from '../../services';
import { HelpButton } from '../HelpButton';
import { Button } from '../ui/button';

interface ScreenViewerProps {
  type: 'screen' | 'webcam';
  backendId: string;
}

export function ScreenViewer({ type, backendId }: ScreenViewerProps) {
  const { activeClient, sendCommand } = useGateway();
  const [isRecording, setIsRecording] = useState(false);
  const [isPaused, setIsPaused] = useState(false);
  const [recordingTime, setRecordingTime] = useState(0);
  const [isFullscreen, setIsFullscreen] = useState(false);
  const recordingIntervalRef = useRef<NodeJS.Timeout | null>(null);
  const containerRef = useRef<HTMLDivElement>(null);

  // Optimistic local state for immediate UI feedback
  const [localStreaming, setLocalStreaming] = useState(false);

  // Get real state from backend
  const stateKey = type === 'screen' ? 'monitor' : 'webcam';
  const backendState = activeClient?.state[stateKey] || 'idle';
  const streamUrl = type === 'screen' ? activeClient?.monitorFrame : activeClient?.webcamFrame;

  // Use optimistic state for immediate UI feedback
  const isStreaming = localStreaming || backendState === 'active';

  // Sync local state with backend state
  useEffect(() => {
    if (backendState === 'active') {
      setLocalStreaming(true);
    } else if (backendState === 'idle') {
      setLocalStreaming(false);
    }
  }, [backendState]);

  const handleStartStream = () => {
    sendCommand(type === 'screen' ? 'start_monitor_stream' : 'start_webcam_stream');
    setTimeout(() => setLocalStreaming(true), 500); // 500ms delay before UI transition
  };

  const handleStopStream = () => {
    sendCommand(type === 'screen' ? 'stop_monitor_stream' : 'stop_webcam_stream');
    if (isRecording) {
      handleStopRecording();
    }
    setTimeout(() => setLocalStreaming(false), 500); // 500ms delay before UI transition
  };

  const handleStartRecording = () => {
    setIsRecording(true);
    setIsPaused(false);
    setRecordingTime(0);
    recordingIntervalRef.current = setInterval(() => {
      setRecordingTime((prev) => prev + 1);
    }, 1000);
  };

  const handleStopRecording = () => {
    setIsRecording(false);
    setIsPaused(false);
    setRecordingTime(0);
    if (recordingIntervalRef.current) {
      clearInterval(recordingIntervalRef.current);
    }
  };

  const handlePauseRecording = () => {
    setIsPaused(!isPaused);
    if (!isPaused && recordingIntervalRef.current) {
      clearInterval(recordingIntervalRef.current);
    } else {
      recordingIntervalRef.current = setInterval(() => {
        setRecordingTime((prev) => prev + 1);
      }, 1000);
    }
  };

  const handleSnapshot = () => {
    if (streamUrl) {
      const a = document.createElement('a');
      a.href = streamUrl;
      a.download = `snapshot_${type}_${Date.now()}.jpg`;
      a.click();
    }
  };

  const toggleFullscreen = () => {
    if (!containerRef.current) return;

    if (!document.fullscreenElement) {
      containerRef.current.requestFullscreen();
      setIsFullscreen(true);
    } else {
      document.exitFullscreen();
      setIsFullscreen(false);
    }
  };

  useEffect(() => {
    const handleFullscreenChange = () => {
      setIsFullscreen(!!document.fullscreenElement);
    };

    document.addEventListener('fullscreenchange', handleFullscreenChange);
    return () => {
      document.removeEventListener('fullscreenchange', handleFullscreenChange);
      if (recordingIntervalRef.current) {
        clearInterval(recordingIntervalRef.current);
      }
    };
  }, []);

  const formatTime = (seconds: number) => {
    const mins = Math.floor(seconds / 60);
    const secs = seconds % 60;
    return `${mins.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
  };

  return (
    <div ref={containerRef} className="h-full flex flex-col p-6">
      <div className="mb-6 flex items-center justify-between">
        <div className="flex items-center gap-3">
          <h3 className="text-2xl">
            {type === 'screen' ? 'Screen Streaming' : 'Webcam'}
          </h3>
          <HelpButton
            title={type === 'screen' ? 'Screen Streaming' : 'Webcam Streaming'}
            description={`View the remote device's ${type === 'screen' ? 'screen' : 'webcam'} in real-time. This is view-only mode.`}
            instructions={[
              'Click "Start Streaming" to begin',
              'Use the record button to save the stream',
              'Take snapshots with the camera button',
              'For mouse/keyboard control, use Remote Control',
            ]}
          />
        </div>
        {isStreaming && (
          <Button
            onClick={toggleFullscreen}
            variant="outline"
            size="sm"
          >
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" className="mr-2">
              <path d="M8 3H5a2 2 0 0 0-2 2v3m18 0V5a2 2 0 0 0-2-2h-3m0 18h3a2 2 0 0 0 2-2v-3M3 16v3a2 2 0 0 0 2 2h3" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
            </svg>
            {isFullscreen ? 'Exit Fullscreen' : 'Fullscreen'}
          </Button>
        )}
      </div>

      {/* Video Container */}
      <div className="flex-1 bg-gray-900 rounded-xl overflow-hidden relative mb-6 flex items-center justify-center">
        {!isStreaming ? (
          <div className="text-center">
            <motion.div
              initial={{ opacity: 0, scale: 0.95 }}
              animate={{ opacity: 1, scale: 1 }}
            >
              <div className="w-20 h-20 bg-white rounded-2xl flex items-center justify-center mx-auto mb-6">
                <svg width="40" height="40" viewBox="0 0 24 24" fill="none">
                  <rect x="2" y="6" width="20" height="12" rx="2" stroke="black" strokeWidth="2"/>
                  <circle cx="12" cy="12" r="3" stroke="black" strokeWidth="2"/>
                </svg>
              </div>
              <h4 className="text-white text-xl mb-3">
                {type === 'screen' ? 'Screen Stream' : 'Webcam Stream'}
              </h4>
              <p className="text-gray-400 mb-6">
                Ready to start streaming (view-only)
              </p>
              <Button
                onClick={handleStartStream}
                className="bg-white text-black hover:bg-gray-200"
              >
                Start Streaming
              </Button>
            </motion.div>
          </div>
        ) : (
          <>
            {/* Live Stream */}
            <div className="absolute inset-0 flex items-center justify-center bg-black">
              {streamUrl ? (
                <img
                  src={streamUrl}
                  alt="Live Stream"
                  className="max-w-full max-h-full object-contain"
                />
              ) : (
                <motion.div
                  animate={{
                    scale: [1, 1.1, 1],
                    opacity: [0.4, 0.7, 0.4]
                  }}
                  transition={{
                    duration: 3,
                    repeat: Infinity,
                    ease: "easeInOut"
                  }}
                  className="text-white text-center"
                >
                  <div className="w-16 h-16 border-4 border-white/30 border-t-white rounded-full animate-spin mx-auto mb-4" />
                  <p className="text-xl font-medium">Waiting for frames...</p>
                  <p className="text-sm text-gray-400 mt-2">Stream starting up</p>
                </motion.div>
              )}
            </div>

            {/* Recording Indicator */}
            {isRecording && (
              <motion.div
                initial={{ opacity: 0, y: -10 }}
                animate={{ opacity: 1, y: 0 }}
                className="absolute top-4 left-4 bg-red-600 text-white px-4 py-2 rounded-full flex items-center gap-2"
              >
                {!isPaused && (
                  <div className="w-3 h-3 rounded-full bg-white animate-pulse" />
                )}
                <span className="font-mono font-medium">{formatTime(recordingTime)}</span>
                {isPaused && <span className="text-xs">PAUSED</span>}
              </motion.div>
            )}

            {/* Info Overlay */}
            <div className="absolute bottom-4 left-4 bg-black/70 backdrop-blur-sm text-white px-3 py-2 rounded-lg text-sm">
              View-only mode
            </div>
          </>
        )}
      </div>

      {/* Controls */}
      {isStreaming && (
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          className="flex items-center gap-3 flex-wrap"
        >
          <Button
            onClick={handleStopStream}
            variant="outline"
            className="border-gray-300"
          >
            <Square className="w-4 h-4 mr-2" />
            Stop Stream
          </Button>

          <div className="h-6 w-px bg-gray-300" />

          {!isRecording ? (
            <Button
              onClick={handleStartRecording}
              className="bg-red-600 hover:bg-red-700 text-white"
            >
              <div className="w-3 h-3 rounded-full bg-white mr-2" />
              Record
            </Button>
          ) : (
            <>
              <Button
                onClick={handlePauseRecording}
                variant="outline"
                className="border-gray-300"
              >
                {isPaused ? (
                  <>
                    <svg width="16" height="16" viewBox="0 0 24 24" fill="none" className="mr-2">
                      <path d="M5 3l14 9-14 9V3z" fill="currentColor"/>
                    </svg>
                    Resume
                  </>
                ) : (
                  <>
                    <svg width="16" height="16" viewBox="0 0 24 24" fill="none" className="mr-2">
                      <rect x="6" y="4" width="4" height="16" fill="currentColor"/>
                      <rect x="14" y="4" width="4" height="16" fill="currentColor"/>
                    </svg>
                    Pause
                  </>
                )}
              </Button>
              <Button
                onClick={handleStopRecording}
                variant="outline"
                className="border-red-300 text-red-600 hover:bg-red-50"
              >
                <Square className="w-4 h-4 mr-2" />
                Stop Recording
              </Button>
            </>
          )}

          <div className="h-6 w-px bg-gray-300" />

          <Button
            onClick={handleSnapshot}
            variant="outline"
            className="border-gray-300"
            disabled={!streamUrl}
          >
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" className="mr-2">
              <rect x="3" y="6" width="18" height="13" rx="2" stroke="currentColor" strokeWidth="2"/>
              <circle cx="12" cy="12" r="3" stroke="currentColor" strokeWidth="2"/>
            </svg>
            Snapshot
          </Button>
        </motion.div>
      )}
    </div>
  );
}
