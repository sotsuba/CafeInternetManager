import React, { useState, useEffect } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import {
  Search, Server, Users, Info, Activity, Monitor,
  FolderTree, List, Play, Camera, StopCircle,
  ChevronRight, ChevronDown, Download, Trash2, X, Maximize2
} from 'lucide-react';

// --- TRẠNG THÁI MOCK DATA ---
const MOCK_BACKENDS = [
  { id: 1, hostname: 'DESKTOP-VUX-01', ip: '192.168.1.15', os: 'Windows 11', status: 'Online', cpu: '12%', ram: '4.2GB' },
  { id: 2, hostname: 'SERVER-LAB-02', ip: '192.168.1.20', os: 'Ubuntu 22.04', status: 'Online', cpu: '5%', ram: '2.1GB' },
];

export default function CafeInternetManager() {
  const [view, setView] = useState('dashboard'); // dashboard, gateway, control
  const [ip, setIp] = useState('');
  const [isConnected, setIsConnected] = useState(false);
  const [selectedClient, setSelectedClient] = useState(null);
  const [activeTab, setActiveTab] = useState('screen');

  // Xử lý kết nối giả lập
  const handleConnect = () => {
    if (ip) {
      setTimeout(() => setIsConnected(true), 1500);
    }
  };

  return (
    <div className="min-h-screen bg-[#09090b] text-slate-200 font-sans overflow-x-hidden">

      {/* 1. NAVIGATION BAR */}
      <nav className="border-b border-white/10 px-6 py-4 flex justify-between items-center bg-black/50 backdrop-blur-md sticky top-0 z-50">
        <div className="flex items-center gap-2 font-bold text-xl tracking-tighter">
          <div className="w-8 h-8 bg-blue-600 rounded-lg flex items-center justify-center text-white">C</div>
          CIM - Cafe Manager
        </div>
        <div className="flex gap-6 text-sm font-medium">
          <button onClick={() => setView('dashboard')} className={view === 'dashboard' ? 'text-blue-500' : 'hover:text-white'}>Dashboard</button>
          <button onClick={() => setView('gateway')} className="bg-blue-600 px-4 py-2 rounded-full text-white hover:bg-blue-700 transition">Go to Console</button>
        </div>
      </nav>

      {/* 2. DASHBOARD SECTION */}
      {view === 'dashboard' && (
        <motion.div initial={{ opacity: 0 }} animate={{ opacity: 1 }} className="max-w-6xl mx-auto p-12">
          <div className="text-center mb-16">
            <h1 className="text-6xl font-extrabold mb-4 bg-gradient-to-r from-blue-400 to-purple-500 bg-clip-text text-transparent">
              Quản lý Hệ thống Tập trung
            </h1>
            <p className="text-slate-400 text-lg max-w-2xl mx-auto">
              Giải pháp tối ưu để điều khiển, giám sát và quản lý các máy trạm qua Gateway với hiệu suất cao nhất.
            </p>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
            <FeatureCard icon={<Monitor />} title="Giám sát Real-time" desc="Theo dõi màn hình và webcam với độ trễ cực thấp." />
            <FeatureCard icon={<FolderTree />} title="Quản lý File" desc="Truy cập và tải file theo phong cách macOS Finder." />
            <FeatureCard icon={<Activity />} title="Kiểm soát Tiến trình" desc="Dừng, chạy ứng dụng từ xa chỉ với vài click." />
          </div>
        </motion.div>
      )}

      {/* 3. GATEWAY SECTION (IP Entry & Animation) */}
      {view === 'gateway' && (
        <div className="max-w-6xl mx-auto p-8 flex flex-col items-center">

          {/* Kiến trúc Frontend <-> Gateway */}
          {!isConnected && (
            <motion.div className="flex items-center justify-center gap-12 mb-12 py-10 opacity-50">
              <div className="flex flex-col items-center"><Monitor size={48} /><span className="text-xs mt-2">Frontend</span></div>
              <div className="h-[2px] w-24 bg-dashed border-t-2 border-dashed border-blue-500/30"></div>
              <div className="flex flex-col items-center"><Server size={64} className="text-blue-500" /><span className="text-xs mt-2">GATEWAY</span></div>
              <div className="h-[2px] w-24 bg-dashed border-t-2 border-dashed border-white/10"></div>
              <div className="flex flex-col items-center"><Users size={48} /><span className="text-xs mt-2">Backends</span></div>
            </motion.div>
          )}

          {/* Thanh tìm kiếm với Animation thu nhỏ */}
          <motion.div
            layout
            transition={{ type: 'spring', stiffness: 100 }}
            className={`bg-[#18181b] p-2 rounded-2xl border border-white/10 flex items-center shadow-2xl ${isConnected ? 'w-full max-w-sm self-start' : 'w-full max-w-2xl'}`}
          >
            <Search className="ml-4 text-slate-500" />
            <input
              value={ip}
              onChange={(e) => setIp(e.target.value)}
              placeholder="Nhập IP Gateway (ví dụ: 192.168.1.100)"
              className="bg-transparent border-none outline-none p-4 flex-1 text-lg"
              disabled={isConnected}
            />
            {!isConnected && (
              <button onClick={handleConnect} className="bg-blue-600 px-6 py-3 rounded-xl font-bold hover:bg-blue-500 transition">KẾT NỐI</button>
            )}
            {isConnected && <div className="text-green-500 mr-4 font-bold flex items-center gap-1"><Activity size={16}/> LIVE</div>}
          </motion.div>

          {/* Hiển thị Backend sau khi kết nối thành công */}
          {isConnected && (
            <motion.div initial={{ y: 20, opacity: 0 }} animate={{ y: 0, opacity: 1 }} className="w-full mt-12">
              <h2 className="text-sm font-semibold text-slate-500 mb-6 uppercase tracking-widest">Danh sách máy trạm đang kết nối</h2>
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
                {MOCK_BACKENDS.map(client => (
                  <BackendCard key={client.id} client={client} onClick={() => setSelectedClient(client)} />
                ))}
              </div>
            </motion.div>
          )}
        </div>
      )}

      {/* 4. CLIENT MODAL (CHÍNH) */}
      <AnimatePresence>
        {selectedClient && (
          <div className="fixed inset-0 z-[100] flex items-center justify-center p-6">
            <motion.div
              initial={{ opacity: 0 }} animate={{ opacity: 1 }} exit={{ opacity: 0 }}
              className="absolute inset-0 bg-black/80 backdrop-blur-xl"
              onClick={() => setSelectedClient(null)}
            />
            <motion.div
              initial={{ scale: 0.9, opacity: 0, y: 20 }} animate={{ scale: 1, opacity: 1, y: 0 }} exit={{ scale: 0.9, opacity: 0, y: 20 }}
              className="bg-[#1c1c1f] w-full max-w-7xl h-[85vh] rounded-3xl border border-white/10 overflow-hidden flex relative shadow-3xl"
            >
              <button onClick={() => setSelectedClient(null)} className="absolute top-4 right-4 p-2 hover:bg-white/10 rounded-full z-10"><X/></button>

              {/* Sidebar 1/4 */}
              <div className="w-1/4 bg-black/20 border-r border-white/5 p-6 flex flex-col">
                <div className="mb-8">
                  <div className="text-xs text-slate-500 font-bold uppercase mb-1">Máy đang chọn</div>
                  <h3 className="text-xl font-bold text-blue-400">{selectedClient.hostname}</h3>
                  <div className="text-xs text-slate-400">{selectedClient.ip} • {selectedClient.os}</div>
                </div>

                <div className="space-y-2">
                  <SidebarItem active={activeTab === 'screen'} icon={<Monitor size={20}/>} label="Screen & Webcam" onClick={() => setActiveTab('screen')} />
                  <SidebarItem active={activeTab === 'file'} icon={<FolderTree size={20}/>} label="File Explorer" onClick={() => setActiveTab('file')} />
                  <SidebarItem active={activeTab === 'process'} icon={<List size={20}/>} label="Apps & Processes" onClick={() => setActiveTab('process')} />
                </div>
              </div>

              {/* Màn hình chính 3/4 */}
              <div className="flex-1 overflow-auto p-8">
                {activeTab === 'screen' && <ScreenView />}
                {activeTab === 'file' && <FileExplorerView />}
                {activeTab === 'process' && <ProcessListView />}
              </div>
            </motion.div>
          </div>
        )}
      </AnimatePresence>
    </div>
  );
}

