#pragma once
#include <atomic>
#include <memory>
#include <stdexcept>

namespace common {

    class OperationCancelledException : public std::runtime_error {
    public:
        OperationCancelledException() : std::runtime_error("Operation Cancelled") {}
    };

    // The Token (View) - Passed to workers
    class CancellationToken {
        struct State {
            std::atomic<bool> requested{false};
        };
        std::shared_ptr<State> state;

    public:
        CancellationToken() : state(std::make_shared<State>()) {}

        // Check with ACQUIRE memory order (sees writes from owner)
        bool is_cancellation_requested() const {
            return state && state->requested.load(std::memory_order_acquire);
        }

        void throw_if_cancellation_requested() const {
            if(is_cancellation_requested()) throw OperationCancelledException();
        }

        friend class CancellationSource;
    };

    // The Source (Owner) - Held by controller
    class CancellationSource {
        CancellationToken token;

    public:
        CancellationSource() {
            // Token wraps the shared state created in its constructor
        }

        // Set with RELEASE memory order (flushes prior writes)
        void cancel() {
            if (token.state) {
                token.state->requested.store(true, std::memory_order_release);
            }
        }

        CancellationToken get_token() const { return token; }

        void reset() {
            token = CancellationToken(); // Create fresh state
        }
    };

} // namespace common
