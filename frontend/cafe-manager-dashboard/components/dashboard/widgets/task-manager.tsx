"use client"

import { useState } from "react"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { RefreshCw, Search, Cpu, X, ChevronUp, ChevronDown } from "lucide-react"
import { cn } from "@/lib/utils"

interface Process {
  pid: number
  name: string
  ram: number
  cpu: number
}

const mockProcesses: Process[] = [
  { pid: 1234, name: "chrome.exe", ram: 512, cpu: 12.5 },
  { pid: 2345, name: "explorer.exe", ram: 128, cpu: 2.3 },
  { pid: 3456, name: "discord.exe", ram: 384, cpu: 5.8 },
  { pid: 4567, name: "spotify.exe", ram: 256, cpu: 3.2 },
  { pid: 5678, name: "code.exe", ram: 720, cpu: 8.9 },
  { pid: 6789, name: "node.exe", ram: 180, cpu: 4.1 },
  { pid: 7890, name: "steam.exe", ram: 450, cpu: 1.5 },
]

interface TaskManagerProps {
  clientId: number
  isOnline: boolean
}

type SortField = "pid" | "name" | "ram" | "cpu"
type SortDir = "asc" | "desc"

export function TaskManager({ clientId, isOnline }: TaskManagerProps) {
  const [search, setSearch] = useState("")
  const [sortField, setSortField] = useState<SortField>("ram")
  const [sortDir, setSortDir] = useState<SortDir>("desc")
  const [isRefreshing, setIsRefreshing] = useState(false)

  const handleSort = (field: SortField) => {
    if (sortField === field) {
      setSortDir(sortDir === "asc" ? "desc" : "asc")
    } else {
      setSortField(field)
      setSortDir("desc")
    }
  }

  const handleRefresh = () => {
    setIsRefreshing(true)
    setTimeout(() => setIsRefreshing(false), 1000)
  }

  const filteredProcesses = mockProcesses
    .filter((p) => p.name.toLowerCase().includes(search.toLowerCase()))
    .sort((a, b) => {
      const modifier = sortDir === "asc" ? 1 : -1
      if (sortField === "name") {
        return a.name.localeCompare(b.name) * modifier
      }
      return (a[sortField] - b[sortField]) * modifier
    })

  const SortIcon = ({ field }: { field: SortField }) => {
    if (sortField !== field) return null
    return sortDir === "asc" ? <ChevronUp className="h-3 w-3" /> : <ChevronDown className="h-3 w-3" />
  }

  return (
    <div className="glass rounded-xl overflow-hidden">
      {/* Widget Header */}
      <div className="flex items-center justify-between border-b border-border px-4 py-3">
        <div className="flex items-center gap-2">
          <Cpu className="h-4 w-4 text-neon-yellow" />
          <span className="font-mono text-sm font-medium text-foreground">Task Manager</span>
        </div>
        <Button
          variant="ghost"
          size="icon"
          className="h-7 w-7"
          onClick={handleRefresh}
          disabled={!isOnline || isRefreshing}
        >
          <RefreshCw className={cn("h-3.5 w-3.5", isRefreshing && "animate-spin")} />
        </Button>
      </div>

      {/* Search Bar */}
      <div className="border-b border-border p-3">
        <div className="relative">
          <Search className="absolute left-2.5 top-1/2 h-3.5 w-3.5 -translate-y-1/2 text-muted-foreground" />
          <Input
            placeholder="Filter processes..."
            value={search}
            onChange={(e) => setSearch(e.target.value)}
            className="h-8 pl-8 font-mono text-xs"
            disabled={!isOnline}
          />
        </div>
      </div>

      {/* Process Table */}
      <div className="max-h-[200px] overflow-auto">
        <table className="w-full">
          <thead className="sticky top-0 bg-card">
            <tr className="border-b border-border text-left font-mono text-[10px] uppercase text-muted-foreground">
              <th className="cursor-pointer px-4 py-2 hover:text-foreground" onClick={() => handleSort("pid")}>
                <div className="flex items-center gap-1">
                  PID <SortIcon field="pid" />
                </div>
              </th>
              <th className="cursor-pointer px-4 py-2 hover:text-foreground" onClick={() => handleSort("name")}>
                <div className="flex items-center gap-1">
                  Name <SortIcon field="name" />
                </div>
              </th>
              <th
                className="cursor-pointer px-4 py-2 text-right hover:text-foreground"
                onClick={() => handleSort("ram")}
              >
                <div className="flex items-center justify-end gap-1">
                  RAM <SortIcon field="ram" />
                </div>
              </th>
              <th
                className="cursor-pointer px-4 py-2 text-right hover:text-foreground"
                onClick={() => handleSort("cpu")}
              >
                <div className="flex items-center justify-end gap-1">
                  CPU <SortIcon field="cpu" />
                </div>
              </th>
              <th className="px-4 py-2"></th>
            </tr>
          </thead>
          <tbody>
            {isOnline ? (
              filteredProcesses.map((process) => (
                <tr
                  key={process.pid}
                  className="border-b border-border/50 font-mono text-xs hover:bg-muted/50 transition-colors"
                >
                  <td className="px-4 py-2 text-muted-foreground">{process.pid}</td>
                  <td className="px-4 py-2 text-foreground">{process.name}</td>
                  <td className="px-4 py-2 text-right text-primary">{process.ram} MB</td>
                  <td className="px-4 py-2 text-right text-accent">{process.cpu}%</td>
                  <td className="px-4 py-2">
                    <Button variant="ghost" size="icon" className="h-6 w-6 text-destructive hover:bg-destructive/20">
                      <X className="h-3 w-3" />
                    </Button>
                  </td>
                </tr>
              ))
            ) : (
              <tr>
                <td colSpan={5} className="px-4 py-8 text-center font-mono text-xs text-muted-foreground">
                  Client offline
                </td>
              </tr>
            )}
          </tbody>
        </table>
      </div>

      {/* Footer */}
      <div className="border-t border-border px-4 py-2">
        <span className="font-mono text-[10px] text-muted-foreground">
          {filteredProcesses.length} processes • ID: taskmgr-{clientId}
        </span>
      </div>
    </div>
  )
}
