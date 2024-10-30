#pragma once
#include <queue>
#include <mutex>
#include <optional>
#include <condition_variable>
#include <chrono>
#include <type_traits>

namespace async_queue {

template<typename T, typename... Extensions>
class AsyncQueue;

// Primary template - the base AsyncQueue without extensions
template<typename T>
class AsyncQueue<T> {
protected:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
    bool closed_ = false;
    const size_t capacity_;

    // Protected interface for extensions
    virtual void on_push([[maybe_unused]] const T& item) {}
    virtual void on_pop([[maybe_unused]] const T& item) {}
    virtual void on_close() {}

public:
    explicit AsyncQueue(size_t capacity = std::numeric_limits<size_t>::max())
        : capacity_(capacity) {}

    virtual ~AsyncQueue() {
        close();
    }

    // Move operations
    AsyncQueue(AsyncQueue&& other) noexcept
        : capacity_(other.capacity_) {
        std::lock_guard<std::mutex> lock(other.mutex_);
        queue_ = std::move(other.queue_);
        closed_ = other.closed_;
    }

    AsyncQueue& operator=(AsyncQueue&& other) noexcept {
        if (this != &other) {
            if (capacity_ != other.capacity_) {
                throw std::runtime_error("Cannot move-assign queues with different capacities");
            }
            std::scoped_lock lock(mutex_, other.mutex_);
            queue_ = std::move(other.queue_);
            closed_ = other.closed_;
        }
        return *this;
    }

    // Delete copy operations
    AsyncQueue(const AsyncQueue&) = delete;
    AsyncQueue& operator=(const AsyncQueue&) = delete;

    // Core operations
    template<typename U>
    bool push(U&& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (closed_) {
            return false;
        }

        cv_.wait(lock, [this] { 
            return queue_.size() < capacity_ || closed_; 
        });

        if (closed_) {
            return false;
        }

        queue_.push(std::forward<U>(item));
        on_push(queue_.back());
        cv_.notify_one();
        return true;
    }

    template<typename Rep, typename Period>
    bool try_push(const T& item, 
                 const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (closed_) {
            return false;
        }

        if (!cv_.wait_for(lock, timeout, [this] { 
            return queue_.size() < capacity_ || closed_; 
        })) {
            return false;
        }

        if (closed_) {
            return false;
        }

        queue_.push(item);
        on_push(queue_.back());
        cv_.notify_one();
        return true;
    }

    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        cv_.wait(lock, [this] { 
            return !queue_.empty() || closed_; 
        });

        if (queue_.empty()) {
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop();
        on_pop(item);
        cv_.notify_one();
        return item;
    }

    template<typename Rep, typename Period>
    std::optional<T> try_pop(const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (!cv_.wait_for(lock, timeout, [this] { 
            return !queue_.empty() || closed_; 
        })) {
            return std::nullopt;
        }

        if (queue_.empty()) {
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop();
        on_pop(item);
        cv_.notify_one();
        return item;
    }

    void close() {
        std::unique_lock<std::mutex> lock(mutex_);
        closed_ = true;
        on_close();
        cv_.notify_all();
    }

    // Queue state
    bool is_closed() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    size_t capacity() const {
        return capacity_;
    }

    // Helper for extensions
    template<typename E>
    bool has_extension() const {
        return false;  // Base case - no extensions
    }
};

// Type trait to check for extensions
template<typename Queue, typename Extension>
struct has_extension : std::false_type {};

} // namespace async_queue
