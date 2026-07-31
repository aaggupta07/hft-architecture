#ifndef REORDER_BUFFER_HPP
#define REORDER_BUFFER_HPP

#include "config.hpp"

#include <atomic>
#include <array>
#include <span>
#include <optional>
#include <stop_token>

template<typename T, size_t CAPACITY>
class ReorderBuffer {
public:
	enum class WriteStatus: uint8_t {
		LikelySlowReader,
		LikelySlowWriter,
		Success,
		AlreadyWritten,
	};
private:
	static_assert((CAPACITY & (CAPACITY - 1)) == 0 && 
		"CAPACITY must be a power of 2.");

	enum class Status: uint8_t {
		Empty,
		Locked,
		Ready,
	};

	// It is likely that the consumer and producer threads read/write close to each other.
	// Thus, store status within the slot, and align each buffer slot to a separate cache line
	// to prevent false sharing. This does cost memory, especially for smaller types T.
	struct alignas(config::CACHE_LINE_SIZE) BufferSlot {
		T data;
		std::atomic<Status> status;
	};

	static constexpr size_t MASK = CAPACITY - 1;

	// This is necessary to prevent an (unlikely) but dangerous silent data race - see below
	static constexpr size_t MAX_INDEX_AHEAD = CAPACITY - 1; 

	std::array<BufferSlot, CAPACITY> buffer_;
	std::atomic<size_t> reader_index_ = 0;

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

	bool try_claim_slot(size_t index);
	WriteStatus validate_and_claim_slot(size_t index);
	

public:
	ReorderBuffer() = default;

	std::optional<T> 	try_consume_next();
	T 					wait_consume_next();
	std::optional<T>	wait_consume_next(std::stop_token stop_token);

	WriteStatus try_write_to(const T& data, size_t index);

	template<typename U>
	WriteStatus try_write_to(const std::span<const U> data, size_t index);
};

template<typename T, size_t CAPACITY>
std::optional<T> ReorderBuffer<T, CAPACITY>::try_consume_next() {
	size_t reader_slot = reader_index_.load(std::memory_order_relaxed) & MASK;
	if(buffer_[reader_slot].status.load(std::memory_order_acquire) != Status::Ready) {
		return std::nullopt;
	}

	/* Must occur in this order: if the buffer slot is marked empty first
	 * a producer with stale data could fill the buffer with stale data
	 * cause new data to be dropped, and silently cause stale data to be read as if new 
	 * MAX_INDEX_AHEAD ensures that a far-ahead producer doesn't see the Ready slot and 
	 * silently drop new data, thinking it was already written, and instead correctly
	 * issues a fatal error that the buffer has fallen too far behind. */
	T item = buffer_[reader_slot].data;
	reader_index_.fetch_add(1, std::memory_order_release);
	buffer_[reader_slot].status.store(Status::Empty, std::memory_order_release);
	return item;
}

template<typename T, size_t CAPACITY>
T ReorderBuffer<T, CAPACITY>::wait_consume_next() {
	while(true) {
		if(auto result = try_consume_next()) {
			return *result;
		}
		wait_for_progress();
	}
}

template<typename T, size_t CAPACITY>
std::optional<T> ReorderBuffer<T, CAPACITY>::wait_consume_next(std::stop_token stop_token) {
	while(!stop_token.stop_requested()) {
		if(auto result = try_consume_next()) {
			return result;
		}
		wait_for_progress();
	}
}

template<typename T, size_t CAPACITY>
bool ReorderBuffer<T, CAPACITY>::try_claim_slot(size_t index) {
	size_t writer_slot = index & MASK;
	Status expected = Status::Empty;

	// Relaxed is sufficient in both cases - the synchronization mechanism between
	// writers and the reader is a the acquire-release on the reader index. 
	return buffer_[writer_slot].status.compare_exchange_strong(
		expected, Status::Locked, std::memory_order_relaxed, std::memory_order_relaxed
	);
}

template<typename T, size_t CAPACITY>
auto ReorderBuffer<T, CAPACITY>::validate_and_claim_slot(size_t index) -> WriteStatus {
	size_t reader_index = reader_index_.load(std::memory_order_acquire);
	if(index >= reader_index + MAX_INDEX_AHEAD) [[unlikely]] {
		Status status = buffer_[reader_index].status.load(std::memory_order_acquire);
		if(status == Status::Ready) return WriteStatus::LikelySlowReader;
		else return WriteStatus::LikelySlowWriter;
	}
	if(!try_claim_slot(index)) return WriteStatus::AlreadyWritten;
	return WriteStatus::Success;
}

template<typename T, size_t CAPACITY>
auto ReorderBuffer<T, CAPACITY>::try_write_to(const T& data, size_t index) -> WriteStatus {
	WriteStatus status = validate_and_claim_slot(index);
	if(status != WriteStatus::Success) return status;
	
	buffer_.data = data;
	buffer_.status.store(Status::Ready, std::memory_order_release);
}

// Template specialization when T = std::array<V, N>
// `data` should have size() = N and U should be type V
template<typename T, size_t CAPACITY>
template<typename U>
auto ReorderBuffer<T, CAPACITY>::try_write_to(const std::span<const U> data, size_t index) -> WriteStatus {
	WriteStatus status = validate_and_claim_slot(index);
	if(status != WriteStatus::Success) return status;

	buffer_.data = data;
	buffer_.status.store(Status::Ready, std::memory_order_release);
}

#endif