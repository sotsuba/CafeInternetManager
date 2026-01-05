import { useState, useEffect, useCallback } from 'react';
import { motion, AnimatePresence } from 'motion/react';
import { ArrowRight, X } from 'lucide-react';
import { Button } from './ui/button';
import { FlowDiagram, FlowNode } from './FlowDiagram';
import { StreamingText } from './StreamingText';

interface LandingPageProps {
  onGetStarted: () => void;
}

const terminalLines = [
  'Remote Gateway Control System',
  'A distributed architecture for remote device management',
  'WebSocket ↔ Gateway ↔ Multi-Backend',
  'Real-time streaming, control, and monitoring',
];

const teamMembers = [
  'Nguyễn Danh Phương',
  'Nguyễn Đình Lê Vũ',
  'Nguyễn Tiến Sơn',
];

// System architecture flow nodes
const systemNodes: FlowNode[] = [
  {
    id: 'frontend',
    label: 'Frontend',
    subtitle: 'Vite + TS',
    icon: (
      <svg width="40" height="40" viewBox="0 0 24 24" fill="none">
        <rect x="2" y="3" width="20" height="14" rx="2" stroke="currentColor" strokeWidth="2" />
        <path d="M2 7h20M7 3v4" stroke="currentColor" strokeWidth="2" />
      </svg>
    ),
    description: 'Browser-based client with WebSocket connection',
    details: [
      'WebSocket client connects to Gateway on port 8888',
      'JMuxer for H.264 video decoding and real-time playback',
      'Per-client video stream handling with state management',
      'ACK-based flow control to prevent frame drops',
      'Real-time UI updates for all backend events',
    ],
    children: [
      {
        id: 'websocket',
        label: 'WebSocket',
        subtitle: 'Client Layer',
        icon: (
          <svg width="32" height="32" viewBox="0 0 24 24" fill="none">
            <path d="M12 2L2 7l10 5 10-5-10-5z" stroke="currentColor" strokeWidth="2" />
            <path d="M2 12l10 5 10-5M2 17l10 5 10-5" stroke="currentColor" strokeWidth="2" />
          </svg>
        ),
        description: 'WebSocket communication layer',
        details: [
          'Maintains persistent connection to Gateway',
          'Binary protocol with 12-byte header',
          'Handles automatic reconnection on disconnect',
          'Routes messages based on backend_id',
        ],
      },
      {
        id: 'jmuxer',
        label: 'JMuxer',
        subtitle: 'Video Decoder',
        icon: (
          <svg width="32" height="32" viewBox="0 0 24 24" fill="none">
            <rect x="2" y="6" width="20" height="12" rx="2" stroke="currentColor" strokeWidth="2" />
            <path d="M10 9l5 3-5 3V9z" fill="currentColor" />
          </svg>
        ),
        description: 'H.264 video stream decoder',
        details: [
          'Decodes H.264 video frames from backend',
          'Per-client JMuxer instance for multi-stream',
          'Renders to HTML5 video element',
          'Low-latency playback optimization',
        ],
      },
      {
        id: 'ui',
        label: 'UI Layer',
        subtitle: 'React Components',
        icon: (
          <svg width="32" height="32" viewBox="0 0 24 24" fill="none">
            <rect x="3" y="3" width="18" height="18" rx="2" stroke="currentColor" strokeWidth="2" />
            <path d="M9 9h6v6H9V9z" stroke="currentColor" strokeWidth="2" />
          </svg>
        ),
        description: 'React UI components',
        details: [
          'Feature modules: Screen, Webcam, Processes, Apps',
          'Remote Control: Mouse/Keyboard input injection',
          'File Explorer with tree navigation',
          'State machine for feature lifecycle',
        ],
      },
    ],
  },
  {
    id: 'gateway',
    label: 'Gateway',
    subtitle: 'C Native',
    icon: (
      <svg width="40" height="40" viewBox="0 0 24 24" fill="none">
        <path d="M12 2L2 7l10 5 10-5-10-5z" stroke="currentColor" strokeWidth="2" strokeLinejoin="round" />
        <path d="M2 17l10 5 10-5M2 12l10 5 10-5" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
      </svg>
    ),
    description: 'Multi-threaded router and message broker',
    details: [
      'WebSocket thread handles frontend connections',
      'Backend thread manages TCP control (9091) and data (9092)',
      'Discovery thread listens for UDP broadcasts on port 9999',
      'Monitor thread performs health checks and metrics',
      'Priority queue: Control messages never drop, Video frames can drop',
      'Circuit breaker pattern for backend connection failures',
    ],
    children: [
      {
        id: 'ws-thread',
        label: 'WS Thread',
        subtitle: 'Frontend Handler',
        icon: (
          <svg width="32" height="32" viewBox="0 0 24 24" fill="none">
            <circle cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="2" />
            <path d="M12 6v6l4 4" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
          </svg>
        ),
        description: 'WebSocket connection handler',
        details: [
          'Accepts WebSocket connections from frontends',
          'HTTP 101 upgrade handshake',
          'Assigns unique client_id to each connection',
          'Routes frames to backend thread via queues',
        ],
      },
      {
        id: 'backend-thread',
        label: 'Backend Thread',
        subtitle: 'Backend Handler',
        icon: (
          <svg width="32" height="32" viewBox="0 0 24 24" fill="none">
            <rect x="3" y="3" width="18" height="18" rx="2" stroke="currentColor" strokeWidth="2" />
            <circle cx="12" cy="12" r="4" stroke="currentColor" strokeWidth="2" />
          </svg>
        ),
        description: 'Backend connection manager',
        details: [
          'Maintains TCP connections to all discovered backends',
          'Dual-channel: Control (9091) + Data (9092)',
          'Routes messages based on client_id and backend_id',
          'Implements circuit breaker for failures',
        ],
      },
      {
        id: 'queue',
        label: 'Priority Queue',
        subtitle: 'Message Queue',
        icon: (
          <svg width="32" height="32" viewBox="0 0 24 24" fill="none">
            <rect x="4" y="4" width="6" height="6" rx="1" stroke="currentColor" strokeWidth="2" />
            <rect x="4" y="13" width="6" height="6" rx="1" stroke="currentColor" strokeWidth="2" />
            <rect x="14" y="4" width="6" height="6" rx="1" stroke="currentColor" strokeWidth="2" />
            <rect x="14" y="13" width="6" height="6" rx="1" stroke="currentColor" strokeWidth="2" />
          </svg>
        ),
        description: 'High/Low priority queuing',
        details: [
          'High priority: TRAFFIC_CONTROL (never drop)',
          'Low priority: TRAFFIC_VIDEO (can drop if full)',
          'ACK-based flow control for video frames',
          'Per-client ready_for_video flag',
        ],
      },
    ],
  },
  {
    id: 'backend',
    label: 'Backend',
    subtitle: 'C++ Native',
    icon: (
      <svg width="40" height="40" viewBox="0 0 24 24" fill="none">
        <rect x="2" y="3" width="20" height="18" rx="2" stroke="currentColor" strokeWidth="2" />
        <path d="M2 8h20M7 3v5M17 3v5" stroke="currentColor" strokeWidth="2" />
        <circle cx="7" cy="14" r="2" stroke="currentColor" strokeWidth="2" />
        <circle cx="17" cy="14" r="2" stroke="currentColor" strokeWidth="2" />
      </svg>
    ),
    description: 'Native agent with platform abstraction layer',
    details: [
      'Clean architecture: Core ↔ Interfaces ↔ Platform HAL',
      'Broadcast bus for pub/sub video streaming',
      'Command dispatcher routes requests to handlers',
      'Platform implementations: Windows/Linux/macOS',
      'Features: Screen/Webcam capture, Keylogger, App/Process manager',
      'UDP broadcast every 5s for gateway discovery',
    ],
    children: [
      {
        id: 'core',
        label: 'Core',
        subtitle: 'Business Logic',
        icon: (
          <svg width="32" height="32" viewBox="0 0 24 24" fill="none">
            <circle cx="12" cy="12" r="9" stroke="currentColor" strokeWidth="2" />
            <path d="M12 3v18M3 12h18" stroke="currentColor" strokeWidth="2" />
          </svg>
        ),
        description: 'Core business logic',
        details: [
          'BackendServer: Main server loop',
          'BroadcastBus: Pub/Sub for streaming',
          'StreamSession: Stream state machine',
          'GatewayConnection: TCP connection handler',
          'CommandDispatcher: Routes commands to handlers',
        ],
      },
      {
        id: 'platform',
        label: 'Platform HAL',
        subtitle: 'OS Abstraction',
        icon: (
          <svg width="32" height="32" viewBox="0 0 24 24" fill="none">
            <rect x="3" y="3" width="7" height="7" rx="1" stroke="currentColor" strokeWidth="2" />
            <rect x="14" y="3" width="7" height="7" rx="1" stroke="currentColor" strokeWidth="2" />
            <rect x="3" y="14" width="7" height="7" rx="1" stroke="currentColor" strokeWidth="2" />
            <rect x="14" y="14" width="7" height="7" rx="1" stroke="currentColor" strokeWidth="2" />
          </svg>
        ),
        description: 'Platform-specific implementations',
        details: [
          'Windows: DXGI screen capture, SetWindowsHookEx keylogger',
          'Linux: X11/XShm capture, /dev/input evdev keylogger',
          'macOS: ScreenCaptureKit, CGEvent keylogger',
          'FFmpeg H.264 encoding on all platforms',
        ],
      },
      {
        id: 'discovery',
        label: 'Discovery',
        subtitle: 'UDP Broadcast',
        icon: (
          <svg width="32" height="32" viewBox="0 0 24 24" fill="none">
            <circle cx="12" cy="12" r="3" stroke="currentColor" strokeWidth="2" />
            <circle cx="12" cy="12" r="7" stroke="currentColor" strokeWidth="2" opacity="0.5" />
            <circle cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="2" opacity="0.3" />
          </svg>
        ),
        description: 'Gateway discovery via UDP',
        details: [
          'Broadcasts UDP packet every 5 seconds',
          'Port 9999 with DISCOVERY_MAGIC (0xCAFE1234)',
          'Advertises hostname, IP, and TCP ports',
          'Gateway listens and connects automatically',
        ],
      },
    ],
  },
];

