#include "core/BroadcastBus.hpp"
#include <iostream>

namespace core {

    BroadcastBus::BroadcastBus() {}
    BroadcastBus::~BroadcastBus() {}

    void BroadcastBus::push(const common::VideoPacket& packet) {
        std::lock_guard<std::mutex> lock(mutex_);

        // 1. Update Global Caches
        if (packet.kind == common::PacketKind::CodecConfig) {
            cached_configs_[packet.generation] = packet;
        } else if (packet.kind == common::PacketKind::KeyFrame) {
            cached_idrs_[packet.generation] = packet;
        }

        // 2. Fan-Out to Subscribers
        for (auto& sub : subscribers_) {
            dispatch_to_subscriber(sub, packet);
        }
    }

    void BroadcastBus::subscribe(uint32_t client_id, PacketCallback send_fn) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Remove existing if any (prevent duplicates)
        auto it = std::remove_if(subscribers_.begin(), subscribers_.end(),
            [client_id](const std::shared_ptr<Subscriber>& s){ return s->id == client_id; });
        subscribers_.erase(it, subscribers_.end());

        auto new_sub = std::make_shared<Subscriber>();
        new_sub->id = client_id;
        new_sub->send_fn = send_fn;

        // 3. Smart Join: Send Cached Header if available
        // Find latest generation (max key)
        if (!cached_configs_.empty()) {
            auto latest_gen = cached_configs_.rbegin()->first;

            // Send Config
            const auto& config_pkt = cached_configs_.rbegin()->second;
            // Check if we have IDR for this gen
            auto idr_it = cached_idrs_.find(latest_gen);

            // Dispatch logic:
            // We can directly call send_fn here since we are inside subscribe
            // and assume it's thread-safe to call from here (blocking the bus lock briefly).
            // In high-perf, queue these. For Phase 1: Direct Send.

            // Re-enforce VideoPacket const-correctness by copying content or passing packet
            // BUT: send_fn takes vector<uint8_t>.
            // We need to Serialize Packet -> Bytes?
            // Architecture Note: The Bus manages VideoPackets. The Callback usually adapter to Bytes.
            // Let's assume the HAL/Streamer produces VideoPacket where 'data' IS the payload to send.

            if (config_pkt.data) new_sub->send_fn(*config_pkt.data);
            if (idr_it != cached_idrs_.end() && idr_it->second.data) {
                new_sub->send_fn(*idr_it->second.data);
            }
        }

        subscribers_.push_back(new_sub);
        std::cout << "[BroadcastBus] Client " << client_id << " subscribed." << std::endl;
    }

    void BroadcastBus::unsubscribe(uint32_t client_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::remove_if(subscribers_.begin(), subscribers_.end(),
            [client_id](const std::shared_ptr<Subscriber>& s){ return s->id == client_id; });
        if (it != subscribers_.end()) {
             subscribers_.erase(it, subscribers_.end());
             std::cout << "[BroadcastBus] Client " << client_id << " unsubscribed." << std::endl;
        }
    }

    void BroadcastBus::dispatch_to_subscriber(const std::shared_ptr<Subscriber>& sub, const common::VideoPacket& pkt) {
        // Backpressure Logic
        // For Phase 1: We simulate "Bound" by checking if previous send is done?
        // Actually, 'send_fn' in current legacy backend is BLOCKING (socket send).
        // This is dangerous.
        // CORRECT FIX: The Subscriber struct has a 'queue'.
        // We push to queue.
        // AND we need a Worker/Thread to drain it.
        // But since we don't have an async IoContext set up yet in this Phase 1 skeleton,
        // we will implement the "Drop Policy" but then call send_fn directly (Blocking).
        // REAL FIX LATER: 'send_fn' should be async / non-blocking.

        // Simulation of Drop Policy:
        if (sub->queue.size() >= sub->max_queue_size) {
            bool drop = true;

            if (pkt.kind == common::PacketKind::KeyFrame) {
                // Priority: Drop others, keep KeyFrame
                sub->queue.clear();
                sub->stats.force_clears++;
                drop = false;
            } else if (pkt.kind == common::PacketKind::CodecConfig) {
                sub->queue.clear();
                drop = false;
            }

            if (drop) {
                sub->stats.dropped_frames++;
                // If drop threshold exceeded -> unsubscribe (not implemented here safely inside loop)
                return;
            }
        }

        // Just push to queue (conceptually)
        // sub->queue.push_back(pkt);

        // Immediate Send (Legacy Mode Compatibility):
        // If we really want to support blocking socket send without killing encoder,
        // we MUST have a thread per subscriber or async IO.
        // For this Skeleton, we assume 'send_fn' is fast enough or client is fast.
        if (pkt.data) sub->send_fn(*pkt.data);
    }

} // namespace core
