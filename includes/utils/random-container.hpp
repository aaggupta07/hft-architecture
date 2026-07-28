#ifndef RANDOM_CONTAINER_HPP
#define RANDOM_CONTAINER_HPP

#include <new>
#include <random>
#include <array>
#include <cstddef>
#include <optional>

template<typename T, size_t CAPACITY>
class RandomContainer {
private:
	struct alignas(T) Slot {
		std::byte storage[sizeof(T)];
	};

	std::array<Slot, CAPACITY> buffer_;
	size_t size_ = 0;

	T* storage_slot(size_t index) noexcept {
		return reinterpret_cast<T*>(buffer_[index].storage);
	}

	size_t get_random_index(std::mt19937_64& generator) const {
		std::uniform_int_distribution<size_t> distribution(0, size_ - 1);
		return distribution(generator);
	}

public:
	RandomContainer() = default;
	~RandomContainer() = default;

	RandomContainer(const RandomContainer&) = delete;
	RandomContainer& operator=(const RandomContainer&) = delete;
	RandomContainer(RandomContainer&&) = delete;
	RandomContainer& operator=(RandomContainer&&) = delete;

	bool add(const T& item) {
		if(size_ < CAPACITY) [[likely]] {
			std::construct_at(storage_slot(size_), item);
			size_++;
			return true;
		}
		
		return false;
	}

	std::optional<T> get_random(std::mt19937_64& generator) const {
		if(size_ == 0) return std::nullopt;

		size_t index = get_random_index(generator);
		return *std::launder(storage_slot(index));
	}

	std::optional<T> remove_random(std::mt19937_64& generator) {
		if(size_ == 0) return std::nullopt;

		size_t index = get_random_index(generator);
		T* item_ptr = storage_slot(index);
		T item = std::move(*std::launder(item_ptr));
		std::destroy_at(item_ptr);

		if(index != size_ - 1) {
			std::construct_at(item_ptr, std::move(*std::launder(storage_slot(size_ - 1))));
			std::destroy_at(storage_slot(size_ - 1));
		}
		
		size_--;
		return item;
	}

	size_t size() const noexcept {
		return size_;
	}

	size_t capacity() const noexcept {
		return CAPACITY;
	}
};

#endif