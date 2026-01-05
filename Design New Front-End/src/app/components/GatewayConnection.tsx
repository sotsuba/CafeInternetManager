import { ArrowRight, Wifi, WifiOff } from 'lucide-react';
import { AnimatePresence, motion } from 'motion/react';
import { useEffect, useState } from 'react';
import { useGateway } from '../services';
import { StreamingText } from './StreamingText';
import { Button } from './ui/button';
import { Input } from './ui/input';

interface GatewayConnectionProps {
  onConnect: (ip: string, port: string, backends: Backend[]) => void;
  onSelectBackend?: (backend: Backend) => void;
  onDisconnect?: () => void;
}

interface Backend {
  id: string;
  hostname: string;
  os: string;
  status: 'online' | 'offline';
  ip: string;
  lastSeen: string;
  cpu?: number;
  memory?: number;
}

export function GatewayConnection({ onConnect, onSelectBackend }: GatewayConnectionProps) {
  const { connect, connectionStatus, clients } = useGateway();
  const [gatewayIP, setGatewayIP] = useState('192.168.1.1');
  const [gatewayPort, setGatewayPort] = useState('8888');
  const [showInstruction, setShowInstruction] = useState(true);

  const isConnecting = connectionStatus === 'CONNECTING';
  const isConnected = connectionStatus === 'CONNECTED';

  // Convert clients Map to Backend array for UI
  const backends: Backend[] = Array.from(clients.values()).map(client => ({
    id: client.id.toString(),
    hostname: client.hostname,
    os: client.os,
    status: client.status,
    ip: client.ip,
    lastSeen: `${Math.floor((Date.now() - client.lastSeen) / 1000)}s ago`,
    cpu: client.cpu,
    memory: client.memory,
  }));

  const handleConnect = () => {
    setShowInstruction(false);
    connect(gatewayIP, gatewayPort);
  };

  // Auto-notify when we get backends
  useEffect(() => {
    if (isConnected && backends.length > 0) {
      // Don't auto-navigate, let user select
    }
  }, [isConnected, backends.length]);

  const handleSelectBackend = (backend: Backend) => {
    if (onSelectBackend) {
      onSelectBackend(backend);
    }
    onConnect(gatewayIP, gatewayPort, backends);
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-gray-50 to-gray-100 relative overflow-hidden">
      {/* Background Pattern */}
      <div className="absolute inset-0 opacity-5">
        <svg width="100%" height="100%">
          <pattern id="gateway-grid" width="40" height="40" patternUnits="userSpaceOnUse">
            <circle cx="20" cy="20" r="1" fill="currentColor" />
          </pattern>
          <rect width="100%" height="100%" fill="url(#gateway-grid)" />
        </svg>
      </div>

      <div className="max-w-5xl mx-auto px-6 py-16 relative z-10">
        {/* Header */}
        <motion.div
          initial={{ opacity: 0, y: -20 }}
          animate={{ opacity: 1, y: 0 }}
          className="text-center mb-12"
        >
          <h1 className="text-4xl mb-3">Connect to Gateway</h1>
          <p className="text-gray-600 text-lg">
            Enter the gateway IP address and port to discover connected backends
          </p>
        </motion.div>

        {/* Connection Form */}
        <motion.div
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          transition={{ delay: 0.2 }}
          className="bg-white rounded-3xl border-2 border-gray-200 shadow-2xl overflow-hidden mb-8"
        >
          <div className="bg-gray-100 border-b border-gray-200 px-6 py-4">
            <h3 className="font-medium">Gateway Configuration</h3>
          </div>

          <div className="p-8">
            {showInstruction && (
              <motion.div
                initial={{ opacity: 0, y: 10 }}
                animate={{ opacity: 1, y: 0 }}
                className="mb-6 p-4 bg-blue-50 border border-blue-200 rounded-xl"
              >
                <div className="text-sm text-blue-900">
                  <StreamingText
                    text="Enter the IP address and port of your gateway server. The system will automatically discover all connected backend devices."
                    speed={20}
                  />
                </div>
              </motion.div>
            )}

            <div className="grid grid-cols-1 md:grid-cols-3 gap-6 mb-8">
              <div className="md:col-span-2">
                <label className="block mb-2 text-sm text-gray-600">
                  Gateway IP Address
                </label>
                <Input
                  type="text"
                  value={gatewayIP}
                  onChange={(e) => setGatewayIP(e.target.value)}
                  placeholder="192.168.1.1"
                  className="h-12 text-lg font-mono"
                  disabled={isConnecting || isConnected}
                />
              </div>

              <div>
                <label className="block mb-2 text-sm text-gray-600">
                  Port
                </label>
                <Input
                  type="text"
                  value={gatewayPort}
                  onChange={(e) => setGatewayPort(e.target.value)}
                  placeholder="8888"
                  className="h-12 text-lg font-mono"
                  disabled={isConnecting || isConnected}
                />
              </div>
            </div>

            {!isConnected && (
              <Button
                onClick={handleConnect}
                disabled={isConnecting || !gatewayIP || !gatewayPort}
                className="w-full h-12 bg-black hover:bg-gray-800 text-white rounded-xl text-lg group"
              >
                {isConnecting ? (
                  <>
                    <motion.div
                      animate={{ rotate: 360 }}
                      transition={{ duration: 1, repeat: Infinity, ease: 'linear' }}
                      className="w-5 h-5 border-2 border-white border-t-transparent rounded-full mr-3"
                    />
                    Connecting to Gateway...
                  </>
                ) : (
                  <>
                    Connect to Gateway
                    <ArrowRight className="ml-3 w-5 h-5 group-hover:translate-x-1 transition-transform" />
                  </>
                )}
              </Button>
            )}

            {isConnected && (
              <motion.div
                initial={{ opacity: 0, scale: 0.95 }}
                animate={{ opacity: 1, scale: 1 }}
                className="flex items-center justify-center gap-3 p-4 bg-green-50 border border-green-200 rounded-xl"
              >
                <Wifi className="w-5 h-5 text-green-600" />
                <span className="text-green-900 font-medium">
                  Connected to {gatewayIP}:{gatewayPort}
                </span>
              </motion.div>
            )}
          </div>
        </motion.div>

        {/* Discovered Backends */}
        <AnimatePresence>
          {isConnected && backends.length > 0 && (
            <motion.div
              initial={{ opacity: 0, y: 20 }}
              animate={{ opacity: 1, y: 0 }}
              className="bg-white rounded-3xl border-2 border-gray-200 shadow-2xl overflow-hidden"
            >
              <div className="bg-gray-100 border-b border-gray-200 px-6 py-4 flex items-center justify-between">
                <h3 className="font-medium">Discovered Backends ({backends.length})</h3>
                <div className="flex items-center gap-2 text-sm text-gray-600">
                  <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse" />
                  <span>Live</span>
                </div>
              </div>

              <div className="p-6 space-y-4">
                {backends.map((backend, index) => (
                  <motion.div
                    key={backend.id}
                    initial={{ opacity: 0, x: -20 }}
                    animate={{ opacity: 1, x: 0 }}
                    transition={{ delay: index * 0.1 }}
                    className="group relative bg-gray-50 hover:bg-gray-100 rounded-2xl p-6 border-2 border-gray-200 hover:border-black transition-all cursor-pointer"
                    onClick={() => handleSelectBackend(backend)}
                  >
                    <div className="flex items-start justify-between mb-4">
                      <div className="flex items-center gap-4">
                        <div className="w-14 h-14 bg-white rounded-xl flex items-center justify-center border border-gray-200">
                          {backend.os.includes('Windows') ? (
                            <svg width="28" height="28" viewBox="0 0 24 24" fill="none">
                              <path d="M3 3h8v8H3V3zm10 0h8v8h-8V3zM3 13h8v8H3v-8zm10 0h8v8h-8v-8z" fill="#0078D4" />
                            </svg>
                          ) : backend.os.includes('macOS') ? (
                            <svg width="28" height="28" viewBox="0 0 24 24" fill="none">
                              <path
                                d="M18.71 19.5c-.83 1.24-1.71 2.45-3.05 2.47-1.34.03-1.77-.79-3.29-.79-1.53 0-2 .77-3.27.82-1.31.05-2.3-1.32-3.14-2.53C4.25 17 2.94 12.45 4.7 9.39c.87-1.52 2.43-2.48 4.12-2.51 1.28-.02 2.5.87 3.29.87.78 0 2.26-1.07 3.81-.91.65.03 2.47.26 3.64 1.98-.09.06-2.17 1.28-2.15 3.81.03 3.02 2.65 4.03 2.68 4.04-.03.07-.42 1.44-1.38 2.83M13 3.5c.73-.83 1.94-1.46 2.94-1.5.13 1.17-.34 2.35-1.04 3.19-.69.85-1.83 1.51-2.95 1.42-.15-1.15.41-2.35 1.05-3.11z"
                                fill="#000000"
                              />
                            </svg>
                          ) : (
                            <svg width="28" height="28" viewBox="0 0 24 24" fill="none">
                              <circle cx="12" cy="12" r="10" stroke="#E95420" strokeWidth="2" />
                              <path d="M12 2v20M2 12h20" stroke="#E95420" strokeWidth="2" />
                            </svg>
                          )}
                        </div>
                        <div>
                          <h4 className="text-lg font-medium mb-1">{backend.hostname}</h4>
                          <p className="text-sm text-gray-600">{backend.os}</p>
                          <p className="text-sm text-gray-500 font-mono mt-1">{backend.ip}</p>
                        </div>
                      </div>

                      <div className="flex items-center gap-2">
                        {backend.status === 'online' ? (
                          <span className="flex items-center gap-2 text-xs bg-green-100 text-green-700 px-3 py-1.5 rounded-full">
                            <div className="w-1.5 h-1.5 bg-green-500 rounded-full animate-pulse" />
                            Online
                          </span>
                        ) : (
                          <span className="flex items-center gap-2 text-xs bg-gray-200 text-gray-600 px-3 py-1.5 rounded-full">
                            <WifiOff className="w-3 h-3" />
                            Offline
                          </span>
                        )}
                      </div>
                    </div>

                    {backend.cpu !== undefined && backend.memory !== undefined && (
                      <div className="grid grid-cols-2 gap-4">
                        <div>
                          <div className="flex items-center justify-between mb-2">
                            <span className="text-xs text-gray-500">CPU Usage</span>
                            <span className="text-xs font-mono font-medium">{backend.cpu}%</span>
                          </div>
                          <div className="h-2 bg-gray-200 rounded-full overflow-hidden">
                            <motion.div
                              initial={{ width: 0 }}
                              animate={{ width: `${backend.cpu}%` }}
                              transition={{ delay: index * 0.1 + 0.3, duration: 0.8 }}
                              className="h-full bg-blue-500 rounded-full"
                            />
                          </div>
                        </div>

                        <div>
                          <div className="flex items-center justify-between mb-2">
                            <span className="text-xs text-gray-500">Memory</span>
                            <span className="text-xs font-mono font-medium">{backend.memory}%</span>
                          </div>
                          <div className="h-2 bg-gray-200 rounded-full overflow-hidden">
                            <motion.div
                              initial={{ width: 0 }}
                              animate={{ width: `${backend.memory}%` }}
                              transition={{ delay: index * 0.1 + 0.3, duration: 0.8 }}
                              className="h-full bg-purple-500 rounded-full"
                            />
                          </div>
                        </div>
                      </div>
                    )}

                    {/* Hover indicator */}
                    <div className="absolute top-1/2 right-6 -translate-y-1/2 opacity-0 group-hover:opacity-100 transition-opacity">
                      <ArrowRight className="w-6 h-6 text-black" />
                    </div>
                  </motion.div>
                ))}
              </div>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Empty state when connected but no backends */}
        {isConnected && backends.length === 0 && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            className="bg-white rounded-3xl border-2 border-gray-200 shadow-2xl p-12 text-center"
          >
            <div className="w-16 h-16 bg-gray-100 rounded-full flex items-center justify-center mx-auto mb-4">
              <motion.div
                animate={{ rotate: 360 }}
                transition={{ duration: 2, repeat: Infinity, ease: 'linear' }}
              >
                <Wifi className="w-8 h-8 text-gray-400" />
              </motion.div>
            </div>
            <h3 className="text-xl mb-2">Discovering backends...</h3>
            <p className="text-gray-600">Waiting for backend devices to connect to the gateway.</p>
          </motion.div>
        )}
      </div>
    </div>
  );
}
