#pragma once
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include "interfaces/IPlatformFactory.hpp"

namespace core {

// ============================================================================
// PlatformRegistry - Singleton registry for platform factories
// ============================================================================
// Thread-safe singleton that manages platform factory registration and
// provides access to the current platform's factory.
//
// Usage:
//   // At startup (in main.cpp):
//   #ifdef PLATFORM_WINDOWS
//   PlatformRegistry::instance().register_factory(
//       std::make_unique<WindowsPlatformFactory>()
//   );
//   #endif
//
//   // In business logic:
//   auto* factory = PlatformRegistry::instance().get_current_platform();
//   auto streamer = factory->create_screen_streamer();
//
// Thread Safety:
// - register_factory(): Thread-safe, call at startup only
// - get_current_platform(): Thread-safe, lock-free after initialization
// ============================================================================

class PlatformRegistry {
public:
    // ========== Singleton Access ==========

    static PlatformRegistry& instance();

    // Deleted copy/move (singleton)
    PlatformRegistry(const PlatformRegistry&) = delete;
    PlatformRegistry& operator=(const PlatformRegistry&) = delete;
    PlatformRegistry(PlatformRegistry&&) = delete;
    PlatformRegistry& operator=(PlatformRegistry&&) = delete;

    // ========== Factory Registration ==========

    // Register a platform factory
    // Typically called once per platform at startup
    void register_factory(std::unique_ptr<interfaces::IPlatformFactory> factory);

    // ========== Platform Access ==========

    // Get the factory for the current running platform
    // Returns nullptr if no matching factory is registered
    // Thread-safe (cached pointer after first call)
    interfaces::IPlatformFactory* get_current_platform();

    // Get a factory by platform name
    // Useful for testing or cross-platform scenarios
    interfaces::IPlatformFactory* get_platform(const std::string& name);

    // List all registered platform names
    std::vector<std::string> list_platforms() const;

    // ========== Lifecycle ==========

    // Initialize the current platform (called automatically on first use)
    void initialize();

    // Shutdown all platforms and release resources
    void shutdown();

private:
    PlatformRegistry() = default;
    ~PlatformRegistry();

    mutable std::mutex mutex_;
    std::vector<std::unique_ptr<interfaces::IPlatformFactory>> factories_;
    interfaces::IPlatformFactory* current_platform_ = nullptr;
    bool initialized_ = false;
};

// ============================================================================
// Helper: Auto-registration helper (for static initialization)
// ============================================================================

template<typename FactoryType>
struct PlatformRegistrar {
    PlatformRegistrar() {
        PlatformRegistry::instance().register_factory(
            std::make_unique<FactoryType>()
        );
    }
};

// Usage:
// static PlatformRegistrar<WindowsPlatformFactory> _windows_registrar;

} // namespace core
