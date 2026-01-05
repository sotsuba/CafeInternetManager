#import "MacOSAppManager.hpp"
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include <iostream>
#include <signal.h>
#include <algorithm>
#include <cctype>

namespace platform {
namespace macos {

MacOSAppManager::MacOSAppManager() {
    std::cout << "[MacOSAppManager] Created" << std::endl;
}

// ============================================================================
// Helper to convert string to lowercase
// ============================================================================

static std::string to_lower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return out;
}

// ============================================================================
// List Applications
// ============================================================================

std::vector<interfaces::AppEntry> MacOSAppManager::list_applications(bool only_running) {
    @autoreleasepool {
        std::vector<interfaces::AppEntry> results;

        if (only_running) {
            // List running applications
            NSArray<NSRunningApplication*>* apps = [[NSWorkspace sharedWorkspace] runningApplications];

            for (NSRunningApplication* app in apps) {
                // Only include regular applications (not background services)
                if ([app activationPolicy] != NSApplicationActivationPolicyRegular) {
                    continue;
                }

                interfaces::AppEntry entry;

                if ([app localizedName]) {
                    entry.name = [[app localizedName] UTF8String];
                }

                if ([app bundleIdentifier]) {
                    entry.id = [[app bundleIdentifier] UTF8String];
                }

                if ([[app bundleURL] path]) {
                    entry.exec = [[[app bundleURL] path] UTF8String];
                }

                entry.pid = static_cast<uint32_t>([app processIdentifier]);

                results.push_back(entry);
            }
        } else {
            // List installed applications
            if (installed_apps_.empty()) {
                refresh_installed_apps();
            }
            return installed_apps_;
        }

        return results;
    }
}

void MacOSAppManager::refresh_installed_apps() {
    @autoreleasepool {
        installed_apps_.clear();

        // Search in common application directories
        NSArray<NSString*>* searchPaths = @[
            @"/Applications",
            [@"~/Applications" stringByExpandingTildeInPath],
            @"/System/Applications"
        ];

        NSFileManager* fm = [NSFileManager defaultManager];

        for (NSString* basePath in searchPaths) {
            NSError* error = nil;
            NSArray<NSString*>* contents = [fm contentsOfDirectoryAtPath:basePath error:&error];

            if (error) continue;

            for (NSString* item in contents) {
                if (![item hasSuffix:@".app"]) continue;

                NSString* appPath = [basePath stringByAppendingPathComponent:item];
                NSBundle* bundle = [NSBundle bundleWithPath:appPath];

                interfaces::AppEntry entry;
                entry.exec = [appPath UTF8String];

                // Get display name
                NSDictionary* info = [bundle infoDictionary];
                NSString* displayName = info[@"CFBundleDisplayName"];
                if (!displayName) {
                    displayName = info[@"CFBundleName"];
                }
                if (!displayName) {
                    displayName = [[item stringByDeletingPathExtension] copy];
                }

                entry.name = [displayName UTF8String];

                // Bundle identifier as ID
                NSString* bundleId = [bundle bundleIdentifier];
                entry.id = bundleId ? [bundleId UTF8String] : entry.name;

                entry.pid = 0;  // Not running

                installed_apps_.push_back(entry);
            }
        }

        // Sort by name
        std::sort(installed_apps_.begin(), installed_apps_.end(),
                  [](const interfaces::AppEntry& a, const interfaces::AppEntry& b) {
                      return to_lower(a.name) < to_lower(b.name);
                  });

        std::cout << "[MacOSAppManager] Found " << installed_apps_.size() << " installed apps" << std::endl;
    }
}

// ============================================================================
// Launch Application
// ============================================================================

common::Result<uint32_t> MacOSAppManager::launch_app(const std::string& command) {
    @autoreleasepool {
        NSString* path = [NSString stringWithUTF8String:command.c_str()];

        // Check if it's a .app bundle path
        if ([path hasSuffix:@".app"]) {
            NSRunningApplication* app = [[NSWorkspace sharedWorkspace]
                launchApplicationAtURL:[NSURL fileURLWithPath:path]
                options:NSWorkspaceLaunchDefault
                configuration:@{}
                error:nil];

            if (app) {
                return common::Result<uint32_t>::ok(static_cast<uint32_t>([app processIdentifier]));
            }
        }

        // Try launching via bundle identifier
        NSRunningApplication* app = [[NSWorkspace sharedWorkspace]
            launchAppWithBundleIdentifier:[NSString stringWithUTF8String:command.c_str()]
            options:NSWorkspaceLaunchDefault
            additionalEventParamDescriptor:nil
            launchIdentifier:nil];

        if (app) {
            return common::Result<uint32_t>::ok(static_cast<uint32_t>([app processIdentifier]));
        }

        // Fallback: use 'open' command
        NSTask* task = [[NSTask alloc] init];
        [task setLaunchPath:@"/usr/bin/open"];
        [task setArguments:@[@"-a", [NSString stringWithUTF8String:command.c_str()]]];

        NSError* error = nil;
        [task launchAndReturnError:&error];

        if (error) {
            return common::Result<uint32_t>::err(
                common::ErrorCode::Unknown,
                [[error localizedDescription] UTF8String]
            );
        }

        // We don't get the PID when using 'open', return 0
        return common::Result<uint32_t>::ok(0);
    }
}

// ============================================================================
// Kill Process
// ============================================================================

common::EmptyResult MacOSAppManager::kill_process(uint32_t pid) {
    @autoreleasepool {
        // First try graceful termination via NSRunningApplication
        NSRunningApplication* app = [NSRunningApplication
            runningApplicationWithProcessIdentifier:static_cast<pid_t>(pid)];

        if (app) {
            if ([app terminate]) {
                return common::Result<common::Ok>::success();
            }
            // If graceful termination fails, force terminate
            if ([app forceTerminate]) {
                return common::Result<common::Ok>::success();
            }
        }

        // Fallback to SIGTERM
        if (kill(static_cast<pid_t>(pid), SIGTERM) == 0) {
            return common::Result<common::Ok>::success();
        }

        return common::Result<common::Ok>::err(
            common::ErrorCode::PermissionDenied,
            "Failed to terminate process " + std::to_string(pid)
        );
    }
}

// ============================================================================
// System Control
// ============================================================================

common::EmptyResult MacOSAppManager::shutdown_system() {
    // Use AppleScript for shutdown (requires user confirmation or admin rights)
    system("osascript -e 'tell app \"System Events\" to shut down'");
    return common::Result<common::Ok>::success();
}

common::EmptyResult MacOSAppManager::restart_system() {
    system("osascript -e 'tell app \"System Events\" to restart'");
    return common::Result<common::Ok>::success();
}

// ============================================================================
// Search Apps
// ============================================================================

std::vector<interfaces::AppEntry> MacOSAppManager::search_apps(const std::string& query) {
    if (installed_apps_.empty()) {
        refresh_installed_apps();
    }

    std::vector<interfaces::AppEntry> results;
    std::string q = to_lower(query);

    for (const auto& app : installed_apps_) {
        if (to_lower(app.name).find(q) != std::string::npos) {
            results.push_back(app);
        }
    }

    return results;
}

} // namespace macos
} // namespace platform
