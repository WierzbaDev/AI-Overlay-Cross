#pragma once

#include "Common.h"

// ============================================================================
// IPCCommand — all actions the WebSocket thread can request of the UI thread
// ============================================================================
struct CmdToggleClickthrough {};

struct CmdSetOpacity {
    uint8_t value; // 0–255
};

using IPCCommand = std::variant<CmdToggleClickthrough, CmdSetOpacity>;

// ============================================================================
// MessageQueue<T>
//
// Thread-safe, multi-producer / single-consumer queue.
//
// Producers (WS thread):  Push()
// Consumer  (UI thread):  TryDrainAll()
//
// After Push(), the producer must notify the UI thread via the platform
// dispatcher (PostMessage on Windows, dispatch_async on macOS).
// ============================================================================
template<typename T>
class MessageQueue {
public:
    MessageQueue() = default;

    // Push from any thread. Returns false if already shut down.
    bool Push(T item) {
        {
            std::lock_guard<std::mutex> lk(mutex_);
            if (shutdown_) return false;
            queue_.push(std::move(item));
        }
        cv_.notify_one();
        return true;
    }

    // Drain all pending items into `out` without blocking.
    // Returns the number of items drained.
    size_t TryDrainAll(std::vector<T>& out) {
        std::lock_guard<std::mutex> lk(mutex_);
        size_t count = 0;
        while (!queue_.empty()) {
            out.push_back(std::move(queue_.front()));
            queue_.pop();
            ++count;
        }
        return count;
    }

    // Blocking pop — for consumer threads only.
    bool BlockingPop(T& out) {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_.wait(lk, [this] { return !queue_.empty() || shutdown_; });
        if (queue_.empty()) return false;
        out = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void Shutdown() {
        {
            std::lock_guard<std::mutex> lk(mutex_);
            shutdown_ = true;
        }
        cv_.notify_all();
    }

    bool IsEmpty() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex      mutex_;
    std::condition_variable cv_;
    std::queue<T>           queue_;
    bool                    shutdown_ = false;
};

using IPCQueue = MessageQueue<IPCCommand>;
