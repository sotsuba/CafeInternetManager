import { useState } from 'react';
import { motion } from 'motion/react';
import { Search, X, Play, Square, Plus } from 'lucide-react';
import { Input } from '../ui/input';
import { Button } from '../ui/button';
import { Dialog, DialogContent, DialogHeader, DialogTitle } from '../ui/dialog';

interface Application {
  id: string;
  name: string;
  icon: string;
  path: string;
  status: 'running' | 'stopped';
  instances?: number;
}

interface AppManagerProps {
  backendId: string;
}

function AppIcon({ type, className = '' }: { type: string; className?: string }) {
  const iconMap: Record<string, React.ReactNode> = {
    browser: (
      <svg width="32" height="32" viewBox="0 0 24 24" fill="none" className={className}>
        <circle cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="2" />
        <path d="M2 12h20M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z" stroke="currentColor" strokeWidth="2" />
      </svg>
    ),
    code: (
      <svg width="32" height="32" viewBox="0 0 24 24" fill="none" className={className}>
        <rect x="2" y="3" width="20" height="18" rx="2" stroke="currentColor" strokeWidth="2" />
        <path d="M8 10l-3 2 3 2M16 10l3 2-3 2M13 8l-2 8" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
      </svg>
    ),
    media: (
      <svg width="32" height="32" viewBox="0 0 24 24" fill="none" className={className}>
        <circle cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="2" />
        <path d="M10 8l6 4-6 4V8z" fill="currentColor" />
      </svg>
    ),
    chat: (
      <svg width="32" height="32" viewBox="0 0 24 24" fill="none" className={className}>
        <path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z" stroke="currentColor" strokeWidth="2" />
      </svg>
    ),
    document: (
      <svg width="32" height="32" viewBox="0 0 24 24" fill="none" className={className}>
        <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8l-6-6z" stroke="currentColor" strokeWidth="2" />
        <path d="M14 2v6h6M9 15h6M9 11h6" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
      </svg>
    ),
    calculator: (
      <svg width="32" height="32" viewBox="0 0 24 24" fill="none" className={className}>
        <rect x="4" y="2" width="16" height="20" rx="2" stroke="currentColor" strokeWidth="2" />
        <rect x="7" y="5" width="10" height="4" rx="1" stroke="currentColor" strokeWidth="2" />
        <line x1="7" y1="14" x2="9" y2="14" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
        <line x1="7" y1="17" x2="9" y2="17" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
        <line x1="12" y1="14" x2="14" y2="14" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
        <line x1="12" y1="17" x2="14" y2="17" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
        <line x1="16" y1="13.5" x2="18" y2="17.5" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
        <line x1="18" y1="13.5" x2="16" y2="17.5" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
      </svg>
    ),
    image: (
      <svg width="32" height="32" viewBox="0 0 24 24" fill="none" className={className}>
        <rect x="3" y="3" width="18" height="18" rx="2" stroke="currentColor" strokeWidth="2" />
        <circle cx="8.5" cy="8.5" r="1.5" fill="currentColor" />
        <path d="M21 15l-5-5L5 21" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" />
      </svg>
    ),
    folder: (
      <svg width="32" height="32" viewBox="0 0 24 24" fill="none" className={className}>
        <path d="M3 7v10a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2V9a2 2 0 0 0-2-2h-7l-2-2H5a2 2 0 0 0-2 2z" stroke="currentColor" strokeWidth="2" />
      </svg>
    ),
    terminal: (
      <svg width="32" height="32" viewBox="0 0 24 24" fill="none" className={className}>
        <rect x="2" y="4" width="20" height="16" rx="2" stroke="currentColor" strokeWidth="2" />
        <path d="M6 9l4 3-4 3M13 15h5" stroke="currentColor" strokeWidth="2" strokeLinecap="round" />
      </svg>
    ),
  };

  return iconMap[type] || iconMap.document;
}

