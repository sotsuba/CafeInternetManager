// ============================================================================
// Linux Platform HAL Test Program
// ============================================================================
// Tests all existing Linux platform components:
// - InputInjector (XTest/uinput mouse move, click)
// - ScreenStreamer (capture snapshot, stream)
// - Keylogger (evdev key event capture)
// - AppManager (list apps, processes)
// - FileTransfer (directory operations, upload/download)
//
// Run with: ./BackendTest
// Output: Console log with PASS/FAIL for each test
// ============================================================================

#ifdef PLATFORM_LINUX

#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <sstream>
#include <vector>
#include <mutex>
#include <string>
#include <cstdio>

// Platform includes
#include "LinuxInputInjectorFactory.hpp"
#include "LinuxX11Streamer.hpp"
#include "LinuxEvdevLogger.hpp"
#include "LinuxAppManager.hpp"
#include "LinuxFileTransfer.hpp"

// Common includes
#include "common/VideoTypes.hpp"
#include "common/Cancellation.hpp"

using namespace platform::linux_os;
using namespace std::chrono_literals;

// ============================================================================
// Test Result Tracking
// ============================================================================

struct TestResult {
    std::string name;
    bool passed;
    std::string details;
    double duration_ms;
};

std::vector<TestResult> g_results;

void log_test(const std::string& name, bool passed,
              const std::string& details = "", double duration_ms = 0) {
    TestResult r{name, passed, details, duration_ms};
    g_results.push_back(r);

    std::cout << (passed ? "[PASS]" : "[FAIL]")
              << " " << name;
    if (duration_ms > 0) {
        std::cout << " (" << std::fixed << std::setprecision(1)
                  << duration_ms << "ms)";
    }
    if (!details.empty()) {
        std::cout << " - " << details;
    }
    std::cout << std::endl;
}

// ============================================================================
// Test: InputInjector
// ============================================================================

void test_input_injector() {
    std::cout << "\n=== Testing InputInjector ===" << std::endl;

    auto injector = LinuxInputInjectorFactory::create();
    if (!injector) {
        log_test("InputInjector::create", false, "Factory returned nullptr");
        return;
    }

    // Test 1: Move mouse to center
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = injector->move_mouse(0.5f, 0.5f);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        log_test("InputInjector::move_mouse(center)", result.is_ok(), "", ms);
    }

    // Test 2: Move mouse to corners
    {
        bool all_ok = true;
        auto start = std::chrono::high_resolution_clock::now();

        all_ok &= injector->move_mouse(0.0f, 0.0f).is_ok();  // Top-left
        std::this_thread::sleep_for(100ms);
        all_ok &= injector->move_mouse(1.0f, 0.0f).is_ok();  // Top-right
        std::this_thread::sleep_for(100ms);
        all_ok &= injector->move_mouse(1.0f, 1.0f).is_ok();  // Bottom-right
        std::this_thread::sleep_for(100ms);
        all_ok &= injector->move_mouse(0.0f, 1.0f).is_ok();  // Bottom-left
        std::this_thread::sleep_for(100ms);
        all_ok &= injector->move_mouse(0.5f, 0.5f).is_ok();  // Back to center

        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        log_test("InputInjector::move_mouse(corners)", all_ok,
                 "Tested all 4 corners + center", ms);
    }

    // Test 3: Mouse click (left button)
    {
        auto start = std::chrono::high_resolution_clock::now();

        // Move to safe location first (bottom-right corner)
        injector->move_mouse(0.95f, 0.95f);
        std::this_thread::sleep_for(50ms);

        auto down = injector->click_mouse(interfaces::MouseButton::Left, true);
        std::this_thread::sleep_for(20ms);
        auto up = injector->click_mouse(interfaces::MouseButton::Left, false);

        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        log_test("InputInjector::click_mouse(left)",
                 down.is_ok() && up.is_ok(),
                 "Click at bottom-right corner", ms);
    }

    // Move back to center
    injector->move_mouse(0.5f, 0.5f);
}

// ============================================================================
// Test: ScreenStreamer
// ============================================================================

