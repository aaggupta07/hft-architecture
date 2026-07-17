#ifndef CLOSED_HASH_MAP_HPP
#define CLOSED_HASH_MAP_HPP

#include <cstddef>
#include <functional>
#include <concepts>

template<typename T>
concept Hashable = requires(T key) {
    {std::hash<T>{}(key)} -> std::convertible_to<std::size_t>;
};

template<Hashable Key, typename Value, std::size_t CAPACITY>
class ClosedHashMap {
private:
    using Index = std::size_t;
    static constexpr size_t MASK = CAPACITY - 1;
    static_assert((CAPACITY & MASK) == 0); // CAPACITY must be a power of 2

    Value slots[CAPACITY];
    bool occupied[CAPACITY];

    static constexpr size_t PRIME = 1610612741;

    static Index hash(const Key& key) {
        std::hash<Key> hash;
        return hash(key) & MASK;
    }

public:
    void add(const Key& key, Value&& value);
    bool remove(const Key& key);
    Value find(const Key& key);
};

#endif