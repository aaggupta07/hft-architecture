#ifndef CLOSED_HASH_MAP_HPP
#define CLOSED_HASH_MAP_HPP

#include <cstddef>
#include <functional>
#include <concepts>
#include <optional>
#include <cassert>
#include <algorithm>

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
    using OptionalValue = std::optional<std::reference_wrapper<Value>>;
	using ConstOptionalValue = std::optional<std::reference_wrapper<const Value>>;
    
    static constexpr size_t NULL_INDEX = std::numeric_limits<size_t>::max();
    static constexpr size_t MASK = CAPACITY - 1;
    static_assert((CAPACITY & MASK) == 0); // CAPACITY must be a power of 2

    std::array<KeyValuePair, CAPACITY> slots;
    std::array<SlotStatus, CAPACITY> occupied;
    Index saved = NULL_INDEX;

    static Index hash(const Key& key) {
        std::hash<Key> hash;
        return hash(key) & MASK;
    }

    Index probe_new(const Key& key) const {
        Index idx = hash(key);
        while(occupied[idx] == SlotStatus::Occupied) {
            idx = (idx + 1) & MASK;
        }
        return idx;
    }

    Index probe_existing(const Key& key) const {
        Index idx = hash(key);
        while(occupied[idx] != SlotStatus::Empty) {
            if(occupied[idx] == SlotStatus::Occupied && slots[idx].key == key) {
                return idx;
            }
            idx = (idx + 1) & MASK;
        }
        return NULL_INDEX;
    }

    // Saves the empty slot or first tombstone to `saved` for a future add on the same key, assuming the key doesn't exist
    Index probe_existing_and_save_tombstone(const Key& key) {
        Index idx = hash(key);
        bool is_saved = false;

        while(occupied[idx] != SlotStatus::Empty) {
            if(!is_saved && occupied[idx] == SlotStatus::Tombstone) {
                saved = idx;
                is_saved = true;
            }
            else if(slots[idx].key == key) {
                return idx;
            }
            idx = (idx + 1) & MASK;
        }

        if(!is_saved) saved = idx;
        return NULL_INDEX;
    }

public:
    ClosedHashMap() {
        std::ranges::fill(occupied, SlotStatus::Empty);
    }

    void add(const Key& key, Value value);
    void add_on_saved_index(const Key& key, Value value);
    bool remove(const Key& key);
    void remove_on_saved_index();

    OptionalValue find(const Key& key);
	ConstOptionalValue find(const Key& key) const;
    OptionalValue find_then_add(const Key& key);
    OptionalValue find_then_remove(const Key& key);
};

template<Hashable Key, typename Value, size_t CAPACITY>
void ClosedHashMap<Key, Value, CAPACITY>::add(const Key& key, Value value) {
    Index slot_index = probe_new(key);
    slots[slot_index] = KeyValuePair(key, std::move(value));
    occupied[slot_index] = SlotStatus::Occupied;
}

template<Hashable Key, typename Value, size_t CAPACITY>
void ClosedHashMap<Key, Value, CAPACITY>::add_on_saved_index(const Key& key, Value value) {
    assert(saved != NULL_INDEX);
    slots[saved] = KeyValuePair(key, std::move(value));
    occupied[saved] = SlotStatus::Occupied;
}

template<Hashable Key, typename Value, size_t CAPACITY>
bool ClosedHashMap<Key, Value, CAPACITY>::remove(const Key& key) {
    Index slot_idx = probe_existing(key);
    if(slot_idx == NULL_INDEX) return false;
    occupied[slot_idx] = SlotStatus::Tombstone;
    return true;
}

template<Hashable Key, typename Value, size_t CAPACITY>
void ClosedHashMap<Key, Value, CAPACITY>::remove_on_saved_index() {
    assert(saved != NULL_INDEX);
    occupied[saved] = SlotStatus::Tombstone;
}

template<Hashable Key, typename Value, size_t CAPACITY>
auto ClosedHashMap<Key, Value, CAPACITY>::find(const Key& key) -> OptionalValue {
    Index slot_idx = probe_existing(key);
    if(slot_idx == NULL_INDEX) return std::nullopt;
	return std::ref(slots[slot_idx].value);
}

template<Hashable Key, typename Value, size_t CAPACITY>
auto ClosedHashMap<Key, Value, CAPACITY>::find(const Key& key) const -> ConstOptionalValue {
    Index slot_idx = probe_existing(key);
    if(slot_idx == NULL_INDEX) return std::nullopt;
	return std::cref(slots[slot_idx].value);
}

template<Hashable Key, typename Value, size_t CAPACITY>
auto ClosedHashMap<Key, Value, CAPACITY>::find_then_add(const Key& key) -> OptionalValue {
    Index slot_idx = probe_existing_and_save_tombstone(key);
    if(slot_idx == NULL_INDEX) return std::nullopt;
    return std::ref(slots[slot_idx].value);
}

template<Hashable Key, typename Value, size_t CAPACITY>
auto ClosedHashMap<Key, Value, CAPACITY>::find_then_remove(const Key& key) -> OptionalValue {
    Index slot_idx = probe_existing(key);
    saved = slot_idx;
    if(slot_idx == NULL_INDEX) return std::nullopt;
    return slots[slot_idx].value;
}

#endif