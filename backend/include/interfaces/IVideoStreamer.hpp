#pragma once
#include <functional>
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
    };

} // namespace interfaces
