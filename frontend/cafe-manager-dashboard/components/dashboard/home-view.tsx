"use client"

import { Monitor, Wifi, WifiOff } from "lucide-react"
import { cn } from "@/lib/utils"
import type { Client } from "@/app/page"

interface HomeViewProps {
  clients: Client[]
  onClientSelect: (id: number) => void
}

export function HomeView({ clients, onClientSelect }: HomeViewProps) {
  return (
    <div className="space-y-6">
      {/* Section Header */}
      <div>
        <h2 className="font-mono text-lg font-bold text-foreground">Client Overview</h2>
        <p className="font-mono text-xs text-muted-foreground">Click on a client to view detailed controls</p>
      </div>

      {/* Client Grid */}
      <div className="grid grid-cols-2 gap-4 sm:grid-cols-3 lg:grid-cols-4 xl:grid-cols-5">
        {clients.map((client) => (
          <button
            key={client.id}
            onClick={() => onClientSelect(client.id)}
            className={cn(
              "group relative aspect-video overflow-hidden rounded-lg border transition-all",
              "hover:border-primary hover:shadow-lg hover:shadow-primary/20",
              client.status === "online" ? "border-border bg-card" : "border-border/50 bg-card/50",
            )}
          >
            {/* Thumbnail Preview */}
            <div className="absolute inset-0 bg-gradient-to-br from-muted/50 to-muted">
              <div className="absolute inset-0 flex items-center justify-center">
                <Monitor
                  className={cn(
                    "h-8 w-8 transition-transform group-hover:scale-110",
                    client.status === "online" ? "text-muted-foreground" : "text-muted-foreground/50",
                  )}
                />
              </div>
              {/* Scanline effect for online clients */}
              {client.status === "online" && <div className="scanlines absolute inset-0" />}
            </div>

            {/* Status Indicator */}
            <div className="absolute right-2 top-2">
              {client.status === "online" ? (
                <div className="flex items-center gap-1 rounded-full bg-accent/20 px-2 py-0.5">
                  <Wifi className="h-3 w-3 text-accent" />
                </div>
              ) : (
                <div className="flex items-center gap-1 rounded-full bg-destructive/20 px-2 py-0.5">
                  <WifiOff className="h-3 w-3 text-destructive" />
                </div>
              )}
            </div>

            {/* Client Info */}
            <div className="absolute inset-x-0 bottom-0 bg-gradient-to-t from-background/90 to-transparent p-3">
              <p className="font-mono text-xs font-medium text-foreground truncate">{client.name}</p>
              <p className="font-mono text-[10px] text-muted-foreground">{client.ip}</p>
            </div>
          </button>
        ))}
      </div>
    </div>
  )
}
