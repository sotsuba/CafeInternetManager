import { AlertCircle, Play, RefreshCw, Search, X } from 'lucide-react';
import { motion } from 'motion/react';
import { useEffect, useState } from 'react';
import { useGateway } from '../../services';
import { Button } from '../ui/button';
import { Checkbox } from '../ui/checkbox';
import { Dialog, DialogContent, DialogFooter, DialogHeader, DialogTitle } from '../ui/dialog';
import { Input } from '../ui/input';

interface ProcessManagerProps {
  backendId: string;
}

export function ProcessManager({ backendId }: ProcessManagerProps) {
  const { activeClient, sendCommand } = useGateway();
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedProcesses, setSelectedProcesses] = useState<Set<string>>(new Set());
  const [sortBy, setSortBy] = useState<'name' | 'pid'>('name');
  const [sortOrder, setSortOrder] = useState<'asc' | 'desc'>('asc');
  const [showLauncher, setShowLauncher] = useState(false);
  const [launchCommand, setLaunchCommand] = useState('');

  const [optimisticallyKilled, setOptimisticallyKilled] = useState<Set<number>>(new Set());
  const [optimisticallyLaunched, setOptimisticallyLaunched] = useState<string[]>([]);

  useEffect(() => {
    sendCommand('list_process');
  }, [backendId, sendCommand]);

  const processes = activeClient?.processes || [];

  // Cleanup optimistic state when actual processes reflect changes
  useEffect(() => {
    if (processes.length > 0) {
      const pids = new Set(processes.map(p => p.pid));

      setOptimisticallyKilled(prev => {
        const next = new Set(prev);
        let changed = false;
        for (const pid of prev) {
          if (!pids.has(pid)) {
            next.delete(pid);
            changed = true;
          }
        }
        return changed ? next : prev;
      });

      setOptimisticallyLaunched(prev => {
        const next = prev.filter(name => {
          const lowerName = name.toLowerCase();
          return !processes.some(p => p.name.toLowerCase().includes(lowerName) || p.cmd.toLowerCase().includes(lowerName));
        });
        return next.length !== prev.length ? next : prev;
      });
    }
  }, [processes]);

  const filteredProcesses = processes
    .filter((p) => !optimisticallyKilled.has(p.pid))
    .filter((p) =>
      p.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
      p.pid.toString().includes(searchQuery)
    )
    .sort((a, b) => {
      const multiplier = sortOrder === 'asc' ? 1 : -1;
      if (sortBy === 'name') return multiplier * a.name.localeCompare(b.name);
      if (sortBy === 'pid') return multiplier * (a.pid - b.pid);
      return 0;
    });

  const handleSelectProcess = (pid: number) => {
    const key = pid.toString();
    const newSelected = new Set(selectedProcesses);
    if (newSelected.has(key)) {
      newSelected.delete(key);
    } else {
      newSelected.add(key);
    }
    setSelectedProcesses(newSelected);
  };

  const handleSelectAll = () => {
    if (selectedProcesses.size === filteredProcesses.length) {
      setSelectedProcesses(new Set());
    } else {
      setSelectedProcesses(new Set(filteredProcesses.map((p) => p.pid.toString())));
    }
  };

  const handleKillSelected = () => {
    if (selectedProcesses.size === 0) return;
    if (!confirm(`Kill ${selectedProcesses.size} selected process(es)?`)) return;

    const pidsToKill = Array.from(selectedProcesses).map(Number);
    setOptimisticallyKilled(prev => {
      const next = new Set(prev);
      pidsToKill.forEach(pid => next.add(pid));
      return next;
    });

    pidsToKill.forEach((pid) => sendCommand(`kill_process ${pid}`));
    setSelectedProcesses(new Set());
    setTimeout(() => sendCommand('list_process'), 500);

    // Safety timeout
    setTimeout(() => {
      setOptimisticallyKilled(prev => {
        const next = new Set(prev);
        pidsToKill.forEach(pid => next.delete(pid));
        return next;
      });
    }, 5000);
  };

  const handleLaunchProcess = () => {
    if (!launchCommand.trim()) return;
    const cmd = launchCommand.trim();

    setOptimisticallyLaunched(prev => [...prev, cmd]);

    sendCommand(`launch_process "${cmd}"`);
    setShowLauncher(false);
    setLaunchCommand('');
    setTimeout(() => sendCommand('list_process'), 1000);

    // Safety timeout
    setTimeout(() => {
      setOptimisticallyLaunched(prev => prev.filter(c => c !== cmd));
    }, 8000);
  };

  const handleRefresh = () => {
    sendCommand('list_process');
  };

  const handleSort = (column: typeof sortBy) => {
    if (sortBy === column) {
      setSortOrder(sortOrder === 'asc' ? 'desc' : 'asc');
    } else {
      setSortBy(column);
      setSortOrder('asc');
    }
  };

  return (
    <div className="h-full flex flex-col p-6">
      <div className="mb-6 flex items-center justify-between">
        <div>
          <h3 className="text-2xl mb-2">Process Manager</h3>
          <p className="text-gray-600">Quản lý các tiến trình đang chạy trên thiết bị</p>
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
            placeholder="Search by name or PID..."
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
          className="bg-green-600 hover:bg-green-700 text-white h-12"
        >
          <Play className="w-4 h-4 mr-2" />
          Launch Process
        </Button>

        <Button
          onClick={handleKillSelected}
          disabled={selectedProcesses.size === 0}
          variant="destructive"
          className="h-12"
        >
          <AlertCircle className="w-4 h-4 mr-2" />
          Kill Selected ({selectedProcesses.size})
        </Button>
      </div>

      {/* Process Table */}
      <div className="flex-1 bg-white rounded-xl border border-gray-200 overflow-hidden flex flex-col">
        {/* Table Header */}
        <div className="bg-gray-50 border-b border-gray-200 px-4 py-3 flex items-center gap-4 text-sm font-medium">
          <div className="w-10">
            <Checkbox
              checked={selectedProcesses.size === filteredProcesses.length && filteredProcesses.length > 0}
              onCheckedChange={handleSelectAll}
            />
          </div>
          <button
            onClick={() => handleSort('name')}
            className="flex-1 text-left hover:text-black transition-colors"
          >
            Process Name {sortBy === 'name' && (sortOrder === 'asc' ? '↑' : '↓')}
          </button>
          <button
            onClick={() => handleSort('pid')}
            className="w-24 text-left hover:text-black transition-colors"
          >
            PID {sortBy === 'pid' && (sortOrder === 'asc' ? '↑' : '↓')}
          </button>
          <div className="w-28 text-center">Status</div>
        </div>

        {/* Table Body */}
        <div className="flex-1 overflow-y-auto">
          {filteredProcesses.length === 0 && optimisticallyLaunched.length === 0 ? (
            <div className="p-8 text-center text-gray-500">
              {searchQuery ? 'No processes match your search' : 'No processes found. Click refresh.'}
            </div>
          ) : (
            <>
              {/* Optimistically Launched */}
              {optimisticallyLaunched.map((cmd, index) => (
                <div
                  key={`launching-${index}`}
                  className="px-4 py-3 flex items-center gap-4 border-b border-gray-100 bg-blue-50/30 animate-pulse"
                >
                  <div className="w-10" />
                  <div className="flex-1 font-mono text-sm truncate opacity-60">{cmd}</div>
                  <div className="w-24 font-mono text-sm text-gray-400">---</div>
                  <div className="w-28 text-center">
                    <span className="inline-block px-2 py-1 rounded-full text-xs bg-blue-100 text-blue-700">
                      Starting...
                    </span>
                  </div>
                </div>
              ))}

              {/* Actual Processes */}
              {filteredProcesses.map((process, index) => (
                <motion.div
                  key={process.pid}
                  initial={{ opacity: 0, x: -20 }}
                  animate={{ opacity: 1, x: 0 }}
                  transition={{ delay: Math.min(index * 0.02, 0.5) }}
                  className={`
                    px-4 py-3 flex items-center gap-4 border-b border-gray-100 hover:bg-gray-50 transition-colors
                    ${selectedProcesses.has(process.pid.toString()) ? 'bg-blue-50' : ''}
                  `}
                >
                  <div className="w-10">
                    <Checkbox
                      checked={selectedProcesses.has(process.pid.toString())}
                      onCheckedChange={() => handleSelectProcess(process.pid)}
                    />
                  </div>
                  <div className="flex-1 font-mono text-sm truncate">{process.name}</div>
                  <div className="w-24 font-mono text-sm text-gray-600">{process.pid}</div>
                  <div className="w-28 text-center">
                    <span className={`inline-block px-2 py-1 rounded-full text-xs ${
                      process.status === 'Running' ? 'bg-green-100 text-green-700' :
                      process.status === 'Sleeping' ? 'bg-yellow-100 text-yellow-700' :
                      'bg-gray-100 text-gray-700'
                    }`}>
                      {process.status || 'Running'}
                    </span>
                  </div>
                </motion.div>
              ))}
            </>
          )}
        </div>
      </div>

      {/* Stats */}
      <div className="mt-4 flex items-center justify-between text-sm text-gray-600">
        <div>
          Total: {filteredProcesses.length} processes
          {searchQuery && ` (filtered from ${processes.length})`}
        </div>

      </div>

      {/* Launch Process Dialog */}
      <Dialog open={showLauncher} onOpenChange={setShowLauncher}>
        <DialogContent className="sm:max-w-md">
          <DialogHeader>
            <DialogTitle>Launch Process</DialogTitle>
          </DialogHeader>
          <div className="space-y-4 py-4">
            <div>
              <label className="block text-sm font-medium mb-2">Command or Path</label>
              <Input
                type="text"
                placeholder="e.g., notepad.exe, calc, /usr/bin/firefox"
                value={launchCommand}
                onChange={(e) => setLaunchCommand(e.target.value)}
                onKeyDown={(e) => e.key === 'Enter' && handleLaunchProcess()}
                autoFocus
              />
              <p className="text-xs text-gray-500 mt-2">
                Enter the executable name or full path to launch
              </p>
            </div>
          </div>
          <DialogFooter>
            <Button variant="outline" onClick={() => setShowLauncher(false)}>Cancel</Button>
            <Button onClick={handleLaunchProcess} disabled={!launchCommand.trim()} className="bg-green-600 hover:bg-green-700">
              <Play className="w-4 h-4 mr-2" /> Launch
            </Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>
    </div>
  );
}
