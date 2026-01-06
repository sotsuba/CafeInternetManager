import {
    ChevronDown,
    Download,
    Edit2,
    Plus,
    RefreshCw,
    Search,
    Trash2,
    Upload
} from 'lucide-react';
import { AnimatePresence, motion } from 'motion/react';
import { useEffect, useRef, useState } from 'react';
import { useGateway } from '../../services';
import type { FileEntry } from '../../services/types';
import { HelpButton } from '../HelpButton';
import { Button } from '../ui/button';
import { Input } from '../ui/input';

interface FileExplorerProps {
  backendId: string;
}

export function FileExplorer({ backendId }: FileExplorerProps) {
  const { activeClient, sendCommand, wsClient } = useGateway();
  const [currentPath, setCurrentPath] = useState('.');
  const [selectedFile, setSelectedFile] = useState<FileEntry | null>(null);
  const [searchQuery, setSearchQuery] = useState('');
  const [uploadProgress, setUploadProgress] = useState<number | null>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);
  const [isLoading, setIsLoading] = useState(false);

  useEffect(() => {
    // Check cache first
    const cached = activeClient?.filesCache?.get(currentPath);
    if (!cached) {
      setIsLoading(true);
      sendCommand(`file_list ${currentPath}`);
    } else {
      setIsLoading(false);
    }
    // Only re-run if these specific values change.
    // We check the CACHED entry specifically, not the whole cache object.
  }, [backendId, currentPath, sendCommand, activeClient?.filesCache?.get(currentPath)]);

  // Stop loading when files arrive
  useEffect(() => {
    if (activeClient?.files && activeClient.files.length >= 0) {
      setIsLoading(false);
    }
  }, [activeClient?.files]);

  // Use cached files if available, otherwise use live files
  const cachedFiles = activeClient?.filesCache?.get(currentPath);
  const files = cachedFiles || activeClient?.files || [];
  const isTransferring = activeClient?.state.fileTransfer !== 'idle';

  const filteredFiles = files.filter(f =>
    f.name.toLowerCase().includes(searchQuery.toLowerCase())
  );

  const handleNavigate = (newPath: string) => {
    if (isTransferring) return;
    console.log('[FileExplorer] Navigate to:', newPath, 'at:', Date.now());
    setIsLoading(true);
    setCurrentPath(newPath);
    setSelectedFile(null);
  };

  const handleRefresh = () => {
    if (isTransferring) return;
    console.log('[FileExplorer] Refresh at:', Date.now());
    // Force refresh: clear cache for current path and show loading
    setIsLoading(true);
    sendCommand(`file_list ${currentPath}`);
  };

  const handleDownloadFile = () => {
    if (isTransferring || !selectedFile || selectedFile.type === 'folder') return;
    sendCommand(`file_download ${selectedFile.path}`);
  };

  const handleUploadFile = () => {
    if (isTransferring) return;
    fileInputRef.current?.click();
  };

  const onFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file || !wsClient || !activeClient) return;

    const reader = new FileReader();
    reader.onload = () => {
      const arrayBuffer = reader.result as ArrayBuffer;
      const bytes = new Uint8Array(arrayBuffer);
      const slash = currentPath.includes('/') ? '/' : '\\';
      const targetPath = currentPath === '.' ? file.name : currentPath + (currentPath.endsWith(slash) ? '' : slash) + file.name;

      wsClient.sendText(activeClient.id, `file_upload_start ${targetPath} ${bytes.length}`);
      setUploadProgress(0);

      const CHUNK_SIZE = 1024 * 30;
      let offset = 0;

      const sendNextChunk = () => {
        if (offset >= bytes.length) {
          wsClient.sendText(activeClient.id, `file_upload_end ${targetPath}`);
          setTimeout(() => {
            handleRefresh();
            setUploadProgress(null);
          }, 500);
          return;
        }

        const chunk = bytes.slice(offset, offset + CHUNK_SIZE);
        let binary = '';
        for (let i = 0; i < chunk.length; i++) binary += String.fromCharCode(chunk[i]);
        const base64 = btoa(binary);

        wsClient.sendText(activeClient.id, `file_upload_chunk ${targetPath} ${base64}`);
        offset += CHUNK_SIZE;

        setUploadProgress(Math.round((offset / bytes.length) * 100));
        setTimeout(sendNextChunk, 10);
      };

      sendNextChunk();
    };
    reader.readAsArrayBuffer(file);
  };

  const handleMkdir = () => {
    const name = prompt('New Folder Name:');
    if (name) {
      const slash = currentPath.includes('/') ? '/' : '\\';
      const path = currentPath === '.' ? name : currentPath + (currentPath.endsWith(slash) ? '' : slash) + name;
      sendCommand(`file_mkdir ${path}`);
      setTimeout(handleRefresh, 500);
    }
  };

  const handleDelete = () => {
    if (selectedFile && confirm(`Delete ${selectedFile.name}?`)) {
      sendCommand(`file_delete ${selectedFile.path}`);
      setSelectedFile(null);
      setTimeout(handleRefresh, 500);
    }
  };

  const handleRename = () => {
    if (selectedFile) {
      const newName = prompt('Rename to:', selectedFile.name);
      if (newName && newName !== selectedFile.name) {
        const dirname = selectedFile.path.substring(0, selectedFile.path.lastIndexOf(selectedFile.path.includes('/') ? '/' : '\\'));
        const slash = selectedFile.path.includes('/') ? '/' : '\\';
        const newPath = dirname ? dirname + slash + newName : newName;
        sendCommand(`file_rename ${selectedFile.path} ${newPath}`);
        setTimeout(handleRefresh, 500);
      }
    }
  };

  const formatFileSize = (bytes?: number) => {
    if (!bytes) return '-';
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(2) + ' KB';
    if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
    return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB';
  };

  const getFileIcon = (file: FileEntry) => {
    if (file.type === 'folder') {
      return (
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
          <path d="M3 7v10a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2V9a2 2 0 0 0-2-2h-7l-2-2H5a2 2 0 0 0-2 2z" fill="#60A5FA" stroke="#3B82F6" strokeWidth="2"/>
        </svg>
      );
    }
    return (
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
        <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8l-6-6z" fill="#E5E7EB" stroke="#9CA3AF" strokeWidth="2"/>
        <path d="M14 2v6h6" stroke="#9CA3AF" strokeWidth="2"/>
      </svg>
    );
  };

  return (
    <div className="h-full flex flex-col p-6">
      <div className="mb-6 flex items-center justify-between">
        <div className="flex items-center gap-3">
          <h3 className="text-2xl">File Explorer</h3>
          <HelpButton
            title="File Explorer"
            description="Browse and manage files on the remote device."
            instructions={[
              'Click folders to navigate',
              'Select a file to see actions',
              'Upload/Download files directly',
            ]}
          />
        </div>
        <div className="flex gap-2">
          <Button variant="outline" size="sm" onClick={handleMkdir}>
            <Plus className="w-4 h-4 mr-2" /> New Folder
          </Button>
          <Button variant="outline" size="icon" onClick={handleRefresh}>
            <RefreshCw className="w-4 h-4" />
          </Button>
        </div>
      </div>

      {/* Path Bar */}
      <div className="flex items-center gap-3 mb-6">
        <div className="flex-1 flex items-center bg-gray-100 rounded-lg px-3 py-2 text-sm font-mono overflow-x-auto whitespace-nowrap">
          <span className="text-gray-500 mr-2">Path:</span>
          <span className="text-blue-600 font-semibold">{currentPath}</span>
        </div>
        <div className="relative w-64">
          <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-gray-400" />
          <Input
            type="text"
            placeholder="Filter files..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            className="pl-9 h-10 bg-white"
            disabled={isTransferring}
          />
        </div>
        <Button
          onClick={handleUploadFile}
          variant="outline"
          className="h-10 bg-white hover:bg-gray-50 border-gray-200"
          disabled={isTransferring}
        >
          <Upload className={`w-4 h-4 mr-2 ${uploadProgress !== null ? 'animate-bounce' : ''}`} />
          {uploadProgress !== null ? `Uploading (${uploadProgress}%)` : 'Upload'}
        </Button>
        <input type="file" ref={fileInputRef} onChange={onFileChange} style={{ display: 'none' }} />
      </div>

      {uploadProgress !== null && (
        <div className="mb-6 h-2 bg-gray-100 rounded-full overflow-hidden border border-gray-200">
          <motion.div
            className="h-full bg-blue-500"
            initial={{ width: 0 }}
            animate={{ width: `${uploadProgress}%` }}
          />
        </div>
      )}

      {activeClient?.state.fileTransfer === 'downloading' && (
        <div className="mb-6 p-3 bg-blue-50 border border-blue-100 rounded-xl flex items-center gap-3">
          <div className="w-8 h-8 rounded-full bg-blue-100 flex items-center justify-center text-blue-600 animate-pulse">
            <Download className="w-4 h-4" />
          </div>
          <div className="text-sm font-medium text-blue-700">Downloading file from remote backend...</div>
        </div>
      )}

      <div className="flex-1 flex gap-6 overflow-hidden">
        {/* File List */}
        <div className="flex-1 bg-white border border-gray-200 rounded-xl overflow-hidden flex flex-col shadow-sm">
          <div className="bg-gray-50 border-b border-gray-200 px-4 py-2.5 flex items-center text-xs font-bold text-gray-400 uppercase tracking-wider">
            <div className="flex-1">Name</div>
            <div className="w-24 text-right">Size</div>
          </div>
          {isLoading ? (
            <div className="flex-1 flex items-center justify-center p-8">
              <div className="animate-spin w-8 h-8 border-4 border-blue-500 border-t-transparent rounded-full"></div>
              <span className="ml-3 text-gray-500">Loading...</span>
            </div>
          ) : (
          <div className="flex-1 overflow-y-auto p-2 scrollbar-thin scrollbar-thumb-gray-200">
            {currentPath !== '.' && (
              <div
                className={`flex items-center gap-2 px-3 py-2.5 rounded-lg cursor-pointer transition-colors italic text-blue-500 ${isTransferring ? 'opacity-50 cursor-not-allowed' : 'hover:bg-blue-50'}`}
                onClick={() => {
                  if (isTransferring) return;
                  const lastSlash = Math.max(currentPath.lastIndexOf('/'), currentPath.lastIndexOf('\\'));
                  if (lastSlash === -1 || (lastSlash === currentPath.length - 1 && currentPath.indexOf('\\') === lastSlash)) {
                    handleNavigate('.');
                  } else {
                    handleNavigate(currentPath.substring(0, lastSlash) || '.');
                  }
                }}
              >
                <ChevronDown className="w-4 h-4 rotate-90" /> .. (Parent Directory)
              </div>
            )}
            {filteredFiles.map((file) => (
              <div
                key={file.path}
                className={`flex items-center gap-2 px-3 py-2.5 rounded-lg cursor-pointer transition-all ${selectedFile?.path === file.path ? 'bg-blue-50 border border-blue-100 shadow-sm' : 'hover:bg-gray-50'}`}
                onClick={() => {
                  if (file.type === 'folder') {
                    handleNavigate(file.path);
                  } else {
                    setSelectedFile(file);
                  }
                }}
              >
                {getFileIcon(file)}
                <span className={`flex-1 truncate ${file.type === 'folder' ? 'font-medium' : ''}`}>{file.name}</span>
                <div className="flex items-center gap-3">
                  {file.type === 'folder' ? (
                    <span className="text-[10px] bg-blue-100/50 text-blue-600 px-2 py-0.5 rounded font-bold">DIR</span>
                  ) : (
                    <span className="text-[10px] bg-gray-100/50 text-gray-500 px-2 py-0.5 rounded font-bold">FILE</span>
                  )}
                  <span className="text-xs text-gray-400 font-mono w-20 text-right">{formatFileSize(file.size)}</span>
                </div>
              </div>
            ))}
            {filteredFiles.length === 0 && !isTransferring && (
              <div className="h-40 flex flex-col items-center justify-center text-gray-400">
                <Search className="w-8 h-8 mb-2 opacity-20" />
                <p className="text-sm">No files found</p>
              </div>
            )}
          </div>
          )}
        </div>

        {/* File Details */}
        <div className="w-80 bg-white border border-gray-200 rounded-xl p-6 shadow-sm">
          <AnimatePresence mode="wait">
            {selectedFile ? (
              <motion.div key={selectedFile.path} initial={{ opacity: 0, scale: 0.95 }} animate={{ opacity: 1, scale: 1 }} exit={{ opacity: 0, scale: 0.95 }} className="space-y-6">
                <div className="text-center">
                  <div className="w-24 h-24 bg-gray-50 rounded-3xl flex items-center justify-center mx-auto mb-4 border border-gray-100">
                    <div className="scale-[2.0]">{getFileIcon(selectedFile)}</div>
                  </div>
                  <h4 className="font-bold text-gray-900 break-all">{selectedFile.name}</h4>
                  <p className="text-[10px] text-gray-400 font-mono break-all mt-3 bg-gray-50 p-2 rounded-lg">{selectedFile.path}</p>
                </div>

                <div className="grid grid-cols-2 gap-2 pt-4">
                  <Button variant="outline" size="sm" onClick={handleRename} disabled={isTransferring} className="bg-white"><Edit2 className="w-3 h-3 mr-2" /> Rename</Button>
                  <Button variant="outline" size="sm" className="text-red-500 border-red-50 bg-white hover:bg-red-50 hover:text-red-600" onClick={handleDelete} disabled={isTransferring}><Trash2 className="w-3 h-3 mr-2" /> Delete</Button>
                </div>

                {selectedFile.type === 'file' && (
                  <Button
                    onClick={handleDownloadFile}
                    className="w-full bg-blue-600 text-white hover:bg-blue-700 shadow-md shadow-blue-100"
                    disabled={isTransferring}
                  >
                    <Download className={`w-4 h-4 mr-2 ${activeClient?.state.fileTransfer === 'downloading' ? 'animate-bounce' : ''}`} />
                    {activeClient?.state.fileTransfer === 'downloading' ? 'Downloading...' : 'Download'}
                  </Button>
                )}
              </motion.div>
            ) : (
              <div className="h-full flex items-center justify-center text-center text-gray-400">
                <div className="px-4">
                  <div className="w-16 h-16 rounded-3xl bg-gray-50 flex items-center justify-center mx-auto mb-4">
                    <Search className="w-8 h-8 opacity-20" />
                  </div>
                  <p className="text-sm font-medium">Select a file to view details</p>
                  <p className="text-xs mt-1 text-gray-300">Click on any file or folder to get started</p>
                </div>
              </div>
            )}
          </AnimatePresence>
        </div>
      </div>
    </div>
  );
}
