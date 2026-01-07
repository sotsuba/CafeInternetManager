import { useState } from 'react';
import { ClientControl } from './components/ClientControl';
import { GatewayConnection } from './components/GatewayConnection';
import { LandingPage } from './components/LandingPage';
import { GatewayProvider, useGateway } from './services';

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

function AppContent() {
  const [appState, setAppState] = useState<AppState>('landing');
  const [selectedBackend, setSelectedBackend] = useState<Backend | null>(null);
  const { setActiveClientId, disconnect } = useGateway();

  const handleGetStarted = () => {
    setAppState('gateway');
  };

  const handleConnect = (_ip: string, _port: string, backends: Backend[]) => {
    setAppState('client');
    if (backends.length > 0) {
      setSelectedBackend(backends[0]);
      // Set active client in context (convert string to number)
      setActiveClientId(parseInt(backends[0].id, 10));
    }
  };

  const handleSelectBackend = (backend: Backend) => {
    setSelectedBackend(backend);
    setActiveClientId(parseInt(backend.id, 10));
  };

  const handleCloseClient = () => {
    setSelectedBackend(null);
    setActiveClientId(null);
    setAppState('gateway');
  };

  const handleDisconnect = () => {
    disconnect();
    setSelectedBackend(null);
    setAppState('landing');
  };

  return (
    <div className="min-h-screen bg-white">
      {appState === 'landing' && (
        <LandingPage onGetStarted={handleGetStarted} />
      )}

      {appState === 'gateway' && (
        <GatewayConnection
          onConnect={handleConnect}
          onSelectBackend={handleSelectBackend}
          onDisconnect={handleDisconnect}
        />
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

function App() {
  return (
    <GatewayProvider>
      <AppContent />
    </GatewayProvider>
  );
}

export default App;