// --- SUB COMPONENTS ---

function FeatureCard({ icon, title, desc }) {
  return (
    <div className="bg-[#18181b] p-8 rounded-3xl border border-white/5 hover:border-blue-500/50 transition cursor-default group">
      <div className="text-blue-500 mb-4 group-hover:scale-110 transition-transform">{icon}</div>
      <h3 className="text-xl font-bold mb-2">{title}</h3>
      <p className="text-slate-500 text-sm leading-relaxed">{desc}</p>
    </div>
  );
}

function BackendCard({ client, onClick }) {
  return (
    <motion.div
      whileHover={{ scale: 1.02 }}
      onClick={onClick}
      className="bg-[#18181b] p-6 rounded-2xl border border-white/10 cursor-pointer group hover:bg-blue-600/5 transition-all"
    >
      <div className="flex justify-between items-start mb-4">
        <div className="bg-slate-800 p-3 rounded-xl"><Server className="text-blue-400" /></div>
        <div className="text-[10px] bg-green-500/20 text-green-500 px-2 py-1 rounded-md font-bold uppercase tracking-widest">{client.status}</div>
      </div>
      <h3 className="font-bold text-lg">{client.hostname}</h3>
      <p className="text-slate-500 text-sm mb-4">{client.ip}</p>

      <div className="grid grid-cols-2 gap-4 pt-4 border-t border-white/5">
        <div><div className="text-[10px] text-slate-500 uppercase">CPU</div><div className="text-sm font-mono">{client.cpu}</div></div>
        <div><div className="text-[10px] text-slate-500 uppercase">RAM</div><div className="text-sm font-mono">{client.ram}</div></div>
      </div>
    </motion.div>
  );
}

