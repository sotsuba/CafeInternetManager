import { Square } from 'lucide-react';
import { motion } from 'motion/react';
import { useEffect, useRef, useState } from 'react';
import { useGateway } from '../../services';
import { InputHandler } from '../../services/input-handler';
import { HelpButton } from '../HelpButton';
import { Button } from '../ui/button';

interface RemoteControlProps {
  backendId: string;
}

export function RemoteControl({ backendId }: RemoteControlProps) {
  const { activeClient, sendCommand, wsClient } = useGateway();
  const streamImageRef = useRef<HTMLImageElement>(null);
  const inputHandlerRef = useRef<InputHandler | null>(null);

  // Optimistic local state for immediate UI feedback
  const [localActive, setLocalActive] = useState(false);

  // Get real state from backend
  const backendState = activeClient?.state.monitor || 'idle';
  const streamUrl = activeClient?.monitorFrame;

  // Use optimistic state for immediate UI feedback
  const isActive = localActive || backendState === 'active';

  // Sync local state with backend state
  useEffect(() => {
    if (backendState === 'active') {
      setLocalActive(true);
    } else if (backendState === 'idle') {
      setLocalActive(false);
    }
  }, [backendState]);

  // Attach/detach input handler when stream is active
  useEffect(() => {
    if (isActive && streamImageRef.current && wsClient && activeClient) {
      inputHandlerRef.current = new InputHandler(
        streamImageRef.current,
        wsClient,
        activeClient.id
      );
      inputHandlerRef.current.attach();

      return () => {
        inputHandlerRef.current?.detach();
        inputHandlerRef.current = null;
      };
    }
  }, [isActive, wsClient, activeClient]);

  const handleStart = () => {
    sendCommand('start_monitor_stream');
    setTimeout(() => setLocalActive(true), 500); // 500ms delay
  };

  const handleStop = () => {
    sendCommand('stop_monitor_stream');
    setTimeout(() => setLocalActive(false), 500); // 500ms delay
  };

  return (
    <div className="h-full flex flex-col p-6">
      <div className="flex items-center justify-between mb-6">
        <div className="flex items-center gap-3">
          <h3 className="text-2xl">Remote Control</h3>
          <HelpButton
            title="Remote Control"
            description="Control the remote device with your mouse and keyboard. The stream shows the remote screen and all your inputs are sent to the remote device."
            instructions={[
              'Click "Start Remote Control" to begin',
              'Mouse movements and clicks are captured',
              'Keyboard input is sent to remote device',
              'Click "Stop" to end the control session',
            ]}
          />
        </div>
      </div>

      {!isActive ? (
        <div className="flex-1 bg-gray-50 rounded-xl flex items-center justify-center">
          <motion.div
            initial={{ opacity: 0, scale: 0.95 }}
            animate={{ opacity: 1, scale: 1 }}
            className="text-center max-w-md"
          >
            <div className="w-20 h-20 bg-black rounded-2xl flex items-center justify-center mx-auto mb-6">
              <svg width="40" height="40" viewBox="0 0 24 24" fill="none">
                <rect x="2" y="3" width="20" height="14" rx="2" stroke="white" strokeWidth="2"/>
                <path d="M8 21h8M12 17v4" stroke="white" strokeWidth="2" strokeLinecap="round"/>
                <path d="M12 8L12 12M12 8L9 11M12 8L15 11" stroke="white" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
              </svg>
            </div>
            <h4 className="text-xl mb-3">Remote Control</h4>
            <p className="text-gray-600 mb-6">
              Take full control of the remote device. Your mouse and keyboard will be mirrored to the remote screen.
            </p>
            <Button
              onClick={handleStart}
              className="bg-black hover:bg-gray-800 text-white"
            >
              Start Remote Control
            </Button>
          </motion.div>
        </div>
      ) : (
        <div className="flex-1 flex flex-col gap-4">
          {/* Stream Container */}
          <div className="flex-1 bg-gray-900 rounded-xl overflow-hidden relative flex items-center justify-center">
            {streamUrl ? (
              <img
                ref={streamImageRef}
                src={streamUrl}
                alt="Remote Screen"
                className="max-w-full max-h-full object-contain cursor-crosshair"
                draggable={false}
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
                className="text-center text-white"
              >
                <div className="w-16 h-16 border-4 border-white/30 border-t-white rounded-full animate-spin mx-auto mb-4" />
                <p className="text-lg">Connecting to remote screen...</p>
                <p className="text-sm text-gray-400 mt-2">Mouse and keyboard will be active once connected</p>
              </motion.div>
            )}

            {/* Control Active Indicator */}
            <div className="absolute top-4 left-4 bg-black/70 backdrop-blur-sm text-white px-3 py-2 rounded-lg text-sm flex items-center gap-2">
              <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse" />
              <span>Control Active</span>
            </div>

            {/* Input Mode Indicator */}
            <div className="absolute top-4 right-4 bg-black/70 backdrop-blur-sm text-white px-3 py-2 rounded-lg text-sm">
              Mouse + Keyboard
            </div>
          </div>

          {/* Control Bar */}
          <div className="bg-black/70 backdrop-blur-sm text-white px-4 py-3 rounded-xl flex items-center justify-between">
            <div className="flex items-center gap-4">
              <div className="flex items-center gap-2">
                <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse" />
                <span className="text-sm">Remote Control Active</span>
              </div>
              <div className="text-sm text-gray-400">
                • Click and type on the stream to control
              </div>
            </div>
            <Button
              onClick={handleStop}
              variant="outline"
              size="sm"
              className="border-red-400 text-red-400 hover:bg-red-400/20"
            >
              <Square className="w-4 h-4 mr-2" />
              Stop Control
            </Button>
          </div>
        </div>
      )}
    </div>
  );
}
