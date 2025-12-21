"use client"

import { useState, useEffect, useRef } from "react"
import { Button } from "@/components/ui/button"
import { Switch } from "@/components/ui/switch"
import { Shield, Power, RotateCcw, Keyboard, AlertTriangle } from "lucide-react"
import { cn } from "@/lib/utils"

interface SecurityPanelProps {
  clientId: number
  clientName: string
  isOnline: boolean
}

export function SecurityPanel({ clientId, clientName, isOnline }: SecurityPanelProps) {
  const [keyloggerActive, setKeyloggerActive] = useState(false)
  const [keystrokes, setKeystrokes] = useState<string[]>([])
  const logRef = useRef<HTMLDivElement>(null)

  // Simulate keystrokes when keylogger is active
  useEffect(() => {
    if (!keyloggerActive || !isOnline) return

    const sampleText = "Hello world this is a test message from the client machine..."
    let index = 0

    const interval = setInterval(() => {
      if (index < sampleText.length) {
        setKeystrokes((prev) => [...prev, sampleText[index]])
        index++
      }
    }, 150)

    return () => clearInterval(interval)
  }, [keyloggerActive, isOnline])

  // Auto-scroll log
  useEffect(() => {
    if (logRef.current) {
      logRef.current.scrollTop = logRef.current.scrollHeight
    }
  }, [keystrokes])

  const handleClearLog = () => {
    setKeystrokes([])
  }

  return (
    <div className="glass rounded-xl overflow-hidden">
      {/* Widget Header */}
      <div className="flex items-center justify-between border-b border-border px-4 py-3">
        <div className="flex items-center gap-2">
          <Shield className="h-4 w-4 text-neon-red" />
          <span className="font-mono text-sm font-medium text-foreground">Security & Power</span>
        </div>
      </div>

      <div className="p-4 space-y-4">
        {/* Keylogger Section */}
        <div className="space-y-3">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-2">
              <Keyboard className="h-4 w-4 text-muted-foreground" />
              <span className="font-mono text-xs text-foreground">Keystroke Capture</span>
            </div>
            <Switch checked={keyloggerActive} onCheckedChange={setKeyloggerActive} disabled={!isOnline} />
          </div>

          {/* Keystroke Log */}
          <div
            ref={logRef}
            className={cn(
              "h-24 overflow-auto rounded-lg bg-background p-3 font-mono text-xs",
              "border border-border",
              keyloggerActive && "border-accent/50",
            )}
          >
            {keystrokes.length > 0 ? (
              <span className="text-accent break-all">
                {keystrokes.join("")}
                <span className="animate-pulse">|</span>
              </span>
            ) : (
              <span className="text-muted-foreground">
                {keyloggerActive ? "Waiting for keystrokes..." : "Enable capture to start logging"}
              </span>
            )}
          </div>

          {keystrokes.length > 0 && (
            <Button variant="ghost" size="sm" className="h-6 font-mono text-[10px]" onClick={handleClearLog}>
              Clear Log
            </Button>
          )}
        </div>

        {/* Divider */}
        <div className="border-t border-border" />

        {/* Power Operations */}
        <div className="space-y-3">
          <div className="flex items-center gap-2">
            <Power className="h-4 w-4 text-muted-foreground" />
            <span className="font-mono text-xs text-foreground">Power Operations</span>
          </div>

          <div className="flex gap-2">
            <Button
              variant="outline"
              size="sm"
              className={cn(
                "flex-1 gap-2 font-mono text-xs",
                "border-neon-yellow/50 text-neon-yellow",
                "hover:bg-neon-yellow/10 hover:border-neon-yellow",
              )}
              disabled={!isOnline}
            >
              <RotateCcw className="h-3.5 w-3.5" />
              Restart
            </Button>
            <Button
              variant="outline"
              size="sm"
              className={cn(
                "flex-1 gap-2 font-mono text-xs",
                "border-destructive/50 text-destructive",
                "hover:bg-destructive/10 hover:border-destructive",
              )}
              disabled={!isOnline}
            >
              <Power className="h-3.5 w-3.5" />
              Shutdown
            </Button>
          </div>

          {/* Warning */}
          <div className="flex items-start gap-2 rounded-lg bg-destructive/10 p-2">
            <AlertTriangle className="h-4 w-4 shrink-0 text-destructive" />
            <p className="font-mono text-[10px] text-destructive">
              Power actions will affect {clientName} immediately.
            </p>
          </div>
        </div>
      </div>

      {/* Footer */}
      <div className="border-t border-border px-4 py-2">
        <span className="font-mono text-[10px] text-muted-foreground">ID: security-{clientId}</span>
      </div>
    </div>
  )
}
