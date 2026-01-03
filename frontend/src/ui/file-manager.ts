export interface FileEntry {
    name: string;
    path: string;
    size: number;
    time: number;
    dir: boolean;
    hidden: boolean;
}

export interface FileManagerCallbacks {
    onNavigate: (path: string) => void;
    onDownload: (path: string) => void;
    onUpload?: (path: string, file: File) => void;
    onDelete?: (path: string) => void;
    onMkdir?: (path: string) => void;
    onRename?: (oldPath: string, newPath: string) => void;
}

export class FileManager {
    private container: HTMLElement;
    private currentPath: string = ".";
    private callbacks: FileManagerCallbacks;
    private fileInput: HTMLInputElement;

    constructor(containerId: string, callbacks: FileManagerCallbacks) {
        this.container = document.getElementById(containerId)!;
        this.callbacks = callbacks;

        // Hidden File Input
        this.fileInput = document.createElement("input");
        this.fileInput.type = "file";
        this.fileInput.style.display = "none";
        this.container.appendChild(this.fileInput);

        this.fileInput.onchange = () => {
             if (this.fileInput.files && this.fileInput.files.length > 0) {
                 const file = this.fileInput.files[0];
                 if (this.callbacks.onUpload) {
                     this.callbacks.onUpload(this.currentPath, file);
                 }
                 this.fileInput.value = ""; // Reset
             }
        };

        this.renderEmpty();
    }

    public updateList(path: string, files: FileEntry[]) {
        this.currentPath = path;
        this.render(files);
    }

    private renderEmpty() {
        this.container.innerHTML = `<div class="p-4 text-muted text-center">Select a client to view files</div>`;
    }

    private render(files: FileEntry[]) {
        // Clear container (keep fileInput)
        Array.from(this.container.children).forEach(c => {
            if (c !== this.fileInput) this.container.removeChild(c);
        });

        // Header (Current Path & Actions)
        const header = document.createElement("div");
        header.className = "file-manager-header p-2 border-b border-white/10 flex gap-2 items-center text-xs";
        header.innerHTML = `
            <button class="btn btn-sm btn-ghost px-1" title="Up" id="btn-fm-up">⬆️</button>
            <input type="text" readonly class="input input-sm flex-1 font-mono text-xs bg-transparent border-none" value="${this.currentPath}">
            <button class="btn btn-sm btn-ghost px-1 text-primary" title="Upload" id="btn-fm-upload">📤</button>
            <button class="btn btn-sm btn-ghost px-1 text-green-400" title="New Folder" id="btn-fm-mkdir">➕📁</button>
            <button class="btn btn-sm btn-ghost px-1" title="Refresh" id="btn-fm-refresh">🔄</button>
        `;
        this.container.appendChild(header);

        // Bind Header Events
        header.querySelector("#btn-fm-up")?.addEventListener("click", () => {
             const parts = this.currentPath.split(/[/\\]/); // Split by / or \
             parts.pop();
             const parent = parts.length === 0 ? "." : parts.join("\\");
             this.callbacks.onNavigate(parent || ".");
        });

        header.querySelector("#btn-fm-refresh")?.addEventListener("click", () => {
            this.callbacks.onNavigate(this.currentPath);
        });

        header.querySelector("#btn-fm-upload")?.addEventListener("click", () => {
            if(this.callbacks.onUpload) this.fileInput.click();
        });

        header.querySelector("#btn-fm-mkdir")?.addEventListener("click", () => {
             const name = prompt("New Folder Name:");
             if (name && this.callbacks.onMkdir) {
                 const newDir = this.joinPath(this.currentPath, name);
                 this.callbacks.onMkdir(newDir);
             }
        });

        // File List
        const list = document.createElement("div");
        list.className = "file-list overflow-y-auto h-[300px] text-sm font-mono";

        // Sort: Dirs first, then files
        const sorted = files.sort((a, b) => {
            if (a.dir === b.dir) return a.name.localeCompare(b.name);
            return a.dir ? -1 : 1;
        });

        sorted.forEach(f => {
            const row = document.createElement("div");
            row.className = "file-item p-1 hover:bg-white/5 cursor-pointer flex items-center gap-2 select-none group";

            const icon = f.dir ? "📁" : "📄";
            const size = f.dir ? "" : this.formatSize(f.size);

            row.innerHTML = `
                <span class="w-6 text-center">${icon}</span>
                <span class="flex-1 truncate">${f.name}</span>
                <span class="text-xs text-muted w-16 text-right group-hover:hidden">${size}</span>
                <div class="hidden group-hover:flex gap-1 pr-1">
                    ${!f.dir ? `<button class="btn btn-xs btn-ghost text-green-400 p-0 w-6" title="Download" data-action="dl">⬇️</button>` : ''}
                    <button class="btn btn-xs btn-ghost text-red-400 p-0 w-6" title="Delete" data-action="del">🗑️</button>
                </div>
            `;

            // Row Click (Navigation / Interactions)
            // Use closest for button delegation
            row.addEventListener("click", (e) => {
                const target = e.target as HTMLElement;
                const btn = target.closest("button");
                if (btn) {
                    const action = btn.getAttribute("data-action");
                    if (action === "dl") this.callbacks.onDownload(f.path);
                    if (action === "del") {
                        if (confirm(`Delete ${f.name}?`)) {
                            if (this.callbacks.onDelete) this.callbacks.onDelete(f.path);
                        }
                    }
                    return; // Don't trigger navigation
                }
            });

            row.addEventListener("dblclick", () => {
                if (f.dir) {
                    this.callbacks.onNavigate(f.path);
                } else {
                    this.callbacks.onDownload(f.path);
                }
            });

            // Rename logic (Right Click?)
            row.addEventListener("contextmenu", (e) => {
                 e.preventDefault();
                 const newName = prompt(`Rename ${f.name} to:`, f.name);
                 if (newName && newName !== f.name && this.callbacks.onRename) {
                     const newPath = this.joinPath(this.currentPath, newName);
                     this.callbacks.onRename(f.path, newPath);
                 }
            });

            list.appendChild(row);
        });

        this.container.appendChild(list);
    }

    private joinPath(base: string, name: string): string {
         const sep = base.includes("/") ? "/" : "\\";
         return base === "." ? name : base + (base.endsWith(sep) ? "" : sep) + name;
    }

    private formatSize(bytes: number): string {
        if (bytes === 0) return "0 B";
        const k = 1024;
        const sizes = ["B", "KB", "MB", "GB"];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + " " + sizes[i];
    }
}
