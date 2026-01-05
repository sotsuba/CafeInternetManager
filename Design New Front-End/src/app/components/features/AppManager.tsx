import { Play, Plus, RefreshCw, Search, Square, X } from 'lucide-react';
import { motion } from 'motion/react';
import { useEffect, useState } from 'react';
import { useGateway } from '../../services';
import { Button } from '../ui/button';
import { Dialog, DialogContent, DialogHeader, DialogTitle } from '../ui/dialog';
import { Input } from '../ui/input';

interface AppManagerProps {
  backendId: string;
}

function AppIcon({ className = '' }: { className?: string }) {
  return (
    <svg width="32" height="32" viewBox="0 0 24 24" fill="none" className={className}>
      <rect x="3" y="3" width="18" height="18" rx="2" stroke="currentColor" strokeWidth="2" />
      <path d="M9 3v18M3 9h18M3 15h18M15 3v18" stroke="currentColor" strokeWidth="2" />
    </svg>
  );
}

export function AppManager({ backendId }: AppManagerProps) {
  const { activeClient, sendCommand } = useGateway();
  const [searchQuery, setSearchQuery] = useState('');
  const [showLauncher, setShowLauncher] = useState(false);
  const [launcherQuery, setLauncherQuery] = useState('');

  useEffect(() => {
    sendCommand('list_apps');
    sendCommand('list_process');
  }, [backendId, sendCommand]);

  const apps = activeClient?.apps || [];
  const processes = activeClient?.processes || [];

  const isAppRunning = (appExec: string) => {
    const binName = appExec.split(/[\\/]/).pop()?.split(' ')[0]?.toLowerCase();
    if (!binName) return false;
    return processes.some(p => p.name.toLowerCase().includes(binName) || p.cmd.toLowerCase().includes(binName));
  };

  const getAppPids = (appExec: string) => {
    const binName = appExec.split(/[\\/]/).pop()?.split(' ')[0]?.toLowerCase();
    if (!binName) return [];
    return processes.filter(p => p.name.toLowerCase().includes(binName) || p.cmd.toLowerCase().includes(binName)).map(p => p.pid);
  };

  const filteredApps = apps.filter((app) =>
    app.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
    app.exec.toLowerCase().includes(searchQuery.toLowerCase())
  );

  const launcherApps = apps.filter((app) =>
    app.name.toLowerCase().includes(launcherQuery.toLowerCase())
  );

  const handleStopApp = (exec: string) => {
    const pids = getAppPids(exec);
    if (pids.length > 0 && confirm(`Kill all instances of this app (${pids.length} process)?`)) {
      pids.forEach(pid => sendCommand(`kill_process ${pid}`));
      setTimeout(() => sendCommand('list_process'), 1000);
    }
  };

  const handleStartApp = (exec: string) => {
    sendCommand(`launch_app "${exec}"`);
    setShowLauncher(false);
    setLauncherQuery('');
    setTimeout(() => sendCommand('list_process'), 1000);
  };

  const handleRefresh = () => {
    sendCommand('list_apps');
    sendCommand('list_process');
  };

  const runningApps = filteredApps.filter((app) => isAppRunning(app.exec));
  const availableApps = filteredApps.filter((app) => !isAppRunning(app.exec));

  return (
    <div className="h-full flex flex-col p-6">
      <div className="mb-6 flex items-center justify-between">
        <div>
          <h3 className="text-2xl mb-2">Application Manager</h3>
          <p className="text-gray-600">Quản lý ứng dụng đang chạy và khởi động ứng dụng mới</p>
        </div>
        <Button variant="outline" size="icon" onClick={handleRefresh}>
          <RefreshCw className="w-5 h-5" />
        </Button>
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
                transition={{ delay: Math.min(index * 0.05, 0.3) }}
                className="bg-white border-2 border-gray-200 rounded-xl p-4 hover:border-black transition-all"
              >
                <div className="flex items-start gap-4">
                  <AppIcon className="flex-shrink-0" />
                  <div className="flex-1 min-w-0">
                    <h5 className="mb-1 truncate">{app.name}</h5>
                    <p className="text-sm text-gray-500 font-mono truncate mb-2">
                      {app.exec}
                    </p>
                    <div className="flex items-center gap-2">
                      <span className="inline-flex items-center gap-1 text-xs bg-green-100 text-green-700 px-2 py-1 rounded-full">
                        <div className="w-1.5 h-1.5 bg-green-500 rounded-full animate-pulse" />
                        Running
                      </span>
                    </div>
                  </div>
                  <Button
                    onClick={() => handleStopApp(app.exec)}
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
                transition={{ delay: Math.min(index * 0.05, 0.3) }}
                className="bg-gray-50 border-2 border-gray-200 rounded-xl p-4 hover:border-gray-400 transition-all"
              >
                <div className="flex items-start gap-4">
                  <div className="opacity-60"><AppIcon /></div>
                  <div className="flex-1 min-w-0">
                    <h5 className="mb-1 truncate text-gray-700">{app.name}</h5>
                    <p className="text-sm text-gray-400 font-mono truncate mb-2">
                      {app.exec}
                    </p>
                    <span className="inline-block text-xs bg-gray-200 text-gray-600 px-2 py-1 rounded-full">
                      Stopped
                    </span>
                  </div>
                  <Button
                    onClick={() => handleStartApp(app.exec)}
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
                    onClick={() => handleStartApp(app.exec)}
                    className="w-full flex items-center gap-3 p-3 rounded-lg hover:bg-gray-100 transition-colors text-left"
                  >
                    <AppIcon className="text-2xl" />
                    <div className="flex-1 min-w-0">
                      <p className="font-medium truncate">{app.name}</p>
                      <p className="text-xs text-gray-500 font-mono truncate">
                        {app.exec}
                      </p>
                    </div>
                    {isAppRunning(app.exec) && (
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
