import { useState } from 'react';
import { motion, AnimatePresence } from 'motion/react';
import { 
  ChevronRight, 
  ChevronDown, 
  Download,
  Upload,
  Search,
  X,
} from 'lucide-react';
import { Button } from '../ui/button';
import { Input } from '../ui/input';
import { HelpButton } from '../HelpButton';

interface FileNode {
  id: string;
  name: string;
  type: 'file' | 'folder';
  path: string;
  size?: number;
  extension?: string;
  children?: FileNode[];
}

interface FileExplorerProps {
  backendId: string;
}

const mockFileSystem: FileNode[] = [
  {
    id: '1',
    name: 'C:',
    type: 'folder',
    path: 'C:',
    children: [
      {
        id: '2',
        name: 'Users',
        type: 'folder',
        path: 'C:\\Users',
        children: [
          {
            id: '3',
            name: 'Admin',
            type: 'folder',
            path: 'C:\\Users\\Admin',
            children: [
              {
                id: '4',
                name: 'Documents',
                type: 'folder',
                path: 'C:\\Users\\Admin\\Documents',
                children: [
                  { id: '5', name: 'report.pdf', type: 'file', path: 'C:\\Users\\Admin\\Documents\\report.pdf', size: 2048576, extension: 'pdf' },
                  { id: '6', name: 'notes.txt', type: 'file', path: 'C:\\Users\\Admin\\Documents\\notes.txt', size: 4096, extension: 'txt' },
                  { id: '7', name: 'presentation.pptx', type: 'file', path: 'C:\\Users\\Admin\\Documents\\presentation.pptx', size: 8192000, extension: 'pptx' },
                ],
              },
              {
                id: '8',
                name: 'Downloads',
                type: 'folder',
                path: 'C:\\Users\\Admin\\Downloads',
                children: [
                  { id: '9', name: 'image.jpg', type: 'file', path: 'C:\\Users\\Admin\\Downloads\\image.jpg', size: 1024000, extension: 'jpg' },
                  { id: '10', name: 'video.mp4', type: 'file', path: 'C:\\Users\\Admin\\Downloads\\video.mp4', size: 52428800, extension: 'mp4' },
                  { id: '11', name: 'archive.zip', type: 'file', path: 'C:\\Users\\Admin\\Downloads\\archive.zip', size: 10485760, extension: 'zip' },
                ],
              },
              {
                id: '12',
                name: 'Pictures',
                type: 'folder',
                path: 'C:\\Users\\Admin\\Pictures',
                children: [
                  { id: '13', name: 'photo1.png', type: 'file', path: 'C:\\Users\\Admin\\Pictures\\photo1.png', size: 2097152, extension: 'png' },
                  { id: '14', name: 'photo2.jpg', type: 'file', path: 'C:\\Users\\Admin\\Pictures\\photo2.jpg', size: 1572864, extension: 'jpg' },
                ],
              },
            ],
          },
        ],
      },
      {
        id: '15',
        name: 'Program Files',
        type: 'folder',
        path: 'C:\\Program Files',
        children: [
          {
            id: '16',
            name: 'Google',
            type: 'folder',
            path: 'C:\\Program Files\\Google',
            children: [],
          },
        ],
      },
    ],
  },
];