function SidebarItem({ icon, label, active, onClick }) {
  return (
    <button
      onClick={onClick}
      className={`w-full flex items-center gap-3 px-4 py-3 rounded-xl transition ${active ? 'bg-blue-600 text-white shadow-lg' : 'hover:bg-white/5 text-slate-400'}`}
    >
      {icon} <span className="text-sm font-medium">{label}</span>
    </button>
  );
}

// --- TÍNH NĂNG CHI TIẾT ---

function ScreenView() {
  return (
    <div className="flex flex-col h-full">
      <div className="aspect-video bg-black rounded-2xl overflow-hidden relative shadow-inner border border-white/10 flex items-center justify-center">
        {/* Giả lập ảnh 16:9 luôn cover khung */}
        <img
          src="https://images.unsplash.com/photo-1542831371-29b0f74f9713?auto=format&fit=crop&q=80&w=2070&ixlib=rb-4.0.3"
          className="w-full h-full object-cover opacity-80"
          alt="Remote Screen"
        />
        <div className="absolute top-4 left-4 flex gap-2">
            <span className="bg-red-600 w-3 h-3 rounded-full animate-pulse"></span>
            <span className="text-xs font-mono text-white/70 tracking-widest uppercase">LIVE 1080p</span>
        </div>
      </div>

      <div className="mt-8 bg-black/40 p-6 rounded-3xl flex items-center justify-between border border-white/5">
        <div className="flex gap-4">
          <button className="bg-red-600 text-white px-6 py-3 rounded-xl flex items-center gap-2 hover:bg-red-500 transition shadow-lg shadow-red-900/20">
            <StopCircle size={20}/> RECORD
          </button>
          <select className="bg-slate-800 border-none rounded-xl px-4 text-sm text-white focus:ring-0">
            <option>10 Seconds</option>
            <option>30 Seconds</option>
            <option>Custom</option>
          </select>
        </div>
        <div className="flex gap-2">
          <button className="p-3 bg-slate-800 rounded-xl hover:bg-slate-700 transition"><Camera size={20}/></button>
          <button className="p-3 bg-slate-800 rounded-xl hover:bg-slate-700 transition"><Maximize2 size={20}/></button>
        </div>
      </div>
    </div>
  );
}

