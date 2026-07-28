#ifndef LOCK_FREE_CACHE
#define LOCK_FREE_CACHE

#include "config.hpp"

#include <array>
#include <atomic>
#include <expected>
#include <limits>

/*
An SPSC ring buffer that allows the consumer to freely access any index
The producer never waits - each slot is secured by a SeqLock
CAPACITY is the number of cachable elements - must be a nonzero multiple of 2
*/
template<typename T, size_t CAPACITY>
class CircularCache {
private:
	using Index = uint32_t;

	static_assert(CAPACITY != 0 && (CAPACITY & (CAPACITY - 1)) == 0 ,
					"CAPACITY must be a nonzero multiple of 2.");
	static_assert(std::atomic<Index>::is_always_lock_free,
					"std::atomic<size_t> must be lock free");
	
	static constexpr size_t MASK = CAPACITY - 1;
	static constexpr Index IS_LOCKED = std::numeric_limits<Index>::max();

	size_t write_index_ = 0;
	alignas(config::CACHE_LINE_SIZE) std::array<std::atomic<Index>, CAPACITY> sequence_;
	alignas(config::CACHE_LINE_SIZE) std::array<std::atomic<T>, CAPACITY> buffer_;
	
	
	void put_item_internal(T&& item);
	
public:
	constexpr CircularCache() = default;

	enum class Error {
		DataTooOld,
		DataDoesNotExist,
		ConcurrentWrite,
	};

	std::expected<T, Error> try_get_item	(Index index) const;
	std::expected<T, Error> wait_get_item	(Index index) const;
	void 					put_item(T&& item) 			{	put_item_internal(std::move(item));	};
	void					put_item(const T& item)		{	put_item_internal(item); };
};

template<typename T, size_t CAPACITY>
auto CircularCache<T, CAPACITY>::try_get_item(Index item_index) const -> std::expected<T, Error> {
	Index read_index = item_index & MASK;
	Index seq_before = sequence_[read_index].load(std::memory_order_acquire);
	if(seq_before == IS_LOCKED) {
		return std::unexpected(Error::ConcurrentWrite);
	}
	if(seq_before < item_index) {
		return std::unexpected(Error::DataDoesNotExist);
	}
	if(seq_before > item_index) {
		return std::unexpected(Error::DataTooOld);
	}
	
	T item = buffer_[read_index].load(std::memory_order_relaxed);

	Index seq_after = sequence_[read_index].load(std::memory_order_acquire);
	if(seq_before != seq_after) {
		return std::unexpected(Error::ConcurrentWrite);
	}

	return item;
}

template<typename T, size_t CAPACITY>
void CircularCache<T, CAPACITY>::put_item_internal(T&& item) {
	Index write_index = write_index_ & MASK;
	sequence_[write_index].store(IS_LOCKED, std::memory_order_release); // Lock SeqLock
	buffer_[write_index].store(item, std::memory_order_relaxed);
	sequence_[write_index].store(write_index_, std::memory_order_release); // Release SeqLock
	write_index_++;
}

template<typename T, size_t CAPACITY>
auto CircularCache<T, CAPACITY>::wait_get_item(Index item_index) const -> std::expected<T, Error> {
	auto result = try_get_item(item_index);
	while(!result && result.error() == Error::ConcurrentWrite) {
		result = try_get_item(item_index);
	}
	return result;
}

#endif