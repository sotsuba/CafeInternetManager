#include "core/PlatformRegistry.hpp"
#include <iostream>

namespace core {

// ============================================================================
// Singleton Instance
// ============================================================================

PlatformRegistry& PlatformRegistry::instance() {
    static PlatformRegistry instance;
    return instance;
}

PlatformRegistry::~PlatformRegistry() {
    shutdown();
}

// ============================================================================
// Factory Registration
// ============================================================================

void PlatformRegistry::register_factory(
    std::unique_ptr<interfaces::IPlatformFactory> factory
) {
    if (!factory) return;

    std::lock_guard<std::mutex> lock(mutex_);

    std::cout << "[Platform] Registered: " << factory->platform_name() << std::endl;
    factories_.push_back(std::move(factory));
}

// ============================================================================
// Platform Access
// ============================================================================

interfaces::IPlatformFactory* PlatformRegistry::get_current_platform() {
    // Fast path: already cached
    if (current_platform_) {
        return current_platform_;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Double-check after acquiring lock
    if (current_platform_) {
        return current_platform_;
    }

    // Find the factory for the current platform
    for (auto& factory : factories_) {
        if (factory->is_current_platform()) {
            current_platform_ = factory.get();

            // Initialize if not already done
            if (!initialized_) {
                current_platform_->initialize();
                initialized_ = true;
            }

            std::cout << "[Platform] Using: " << current_platform_->platform_name()
                      << std::endl;
            return current_platform_;
        }
    }

    std::cerr << "[Platform] WARNING: No factory for current platform!" << std::endl;
    return nullptr;
}

interfaces::IPlatformFactory* PlatformRegistry::get_platform(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& factory : factories_) {
        if (factory->platform_name() == name) {
            return factory.get();
        }
    }

    return nullptr;
}

std::vector<std::string> PlatformRegistry::list_platforms() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> names;
    names.reserve(factories_.size());

    for (const auto& factory : factories_) {
        names.push_back(factory->platform_name());
    }

    return names;
}

// ============================================================================
// Lifecycle
// ============================================================================

void PlatformRegistry::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) return;

    if (current_platform_) {
        current_platform_->initialize();
    }

    initialized_ = true;
}

void PlatformRegistry::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) return;

    for (auto& factory : factories_) {
        factory->shutdown();
    }

    current_platform_ = nullptr;
    initialized_ = false;
}

} // namespace core
