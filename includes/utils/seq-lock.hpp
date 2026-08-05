#ifndef SEQ_LOCK_HPP
#define SEQ_LOCK_HPP

#include <atomic>
#include <limits>
#include <optional>
#include <stop_token>

// Used as a personalized reader for a consumer reading a SeqLock
template<typename T>
class SeqLockReader;

// A simple seqlock for a single producer and one or more consumers
template<typename T>
class SeqLock {
private:
	static constexpr size_t LOCKED = std::numeric_limits<size_t>::max();
	// Set as locked when initialized to prevent readers from reading garbage data in T
	// Only works since the SeqLock is built for a single producer
	std::atomic<size_t> generation_ = LOCKED;
	T data_;

public:
	SeqLock() = default;

	void write(const T& data) {
		size_t previous_generation = generation_.load(std::memory_order_relaxed);
		generation_.store(LOCKED, std::memory_order_release);

		// Necessary to prevent UB - a concurrent unsynchronized read/write is a data race
		std::atomic_ref(data_).store(data, std::memory_order_relaxed);

		generation_.store(previous_generation + 1, std::memory_order_release);
	}

	std::optional<T> try_read() const {
		size_t first_generation = generation_.load(std::memory_order_acquire);
		if(first_generation == LOCKED) return std::nullopt;

		// Necessary to prevent UB - a concurrent unsynchronized read/write is a data race
		T data = std::atomic_ref(data_).load(std::memory_order_relaxed);

		size_t second_generation = generation_.load(std::memory_order_acquire);
		if(second_generation != first_generation) return std::nullopt;

		return data;
	}

	// Gets the value and updates the generation tracker to the new generation
	std::optional<T> try_read_and_update(size_t& generation_tracker) const {
		size_t first_generation = generation_.load(std::memory_order_acquire);
		if(first_generation == LOCKED) return std::nullopt;

		// Necessary to prevent UB - a concurrent unsynchronized read/write is a data race
		T data = std::atomic_ref(data_).load(std::memory_order_relaxed);

		size_t second_generation = generation_.load(std::memory_order_acquire);
		if(second_generation != first_generation) return std::nullopt;

		generation_tracker = second_generation;
		return data;
	}

	std::optional<T> wait_read(std::stop_token stop_token) const {
		while(!stop_token.stop_requested()) {
			std::optional<T> result = try_read();
			if(result) return result;
		}
		return std::nullopt;
	}

	std::optional<T> wait_read_and_update(std::stop_token stop_token, size_t& generation_tracker) const {
		while(!stop_token.stop_requested()) {
			std::optional<T> result = try_read_and_update(generation_tracker);
			if(result) return result;
		}
		return std::nullopt;
	}


	friend class SeqLockReader<T>;
	SeqLockReader<T> get_personalized_reader() const;
};

template<typename T>
class SeqLockReader {
private:
	const SeqLock<T>& seq_lock_;
	mutable size_t last_generation_seen_ = 0;
public:
	SeqLockReader(const SeqLock<T>& seq_lock)
		: seq_lock_(seq_lock) {}

	std::optional<T> wait_read_data(std::stop_token stop_token) const {
		while(!stop_token.stop_requested()) {
			size_t current_generation = seq_lock_.generation_.load(std::memory_order_acquire);
			if(current_generation == last_generation_seen_) continue;
			return seq_lock_.wait_read_and_update(stop_token, last_generation_seen_);
		}
		return std::nullopt;
	}
};

#endif