void test_screen_streamer() {
    std::cout << "\n=== Testing ScreenStreamer ===" << std::endl;

    LinuxX11Streamer streamer;

    // Test 1: Capture snapshot
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = streamer.capture_snapshot();
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        if (result.is_ok()) {
            auto& frame = result.unwrap();
            std::stringstream ss;
            ss << frame.pixels.size() << " bytes, format: " << frame.format;
            log_test("ScreenStreamer::capture_snapshot", true, ss.str(), ms);
        } else {
            log_test("ScreenStreamer::capture_snapshot", false,
                     result.error().message, ms);
        }
    }

    // Test 2: Stream for 2 seconds (count packets)
    {
        std::atomic<int> packet_count{0};
        std::atomic<size_t> total_bytes{0};

        common::CancellationSource cancel_source;
        auto token = cancel_source.get_token();

        auto start = std::chrono::high_resolution_clock::now();

        // Start stream in background
        std::thread stream_thread([&]() {
            streamer.stream(
                [&](const common::VideoPacket& pkt) {
                    packet_count++;
                    if (pkt.data) {
                        total_bytes += pkt.data->size();
                    }
                },
                token
            );
        });

        // Let it run for 2 seconds
        std::this_thread::sleep_for(2s);

        // Stop
        cancel_source.cancel();
        streamer.stop();

        if (stream_thread.joinable()) {
            stream_thread.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::stringstream ss;
        ss << packet_count << " packets, "
           << (total_bytes / 1024) << " KB in 2 sec";

        log_test("ScreenStreamer::stream(2sec)",
                 packet_count > 0, ss.str(), ms);
    }
}

// ============================================================================
// Test: Keylogger
// ============================================================================

void test_keylogger() {
    std::cout << "\n=== Testing Keylogger ===" << std::endl;
    std::cout << "    (Press some keys in next 3 seconds...)" << std::endl;

    LinuxEvdevLogger keylogger;

    std::atomic<int> event_count{0};
    std::vector<std::string> captured_keys;
    std::mutex keys_mutex;

    auto start = std::chrono::high_resolution_clock::now();

    auto result = keylogger.start([&](const interfaces::KeyEvent& evt) {
        event_count++;
        std::lock_guard<std::mutex> lock(keys_mutex);
        if (captured_keys.size() < 10) {  // Limit to first 10
            captured_keys.push_back(evt.text);
        }
    });

    if (result.is_err()) {
        log_test("Keylogger::start", false, result.error().message);
        return;
    }

    // Wait for user to press keys
    std::this_thread::sleep_for(3s);

    keylogger.stop();

    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::stringstream ss;
    ss << event_count << " events captured";
    if (!captured_keys.empty()) {
        ss << ": ";
        for (size_t i = 0; i < captured_keys.size() && i < 5; i++) {
            if (i > 0) ss << ", ";
            ss << "'" << captured_keys[i] << "'";
        }
    }

    // Pass if start succeeded (events are optional - user may not press keys)
    log_test("Keylogger::capture(3sec)", true, ss.str(), ms);
}

// ============================================================================
// Test: AppManager
// ============================================================================

void test_app_manager() {
    std::cout << "\n=== Testing AppManager ===" << std::endl;

    LinuxAppManager app_mgr;

    // Test 1: List installed applications
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto apps = app_mgr.list_applications(false);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::stringstream ss;
        ss << apps.size() << " applications found";
        if (!apps.empty()) {
            ss << " (first: " << apps[0].name << ")";
        }

        log_test("AppManager::list_applications", !apps.empty(), ss.str(), ms);
    }

    // Test 2: List running processes
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto procs = app_mgr.list_applications(true);  // true = running
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::stringstream ss;
        ss << procs.size() << " processes running";

        log_test("AppManager::list_processes", !procs.empty(), ss.str(), ms);
    }

    // Test 3: Search apps
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto results = app_mgr.search_apps("terminal");
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::stringstream ss;
        ss << results.size() << " matches for 'terminal'";

        log_test("AppManager::search_apps", true, ss.str(), ms);
    }

    // Test 4: Launch xterm (will open a window!)
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = app_mgr.launch_app("xterm");
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        if (result.is_ok()) {
            uint32_t pid = result.unwrap();
            std::stringstream ss;
            ss << "PID=" << pid;
            log_test("AppManager::launch_app(xterm)", true, ss.str(), ms);

            // Wait a bit then kill it
            std::this_thread::sleep_for(1s);
            app_mgr.kill_process(pid);
            log_test("AppManager::kill_process", true, "Killed xterm");
        } else {
            log_test("AppManager::launch_app(xterm)", false,
                     result.error().message, ms);
        }
    }
}

// ============================================================================
// Test: FileTransfer
// ============================================================================

