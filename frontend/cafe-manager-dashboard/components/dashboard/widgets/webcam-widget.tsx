"use client"

import { useState } from "react"
import { Button } from "@/components/ui/button"
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select"
import { Play, Pause, Video, Signal, User } from "lucide-react"
import { cn } from "@/lib/utils"

interface WebcamWidgetProps {
  clientId: number
  isOnline: boolean
}

export function WebcamWidget({ clientId, isOnline }: WebcamWidgetProps) {
  const [isStreaming, setIsStreaming] = useState(false)
  const [selectedCamera, setSelectedCamera] = useState("cam0")

  return (
    <div className="glass rounded-xl overflow-hidden">
      {/* Widget Header */}
      <div className="flex items-center justify-between border-b border-border px-4 py-3">
        <div className="flex items-center gap-2">
          <Video className="h-4 w-4 text-accent" />
          <span className="font-mono text-sm font-medium text-foreground">Webcam Feed</span>
          {isStreaming && (
            <span className="flex items-center gap-1 rounded-full bg-accent/20 px-2 py-0.5 font-mono text-[10px] text-accent">
              <Signal className="h-2.5 w-2.5 animate-pulse" />
              LIVE
            </span>
          )}
        </div>
        <Select value={selectedCamera} onValueChange={setSelectedCamera} disabled={!isOnline}>
          <SelectTrigger className="h-7 w-28 font-mono text-xs">
            <SelectValue />
          </SelectTrigger>
          <SelectContent>
            <SelectItem value="cam0">Camera 0</SelectItem>
            <SelectItem value="cam1">Camera 1</SelectItem>
          </SelectContent>
        </Select>
      </div>

      {/* Video Container */}
      <div className="relative aspect-video bg-background">
        <div
          className={cn("absolute inset-0 flex items-center justify-center", isStreaming ? "bg-muted" : "bg-muted/50")}
        >
          {isStreaming ? (
            <div className="relative w-full h-full">
              {/* Simulated webcam content */}
              <div className="absolute inset-0 flex items-center justify-center bg-gradient-to-br from-muted to-muted/50">
                <div className="h-20 w-20 rounded-full bg-muted-foreground/20 flex items-center justify-center">
                  <User className="h-10 w-10 text-muted-foreground/50" />
                </div>
              </div>
              {/* Scanline effect */}
              <div className="scanlines absolute inset-0" />
            </div>
          ) : (
            <div className="text-center">
              <Video className="mx-auto h-12 w-12 text-muted-foreground/50" />
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
            <div className="flex h-14 w-14 items-center justify-center rounded-full bg-accent/20 text-accent backdrop-blur-sm transition-transform hover:scale-110">
              {isStreaming ? <Pause className="h-6 w-6" /> : <Play className="h-6 w-6 ml-1" />}
            </div>
          </button>
        )}
      </div>

      {/* Controls Footer */}
      <div className="flex items-center justify-between border-t border-border px-4 py-2">
        <Button
          variant={isStreaming ? "destructive" : "secondary"}
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
        <span className="font-mono text-[10px] text-muted-foreground">ID: webcam-{clientId}</span>
      </div>
    </div>
  )
}
