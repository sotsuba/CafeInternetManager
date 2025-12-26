#include "PacketDispatcher.hpp"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace core {
namespace network {

    PacketDispatcher::PacketDispatcher(std::shared_ptr<INetworkSocket> socket)
        : socket_(socket) {
        rx_.reading_header = true;
        rx_.expected_len = sizeof(protocol::PacketHeader);
        rx_.buffer.resize(rx_.expected_len);
    }

    PacketDispatcher::~PacketDispatcher() {}

    void PacketDispatcher::register_handler(protocol::PacketType type, PacketHandler handler) {
        handlers_[type] = handler;
    }

    void PacketDispatcher::enqueue_packet(protocol::PacketType type, const std::vector<uint8_t>& payload, protocol::PriorityLane lane, uint32_t cid, uint32_t bid) {
        std::vector<uint8_t> raw = build_packet(type, payload, lane, cid, bid);
        OutgoingPacket pkt { lane, raw, cid, bid };

        std::lock_guard<std::mutex> lock(queue_mutex_);

        switch (lane) {
            case protocol::PriorityLane::Critical:
                lane_critical_.push_back(pkt);
                if (lane_critical_.size() > 2000) lane_critical_.pop_front();
                break;
            case protocol::PriorityLane::RealTime:
                if (lane_realtime_.size() >= 50) {
                    lane_realtime_.pop_front();
                }
                lane_realtime_.push_back(pkt);
                break;
            case protocol::PriorityLane::Bulk:
                 lane_bulk_.push_back(pkt);
                 break;
        }
    }

    bool PacketDispatcher::process_outbound_queue() {
        std::lock_guard<std::mutex> lock(queue_mutex_);

        OutgoingPacket* pkt = nullptr;

        if (!lane_critical_.empty()) {
            pkt = &lane_critical_.front();
        }
        else if (!lane_realtime_.empty()) {
            pkt = &lane_realtime_.front();
        }
        else if (!lane_bulk_.empty()) {
            pkt = &lane_bulk_.front();
        } else {
            return false; // Idle
        }

        auto [sent, err] = socket_->send(pkt->data.data(), pkt->data.size());

        if (err == SocketError::WouldBlock) {
            return true; // Busy
        }

        if (err == SocketError::Ok) {
            if (sent == pkt->data.size()) {
                if (pkt->lane == protocol::PriorityLane::Critical) lane_critical_.pop_front();
                else if (pkt->lane == protocol::PriorityLane::RealTime) lane_realtime_.pop_front();
                else lane_bulk_.pop_front();
            } else {
                std::vector<uint8_t> remaining(pkt->data.begin() + sent, pkt->data.end());
                pkt->data = remaining;
            }
            return true;
        }

        return false;
    }

    void PacketDispatcher::process_incoming_data(uint32_t cid) {
        (void)cid;
    }

    std::vector<uint8_t> PacketDispatcher::build_packet(protocol::PacketType type, const std::vector<uint8_t>& payload, protocol::PriorityLane lane, uint32_t cid, uint32_t bid) {
        // --- LEGACY HEADER ADAPTER (12 BYTES) ---
        static const int LEGACY_HEADER_SIZE = 12;

        // FALLBACK: Gateway connects to backend_id=1. If we have 0, force 1.
        if (bid == 0) bid = 1;

        uint32_t len = (uint32_t)payload.size();
        uint32_t net_len = htonl(len);
        uint32_t net_cid = htonl(cid);
        uint32_t net_bid = htonl(bid);

        // DEBUG: Inspect Values
        // std::cout << "[Dispatcher] Build Header: CID=" << cid << " BID=" << bid << " Len=" << len << std::endl;

        std::vector<uint8_t> packet;
        packet.reserve(LEGACY_HEADER_SIZE + len);

        uint8_t header[LEGACY_HEADER_SIZE];
        memcpy(header, &net_len, 4);
        memcpy(header+4, &net_cid, 4);
        memcpy(header+8, &net_bid, 4);

        packet.insert(packet.end(), header, header + LEGACY_HEADER_SIZE);
        packet.insert(packet.end(), payload.begin(), payload.end());

        return packet;
    }

} // namespace network
} // namespace core
