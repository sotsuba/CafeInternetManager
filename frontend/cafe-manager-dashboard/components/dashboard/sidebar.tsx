"use client"

import { cn } from "@/lib/utils"
import { Monitor, Users, Wifi, WifiOff, ChevronLeft, Shield } from "lucide-react"
import type { Client } from "@/app/page"

interface SidebarProps {
  clients: Client[]
  activeClientId: number | null
  onClientSelect: (id: number | null) => void
  isOpen: boolean
  onToggle: () => void
  className?: string
}

export function Sidebar({ clients, activeClientId, onClientSelect, isOpen, onToggle, className }: SidebarProps) {
  const onlineCount = clients.filter((c) => c.status === "online").length

  return (
    <aside className={cn("flex flex-col border-r border-border bg-sidebar", className)}>
      {/* Logo Section */}
      <div className="flex h-[60px] items-center gap-3 border-b border-border px-4">
        <div className="flex h-10 w-10 items-center justify-center rounded-lg bg-primary/20">
          <Shield className="h-6 w-6 text-primary" />
        </div>
        <div>
          <h1 className="font-mono text-sm font-bold tracking-wider text-foreground">SAFESCHOOL</h1>
          <p className="font-mono text-[10px] text-muted-foreground">CAFE MANAGER v1.0</p>
        </div>
      </div>

      {/* Group Info */}
      <div className="border-b border-border p-4">
        <div className="glass rounded-lg p-3">
          <div className="flex items-center gap-2 text-xs text-muted-foreground">
            <Users className="h-3.5 w-3.5" />
            <span className="font-mono uppercase">Lab Info</span>
          </div>
          <p className="mt-2 font-mono text-sm text-foreground">Computer Lab A</p>
          <p className="font-mono text-xs text-primary">Network: 192.168.1.0/24</p>
        </div>
      </div>

      {/* Status Summary */}
      <div className="border-b border-border p-4">
        <div className="flex items-center justify-between">
          <span className="font-mono text-xs uppercase text-muted-foreground">Clients</span>
          <span className="font-mono text-xs text-primary">
            {onlineCount}/{clients.length} Online
          </span>
        </div>
        <div className="mt-2 h-1.5 overflow-hidden rounded-full bg-muted">
          <div
            className="h-full bg-accent transition-all"
            style={{ width: `${(onlineCount / clients.length) * 100}%` }}
          />
        </div>
      </div>

      {/* Client List */}
      <div className="flex-1 overflow-auto p-2">
        <div className="space-y-1">
          {clients.map((client) => (
            <button
              key={client.id}
              onClick={() => onClientSelect(client.id)}
              className={cn(
                "flex w-full items-center gap-3 rounded-lg px-3 py-2.5 text-left transition-all",
                "hover:bg-sidebar-accent",
                activeClientId === client.id && "bg-sidebar-accent glow-cyan",
              )}
            >
              <div className="relative">
                <Monitor
                  className={cn("h-4 w-4", client.status === "online" ? "text-accent" : "text-muted-foreground")}
                />
                <span
                  className={cn(
                    "absolute -right-0.5 -top-0.5 h-2 w-2 rounded-full",
                    client.status === "online" ? "bg-accent glow-green" : "bg-destructive",
                  )}
                />
              </div>
              <div className="flex-1 min-w-0">
                <p
                  className={cn(
                    "font-mono text-xs font-medium truncate",
                    client.status === "online" ? "text-foreground" : "text-muted-foreground",
                  )}
                >
                  {client.name}
                </p>
                <p className="font-mono text-[10px] text-muted-foreground">{client.ip}</p>
              </div>
              {client.status === "online" ? (
                <Wifi className="h-3 w-3 text-accent" />
              ) : (
                <WifiOff className="h-3 w-3 text-muted-foreground" />
              )}
            </button>
          ))}
        </div>
      </div>

      {/* Toggle Button */}
      <button
        onClick={onToggle}
        className="flex h-[30px] items-center justify-center border-t border-border text-muted-foreground hover:text-foreground transition-colors"
      >
        <ChevronLeft className="h-4 w-4" />
      </button>
    </aside>
  )
}
