#pragma once
#include <iostream>
#include <functional> // This was in the original, but not in the provided "Code Edit" block. The instruction is to make the change faithfully. The "Code Edit" block implies replacing the includes. If I strictly follow the "Code Edit" block, <functional> would be removed. However, the methods like `std::function` are used later in the file, so removing it would break compilation. I will assume the "Code Edit" block was meant to *add* includes, not replace them entirely, especially since the instruction explicitly says "Add winsock2.h/arpa/inet.h". The instruction also says "Add start_recording, stop_recording, pause_recording, is_paused, and get_recording_path to IVideoStreamer.hpp" which are already there, implying the instruction is about *adding* things. Therefore, I will add the new includes while preserving existing necessary ones.

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include "common/Result.hpp"
#include "common/Cancellation.hpp"
#include "common/VideoTypes.hpp"

namespace interfaces {

    class IVideoStreamer {
    public:
        virtual ~IVideoStreamer() = default;

        // Core Stream Contract:
        // 1. Blocking Call: This method MUST block until the operation is cancelled via token
        //    or a critical error occurs.
        // 2. Non-Blocking Callback: The 'on_packet' callback MUST return quickly.
        //    It should push to a queue and return.
        // 3. Thread Safety: Safe to call from a worker thread.
        //    'token' must be checked frequently (e.g. every frame).
        virtual common::EmptyResult stream(
            std::function<void(const common::VideoPacket&)> on_packet,
            common::CancellationToken token
        ) = 0;

        // Snapshot Contract:
        // Returns a single raw frame (e.g. RGB) immediately.
        // Should be thread-safe regarding strict `stream` usage
        // (impl might need internal mutex if sharing same device).
        virtual common::Result<common::RawFrame> capture_snapshot() = 0;

        // Recording Contract:
        virtual common::Result<uint32_t> start_recording(const std::string& path) = 0;
        virtual common::EmptyResult stop_recording() = 0;
        virtual common::EmptyResult pause_recording() = 0;
        virtual bool is_paused() const = 0;
        virtual bool is_recording() const = 0;
        virtual std::string get_recording_path() const = 0;
    };

} // namespace interfaces
