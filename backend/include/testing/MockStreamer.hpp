#pragma once
#include "interfaces/IVideoStreamer.hpp"
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>

namespace testing {

    class MockStreamer : public interfaces::IVideoStreamer {
    public:
        common::EmptyResult stream(
            std::function<void(const common::VideoPacket&)> on_packet,
            common::CancellationToken token
        ) override {
            uint64_t pts = 0;
            uint64_t gen = 1;

            // Generate dummy Config
            {
                auto cfg_data = std::make_shared<std::vector<uint8_t>>(10, 0xCC); // Fake SPS/PPS
                common::VideoPacket pkt{cfg_data, pts++, gen, common::PacketKind::CodecConfig};
                on_packet(pkt);
            }

            // Loop
            while (!token.is_cancellation_requested()) {
                // Fake IDR every 30 frames
                bool is_key = (pts % 30 == 0);
                auto data = std::make_shared<std::vector<uint8_t>>(1024, is_key ? 0xFF : 0x00);

                common::VideoPacket pkt{
                    data,
                    pts++,
                    gen,
                    is_key ? common::PacketKind::KeyFrame : common::PacketKind::InterFrame
                };

                on_packet(pkt);
                std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30fps
            }

            return common::Result<common::Ok>::success();
        }

        common::Result<common::RawFrame> capture_snapshot() override {
            common::RawFrame frame;
            frame.width = 640;
            frame.height = 480;
            frame.pixels.resize(640 * 480 * 3, 255); // White noise
            return common::Result<common::RawFrame>::ok(frame);
        }
    };

} // namespace testing
