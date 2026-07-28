#ifndef LOCK_FREE_BUFFER_HPP
#define LOCK_FREE_BUFFER_HPP

#include "config.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <new>

/*
 * An efficient, lazy-initialized single-producer single-consumer 
 * C++23 thread-safe ring buffer for placeable in MAP_SHARED.
 * Wait semantics spin instead of using sem_t or timeouts. On Apple Silicon,
 * waiters use ARM's event mechanism to reduce power while awaiting progress.
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

    static constexpr size_t MASK = CAPACITY - 1;

    struct alignas(T) Slot {
        std::byte storage[sizeof(T)];
    };

    // Keep the ownership indices on distinct cache lines.  All remaining
    // state is position independent, so mappings may have different bases.
    alignas(config::CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
    alignas(config::CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};
    alignas(config::CACHE_LINE_SIZE) std::array<Slot, CAPACITY> buffer_{};

    inline static constexpr bool empty(size_t head, size_t tail) noexcept {
        return head == tail;
    }

    inline static constexpr bool full(size_t head, size_t tail) noexcept {
        return tail - head == CAPACITY;
    }

    inline constexpr T* storage_slot(size_t sequence) noexcept {
        return reinterpret_cast<T*>(buffer_[sequence & MASK].storage);
    }

    /* Apple Silicon optimization only: 
	 * Uses ARM's event mechanism to reduce power while spinning. 
	 * SEVL makes the first WFE return immediately. Publishers issue SEV
	 * after releasing their cursor to wake a thread that is already waiting.
	*/ 
    static inline void wait_for_progress() noexcept {
		#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
				asm volatile("sevl\n\twfe" ::: "memory");
		#endif
    }

    static inline void notify_waiter() noexcept {
		#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
				asm volatile("sev" ::: "memory");
		#endif
    }

    template <typename U>
    bool try_push_internal(U&& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        if (full(head_.load(std::memory_order_acquire), current_tail)) {
            return false;
        }

        std::construct_at(storage_slot(current_tail), std::forward<U>(item));
        tail_.store(current_tail + 1, std::memory_order_release);
        notify_waiter();
        return true;
    }

    template <typename U>
    void wait_push_internal(U&& item) {
        while (!try_push_internal(std::forward<U>(item))) {
            wait_for_progress();
        }
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

    // Spins until space is available.
    void wait_push(T&& item) {
        wait_push_internal(std::move(item));
    }

    void wait_push(const T& item) {
        wait_push_internal(item);
    }

    std::optional<T> try_pop() {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        if (empty(current_head, tail_.load(std::memory_order_acquire))) {
            return std::nullopt;
        }

        T value = *std::launder(storage_slot(current_head));
        head_.store(current_head + 1, std::memory_order_release);
        notify_waiter();
        return value;
    }

    // Spins until an item is available.
    T wait_pop() {
        while (true) {
            if (auto value = try_pop()) {
                return std::move(*value);
            }
            wait_for_progress();
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
