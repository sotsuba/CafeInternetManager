/**
 * Mock Data for Demo Mode
 * Simulates backend connections without real Gateway
 */

export interface MockBackend {
  id: number;
  name: string;
  os: string;
  status: 'online' | 'offline';
  ip: string;
  uptime: string;
}

export interface MockProcess {
  pid: string;
  name: string;
  user: string;
  cmd: string;
  cpu: string;
  mem: string;
}

export interface MockApp {
  id: string;
  name: string;
  icon: string;
  exec: string;
}

export interface MockFile {
  name: string;
  isDir: boolean;
  size?: number;
  children?: MockFile[];
}

// Demo backends
export const MOCK_BACKENDS: MockBackend[] = [
  { id: 101, name: 'PC-Lab-01', os: 'Windows 11', status: 'online', ip: '192.168.1.101', uptime: '2h 45m' },
  { id: 102, name: 'PC-Lab-02', os: 'Windows 10', status: 'online', ip: '192.168.1.102', uptime: '5h 12m' },
  { id: 103, name: 'Linux-Server', os: 'Ubuntu 22.04', status: 'online', ip: '192.168.1.103', uptime: '12d 3h' },
  { id: 104, name: 'PC-Reception', os: 'Windows 11', status: 'offline', ip: '192.168.1.104', uptime: '-' },
  { id: 105, name: 'MacBook-Admin', os: 'macOS Sonoma', status: 'online', ip: '192.168.1.105', uptime: '8h 30m' },
];

// Demo processes
export const MOCK_PROCESSES: MockProcess[] = [
  { pid: '1234', name: 'chrome.exe', user: 'User', cmd: 'C:\\Program Files\\Google\\Chrome\\chrome.exe', cpu: '12%', mem: '450MB' },
  { pid: '2345', name: 'explorer.exe', user: 'System', cmd: 'C:\\Windows\\explorer.exe', cpu: '2%', mem: '85MB' },
  { pid: '3456', name: 'Code.exe', user: 'User', cmd: 'C:\\Users\\User\\AppData\\Local\\Programs\\Microsoft VS Code\\Code.exe', cpu: '8%', mem: '320MB' },
  { pid: '4567', name: 'discord.exe', user: 'User', cmd: 'C:\\Users\\User\\AppData\\Local\\Discord\\app-1.0.9023\\Discord.exe', cpu: '3%', mem: '180MB' },
  { pid: '5678', name: 'spotify.exe', user: 'User', cmd: 'C:\\Users\\User\\AppData\\Roaming\\Spotify\\Spotify.exe', cpu: '1%', mem: '120MB' },
  { pid: '6789', name: 'firefox.exe', user: 'User', cmd: 'C:\\Program Files\\Mozilla Firefox\\firefox.exe', cpu: '6%', mem: '380MB' },
  { pid: '7890', name: 'notepad.exe', user: 'User', cmd: 'C:\\Windows\\System32\\notepad.exe', cpu: '0%', mem: '12MB' },
  { pid: '8901', name: 'steam.exe', user: 'User', cmd: 'C:\\Program Files (x86)\\Steam\\Steam.exe', cpu: '4%', mem: '200MB' },
  { pid: '9012', name: 'svchost.exe', user: 'System', cmd: 'C:\\Windows\\System32\\svchost.exe', cpu: '1%', mem: '45MB' },
  { pid: '1011', name: 'msedge.exe', user: 'User', cmd: 'C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe', cpu: '5%', mem: '290MB' },
];

// Demo apps
export const MOCK_APPS: MockApp[] = [
  { id: '1', name: 'Google Chrome', icon: '', exec: 'chrome.exe' },
  { id: '2', name: 'Mozilla Firefox', icon: '', exec: 'firefox.exe' },
  { id: '3', name: 'Microsoft Edge', icon: '', exec: 'msedge.exe' },
  { id: '4', name: 'Visual Studio Code', icon: '', exec: 'code.exe' },
  { id: '5', name: 'Notepad', icon: '', exec: 'notepad.exe' },
  { id: '6', name: 'Calculator', icon: '', exec: 'calc.exe' },
  { id: '7', name: 'File Explorer', icon: '', exec: 'explorer.exe' },
  { id: '8', name: 'Command Prompt', icon: '', exec: 'cmd.exe' },
  { id: '9', name: 'PowerShell', icon: '', exec: 'powershell.exe' },
  { id: '10', name: 'Task Manager', icon: '', exec: 'taskmgr.exe' },
  { id: '11', name: 'Discord', icon: '', exec: 'discord.exe' },
  { id: '12', name: 'Spotify', icon: '', exec: 'spotify.exe' },
  { id: '13', name: 'Steam', icon: '', exec: 'steam.exe' },
  { id: '14', name: 'VLC Media Player', icon: '', exec: 'vlc.exe' },
  { id: '15', name: 'Microsoft Word', icon: '', exec: 'winword.exe' },
  { id: '16', name: 'Microsoft Excel', icon: '', exec: 'excel.exe' },
];

