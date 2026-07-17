#ifndef CLOSED_HASH_MAP_HPP
#define CLOSED_HASH_MAP_HPP

#include <cstddef>
#include <functional>
#include <concepts>
#include <optional>

template<typename T>
concept Hashable = requires(T key) {
    {std::hash<T>{}(key)} -> std::convertible_to<size_t>;
};

/*
Closed Hash Hash Map (Linear Probing)
    add() creates a duplicate key if the key already exists — use find() first
    remove() is safe even if the key does not exist
*/
template<Hashable Key, typename Value, size_t CAPACITY>
class ClosedHashMap {
private:
    enum class SlotStatus : uint8_t {
        Empty,
        Tombstone,
        Occupied,
    };

    struct KeyValuePair {
        Key key;
        Value value;

        KeyValuePair() = default;
        KeyValuePair(Key k, Value v): key(k), value(v) {}
    };

    using Index = size_t;
    
    static constexpr size_t NULL_INDEX = std::numeric_limits<size_t>::max();
    static constexpr size_t MASK = CAPACITY - 1;
    static_assert((CAPACITY & MASK) == 0); // CAPACITY must be a power of 2

    KeyValuePair slots[CAPACITY];
    SlotStatus occupied[CAPACITY];

    static constexpr size_t PRIME = 1610612741;

    static Index hash(const Key& key) {
        std::hash<Key> hash;
        return hash(key) & MASK;
    }

    Index probe_new(const Key& key) {
        Index idx = hash(key);
        while(occupied[idx] == SlotStatus::Occupied) {
            idx = (idx + 1) & MASK;
        }
        return idx;
    }

    Index probe_existing(const Key& key) {
        Index idx = hash(key);
        while(occupied[idx] != SlotStatus::Empty) {
            if(occupied[idx] == SlotStatus::Occupied && slots[idx].key == key) {
                return idx;
            }
            idx = (idx + 1) & MASK;
        }
        return NULL_INDEX;
    }

public:
    void add(const Key& key, Value&& value);
    bool remove(const Key& key);
    std::optional<std::reference_wrapper<Value>> find(const Key& key);
};

template<Hashable Key, typename Value, size_t CAPACITY>
void ClosedHashMap<Key, Value, CAPACITY>::add(const Key& key, Value&& value) {
    Index slot_idx = probe_new(key);
    slots[slot_idx] = KeyValuePair(key, std::move(value));
    occupied[slot_idx] = SlotStatus::Occupied;
}

template<Hashable Key, typename Value, size_t CAPACITY>
bool ClosedHashMap<Key, Value, CAPACITY>::remove(const Key& key) {
    Index slot_idx = probe_existing(key);
    if(slot_idx == NULL_INDEX) return false;
    occupied[slot_idx] = SlotStatus::Tombstone;
}

template<Hashable Key, typename Value, size_t CAPACITY>
std::optional<std::reference_wrapper<Value>> ClosedHashMap<Key, Value, CAPACITY>::find(const Key& key) {
    Index slot_idx = probe_existing(key);
    if(slot_idx == NULL_INDEX) return std::nullopt;
    return slots[slot_idx].value;
}

#endif