export function FileExplorer({ backendId }: FileExplorerProps) {
  const [fileSystem] = useState<FileNode[]>(mockFileSystem);
  const [expandedFolders, setExpandedFolders] = useState<Set<string>>(new Set(['1']));
  const [selectedFile, setSelectedFile] = useState<FileNode | null>(null);
  const [searchQuery, setSearchQuery] = useState('');

  const toggleFolder = (folderId: string) => {
    const newExpanded = new Set(expandedFolders);
    if (newExpanded.has(folderId)) {
      newExpanded.delete(folderId);
    } else {
      newExpanded.add(folderId);
    }
    setExpandedFolders(newExpanded);
  };

  const handleSelectFile = (file: FileNode) => {
    setSelectedFile(file);
  };

  const handleDownloadFile = () => {
    if (!selectedFile) return;
    // Send download command immediately
    console.log('Downloading file:', selectedFile.path);
  };

  const handleUploadFile = () => {
    // Open upload dialog
    console.log('Upload file');
  };

  const formatFileSize = (bytes: number) => {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(2) + ' KB';
    if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(2) + ' MB';
    return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB';
  };

  const getFileIcon = (file: FileNode) => {
    if (file.type === 'folder') {
      return (
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
          <path d="M3 7v10a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2V9a2 2 0 0 0-2-2h-7l-2-2H5a2 2 0 0 0-2 2z" fill="#60A5FA" stroke="#3B82F6" strokeWidth="2"/>
        </svg>
      );
    }

    const ext = file.extension?.toLowerCase();
    if (!ext) return (
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
        <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8l-6-6z" fill="#E5E7EB" stroke="#9CA3AF" strokeWidth="2"/>
        <path d="M14 2v6h6" stroke="#9CA3AF" strokeWidth="2"/>
      </svg>
    );

    if (['txt', 'pdf', 'doc', 'docx'].includes(ext)) {
      return (
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
          <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8l-6-6z" fill="#DBEAFE" stroke="#3B82F6" strokeWidth="2"/>
          <path d="M14 2v6h6M9 13h6M9 17h6M9 9h3" stroke="#3B82F6" strokeWidth="2" strokeLinecap="round"/>
        </svg>
      );
    }
    if (['jpg', 'jpeg', 'png', 'gif', 'svg'].includes(ext)) {
      return (
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
          <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8l-6-6z" fill="#D1FAE5" stroke="#10B981" strokeWidth="2"/>
          <circle cx="10" cy="11" r="2" stroke="#10B981" strokeWidth="2"/>
          <path d="M14 2v6h6M6 18l4-4 4 4 4-6" stroke="#10B981" strokeWidth="2" strokeLinecap="round"/>
        </svg>
      );
    }
    if (['mp4', 'avi', 'mov', 'mkv'].includes(ext)) {
      return (
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
          <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8l-6-6z" fill="#E9D5FF" stroke="#A855F7" strokeWidth="2"/>
          <path d="M14 2v6h6M10 11l5 3-5 3v-6z" stroke="#A855F7" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
        </svg>
      );
    }
    if (['zip', 'rar', '7z', 'tar', 'gz'].includes(ext)) {
      return (
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
          <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8l-6-6z" fill="#FED7AA" stroke="#F97316" strokeWidth="2"/>
          <path d="M14 2v6h6M12 9v2M12 13v2M12 17v2" stroke="#F97316" strokeWidth="2" strokeLinecap="round"/>
        </svg>
      );
    }
    if (['js', 'ts', 'jsx', 'tsx', 'py', 'java', 'cpp', 'c', 'html', 'css'].includes(ext)) {
      return (
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
          <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8l-6-6z" fill="#BFDBFE" stroke="#3B82F6" strokeWidth="2"/>
          <path d="M14 2v6h6M9 12l2 2-2 2M15 12l-2 2 2 2" stroke="#3B82F6" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
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

  const renderFileTree = (nodes: FileNode[], depth: number = 0) => {
    return nodes.map((node) => {
      const isExpanded = expandedFolders.has(node.id);
      const isSelected = selectedFile?.id === node.id;

      return (
        <div key={node.id}>
          <motion.div
            initial={{ opacity: 0, x: -10 }}
            animate={{ opacity: 1, x: 0 }}
            className={`
              flex items-center gap-2 px-3 py-2 rounded-lg cursor-pointer transition-colors
              ${isSelected ? 'bg-blue-100 border border-blue-300' : 'hover:bg-gray-100'}
            `}
            style={{ paddingLeft: `${depth * 20 + 12}px` }}
            onClick={() => {
              if (node.type === 'folder') {
                toggleFolder(node.id);
              } else {
                handleSelectFile(node);
              }
            }}
          >
            {node.type === 'folder' && (
              <button
                onClick={(e) => {
                  e.stopPropagation();
                  toggleFolder(node.id);
                }}
                className="hover:bg-gray-200 rounded p-0.5"
              >
                {isExpanded ? (
                  <ChevronDown className="w-4 h-4 text-gray-600" />
                ) : (
                  <ChevronRight className="w-4 h-4 text-gray-600" />
                )}
              </button>
            )}
            {node.type === 'file' && <div className="w-5" />}
            
            {getFileIcon(node)}
            
            <span className={`flex-1 truncate ${node.type === 'folder' ? 'font-medium' : ''}`}>
              {node.name}
            </span>

            {node.type === 'file' && node.size && (
              <span className="text-xs text-gray-500 font-mono">
                {formatFileSize(node.size)}
              </span>
            )}
          </motion.div>

          <AnimatePresence>
            {node.type === 'folder' && isExpanded && node.children && (
              <motion.div
                initial={{ height: 0, opacity: 0 }}
                animate={{ height: 'auto', opacity: 1 }}
                exit={{ height: 0, opacity: 0 }}
                transition={{ duration: 0.2 }}
              >
                {renderFileTree(node.children, depth + 1)}
              </motion.div>
            )}
          </AnimatePresence>
        </div>
      );
    });
  };

  return (
    <div className="h-full flex flex-col p-6">
      <div className="mb-6 flex items-center gap-3">
        <h3 className="text-2xl">File Explorer</h3>
        <HelpButton
          title="File Explorer"
          description="Browse and manage files on the remote device. Navigate through folders, view file details, and download files to your local machine."
          instructions={[
            'Click the arrow ">" to expand/collapse folders',
            'Click on a file to view details in the right panel',
            'Selected files will be highlighted',
            'Use the "Download File" button to transfer files',
          ]}
        />
      </div>

      {/* Search and Actions */}
      <div className="flex items-center gap-3 mb-6">
        <div className="relative flex-1">
          <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-5 h-5 text-gray-400" />
          <Input
            type="text"
            placeholder="Search files and folders..."
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
          onClick={handleUploadFile}
          variant="outline"
          className="h-12 border-gray-300"
        >
          <Upload className="w-4 h-4 mr-2" />
          Upload
        </Button>
      </div>

      <div className="flex-1 flex gap-6 overflow-hidden">
        {/* File Tree */}
        <div className="flex-1 bg-white border border-gray-200 rounded-xl overflow-hidden flex flex-col">
          <div className="bg-gray-50 border-b border-gray-200 px-4 py-3 flex items-center gap-3">
            <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
              <rect x="3" y="5" width="18" height="14" rx="2" stroke="currentColor" strokeWidth="2"/>
              <path d="M7 2v4M17 2v4" stroke="currentColor" strokeWidth="2" strokeLinecap="round"/>
            </svg>
            <span className="font-medium">Remote File System</span>
          </div>
          <div className="flex-1 overflow-y-auto p-2">
            {renderFileTree(fileSystem)}
          </div>
        </div>

        {/* File Details */}
        <div className="w-80 bg-white border border-gray-200 rounded-xl p-6">
          {selectedFile ? (
            <motion.div
              key={selectedFile.id}
              initial={{ opacity: 0, y: 20 }}
              animate={{ opacity: 1, y: 0 }}
              className="space-y-6"
            >
              <div className="text-center">
                <div className="w-20 h-20 bg-gray-50 rounded-2xl flex items-center justify-center mx-auto mb-4">
                  <div className="scale-150">
                    {getFileIcon(selectedFile)}
                  </div>
                </div>
                <h4 className="mb-1 break-all">{selectedFile.name}</h4>
                <p className="text-sm text-gray-500 font-mono break-all">
                  {selectedFile.path}
                </p>
              </div>

              {selectedFile.type === 'file' && (
                <div className="space-y-3 pt-4 border-t border-gray-200">
                  <div className="flex justify-between text-sm">
                    <span className="text-gray-600">Type:</span>
                    <span className="font-medium uppercase">{selectedFile.extension || 'Unknown'}</span>
                  </div>
                  {selectedFile.size && (
                    <div className="flex justify-between text-sm">
                      <span className="text-gray-600">Size:</span>
                      <span className="font-medium">{formatFileSize(selectedFile.size)}</span>
                    </div>
                  )}
                </div>
              )}

              {selectedFile.type === 'file' && (
                <div className="space-y-3">
                  <Button
                    onClick={handleDownloadFile}
                    className="w-full bg-black hover:bg-gray-800 text-white"
                  >
                    <Download className="w-4 h-4 mr-2" />
                    Download File
                  </Button>
                  <p className="text-xs text-gray-500 text-center">
                    File will be downloaded to your device
                  </p>
                </div>
              )}

              {selectedFile.type === 'folder' && (
                <div className="bg-blue-50 rounded-lg p-4 border border-blue-100">
                  <p className="text-sm text-blue-700">
                    This is a folder. Click the arrow to expand/collapse.
                  </p>
                </div>
              )}
            </motion.div>
          ) : (
            <div className="h-full flex items-center justify-center text-center">
              <div className="text-gray-400">
                <svg width="60" height="60" viewBox="0 0 24 24" fill="none" className="mx-auto mb-4 opacity-50">
                  <path d="M3 7v10a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2V9a2 2 0 0 0-2-2h-7l-2-2H5a2 2 0 0 0-2 2z" stroke="currentColor" strokeWidth="2"/>
                </svg>
                <p className="text-sm">
                  Select a file or folder to view details
                </p>
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}