const mockApps: Application[] = [
  { id: '1', name: 'Google Chrome', icon: 'browser', path: 'C:\\Program Files\\Google\\Chrome\\chrome.exe', status: 'running', instances: 3 },
  { id: '2', name: 'Visual Studio Code', icon: 'code', path: 'C:\\Program Files\\VS Code\\Code.exe', status: 'running', instances: 1 },
  { id: '3', name: 'Spotify', icon: 'media', path: 'C:\\Users\\User\\AppData\\Roaming\\Spotify\\Spotify.exe', status: 'running', instances: 1 },
  { id: '4', name: 'Discord', icon: 'chat', path: 'C:\\Users\\User\\AppData\\Local\\Discord\\Discord.exe', status: 'running', instances: 1 },
  { id: '5', name: 'Notepad', icon: 'document', path: 'C:\\Windows\\System32\\notepad.exe', status: 'stopped' },
  { id: '6', name: 'Calculator', icon: 'calculator', path: 'C:\\Windows\\System32\\calc.exe', status: 'stopped' },
  { id: '7', name: 'Paint', icon: 'image', path: 'C:\\Windows\\System32\\mspaint.exe', status: 'stopped' },
  { id: '8', name: 'Microsoft Edge', icon: 'browser', path: 'C:\\Program Files\\Microsoft\\Edge\\msedge.exe', status: 'stopped' },
  { id: '9', name: 'File Explorer', icon: 'folder', path: 'C:\\Windows\\explorer.exe', status: 'running', instances: 2 },
  { id: '10', name: 'Terminal', icon: 'terminal', path: 'C:\\Program Files\\WindowsApps\\Terminal\\wt.exe', status: 'stopped' },
];