function FileExplorerView() {
  return (
    <div className="flex h-full gap-6">
      <div className="w-1/3 bg-black/20 rounded-2xl p-4 overflow-auto border border-white/5">
        <div className="text-xs font-bold text-slate-600 mb-4 px-2 uppercase tracking-widest">Directories</div>
        <div className="space-y-1">
          <FileNode name="Applications" isOpen />
          <div className="ml-6 space-y-1">
            <FileNode name="System" />
            <FileNode name="Users" isOpen />
            <div className="ml-6 space-y-1 text-blue-400 font-semibold"><FileNode name="Desktop" isSelected /></div>
          </div>
          <FileNode name="Documents" />
        </div>
      </div>
      <div className="flex-1 bg-black/10 rounded-2xl p-6 border border-white/5 flex flex-col">
        <div className="flex-1">
            <div className="grid grid-cols-4 gap-6">
                <FileItem name="CV_Frontend.pdf" type="pdf" />
                <FileItem name="Project_Cafe.zip" type="zip" />
                <FileItem name="Meeting_Note.txt" type="txt" isSelected />
                <FileItem name="Icon_Mac.png" type="image" />
            </div>
        </div>
        <div className="mt-4 p-4 bg-blue-600/10 border border-blue-500/30 rounded-2xl flex justify-between items-center animate-in slide-in-from-bottom-2">
            <div>
                <div className="text-sm font-bold text-blue-400">Meeting_Note.txt</div>
                <div className="text-xs text-slate-500">2.4 MB • Modified 2 hours ago</div>
            </div>
            <button className="bg-blue-600 px-6 py-2 rounded-lg flex items-center gap-2 font-bold text-sm"><Download size={16}/> Lấy file này</button>
        </div>
      </div>
    </div>
  );
}

function ProcessListView() {
  return (
    <div className="flex flex-col h-full">
      <div className="flex justify-between items-center mb-6">
        <h4 className="text-lg font-bold">Tác vụ đang chạy</h4>
        <div className="flex gap-2">
          <button className="bg-blue-600/10 text-blue-400 border border-blue-500/20 px-4 py-2 rounded-xl text-sm font-bold">Start New App</button>
          <button className="bg-red-600 text-white px-4 py-2 rounded-xl text-sm font-bold flex items-center gap-2 shadow-lg shadow-red-900/20"><Trash2 size={16}/> Kill Process</button>
        </div>
      </div>
      <div className="bg-black/20 rounded-2xl overflow-hidden border border-white/5">
        <table className="w-full text-left text-sm">
          <thead className="bg-white/5 text-slate-500 uppercase text-[10px] font-bold tracking-widest">
            <tr>
              <th className="p-4 w-8"><input type="checkbox" className="rounded bg-slate-800 border-white/10" /></th>
              <th className="p-4">Process Name</th>
              <th className="p-4">PID</th>
              <th className="p-4 text-right">CPU</th>
              <th className="p-4 text-right">Memory</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-white/5">
            <ProcessRow name="Chrome.exe" pid="4512" cpu="4.2%" mem="842 MB" />
            <ProcessRow name="Code.exe (VS Code)" pid="1209" cpu="1.8%" mem="450 MB" />
            <ProcessRow name="Discord.exe" pid="8832" cpu="0.5%" mem="120 MB" />
            <ProcessRow name="System Idle" pid="0" cpu="88.0%" mem="16 KB" />
          </tbody>
        </table>
      </div>
    </div>
  );
}

// Helper Mini Components
function FileNode({ name, isOpen, isSelected }) {
  return (
    <div className={`flex items-center gap-1 p-1 rounded-md cursor-pointer ${isSelected ? 'bg-blue-600 text-white' : 'hover:bg-white/5 text-slate-400'}`}>
      {isOpen ? <ChevronDown size={14}/> : <ChevronRight size={14}/>}
      <FolderTree size={16} className={isSelected ? 'text-white' : 'text-blue-500/50'} />
      <span className="text-sm">{name}</span>
    </div>
  );
}

function FileItem({ name, isSelected }) {
  return (
    <div className={`flex flex-col items-center p-4 rounded-2xl border transition-all cursor-pointer ${isSelected ? 'bg-blue-600/20 border-blue-500 text-blue-400 shadow-xl' : 'bg-black/20 border-white/5 text-slate-500 hover:border-white/20'}`}>
        <div className="w-12 h-12 bg-slate-800/50 rounded-lg mb-3 flex items-center justify-center">📄</div>
        <span className="text-[10px] text-center font-medium line-clamp-1">{name}</span>
    </div>
  );
}

function ProcessRow({ name, pid, cpu, mem }) {
  return (
    <tr className="hover:bg-blue-600/5 transition group cursor-pointer">
      <td className="p-4"><input type="checkbox" className="rounded bg-slate-800 border-white/10" /></td>
      <td className="p-4 font-medium group-hover:text-blue-400 transition">{name}</td>
      <td className="p-4 text-slate-500 font-mono text-xs">{pid}</td>
      <td className="p-4 text-right text-green-500 font-mono">{cpu}</td>
      <td className="p-4 text-right text-slate-400 font-mono">{mem}</td>
    </tr>
  );
}