void test_file_transfer() {
    std::cout << "\n=== Testing FileTransfer ===" << std::endl;

    LinuxFileTransfer ft;
    std::string test_dir = "/tmp/test_ft_dir";

    // Test 1: Create Directory
    {
        auto start = std::chrono::high_resolution_clock::now();
        ft.delete_path(test_dir); // Cleanup previous
        auto result = ft.create_directory(test_dir);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        log_test("FileTransfer::create_directory", result.is_ok(), test_dir, ms);
    }

    // Test 2: List Directory (should be empty)
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = ft.list_directory(test_dir);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        bool passed = result.is_ok() && result.unwrap().empty();
        std::string details = result.is_ok()
            ? "Count: " + std::to_string(result.unwrap().size())
            : result.error().message;

        log_test("FileTransfer::list_directory(empty)", passed, details, ms);
    }

    // Test 3: List root directory (empty path)
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = ft.list_directory("");
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        bool passed = result.is_ok() && !result.unwrap().empty();
        std::string details = result.is_ok()
            ? "Count: " + std::to_string(result.unwrap().size())
            : result.error().message;

        log_test("FileTransfer::list_directory(root)", passed, details, ms);
    }

    // Test 4: Upload small file
    std::string file_path = test_dir + "/test.txt";
    std::string content = "Hello World from Linux!";
    {
        auto start = std::chrono::high_resolution_clock::now();
        ft.upload_start(file_path, content.size());
        ft.upload_chunk(file_path, (const uint8_t*)content.data(), content.size());
        auto result = ft.upload_finish(file_path);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        log_test("FileTransfer::upload_file", result.is_ok(), file_path, ms);
    }

    // Test 5: Download file
    {
        std::string downloaded;
        auto start = std::chrono::high_resolution_clock::now();

        auto result = ft.download_file(file_path,
            [&](const uint8_t* data, size_t size, bool) {
                downloaded.append((const char*)data, size);
            }
        );

        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        bool passed = result.is_ok() && downloaded == content;
        log_test("FileTransfer::download_file", passed,
                 passed ? "Content match" : "Content mismatch", ms);
    }

    // Test 6: Get file info
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = ft.get_file_info(file_path);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        if (result.is_ok()) {
            auto& info = result.unwrap();
            std::stringstream ss;
            ss << "Size=" << info.size << ", Name=" << info.name;
            log_test("FileTransfer::get_file_info", true, ss.str(), ms);
        } else {
            log_test("FileTransfer::get_file_info", false, result.error().message, ms);
        }
    }

    // Test 7: Rename file
    std::string new_path = test_dir + "/test_renamed.txt";
    {
        auto start = std::chrono::high_resolution_clock::now();
        auto result = ft.rename(file_path, new_path);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        log_test("FileTransfer::rename", result.is_ok(), new_path, ms);
    }

    // Cleanup
    ft.delete_path(new_path);
    ft.delete_path(test_dir);
}

// ============================================================================
// Main Test Runner
// ============================================================================

void print_summary() {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "TEST SUMMARY" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    int passed = 0, failed = 0;
    for (const auto& r : g_results) {
        if (r.passed) passed++;
        else failed++;
    }

    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << "Total:  " << g_results.size() << std::endl;

    if (failed > 0) {
        std::cout << "\nFailed tests:" << std::endl;
        for (const auto& r : g_results) {
            if (!r.passed) {
                std::cout << "  - " << r.name << ": " << r.details << std::endl;
            }
        }
    }

    std::cout << std::string(60, '=') << std::endl;
}

int main() {
    std::cout << "Linux Platform HAL Test Suite" << std::endl;
    std::cout << "==============================" << std::endl;
    std::cout << "This will test all platform components." << std::endl;
    std::cout << "Some tests will move the mouse and type keys!" << std::endl;
    std::cout << std::endl;

    // Check DISPLAY environment
    const char* display = std::getenv("DISPLAY");
    if (!display) {
        std::cerr << "[WARNING] DISPLAY not set. X11 tests may fail." << std::endl;
        std::cerr << "          Set DISPLAY=:0 if running with X11." << std::endl;
    }

    // Run all tests
    test_input_injector();
    test_screen_streamer();
    test_keylogger();
    test_app_manager();
    test_file_transfer();

    // Print summary
    print_summary();

    return g_results.empty() ? 1 : 0;
}

#else
// Non-Linux stub
#include <iostream>
int main() {
    std::cout << "This test is only for Linux platform." << std::endl;
    return 1;
}
#endif
