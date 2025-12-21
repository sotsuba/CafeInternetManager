"use client"

import { useState, useEffect } from "react"
import { Activity, Wifi, Clock, Terminal } from "lucide-react"

export function Footer() {
  const [logs, setLogs] = useState<string[]>([
    "System initialized",
    "Connected to gateway",
    "Client PC-LAB-01 connected",
    "Client PC-LAB-02 connected",
  ])
  const [currentLog, setCurrentLog] = useState(0)

  useEffect(() => {
    const interval = setInterval(() => {
      setCurrentLog((prev) => (prev + 1) % logs.length)
    }, 3000)
    return () => clearInterval(interval)
  }, [logs.length])

  return (
    <footer className="flex items-center justify-between border-t border-border bg-card px-4 font-mono text-[10px]">
      {/* Left - System Status */}
      <div className="flex items-center gap-4">
        <div className="flex items-center gap-1.5 text-accent">
          <Wifi className="h-3 w-3" />
          <span>WS Connected</span>
        </div>
        <div className="hidden sm:flex items-center gap-1.5 text-muted-foreground">
          <Activity className="h-3 w-3" />
          <span>Ping: 12ms</span>
        </div>
        <div className="hidden md:flex items-center gap-1.5 text-muted-foreground">
          <Clock className="h-3 w-3" />
          <span>FPS: 30</span>
        </div>
      </div>

      {/* Right - Log Ticker */}
      <div className="flex items-center gap-2 text-muted-foreground overflow-hidden max-w-[200px] sm:max-w-none">
        <Terminal className="h-3 w-3 shrink-0 text-primary" />
        <span className="truncate animate-pulse">
          {">"} {logs[currentLog]}
        </span>
      </div>
    </footer>
  )
}
