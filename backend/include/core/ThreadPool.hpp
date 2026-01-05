#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <future>

namespace core {

/**
 * @brief A simple ThreadPool for executing tasks asynchronously.
 *
 * This pool maintains a fixed number of worker threads that process
 * tasks from a shared queue. Designed for command processing where
 * tasks should not block each other.
 */
class ThreadPool {
public:
    /**
     * @brief Construct a ThreadPool with the specified number of workers.
     * @param num_threads Number of worker threads (default: 4)
     */
    explicit ThreadPool(size_t num_threads = 4) : stop_(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this, i]() {
                worker_loop(i);
            });
        }
    }

    /**
     * @brief Destructor. Gracefully shuts down all workers.
     */
    ~ThreadPool() {
        shutdown();
    }

    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * @brief Submit a task to be executed by a worker thread.
     * @param task The callable to execute
     * @return std::future<void> to wait for completion (optional)
     */
    std::future<void> submit(std::function<void()> task) {
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (stop_) {
                promise->set_exception(std::make_exception_ptr(
                    std::runtime_error("ThreadPool is stopped")));
                return future;
            }

            tasks_.emplace([task = std::move(task), promise]() mutable {
                try {
                    task();
                    promise->set_value();
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            });
        }

        condition_.notify_one();
        return future;
    }

    /**
     * @brief Submit a task without waiting for result (fire-and-forget).
     * @param task The callable to execute
     */
    void submit_detached(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (stop_) return;
            tasks_.emplace(std::move(task));
        }
        condition_.notify_one();
    }

    /**
     * @brief Gracefully shutdown the pool, waiting for all tasks to complete.
     */
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (stop_) return;
            stop_ = true;
        }
        condition_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    /**
     * @brief Get the number of pending tasks in the queue.
     */
    size_t pending_tasks() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }

    /**
     * @brief Get the number of worker threads.
     */
    size_t num_workers() const {
        return workers_.size();
    }

private:
    void worker_loop(size_t worker_id) {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                condition_.wait(lock, [this]() {
                    return stop_ || !tasks_.empty();
                });

                if (stop_ && tasks_.empty()) {
                    return; // Exit worker
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            }

            // Execute task outside the lock
            task();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
};

} // namespace core
