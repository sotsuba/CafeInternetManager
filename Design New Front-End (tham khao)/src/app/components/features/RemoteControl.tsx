import { useState, useRef, useEffect } from 'react';
import { motion, AnimatePresence } from 'motion/react';
import { X } from 'lucide-react';
import { Button } from '../ui/button';
import { LoadingState } from '../LoadingState';
import { HelpButton } from '../HelpButton';

interface RemoteControlProps {
  backendId: string;
}

type ControlMode = 'mouse' | 'keyboard';

export function RemoteControl({ backendId }: RemoteControlProps) {
  const [mode, setMode] = useState<ControlMode>('mouse');
  const [isStreaming, setIsStreaming] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [isFullscreen, setIsFullscreen] = useState(false);
  const [mousePosition, setMousePosition] = useState({ x: 0, y: 0 });
  const canvasRef = useRef<HTMLDivElement>(null);
  const containerRef = useRef<HTMLDivElement>(null);

  const handleStartControl = () => {
    setIsLoading(true);

    setTimeout(() => {
      setIsLoading(false);
      setIsStreaming(true);
    }, 1500);
  };

  const handleStopControl = () => {
    setIsStreaming(false);
  };

  const handleMouseMove = (e: React.MouseEvent<HTMLDivElement>) => {
    if (!canvasRef.current || mode !== 'mouse') return;

    const rect = canvasRef.current.getBoundingClientRect();
    const x = ((e.clientX - rect.left) / rect.width) * 100;
    const y = ((e.clientY - rect.top) / rect.height) * 100;

    setMousePosition({ x, y });
    console.log('Mouse move:', { x, y });
  };

  const handleMouseClick = (e: React.MouseEvent<HTMLDivElement>) => {
    if (mode !== 'mouse') return;
    console.log('Mouse click:', e.button);
  };

  const handleKeyPress = (e: React.KeyboardEvent<HTMLTextAreaElement>) => {
    console.log('Key press:', e.key);
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
    };
  }, []);

  return (
    <div ref={containerRef} className="h-full flex p-6 gap-6">
      {/* Left Sidebar - Mode Selection */}
      <div className="w-64 bg-gray-50 rounded-xl p-4 flex flex-col gap-3">
        <div className="mb-2">
          <h4 className="text-sm font-medium text-gray-500 uppercase tracking-wide mb-1">
            Control Mode
          </h4>
        </div>

        <button
          onClick={() => setMode('mouse')}
          className={`
            w-full p-4 rounded-lg transition-all text-left
            ${mode === 'mouse'
              ? 'bg-black text-white shadow-lg'
              : 'bg-white hover:bg-gray-100 border border-gray-200'
            }
          `}
        >
          <div className="flex items-center gap-3 mb-2">
            <svg
              width="24"
              height="24"
              viewBox="0 0 24 24"
              fill="none"
              className={mode === 'mouse' ? 'text-white' : 'text-gray-700'}
            >
              <path
                d="M12 2L12 12M12 2L8 6M12 2L16 6M12 12L6 18M12 12L18 18"
                stroke="currentColor"
                strokeWidth="2"
                strokeLinecap="round"
                strokeLinejoin="round"
              />
            </svg>
            <span className="font-medium">Mouse</span>
          </div>
          <p className={`text-xs ${mode === 'mouse' ? 'text-gray-300' : 'text-gray-500'}`}>
            Control cursor movement and clicks
          </p>
        </button>

        <button
          onClick={() => setMode('keyboard')}
          className={`
            w-full p-4 rounded-lg transition-all text-left
            ${mode === 'keyboard'
              ? 'bg-black text-white shadow-lg'
              : 'bg-white hover:bg-gray-100 border border-gray-200'
            }
          `}
        >
          <div className="flex items-center gap-3 mb-2">
            <svg
              width="24"
              height="24"
              viewBox="0 0 24 24"
              fill="none"
              className={mode === 'keyboard' ? 'text-white' : 'text-gray-700'}
            >
              <rect
                x="3"
                y="6"
                width="18"
                height="12"
                rx="2"
                stroke="currentColor"
                strokeWidth="2"
              />
              <path d="M7 10H7.01M11 10H11.01M15 10H15.01M7 14H17" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
            </svg>
            <span className="font-medium">Keyboard</span>
          </div>
          <p className={`text-xs ${mode === 'keyboard' ? 'text-gray-300' : 'text-gray-500'}`}>
            Send keyboard input and shortcuts
          </p>
        </button>

        {isStreaming && (
          <motion.div
            initial={{ opacity: 0, y: 10 }}
            animate={{ opacity: 1, y: 0 }}
            className="mt-auto pt-4 border-t border-gray-200"
          >
            <div className="flex items-center gap-2 text-sm text-gray-600 mb-3">
              <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse" />
              <span>Active</span>
            </div>
            <Button
              onClick={handleStopControl}
              variant="outline"
              className="w-full"
            >
              Stop Control
            </Button>
          </motion.div>
        )}
      </div>

      {/* Main Content */}
      <div className="flex-1 flex flex-col">
        <div className="flex items-center justify-between mb-4">
          <div className="flex items-center gap-3">
            <h3 className="text-2xl">Remote Control</h3>
            <HelpButton
              title="Remote Control"
              description={`Control the remote ${mode === 'mouse' ? 'mouse cursor' : 'keyboard input'} with live streaming. The stream starts automatically when you begin control.`}
              instructions={
                mode === 'mouse'
                  ? [
                      'Click "Start Control" to begin streaming',
                      'Move your mouse within the stream area to control the remote cursor',
                      'Click to send mouse clicks to the remote device',
                      'Use the fullscreen button for better control',
                    ]
                  : [
                      'Click "Start Control" to begin streaming',
                      'Type in the text area or focus the window to send keyboard input',
                      'All keystrokes will be sent to the remote device',
                      'Use "Stop Control" to end the session',
                    ]
              }
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

        {!isStreaming ? (
          <div className="flex-1 bg-gray-50 rounded-xl flex items-center justify-center">
            {isLoading ? (
              <LoadingState />
            ) : (
              <motion.div
                initial={{ opacity: 0, scale: 0.95 }}
                animate={{ opacity: 1, scale: 1 }}
                className="text-center max-w-md"
              >
                <div className="w-20 h-20 bg-black rounded-2xl flex items-center justify-center mx-auto mb-6">
                  {mode === 'mouse' ? (
                    <svg width="40" height="40" viewBox="0 0 24 24" fill="none">
                      <path d="M12 2L12 12M12 2L8 6M12 2L16 6M12 12L6 18M12 12L18 18" stroke="white" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
                    </svg>
                  ) : (
                    <svg width="40" height="40" viewBox="0 0 24 24" fill="none">
                      <rect x="3" y="6" width="18" height="12" rx="2" stroke="white" strokeWidth="2"/>
                      <path d="M7 10H7.01M11 10H11.01M15 10H15.01M7 14H17" stroke="white" strokeWidth="2" strokeLinecap="round"/>
                    </svg>
                  )}
                </div>
                <h4 className="text-xl mb-3">
                  {mode === 'mouse' ? 'Mouse Control' : 'Keyboard Control'}
                </h4>
                <p className="text-gray-600 mb-6">
                  {mode === 'mouse'
                    ? 'Control the remote cursor with live streaming'
                    : 'Send keyboard input to the remote device'
                  }
                </p>
                <Button
                  onClick={handleStartControl}
                  className="bg-black hover:bg-gray-800 text-white"
                >
                  Start Control
                </Button>
              </motion.div>
            )}
          </div>
        ) : (
          <div className="flex-1 flex flex-col gap-4">
            {/* Stream Display */}
            <div className="flex-1 bg-gray-900 rounded-xl overflow-hidden relative">
              <div
                ref={canvasRef}
                className="absolute inset-0 bg-gradient-to-br from-blue-900 to-purple-900 cursor-crosshair"
                onMouseMove={handleMouseMove}
                onClick={handleMouseClick}
              >
                <motion.div
                  animate={{
                    scale: [1, 1.05, 1],
                    opacity: [0.4, 0.6, 0.4],
                  }}
                  transition={{
                    duration: 3,
                    repeat: Infinity,
                    ease: 'easeInOut',
                  }}
                  className="absolute inset-0 flex items-center justify-center text-white text-center pointer-events-none"
                >
                  <div>
                    <div className="text-6xl mb-4">
                      {mode === 'mouse' ? (
                        <svg width="60" height="60" viewBox="0 0 24 24" fill="none" className="mx-auto">
                          <path d="M12 2L12 12M12 2L8 6M12 2L16 6M12 12L6 18M12 12L18 18" stroke="white" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
                        </svg>
                      ) : (
                        <svg width="60" height="60" viewBox="0 0 24 24" fill="none" className="mx-auto">
                          <rect x="3" y="6" width="18" height="12" rx="2" stroke="white" strokeWidth="2"/>
                          <path d="M7 10H7.01M11 10H11.01M15 10H15.01M7 14H17" stroke="white" strokeWidth="2" strokeLinecap="round"/>
                        </svg>
                      )}
                    </div>
                    <p className="text-xl font-medium">Live Stream Active</p>
                    <p className="text-sm text-gray-300 mt-2">1920×1080 • 30fps</p>
                  </div>
                </motion.div>

                {mode === 'mouse' && (
                  <motion.div
                    className="absolute w-4 h-4 bg-white rounded-full border-2 border-black pointer-events-none transform -translate-x-1/2 -translate-y-1/2 shadow-lg"
                    style={{
                      left: `${mousePosition.x}%`,
                      top: `${mousePosition.y}%`,
                    }}
                    initial={{ scale: 0 }}
                    animate={{ scale: 1 }}
                  />
                )}
              </div>

              {/* Mode Badge */}
              <div className="absolute top-4 left-4 bg-black/70 backdrop-blur-sm text-white px-4 py-2 rounded-full flex items-center gap-2">
                {mode === 'mouse' ? (
                  <svg width="16" height="16" viewBox="0 0 24 24" fill="none">
                    <path d="M12 2L12 12M12 2L8 6M12 2L16 6M12 12L6 18M12 12L18 18" stroke="white" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
                  </svg>
                ) : (
                  <svg width="16" height="16" viewBox="0 0 24 24" fill="none">
                    <rect x="3" y="6" width="18" height="12" rx="2" stroke="white" strokeWidth="2"/>
                    <path d="M7 10H7.01M11 10H11.01M15 10H15.01M7 14H17" stroke="white" strokeWidth="2" strokeLinecap="round"/>
                  </svg>
                )}
                <span className="text-sm font-medium">
                  {mode === 'mouse' ? 'Mouse' : 'Keyboard'} Control
                </span>
              </div>
            </div>

            {/* Keyboard Input Area */}
            {mode === 'keyboard' && (
              <motion.div
                initial={{ opacity: 0, y: 10 }}
                animate={{ opacity: 1, y: 0 }}
              >
                <label className="block mb-2 text-sm text-gray-600">
                  Type to send keyboard input:
                </label>
                <textarea
                  onKeyDown={handleKeyPress}
                  className="w-full h-24 px-4 py-3 border border-gray-300 rounded-xl resize-none focus:outline-none focus:ring-2 focus:ring-black"
                  placeholder="Type here to send input to the remote device..."
                  autoFocus
                />
              </motion.div>
            )}
          </div>
        )}
      </div>
    </div>
  );
}