export function LandingPage({ onGetStarted }: LandingPageProps) {
  const [currentLineIndex, setCurrentLineIndex] = useState(0);
  const [completedLines, setCompletedLines] = useState<string[]>([]);
  const [showDiagram, setShowDiagram] = useState(false);
  const [selectedPath, setSelectedPath] = useState<string[]>([]);
  const [selectedNode, setSelectedNode] = useState<FlowNode | null>(null);
  const [showFlow, setShowFlow] = useState(false);
  const [flowStep, setFlowStep] = useState(0);
  const [currentTeamIndex, setCurrentTeamIndex] = useState(0);

  // Handle terminal line completion
  const handleLineComplete = useCallback(() => {
    setCompletedLines((prev) => [...prev, terminalLines[currentLineIndex]]);
    if (currentLineIndex < terminalLines.length - 1) {
      setTimeout(() => {
        setCurrentLineIndex((prev) => prev + 1);
      }, 400);
    } else {
      setTimeout(() => {
        setShowDiagram(true);
      }, 800);
    }
  }, [currentLineIndex]);

  // Team carousel
  useEffect(() => {
    const interval = setInterval(() => {
      setCurrentTeamIndex((prev) => (prev + 1) % teamMembers.length);
    }, 3000);
    return () => clearInterval(interval);
  }, []);

  // Flow animation
  useEffect(() => {
    if (showFlow) {
      const interval = setInterval(() => {
        setFlowStep((prev) => (prev + 1) % systemNodes.length);
      }, 2000);
      return () => clearInterval(interval);
    }
  }, [showFlow]);

  const handleNodeClick = useCallback((nodeId: string) => {
    // Find node in hierarchy
    const findNode = (nodes: FlowNode[], id: string, path: string[] = []): { node: FlowNode; path: string[] } | null => {
      for (const node of nodes) {
        const currentPath = [...path, node.id];
        if (node.id === id) {
          return { node, path: currentPath };
        }
        if (node.children) {
          const found = findNode(node.children, id, currentPath);
          if (found) return found;
        }
      }
      return null;
    };

    const result = findNode(systemNodes, nodeId);
    if (result) {
      setSelectedPath(result.path);
      setSelectedNode(result.node);
    }
  }, []);

  // Get current level nodes based on selected path
  const getCurrentLevelNodes = (): FlowNode[] => {
    if (selectedPath.length === 0) return systemNodes;

    let currentNodes = systemNodes;
    for (let i = 0; i < selectedPath.length - 1; i++) {
      const node = currentNodes.find((n) => n.id === selectedPath[i]);
      if (node?.children) {
        currentNodes = node.children;
      }
    }
    return currentNodes;
  };

  const currentLevelNodes = getCurrentLevelNodes();

  return (
    <div className="min-h-screen bg-gradient-to-br from-gray-50 to-gray-100 relative overflow-hidden">
      {/* Animated background pattern */}
      <div className="absolute inset-0 opacity-5">
        <svg width="100%" height="100%">
          <pattern id="grid" width="40" height="40" patternUnits="userSpaceOnUse">
            <circle cx="20" cy="20" r="1" fill="currentColor" />
          </pattern>
          <rect width="100%" height="100%" fill="url(#grid)" />
        </svg>
      </div>

      <div className="max-w-7xl mx-auto px-6 py-16 relative z-10">
        {/* Terminal Section */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          className="mb-16"
        >
          <div className="bg-white rounded-3xl border-2 border-gray-200 overflow-hidden shadow-2xl">
            {/* Terminal Header */}
            <div className="bg-gray-100 border-b border-gray-200 px-5 py-3 flex items-center gap-2">
              <div className="flex gap-2">
                <div className="w-3 h-3 rounded-full bg-red-500" />
                <div className="w-3 h-3 rounded-full bg-yellow-500" />
                <div className="w-3 h-3 rounded-full bg-green-500" />
              </div>
              <span className="ml-4 text-sm text-gray-600 font-mono">system</span>
            </div>

            {/* Terminal Content */}
            <div className="p-10 min-h-[320px] bg-white font-mono">
              <div className="space-y-4">
                {completedLines.map((line, index) => (
                  <motion.div
                    key={index}
                    initial={{ opacity: 0 }}
                    animate={{ opacity: 1 }}
                    className="text-xl text-gray-700 leading-relaxed"
                  >
                    {line}
                  </motion.div>
                ))}

                {currentLineIndex < terminalLines.length && completedLines.length < terminalLines.length && (
                  <div className="text-xl text-gray-900 font-medium leading-relaxed">
                    <StreamingText
                      text={terminalLines[currentLineIndex]}
                      speed={40}
                      onComplete={handleLineComplete}
                    />
                  </div>
                )}
              </div>

              {completedLines.length === terminalLines.length && (
                <motion.div
                  initial={{ opacity: 0, y: 10 }}
                  animate={{ opacity: 1, y: 0 }}
                  transition={{ delay: 0.5 }}
                  className="mt-10"
                >
                  <Button
                    onClick={onGetStarted}
                    className="bg-black hover:bg-gray-800 text-white px-8 py-6 text-lg rounded-xl group"
                  >
                    Get Started
                    <ArrowRight className="ml-3 w-5 h-5 group-hover:translate-x-1 transition-transform" />
                  </Button>
                </motion.div>
              )}
            </div>
          </div>

          {/* Team Carousel */}
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            transition={{ delay: 2 }}
            className="mt-6 text-center overflow-hidden"
          >
            <div className="text-sm text-gray-500 mb-2">Built by</div>
            <div className="h-8 relative">
              <AnimatePresence mode="wait">
                <motion.div
                  key={currentTeamIndex}
                  initial={{ y: 20, opacity: 0 }}
                  animate={{ y: 0, opacity: 1 }}
                  exit={{ y: -20, opacity: 0 }}
                  transition={{ duration: 0.5 }}
                  className="absolute inset-0 flex items-center justify-center"
                >
                  <span className="font-medium text-gray-900 text-lg">
                    {teamMembers[currentTeamIndex]}
                  </span>
                </motion.div>
              </AnimatePresence>
            </div>
          </motion.div>
        </motion.div>

        {/* System Architecture Diagram */}
        <AnimatePresence>
          {showDiagram && (
            <motion.div
              initial={{ opacity: 0, y: 40 }}
              animate={{ opacity: 1, y: 0 }}
              className="space-y-6"
            >
              <div className="flex items-center justify-between">
                <h2 className="text-3xl">System Architecture</h2>
                <Button
                  onClick={() => setShowFlow(!showFlow)}
                  variant="outline"
                  size="sm"
                  className="rounded-xl"
                >
                  {showFlow ? 'Stop Flow' : 'Show Data Flow'}
                </Button>
              </div>

              {/* Breadcrumb */}
              {selectedPath.length > 0 && (
                <div className="flex items-center gap-2 text-sm">
                  <button
                    onClick={() => {
                      setSelectedPath([]);
                      setSelectedNode(null);
                    }}
                    className="text-gray-500 hover:text-gray-900 transition-colors"
                  >
                    System
                  </button>
                  {selectedPath.map((id, index) => {
                    const node = systemNodes.find((n) => n.id === id) || 
                                 systemNodes.flatMap(n => n.children || []).find(n => n.id === id);
                    return (
                      <div key={id} className="flex items-center gap-2">
                        <span className="text-gray-400">/</span>
                        <button
                          onClick={() => {
                            setSelectedPath(selectedPath.slice(0, index + 1));
                            setSelectedNode(node || null);
                          }}
                          className="text-gray-700 hover:text-black transition-colors font-medium"
                        >
                          {node?.label}
                        </button>
                      </div>
                    );
                  })}
                </div>
              )}

              <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
                {/* Flow Diagram - centered when node selected */}
                <div className={`bg-white rounded-2xl border-2 border-gray-200 p-8 transition-all ${
                  selectedNode ? 'lg:col-span-3 flex justify-center' : 'lg:col-span-2'
                }`}>
                  <FlowDiagram
                    nodes={currentLevelNodes}
                    selectedPath={selectedPath}
                    onNodeClick={handleNodeClick}
                    showFlow={showFlow}
                    flowStep={flowStep}
                  />
                </div>

                {/* Details Panel - below diagram when node selected */}
                {selectedNode && (
                  <motion.div
                    key={selectedNode.id}
                    initial={{ opacity: 0, y: 20 }}
                    animate={{ opacity: 1, y: 0 }}
                    className="lg:col-span-3 bg-white rounded-2xl border-2 border-gray-200 p-8"
                  >
                    <div className="max-w-4xl mx-auto space-y-6">
                      <div className="flex items-center gap-4 pb-6 border-b border-gray-200">
                        <div className="w-16 h-16 flex items-center justify-center text-gray-700 bg-gray-50 rounded-2xl">
                          {selectedNode.icon}
                        </div>
                        <div>
                          <h3 className="text-2xl font-medium">{selectedNode.label}</h3>
                          {selectedNode.subtitle && (
                            <p className="text-gray-500 mt-1">{selectedNode.subtitle}</p>
                          )}
                        </div>
                      </div>

                      {selectedNode.description && (
                        <div className="text-lg text-gray-700">
                          <StreamingText text={selectedNode.description} speed={20} />
                        </div>
                      )}

                      {selectedNode.details && selectedNode.details.length > 0 && (
                        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                          {selectedNode.details.map((detail, index) => (
                            <motion.div
                              key={index}
                              initial={{ opacity: 0, y: 10 }}
                              animate={{ opacity: 1, y: 0 }}
                              transition={{ delay: index * 0.15 }}
                              className="flex items-start gap-3 p-4 bg-gray-50 rounded-xl border border-gray-200"
                            >
                              <div className="w-6 h-6 rounded-full bg-black text-white flex items-center justify-center text-xs flex-shrink-0 mt-0.5">
                                {index + 1}
                              </div>
                              <div className="text-sm text-gray-700 leading-relaxed">
                                <StreamingText text={detail} speed={15} />
                              </div>
                            </motion.div>
                          ))}
                        </div>
                      )}

                      {selectedNode.children && selectedNode.children.length > 0 && (
                        <motion.div
                          initial={{ opacity: 0 }}
                          animate={{ opacity: 1 }}
                          transition={{ delay: 0.5 }}
                          className="pt-6 border-t border-gray-200 text-center"
                        >
                          <p className="text-gray-600 mb-4">
                            This component has <span className="font-semibold text-black">{selectedNode.children.length} sub-components</span>
                          </p>
                          <p className="text-sm text-gray-500">
                            Click nodes in the diagram above to explore deeper
                          </p>
                        </motion.div>
                      )}
                    </div>
                  </motion.div>
                )}

                {/* Empty state when no node selected */}
                {!selectedNode && (
                  <div className="bg-white rounded-2xl border-2 border-gray-200 p-6">
                    <div className="h-full flex items-center justify-center text-center text-gray-400">
                      <div>
                        <svg
                          width="48"
                          height="48"
                          viewBox="0 0 24 24"
                          fill="none"
                          className="mx-auto mb-3 opacity-50"
                        >
                          <circle cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="2" />
                          <path d="M12 16v-4M12 8h.01" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
                        </svg>
                        <p className="text-sm">Click a component to view details</p>
                      </div>
                    </div>
                  </div>
                )}
              </div>
            </motion.div>
          )}
        </AnimatePresence>
      </div>
    </div>
  );
}