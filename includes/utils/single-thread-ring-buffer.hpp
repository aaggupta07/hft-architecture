#ifndef SINGLE_THREAD_RING_BUFFER_HPP
#define SINGLE_THREAD_RING_BUFFER_HPP

#include <array>
#include <cstddef>
#include <utility>

/* A fixed-capacity ring buffer for use by a single thread. */
template<typename T, size_t CAPACITY>
class SingleThreadRingBuffer {
private:
	static_assert(CAPACITY != 0 && (CAPACITY & (CAPACITY - 1)) == 0,
		"CAPACITY must be a non-zero power of 2");

	static constexpr size_t MASK = CAPACITY - 1;

	std::array<T, CAPACITY> buffer_{};
	size_t head_ = 0;
	size_t tail_ = 0;

public:
	bool try_push(const T& item) noexcept {
		if(full()) return false;
		buffer_[head_ & MASK] = item;
		++head_;
		return true;
	}

	bool try_push(T&& item) noexcept {
		if(full()) return false;
		buffer_[head_ & MASK] = std::move(item);
		++head_;
		return true;
	}

	T* try_front() noexcept {
		if(empty()) return nullptr;
		return &buffer_[tail_ & MASK];
	}

	void pop() noexcept {
		if(!empty()) ++tail_;
	}

	void clear() noexcept { tail_ = head_; }
	bool empty() const noexcept { return head_ == tail_; }
	bool full() const noexcept { return head_ - tail_ == CAPACITY; }
	size_t size() const noexcept { return head_ - tail_; }
	size_t available() const noexcept { return CAPACITY - size(); }
	static constexpr size_t capacity() noexcept { return CAPACITY; }
};

#endif
