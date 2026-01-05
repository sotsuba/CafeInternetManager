import { useState } from 'react';
import { LandingPage } from './components/LandingPage';
import { GatewayConnection } from './components/GatewayConnection';
import { ClientControl } from './components/ClientControl';

type AppState = 'landing' | 'gateway' | 'client';

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

function App() {
  const [appState, setAppState] = useState<AppState>('landing');
  const [selectedBackend, setSelectedBackend] = useState<Backend | null>(null);
  const [gatewayInfo, setGatewayInfo] = useState<{ ip: string; port: string } | null>(null);

  const handleGetStarted = () => {
    setAppState('gateway');
  };

  const handleConnect = (ip: string, port: string, backends: Backend[]) => {
    setGatewayInfo({ ip, port });
    setAppState('client');
    // Auto-select first backend for demo, but in real app user would select from list
    if (backends.length > 0) {
      setSelectedBackend(backends[0]);
    }
  };

  const handleCloseClient = () => {
    setSelectedBackend(null);
    setAppState('gateway');
  };

  return (
    <div className="min-h-screen bg-white">
      {appState === 'landing' && (
        <LandingPage onGetStarted={handleGetStarted} />
      )}

      {appState === 'gateway' && (
        <GatewayConnection onConnect={handleConnect} />
      )}

      {appState === 'client' && selectedBackend && (
        <ClientControl
          backend={selectedBackend}
          onClose={handleCloseClient}
        />
      )}
    </div>
  );
}

export default App;
