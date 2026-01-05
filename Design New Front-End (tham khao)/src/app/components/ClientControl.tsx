import { useState } from 'react';
import { motion, AnimatePresence } from 'motion/react';
import { X, ChevronRight } from 'lucide-react';
import { Button } from './ui/button';
import { ScreenViewer } from './features/ScreenViewer';
import { ProcessManager } from './features/ProcessManager';
import { AppManager } from './features/AppManager';
import { RemoteControl } from './features/RemoteControl';
import { FileExplorer } from './features/FileExplorer';
import { SystemPower } from './features/SystemPower';

interface ClientControlProps {
  backend: {
    id: string;
    hostname: string;
    os: string;
    ip: string;
  };
  onClose: () => void;
}

type FeatureType = 'screen' | 'webcam' | 'processes' | 'apps' | 'control' | 'files' | 'power';

interface MenuItem {
  id: FeatureType;
  icon: React.ReactNode;
  label: string;
}

const menuItems: MenuItem[] = [
  {
    id: 'screen',
    icon: (
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
        <rect x="2" y="3" width="20" height="14" rx="2" stroke="currentColor" strokeWidth="2" />
        <path d="M8 21h8M12 17v4" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
      </svg>
    ),
    label: 'Screen',
  },
  {
    id: 'webcam',
    icon: (
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
        <circle cx="12" cy="10" r="7" stroke="currentColor" strokeWidth="2" />
        <path d="M12 17v4M8 21h8" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
        <circle cx="12" cy="10" r="3" stroke="currentColor" strokeWidth="2" />
      </svg>
    ),
    label: 'Webcam',
  },
  {
    id: 'processes',
    icon: (
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
        <rect x="3" y="3" width="7" height="7" rx="1" stroke="currentColor" strokeWidth="2" />
        <rect x="14" y="3" width="7" height="7" rx="1" stroke="currentColor" strokeWidth="2" />
        <rect x="3" y="14" width="7" height="7" rx="1" stroke="currentColor" strokeWidth="2" />
        <rect x="14" y="14" width="7" height="7" rx="1" stroke="currentColor" strokeWidth="2" />
      </svg>
    ),
    label: 'Processes',
  },
  {
    id: 'apps',
    icon: (
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
        <rect x="3" y="3" width="18" height="18" rx="2" stroke="currentColor" strokeWidth="2" />
        <path d="M9 3v18M3 9h18M3 15h18M15 3v18" stroke="currentColor" strokeWidth="2" />
      </svg>
    ),
    label: 'Applications',
  },
  {
    id: 'control',
    icon: (
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
        <path
          d="M12 2L12 12M12 2L8 6M12 2L16 6M12 12L6 18M12 12L18 18"
          stroke="currentColor"
          strokeWidth="2"
          strokeLinecap="round"
          strokeLinejoin="round"
        />
      </svg>
    ),
    label: 'Remote Control',
  },
  {
    id: 'files',
    icon: (
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
        <path
          d="M3 7v10a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2V9a2 2 0 0 0-2-2h-7l-2-2H5a2 2 0 0 0-2 2z"
          stroke="currentColor"
          strokeWidth="2"
        />
      </svg>
    ),
    label: 'File Explorer',
  },
  {
    id: 'power',
    icon: (
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
        <path
          d="M18.36 6.64a9 9 0 1 1-12.73 0"
          stroke="currentColor"
          strokeWidth="2"
          strokeLinecap="round"
          strokeLinejoin="round"
        />
        <line x1="12" y1="2" x2="12" y2="12" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
      </svg>
    ),
    label: 'System Power',
  },
];

export function ClientControl({ backend, onClose }: ClientControlProps) {
  const [selectedFeature, setSelectedFeature] = useState<FeatureType>('screen');

  const renderFeature = () => {
    switch (selectedFeature) {
      case 'screen':
        return <ScreenViewer type="screen" backendId={backend.id} />;
      case 'webcam':
        return <ScreenViewer type="webcam" backendId={backend.id} />;
      case 'processes':
        return <ProcessManager backendId={backend.id} />;
      case 'apps':
        return <AppManager backendId={backend.id} />;
      case 'control':
        return <RemoteControl backendId={backend.id} />;
      case 'files':
        return <FileExplorer backendId={backend.id} />;
      case 'power':
        return <SystemPower backendId={backend.id} />;
      default:
        return null;
    }
  };

  return (
    <AnimatePresence>
      <motion.div
        initial={{ opacity: 0 }}
        animate={{ opacity: 1 }}
        exit={{ opacity: 0 }}
        className="fixed inset-0 bg-black/50 backdrop-blur-sm z-50 flex items-center justify-center p-4"
        onClick={onClose}
      >
        <motion.div
          initial={{ scale: 0.95, opacity: 0 }}
          animate={{ scale: 1, opacity: 1 }}
          exit={{ scale: 0.95, opacity: 0 }}
          transition={{ type: 'spring', duration: 0.4 }}
          className="w-full max-w-7xl h-[90vh] bg-white rounded-2xl shadow-2xl flex flex-col overflow-hidden"
          onClick={(e) => e.stopPropagation()}
        >
          {/* Header */}
          <div className="flex items-center justify-between px-6 py-4 border-b border-gray-200">
            <div>
              <h2 className="text-xl">{backend.hostname}</h2>
              <p className="text-sm text-gray-500">{backend.os} • {backend.ip}</p>
            </div>
            <Button
              variant="ghost"
              size="icon"
              onClick={onClose}
              className="hover:bg-gray-100 rounded-lg"
            >
              <X className="w-5 h-5" />
            </Button>
          </div>

          {/* Main Content */}
          <div className="flex flex-1 overflow-hidden">
            {/* Sidebar */}
            <motion.div
              initial={{ x: -20, opacity: 0 }}
              animate={{ x: 0, opacity: 1 }}
              transition={{ delay: 0.1 }}
              className="w-64 bg-gray-50 border-r border-gray-200 p-4 overflow-y-auto"
            >
              <nav className="space-y-1">
                {menuItems.map((item) => (
                  <button
                    key={item.id}
                    onClick={() => setSelectedFeature(item.id)}
                    className={`
                      w-full flex items-center gap-3 px-4 py-3 rounded-lg text-left transition-all
                      ${selectedFeature === item.id 
                        ? 'bg-black text-white' 
                        : 'hover:bg-gray-200 text-gray-700'
                      }
                    `}
                  >
                    {item.icon}
                    <span className="flex-1">{item.label}</span>
                    {selectedFeature === item.id && (
                      <ChevronRight className="w-4 h-4" />
                    )}
                  </button>
                ))}
              </nav>
            </motion.div>

            {/* Content Area */}
            <motion.div
              key={selectedFeature}
              initial={{ opacity: 0, x: 20 }}
              animate={{ opacity: 1, x: 0 }}
              transition={{ duration: 0.2 }}
              className="flex-1 overflow-y-auto"
            >
              {renderFeature()}
            </motion.div>
          </div>
        </motion.div>
      </motion.div>
    </AnimatePresence>
  );
}