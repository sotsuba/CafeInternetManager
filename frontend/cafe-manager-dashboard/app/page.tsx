"use client"

import { useState } from "react"
import { Sidebar } from "@/components/dashboard/sidebar"
import { Header } from "@/components/dashboard/header"
import { Footer } from "@/components/dashboard/footer"
import { HomeView } from "@/components/dashboard/home-view"
import { ClientController } from "@/components/dashboard/client-controller"

export interface Client {
  id: number
  name: string
  status: "online" | "offline"
  ip: string
  lastSeen: string
}

const mockClients: Client[] = [
  { id: 1, name: "PC-LAB-01", status: "online", ip: "192.168.1.101", lastSeen: "Now" },
  { id: 2, name: "PC-LAB-02", status: "online", ip: "192.168.1.102", lastSeen: "Now" },
  { id: 3, name: "PC-LAB-03", status: "offline", ip: "192.168.1.103", lastSeen: "5m ago" },
  { id: 4, name: "PC-LAB-04", status: "online", ip: "192.168.1.104", lastSeen: "Now" },
  { id: 5, name: "PC-LAB-05", status: "online", ip: "192.168.1.105", lastSeen: "Now" },
  { id: 6, name: "PC-LAB-06", status: "offline", ip: "192.168.1.106", lastSeen: "1h ago" },
  { id: 7, name: "PC-LAB-07", status: "online", ip: "192.168.1.107", lastSeen: "Now" },
  { id: 8, name: "PC-LAB-08", status: "online", ip: "192.168.1.108", lastSeen: "Now" },
]

export default function Dashboard() {
  const [activeClientId, setActiveClientId] = useState<number | null>(null)
  const [sidebarOpen, setSidebarOpen] = useState(true)

  const activeClient = activeClientId ? mockClients.find((c) => c.id === activeClientId) : null

  return (
    <div className="grid h-screen grid-cols-1 grid-rows-[60px_1fr_30px] md:grid-cols-[250px_1fr]">
      {/* Sidebar - hidden on mobile, shown on md+ */}
      <Sidebar
        clients={mockClients}
        activeClientId={activeClientId}
        onClientSelect={setActiveClientId}
        isOpen={sidebarOpen}
        onToggle={() => setSidebarOpen(!sidebarOpen)}
        className="hidden md:flex md:row-span-3"
      />

      {/* Header */}
      <Header
        activeClient={activeClient}
        onBackToHome={() => setActiveClientId(null)}
        onToggleSidebar={() => setSidebarOpen(!sidebarOpen)}
      />

      {/* Main Content */}
      <main className="overflow-auto bg-background p-4">
        {activeClient ? (
          <ClientController client={activeClient} />
        ) : (
          <HomeView clients={mockClients} onClientSelect={setActiveClientId} />
        )}
      </main>

      {/* Footer */}
      <Footer />
    </div>
  )
}
