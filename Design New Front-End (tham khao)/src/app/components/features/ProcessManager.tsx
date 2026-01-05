import { useState } from 'react';
import { motion } from 'motion/react';
import { Search, X, AlertCircle } from 'lucide-react';
import { Input } from '../ui/input';
import { Button } from '../ui/button';
import { Checkbox } from '../ui/checkbox';

interface Process {
  id: string;
  name: string;
  pid: number;
  cpu: number;
  memory: number;
  status: 'Running' | 'Sleeping' | 'Stopped';
}

interface ProcessManagerProps {
  backendId: string;
}

const mockProcesses: Process[] = [
  { id: '1', name: 'chrome.exe', pid: 1234, cpu: 15.2, memory: 1024, status: 'Running' },
  { id: '2', name: 'code.exe', pid: 5678, cpu: 8.5, memory: 512, status: 'Running' },
  { id: '3', name: 'node.exe', pid: 9012, cpu: 5.1, memory: 256, status: 'Running' },
  { id: '4', name: 'firefox.exe', pid: 3456, cpu: 12.8, memory: 896, status: 'Running' },
  { id: '5', name: 'spotify.exe', pid: 7890, cpu: 3.2, memory: 384, status: 'Running' },
  { id: '6', name: 'discord.exe', pid: 2345, cpu: 6.7, memory: 448, status: 'Running' },
  { id: '7', name: 'explorer.exe', pid: 6789, cpu: 1.2, memory: 128, status: 'Running' },
  { id: '8', name: 'System', pid: 4, cpu: 0.3, memory: 64, status: 'Running' },
  { id: '9', name: 'docker.exe', pid: 8901, cpu: 4.5, memory: 320, status: 'Running' },
  { id: '10', name: 'python.exe', pid: 4567, cpu: 2.1, memory: 192, status: 'Sleeping' },
];

export function ProcessManager({ backendId }: ProcessManagerProps) {
  const [processes] = useState<Process[]>(mockProcesses);
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedProcesses, setSelectedProcesses] = useState<Set<string>>(new Set());
  const [sortBy, setSortBy] = useState<'name' | 'cpu' | 'memory' | 'pid'>('cpu');
  const [sortOrder, setSortOrder] = useState<'asc' | 'desc'>('desc');

  const filteredProcesses = processes
    .filter((p) => 
      p.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
      p.pid.toString().includes(searchQuery)
    )
    .sort((a, b) => {
      const multiplier = sortOrder === 'asc' ? 1 : -1;
      if (sortBy === 'name') {
        return multiplier * a.name.localeCompare(b.name);
      }
      return multiplier * (a[sortBy] - b[sortBy]);
    });

  const handleSelectProcess = (processId: string) => {
    const newSelected = new Set(selectedProcesses);
    if (newSelected.has(processId)) {
      newSelected.delete(processId);
    } else {
      newSelected.add(processId);
    }
    setSelectedProcesses(newSelected);
  };

  const handleSelectAll = () => {
    if (selectedProcesses.size === filteredProcesses.length) {
      setSelectedProcesses(new Set());
    } else {
      setSelectedProcesses(new Set(filteredProcesses.map((p) => p.id)));
    }
  };

  const handleKillSelected = () => {
    if (selectedProcesses.size === 0) return;
    // Send kill command immediately - no loading state
    console.log('Killing processes:', Array.from(selectedProcesses));
    setSelectedProcesses(new Set());
  };

  const handleSort = (column: typeof sortBy) => {
    if (sortBy === column) {
      setSortOrder(sortOrder === 'asc' ? 'desc' : 'asc');
    } else {
      setSortBy(column);
      setSortOrder('desc');
    }
  };

  return (
    <div className="h-full flex flex-col p-6">
      <div className="mb-6">
        <h3 className="text-2xl mb-2">Process Manager</h3>
        <p className="text-gray-600">
          Quản lý các tiến trình đang chạy trên thiết bị
        </p>
      </div>

      {/* Search and Actions */}
      <div className="flex items-center gap-3 mb-6">
        <div className="relative flex-1">
          <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-5 h-5 text-gray-400" />
          <Input
            type="text"
            placeholder="Tìm kiếm theo tên hoặc PID..."
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
          <button
            onClick={() => handleSort('cpu')}
            className="w-24 text-right hover:text-black transition-colors"
          >
            CPU {sortBy === 'cpu' && (sortOrder === 'asc' ? '↑' : '↓')}
          </button>
          <button
            onClick={() => handleSort('memory')}
            className="w-24 text-right hover:text-black transition-colors"
          >
            Memory {sortBy === 'memory' && (sortOrder === 'asc' ? '↑' : '↓')}
          </button>
          <div className="w-28 text-center">Status</div>
        </div>

        {/* Table Body */}
        <div className="flex-1 overflow-y-auto">
          {filteredProcesses.map((process, index) => (
            <motion.div
              key={process.id}
              initial={{ opacity: 0, x: -20 }}
              animate={{ opacity: 1, x: 0 }}
              transition={{ delay: index * 0.02 }}
              className={`
                px-4 py-3 flex items-center gap-4 border-b border-gray-100 hover:bg-gray-50 transition-colors
                ${selectedProcesses.has(process.id) ? 'bg-blue-50' : ''}
              `}
            >
              <div className="w-10">
                <Checkbox
                  checked={selectedProcesses.has(process.id)}
                  onCheckedChange={() => handleSelectProcess(process.id)}
                />
              </div>
              <div className="flex-1 font-mono text-sm">{process.name}</div>
              <div className="w-24 font-mono text-sm text-gray-600">{process.pid}</div>
              <div className="w-24 text-right">
                <span className={`
                  inline-block px-2 py-1 rounded text-sm
                  ${process.cpu > 10 ? 'bg-red-100 text-red-700' : 'bg-gray-100 text-gray-700'}
                `}>
                  {process.cpu.toFixed(1)}%
                </span>
              </div>
              <div className="w-24 text-right text-sm text-gray-600">
                {process.memory} MB
              </div>
              <div className="w-28 text-center">
                <span className={`
                  inline-block px-2 py-1 rounded-full text-xs
                  ${process.status === 'Running' ? 'bg-green-100 text-green-700' : ''}
                  ${process.status === 'Sleeping' ? 'bg-yellow-100 text-yellow-700' : ''}
                  ${process.status === 'Stopped' ? 'bg-gray-100 text-gray-700' : ''}
                `}>
                  {process.status}
                </span>
              </div>
            </motion.div>
          ))}
        </div>
      </div>

      {/* Stats */}
      <div className="mt-4 flex items-center justify-between text-sm text-gray-600">
        <div>
          Tổng: {filteredProcesses.length} processes
          {searchQuery && ` (filtered from ${processes.length})`}
        </div>
        <div>
          Total CPU: {processes.reduce((sum, p) => sum + p.cpu, 0).toFixed(1)}%
          {' • '}
          Total Memory: {processes.reduce((sum, p) => sum + p.memory, 0)} MB
        </div>
      </div>
    </div>
  );
}