// Demo file tree
export const MOCK_FILES: MockFile[] = [
  {
    name: 'Documents',
    isDir: true,
    children: [
      {
        name: 'Projects',
        isDir: true,
        children: [
          { name: 'CafeManager', isDir: true },
          { name: 'WebApp', isDir: true },
          { name: 'notes.md', isDir: false, size: 4520 },
        ]
      },
      {
        name: 'Reports',
        isDir: true,
        children: [
          { name: 'Q4-2025.xlsx', isDir: false, size: 125000 },
          { name: 'summary.pdf', isDir: false, size: 2400000 },
        ]
      },
      { name: 'readme.txt', isDir: false, size: 1280 },
      { name: 'config.json', isDir: false, size: 890 },
    ]
  },
  {
    name: 'Downloads',
    isDir: true,
    children: [
      { name: 'installer.exe', isDir: false, size: 45000000 },
      { name: 'photo.jpg', isDir: false, size: 3500000 },
      { name: 'archive.zip', isDir: false, size: 12000000 },
    ]
  },
  {
    name: 'Pictures',
    isDir: true,
    children: [
      { name: 'Screenshots', isDir: true },
      { name: 'Wallpapers', isDir: true },
      { name: 'avatar.png', isDir: false, size: 150000 },
    ]
  },
  {
    name: 'Music',
    isDir: true,
    children: [
      { name: 'playlist.m3u', isDir: false, size: 2400 },
    ]
  },
  { name: 'desktop.ini', isDir: false, size: 282 },
  { name: 'notes.txt', isDir: false, size: 560 },
];

// Demo keylog data
export const MOCK_KEYLOG = `[10:23:45] Hello world
[10:23:48] This is a demo keylogger output
[10:24:01] user@pc-lab-01 ~ $ cd Documents
[10:24:05] user@pc-lab-01 ~/Documents $ ls -la
[10:24:12] total 24
[10:24:15] drwxr-xr-x  4 user user 4096 Jan  5 10:20 .
[10:24:18] drwxr-xr-x 18 user user 4096 Jan  4 15:30 ..
[10:24:22] -rw-r--r--  1 user user 1280 Jan  3 09:15 readme.txt
[10:24:25] drwxr-xr-x  3 user user 4096 Jan  2 14:00 Projects
[10:25:01] Password: ********
[10:25:10] Login successful!
`;

// Demo stream frame (simple colored rectangle as base64)
export function generateMockFrame(): string {
  // Create a simple canvas with random color variation
  const canvas = document.createElement('canvas');
  canvas.width = 640;
  canvas.height = 360;
  const ctx = canvas.getContext('2d')!;

  // Dark background
  ctx.fillStyle = '#1a1a24';
  ctx.fillRect(0, 0, 640, 360);

  // Simulate desktop elements
  // Taskbar
  ctx.fillStyle = '#252533';
  ctx.fillRect(0, 320, 640, 40);

  // Windows
  const time = Date.now();
  ctx.fillStyle = '#2d2d3d';
  ctx.fillRect(20 + Math.sin(time / 1000) * 5, 20, 280, 200);
  ctx.fillRect(320 + Math.cos(time / 1000) * 5, 50, 300, 250);

  // Window title bars
  ctx.fillStyle = '#4F46E5';
  ctx.fillRect(20, 20, 280, 24);
  ctx.fillRect(320, 50, 300, 24);

  // Text
  ctx.fillStyle = '#f8fafc';
  ctx.font = '12px Inter, sans-serif';
  ctx.fillText('Browser', 30, 37);
  ctx.fillText('Terminal', 330, 67);

  // Clock in taskbar
  const now = new Date();
  ctx.fillText(now.toLocaleTimeString(), 560, 345);

  // Start button
  ctx.fillStyle = '#4F46E5';
  ctx.beginPath();
  ctx.arc(25, 340, 12, 0, Math.PI * 2);
  ctx.fill();

  return canvas.toDataURL('image/jpeg', 0.7);
}

// Export demo mode state
export let isDemoMode = false;

export function enableDemoMode(): void {
  isDemoMode = true;
  console.log('🎭 Demo Mode Enabled');
}

export function disableDemoMode(): void {
  isDemoMode = false;
  console.log('🎭 Demo Mode Disabled');
}
