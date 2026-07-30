#ifndef LAZY_RING_BUFFER_HPP
#define LAZY_RING_BUFFER_HPP

#include "config.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <stop_token>

/* A single-producer, single-consumer ring buffer using a claim-then-publish
 * interface. Unlike SharedRingBuffer, each slot is a T object, so the array
 * default-initializes all CAPACITY objects when the buffer is constructed.
 *
 * The producer claims a writable slot with get_head_ref(), writes it, then
 * calls publish(). The consumer claims a readable slot with get_tail_ref(),
 * reads or moves from it, then calls consume().  A claimed reference remains
 * valid until its endpoint advances its corresponding cursor.
 *
 * CAPACITY must be a non-zero power of two. std::atomic<size_t> must be
 * lock-free. T must be default constructible. */
template <typename T, size_t CAPACITY>
class LazyRingBuffer {
private:
    static_assert(CAPACITY != 0 && (CAPACITY & (CAPACITY - 1)) == 0,
                  "CAPACITY must be a non-zero power of 2");
    static_assert(std::atomic<size_t>::is_always_lock_free,
                  "std::atomic<size_t> must be lock-free");

    static constexpr size_t MASK = CAPACITY - 1;

    // Avoid false sharing
    alignas(config::CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
    alignas(config::CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};
    alignas(config::CACHE_LINE_SIZE) std::array<T, CAPACITY> buffer_{};

    static constexpr bool empty(size_t head, size_t tail) noexcept {
        return head == tail;
    }

    static constexpr bool full(size_t head, size_t tail) noexcept {
        return head - tail == CAPACITY;
    }

    /* Apple Silicon optimization only: 
	 * Uses ARM's event mechanism to reduce power while spinning. 
	 * SEVL makes the first WFE return immediately. Publishers issue SEV
	 * after releasing their cursor to wake a thread that is already waiting. */ 
    static void wait_for_progress() noexcept {
        #if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
            asm volatile("sevl\n\twfe" ::: "memory");
        #endif
    }

    static void notify_waiter() noexcept {
        #if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
            asm volatile("sev" ::: "memory");
        #endif
    }

public:
    LazyRingBuffer() = default;
    ~LazyRingBuffer() = default;

    LazyRingBuffer(const LazyRingBuffer&) = delete;
    LazyRingBuffer& operator=(const LazyRingBuffer&) = delete;
    LazyRingBuffer(LazyRingBuffer&&) = delete;
    LazyRingBuffer& operator=(LazyRingBuffer&&) = delete;

    // Returns a writable producer slot, or nullptr when the buffer is full.
    T* try_get_head_ref() noexcept {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        if (full(current_head, tail_.load(std::memory_order_acquire))) {
            return nullptr;
        }
        return &buffer_[current_head & MASK];
    }

    // Spins until a writable producer slot is available.
    T& wait_get_head_ref() noexcept {
        while (true) {
            if (auto slot = try_get_head_ref()) {
                return *slot;
            }
            wait_for_progress();
        }
    }

    T* wait_get_head_ref(std::stop_token stop_token) noexcept {
        while (!stop_token.stop_requested()) {
            if (auto slot = try_get_head_ref()) {
                return slot;
            }
            wait_for_progress();
        }
        return nullptr;
    }

    // Makes the currently claimed producer slot visible to the consumer.
    void publish() noexcept {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        head_.store(current_head + 1, std::memory_order_release);
        notify_waiter();
    }

    // Returns a readable consumer slot, or nullptr when the buffer is empty.
    T* try_get_tail_ref() noexcept {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        if (empty(head_.load(std::memory_order_acquire), current_tail)) {
            return nullptr;
        }
        return &buffer_[current_tail & MASK];
    }

    // Spins until a readable consumer slot is available.
    T& wait_get_tail_ref() noexcept {
        while (true) {
            if (auto slot = try_get_tail_ref()) {
                return *slot;
            }
            wait_for_progress();
        }
    }

    T* wait_get_tail_ref(std::stop_token stop_token) noexcept {
        while (!stop_token.stop_requested()) {
            if (auto slot = try_get_tail_ref()) {
                return slot;
            }
            wait_for_progress();
        }
        return nullptr;
    }

    // Releases the currently claimed consumer slot back to the producer.
    void consume() noexcept {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        tail_.store(current_tail + 1, std::memory_order_release);
        notify_waiter();
    }

    // These observations are inherently racy with the other endpoint.
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
