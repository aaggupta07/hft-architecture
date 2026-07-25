#ifndef LOCK_FREE_BUFFER_HPP
#define LOCK_FREE_BUFFER_HPP

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <new>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>

/*
 * An efficient, lazy-initialized single-producer single-consumer 
 * C++23 thread-safe ring buffer for placeable in MAP_SHARED.
 * Wait semantics use polling instead of sem_t for performance.
 *
 * Required:
 * - CAPACITY is the number of elements of type T storable in the ring buffer.
 * 	 This must be a non-zero power of 2.
 * - T is trivially copyable and destroyable.
 * - std::atomic<size_t> is lock-free.
 */
template <typename T, size_t CAPACITY>
class SharedRingBuffer {
private:
    static_assert(CAPACITY != 0 && (CAPACITY & (CAPACITY - 1)) == 0,
                  "CAPACITY must be a non-zero power of 2");
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable for shared-memory IPC");
    static_assert(std::is_trivially_destructible_v<T>,
                  "T must be trivially destructible for shared-memory IPC");
    static_assert(std::atomic<size_t>::is_always_lock_free,
                  "std::atomic<size_t> must be lock-free");

    using Clock = std::chrono::steady_clock;
    using TimeOut = std::chrono::time_point<Clock>;
    using milliseconds = std::chrono::milliseconds;

    static constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
    static constexpr size_t MASK = CAPACITY - 1;
    static constexpr milliseconds DEFAULT_TIMEOUT{1};

    struct alignas(T) Slot {
        std::byte storage[sizeof(T)];
    };

    // Keep the ownership indices on distinct cache lines.  All remaining
    // state is position independent, so mappings may have different bases.
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};
    alignas(CACHE_LINE_SIZE) std::array<Slot, CAPACITY> buffer_{};

    inline static constexpr bool empty(size_t head, size_t tail) noexcept {
        return head == tail;
    }

    inline static constexpr bool full(size_t head, size_t tail) noexcept {
        return tail - head == CAPACITY;
    }

    inline constexpr T* storage_slot(size_t sequence) noexcept {
        return reinterpret_cast<T*>(buffer_[sequence & MASK].storage);
    }

    template <typename U>
    bool try_push_internal(U&& item) {
        const size_t current_tail = tail_.load(std::memory_order_acquire);
        if (full(head_.load(std::memory_order_relaxed), current_tail)) {
            return false;
        }

        std::construct_at(storage_slot(current_tail), std::forward<U>(item));
        tail_.store(current_tail + 1, std::memory_order_release);
        return true;
    }

    template <typename U>
    bool wait_push_internal(U&& item, milliseconds timeout, TimeOut until) {
        while (!try_push_internal(std::forward<U>(item))) {
            if (Clock::now() >= until) {
                return false;
            }
            std::this_thread::sleep_for(timeout);
        }
        return true;
    }

public:
    SharedRingBuffer() = default;
    ~SharedRingBuffer() = default;

    SharedRingBuffer(const SharedRingBuffer&) = delete;
    SharedRingBuffer& operator=(const SharedRingBuffer&) = delete;
    SharedRingBuffer(SharedRingBuffer&&) = delete;
    SharedRingBuffer& operator=(SharedRingBuffer&&) = delete;

    bool try_push(T&& item) { return try_push_internal(std::move(item)); }
    bool try_push(const T& item) { return try_push_internal(item); }

    // May sleep past `until` by at most `timeout`. Waiting is polling-based for performance
    bool wait_push(T&& item, milliseconds timeout = DEFAULT_TIMEOUT,
                   TimeOut until = TimeOut::max()) {
        return wait_push_internal(std::move(item), timeout, until);
    }

    bool wait_push(const T& item, milliseconds timeout = DEFAULT_TIMEOUT,
                   TimeOut until = TimeOut::max()) {
        return wait_push_internal(item, timeout, until);
    }

    std::optional<T> try_pop() {
        const size_t current_head = head_.load(std::memory_order_acquire);
        if (empty(current_head, tail_.load(std::memory_order_relaxed))) {
            return std::nullopt;
        }

        T value = *std::launder(storage_slot(current_head));
        head_.store(current_head + 1, std::memory_order_release);
        return value;
    }

    // May sleep past `until` by at most `timeout`
    std::optional<T> wait_pop(milliseconds timeout = DEFAULT_TIMEOUT,
                                             TimeOut until = TimeOut::max()) {
        while (true) {
            if (auto value = try_pop()) {
                return value;
            }
            if (Clock::now() >= until) {
                return std::nullopt;
            }
            std::this_thread::sleep_for(timeout);
        }
    }

    // These observations are inherently racy with the other endpoint
    bool empty() const noexcept {
        return empty(head_.load(std::memory_order_relaxed),
                     tail_.load(std::memory_order_relaxed));
    }

    bool full() const noexcept {
        return full(head_.load(std::memory_order_relaxed),
                    tail_.load(std::memory_order_relaxed));
    }

    static constexpr size_t capacity() noexcept { return CAPACITY; }
};

#endif
