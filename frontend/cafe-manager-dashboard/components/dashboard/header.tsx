"use client"

import { Button } from "@/components/ui/button"
import { ChevronRight, Home, MessageSquare, Power, Menu, Bell } from "lucide-react"
import type { Client } from "@/app/page"

interface HeaderProps {
  activeClient: Client | null
  onBackToHome: () => void
  onToggleSidebar: () => void
}

export function Header({ activeClient, onBackToHome, onToggleSidebar }: HeaderProps) {
  return (
    <header className="flex items-center justify-between border-b border-border bg-card px-4">
      {/* Left - Breadcrumbs */}
      <div className="flex items-center gap-2">
        <button
          onClick={onToggleSidebar}
          className="mr-2 rounded-lg p-2 text-muted-foreground hover:bg-muted hover:text-foreground md:hidden"
        >
          <Menu className="h-5 w-5" />
        </button>

        <button
          onClick={onBackToHome}
          className="flex items-center gap-1.5 font-mono text-xs text-muted-foreground hover:text-primary transition-colors"
        >
          <Home className="h-3.5 w-3.5" />
          <span>Home</span>
        </button>

        {activeClient && (
          <>
            <ChevronRight className="h-3.5 w-3.5 text-muted-foreground" />
            <span className="font-mono text-xs text-foreground">{activeClient.name}</span>
            <span
              className={`ml-2 inline-flex items-center rounded-full px-2 py-0.5 font-mono text-[10px] ${
                activeClient.status === "online" ? "bg-accent/20 text-accent" : "bg-destructive/20 text-destructive"
              }`}
            >
              {activeClient.status.toUpperCase()}
            </span>
          </>
        )}
      </div>

      {/* Right - Global Actions */}
      <div className="flex items-center gap-2">
        <Button variant="ghost" size="sm" className="hidden sm:flex gap-2 font-mono text-xs">
          <Bell className="h-4 w-4" />
          <span className="hidden lg:inline">Alerts</span>
        </Button>
        <Button
          variant="outline"
          size="sm"
          className="gap-2 border-primary/50 font-mono text-xs text-primary hover:bg-primary/10 bg-transparent"
        >
          <MessageSquare className="h-4 w-4" />
          <span className="hidden sm:inline">Broadcast</span>
        </Button>
        <Button
          variant="outline"
          size="sm"
          className="gap-2 border-destructive/50 font-mono text-xs text-destructive hover:bg-destructive/10 bg-transparent"
        >
          <Power className="h-4 w-4" />
          <span className="hidden sm:inline">Shutdown All</span>
        </Button>
      </div>
    </header>
  )
}