export function AppManager({ backendId }: AppManagerProps) {
  const [apps, setApps] = useState<Application[]>(mockApps);
  const [searchQuery, setSearchQuery] = useState('');
  const [showLauncher, setShowLauncher] = useState(false);
  const [launcherQuery, setLauncherQuery] = useState('');

  const filteredApps = apps.filter((app) =>
    app.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
    app.path.toLowerCase().includes(searchQuery.toLowerCase())
  );

  const launcherApps = apps.filter((app) =>
    app.name.toLowerCase().includes(launcherQuery.toLowerCase())
  );

  const handleStopApp = (appId: string) => {
    // Instant stop - send command immediately
    console.log('Stopping app:', appId);
    setApps((prev) =>
      prev.map((app) =>
        app.id === appId ? { ...app, status: 'stopped' as const, instances: 0 } : app
      )
    );
  };

  const handleStartApp = (appId: string) => {
    // Instant start - send command immediately
    console.log('Starting app:', appId);
    setApps((prev) =>
      prev.map((app) =>
        app.id === appId ? { ...app, status: 'running' as const, instances: 1 } : app
      )
    );
    setShowLauncher(false);
    setLauncherQuery('');
  };

  const runningApps = filteredApps.filter((app) => app.status === 'running');
  const availableApps = filteredApps.filter((app) => app.status === 'stopped');

  return (
    <div className="h-full flex flex-col p-6">
      <div className="mb-6">
        <h3 className="text-2xl mb-2">Application Manager</h3>
        <p className="text-gray-600">
          Quản lý ứng dụng đang chạy và khởi động ứng dụng mới
        </p>
      </div>

      {/* Search and Actions */}
      <div className="flex items-center gap-3 mb-6">
        <div className="relative flex-1">
          <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-5 h-5 text-gray-400" />
          <Input
            type="text"
            placeholder="Tìm kiếm ứng dụng..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            className="pl-10 h-12"
          />
          {searchQuery && (
            <button
              onClick={() => setSearchQuery('')}
              className="absolute right-3 top-1/2 -translate-y-1/2 text-gray-400 hover:text-gray-600"
            >
              <X className="w-5 h-5" />
            </button>
          )}
        </div>

        <Button
          onClick={() => setShowLauncher(true)}
          className="bg-black hover:bg-gray-800 text-white h-12"
        >
          <Plus className="w-4 h-4 mr-2" />
          Start App
        </Button>
      </div>

      <div className="flex-1 overflow-y-auto space-y-8">
        {/* Running Apps */}
        <div>
          <h4 className="mb-4 text-lg">
            Running Applications ({runningApps.length})
          </h4>
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            {runningApps.map((app, index) => (
              <motion.div
                key={app.id}
                initial={{ opacity: 0, y: 20 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ delay: index * 0.05 }}
                className="bg-white border-2 border-gray-200 rounded-xl p-4 hover:border-black transition-all"
              >
                <div className="flex items-start gap-4">
                  <AppIcon type={app.icon} className="text-4xl" />
                  <div className="flex-1 min-w-0">
                    <h5 className="mb-1 truncate">{app.name}</h5>
                    <p className="text-sm text-gray-500 font-mono truncate mb-2">
                      {app.path}
                    </p>
                    <div className="flex items-center gap-2">
                      <span className="inline-flex items-center gap-1 text-xs bg-green-100 text-green-700 px-2 py-1 rounded-full">
                        <div className="w-1.5 h-1.5 bg-green-500 rounded-full animate-pulse" />
                        Running
                      </span>
                      {app.instances && app.instances > 1 && (
                        <span className="text-xs bg-blue-100 text-blue-700 px-2 py-1 rounded-full">
                          {app.instances} instances
                        </span>
                      )}
                    </div>
                  </div>
                  <Button
                    onClick={() => handleStopApp(app.id)}
                    variant="outline"
                    size="sm"
                    className="border-red-300 text-red-600 hover:bg-red-50"
                  >
                    <Square className="w-4 h-4 mr-1" />
                    Stop
                  </Button>
                </div>
              </motion.div>
            ))}
          </div>
          {runningApps.length === 0 && (
            <div className="text-center py-12 text-gray-500">
              Không có ứng dụng nào đang chạy
            </div>
          )}
        </div>

        {/* Available Apps */}
        <div>
          <h4 className="mb-4 text-lg">
            Available Applications ({availableApps.length})
          </h4>
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            {availableApps.map((app, index) => (
              <motion.div
                key={app.id}
                initial={{ opacity: 0, y: 20 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ delay: index * 0.05 }}
                className="bg-gray-50 border-2 border-gray-200 rounded-xl p-4 hover:border-gray-400 transition-all"
              >
                <div className="flex items-start gap-4">
                  <AppIcon type={app.icon} className="text-4xl opacity-60" />
                  <div className="flex-1 min-w-0">
                    <h5 className="mb-1 truncate text-gray-700">{app.name}</h5>
                    <p className="text-sm text-gray-400 font-mono truncate mb-2">
                      {app.path}
                    </p>
                    <span className="inline-block text-xs bg-gray-200 text-gray-600 px-2 py-1 rounded-full">
                      Stopped
                    </span>
                  </div>
                  <Button
                    onClick={() => handleStartApp(app.id)}
                    variant="outline"
                    size="sm"
                    className="border-green-300 text-green-600 hover:bg-green-50"
                  >
                    <Play className="w-4 h-4 mr-1" />
                    Start
                  </Button>
                </div>
              </motion.div>
            ))}
          </div>
        </div>
      </div>

      {/* App Launcher Dialog */}
      <Dialog open={showLauncher} onOpenChange={setShowLauncher}>
        <DialogContent className="sm:max-w-lg">
          <DialogHeader>
            <DialogTitle>Start Application</DialogTitle>
          </DialogHeader>
          <div className="space-y-4">
            <div className="relative">
              <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-5 h-5 text-gray-400" />
              <Input
                type="text"
                placeholder="Tìm kiếm và khởi động ứng dụng..."
                value={launcherQuery}
                onChange={(e) => setLauncherQuery(e.target.value)}
                className="pl-10"
                autoFocus
              />
            </div>

            <div className="max-h-96 overflow-y-auto space-y-2">
              {launcherApps.length > 0 ? (
                launcherApps.map((app) => (
                  <motion.button
                    key={app.id}
                    whileHover={{ scale: 1.02 }}
                    onClick={() => handleStartApp(app.id)}
                    className="w-full flex items-center gap-3 p-3 rounded-lg hover:bg-gray-100 transition-colors text-left"
                  >
                    <AppIcon type={app.icon} className="text-2xl" />
                    <div className="flex-1 min-w-0">
                      <p className="font-medium truncate">{app.name}</p>
                      <p className="text-xs text-gray-500 font-mono truncate">
                        {app.path}
                      </p>
                    </div>
                    {app.status === 'running' && (
                      <span className="text-xs bg-green-100 text-green-700 px-2 py-1 rounded-full">
                        Running
                      </span>
                    )}
                  </motion.button>
                ))
              ) : (
                <div className="text-center py-8 text-gray-500">
                  {launcherQuery ? 'Không tìm thấy ứng dụng' : 'Nhập tên ứng dụng để tìm kiếm'}
                </div>
              )}
            </div>
          </div>
        </DialogContent>
      </Dialog>
    </div>
  );
}