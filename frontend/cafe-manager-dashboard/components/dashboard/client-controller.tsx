"use client"

import type { Client } from "@/app/page"
import { LiveMonitor } from "@/components/dashboard/widgets/live-monitor"
import { WebcamWidget } from "@/components/dashboard/widgets/webcam-widget"
import { TaskManager } from "@/components/dashboard/widgets/task-manager"
import { SecurityPanel } from "@/components/dashboard/widgets/security-panel"

interface ClientControllerProps {
  client: Client
}

export function ClientController({ client }: ClientControllerProps) {
  return (
    <div className="space-y-4">
      {/* Section Header */}
      <div>
        <h2 className="font-mono text-lg font-bold text-foreground">Control Panel: {client.name}</h2>
        <p className="font-mono text-xs text-muted-foreground">
          IP: {client.ip} • Last seen: {client.lastSeen}
        </p>
      </div>

      {/* Widget Grid */}
      <div className="grid gap-4 lg:grid-cols-2">
        {/* Row 1: Live Monitor & Webcam */}
        <LiveMonitor clientId={client.id} isOnline={client.status === "online"} />
        <WebcamWidget clientId={client.id} isOnline={client.status === "online"} />

        {/* Row 2: Task Manager & Security */}
        <TaskManager clientId={client.id} isOnline={client.status === "online"} />
        <SecurityPanel clientId={client.id} clientName={client.name} isOnline={client.status === "online"} />
      </div>
    </div>
  )
}
