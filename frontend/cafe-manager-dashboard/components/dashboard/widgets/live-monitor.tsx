"use client"

import { useState } from "react"
import { Button } from "@/components/ui/button"
import { Play, Pause, Camera, Maximize2, Monitor, Signal } from "lucide-react"
import { cn } from "@/lib/utils"

interface LiveMonitorProps {
  clientId: number
  isOnline: boolean
}

export function LiveMonitor({ clientId, isOnline }: LiveMonitorProps) {
  const [isStreaming, setIsStreaming] = useState(false)

  return (
    <div className="glass rounded-xl overflow-hidden">
      {/* Widget Header */}
      <div className="flex items-center justify-between border-b border-border px-4 py-3">
        <div className="flex items-center gap-2">
          <Monitor className="h-4 w-4 text-primary" />
          <span className="font-mono text-sm font-medium text-foreground">Screen Share</span>
          {isStreaming && (
            <span className="flex items-center gap-1 rounded-full bg-accent/20 px-2 py-0.5 font-mono text-[10px] text-accent">
              <Signal className="h-2.5 w-2.5 animate-pulse" />
              LIVE
            </span>
          )}
        </div>
        <div className="flex items-center gap-1">
          <Button variant="ghost" size="icon" className="h-7 w-7" disabled={!isOnline}>
            <Camera className="h-3.5 w-3.5" />
          </Button>
          <Button variant="ghost" size="icon" className="h-7 w-7">
            <Maximize2 className="h-3.5 w-3.5" />
          </Button>
        </div>
      </div>

      {/* Video Container */}
      <div className="relative aspect-video bg-background">
        {/* Placeholder/Video */}
        <div
          className={cn("absolute inset-0 flex items-center justify-center", isStreaming ? "bg-muted" : "bg-muted/50")}
        >
          {isStreaming ? (
            <div className="relative w-full h-full">
              {/* Simulated screen content */}
              <div className="absolute inset-4 rounded border border-border/50 bg-card p-4">
                <div className="h-3 w-24 rounded bg-muted-foreground/20" />
                <div className="mt-3 space-y-2">
                  <div className="h-2 w-full rounded bg-muted-foreground/10" />
                  <div className="h-2 w-3/4 rounded bg-muted-foreground/10" />
                  <div className="h-2 w-5/6 rounded bg-muted-foreground/10" />
                </div>
              </div>
              {/* Scanline effect */}
              <div className="scanlines absolute inset-0" />
            </div>
          ) : (
            <div className="text-center">
              <Monitor className="mx-auto h-12 w-12 text-muted-foreground/50" />
              <p className="mt-2 font-mono text-xs text-muted-foreground">
                {isOnline ? "Click play to start stream" : "Client offline"}
              </p>
            </div>
          )}
        </div>

        {/* Play/Pause Overlay */}
        {isOnline && (
          <button
            onClick={() => setIsStreaming(!isStreaming)}
            className={cn(
              "absolute inset-0 flex items-center justify-center transition-opacity",
              isStreaming ? "opacity-0 hover:opacity-100" : "opacity-100",
              "bg-background/50",
            )}
          >
            <div className="flex h-14 w-14 items-center justify-center rounded-full bg-primary/20 text-primary backdrop-blur-sm transition-transform hover:scale-110">
              {isStreaming ? <Pause className="h-6 w-6" /> : <Play className="h-6 w-6 ml-1" />}
            </div>
          </button>
        )}
      </div>

      {/* Controls Footer */}
      <div className="flex items-center justify-between border-t border-border px-4 py-2">
        <div className="flex items-center gap-2">
          <Button
            variant={isStreaming ? "destructive" : "default"}
            size="sm"
            className="h-7 gap-2 font-mono text-xs"
            onClick={() => setIsStreaming(!isStreaming)}
            disabled={!isOnline}
          >
            {isStreaming ? (
              <>
                <Pause className="h-3 w-3" />
                Stop
              </>
            ) : (
              <>
                <Play className="h-3 w-3" />
                Start
              </>
            )}
          </Button>
        </div>
        <span className="font-mono text-[10px] text-muted-foreground">ID: monitor-{clientId}</span>
      </div>
    </div>
  )
}
