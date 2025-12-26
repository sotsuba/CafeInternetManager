#pragma once
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <functional>
#include <atomic>
#include "common/VideoTypes.hpp"
#include "common/Result.hpp"

namespace core {

    // Define a Subscriber interface locally or strict types
    // For now, simpler: Callback + bounded queue is managed internally
    using PacketCallback = std::function<void(const std::vector<uint8_t>&)>;

    // We can't use VideoPacket directly in the network callback because the
    // network layer (BackendServer) expects raw bytes or a specific packet format
    // to send over TCP.
    // However, the BroadcastBus deals with VideoPackets.
    // The Subscriber adapter will convert VideoPacket -> TCP bytes.

    struct SubscriberStats {
        uint64_t dropped_frames = 0;
        uint64_t force_clears = 0;
    };

    class BroadcastBus {
    public:
        BroadcastBus();
        ~BroadcastBus();

        // Called by HAL/Encoder to push a new packet
        void push(const common::VideoPacket& packet);

        // Called by Networking layer to add a client (thread-safe)
        // 'send_fn': function to actually send bytes to the socket
        void subscribe(uint32_t client_id, PacketCallback send_fn);

        // Remove client
        void unsubscribe(uint32_t client_id);

    private:
        struct Subscriber {
            uint32_t id;
            PacketCallback send_fn;
            // Bounded Queue could be here, or simplified for Phase 1
            // Phase 1 (Simplest): Direct Send with fallback?
            // Design Doc says Bounded Queue.
            // We will define a basic lock-protected queue or assume send_fn is fast-ish
            // but strict design says "BoundedBlockingQueue".
            // Let's implement queue policy in .cpp
            std::vector<common::VideoPacket> queue;
            size_t max_queue_size = 60;
            SubscriberStats stats;
        };

        std::mutex mutex_; // Protects subscribers list and caches
        std::vector<std::shared_ptr<Subscriber>> subscribers_;

        // Caches for Smart Join
        // Map GenerationID -> Packet
        std::map<uint64_t, common::VideoPacket> cached_configs_;
        std::map<uint64_t, common::VideoPacket> cached_idrs_;

        // Helper to process a single subscriber's queue
        // (In a real async system, this would be a worker.
        // For Phase 1, we might just loop-send if non-blocking IO isn't ready)
        // Design Doc: "Ideally async I/O group. For Phase 1, 1-thread-per-client or loop."
        // Let's assume we push to queue, and a worker drains it.
        // Or simpler: push calls send directly IF queue empty ?
        // Let's blindly implement Bounded Queue logic in push() then drain.
        void dispatch_to_subscriber(const std::shared_ptr<Subscriber>& sub, const common::VideoPacket& pkt);
    };

} // namespace core
