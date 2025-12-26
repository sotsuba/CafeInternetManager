#pragma once
#include <vector>
#include <deque>
#include <mutex>
#include <functional>
#include <map>
#include <atomic>
#include <memory>
#include "core/Protocol.hpp"
#include "interfaces/INetworkSocket.hpp"

namespace core {
namespace network {

    struct OutgoingPacket {
        protocol::PriorityLane lane;
        std::vector<uint8_t> data;
        uint32_t cid;
        uint32_t bid; // Backend ID (Echoed)
    };

    class PacketDispatcher {
    public:
        using PacketHandler = std::function<void(uint32_t cid, const std::vector<uint8_t>& payload)>;

        explicit PacketDispatcher(std::shared_ptr<INetworkSocket> socket);
        ~PacketDispatcher();

        // Updated to accept BID
        void enqueue_packet(protocol::PacketType type, const std::vector<uint8_t>& payload, protocol::PriorityLane lane, uint32_t cid, uint32_t bid);

        bool process_outbound_queue();

        void register_handler(protocol::PacketType type, PacketHandler handler);
        void process_incoming_data(uint32_t cid);

    private:
        std::vector<uint8_t> build_packet(protocol::PacketType type, const std::vector<uint8_t>& payload, protocol::PriorityLane lane, uint32_t cid, uint32_t bid);

        std::shared_ptr<INetworkSocket> socket_;
        std::map<protocol::PacketType, PacketHandler> handlers_;

        std::mutex queue_mutex_;
        std::deque<OutgoingPacket> lane_critical_;
        std::deque<OutgoingPacket> lane_realtime_;
        std::deque<OutgoingPacket> lane_bulk_;

        struct RxState {
            std::vector<uint8_t> buffer;
            size_t expected_len = 0;
            bool reading_header = true;
        } rx_;
    };

} // namespace network
} // namespace core
