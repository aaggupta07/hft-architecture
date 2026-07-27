#ifndef ORDER_POOL_HPP
#define ORDER_POOL_HPP

#include <cstdint>
#include <limits>
#include <numeric>
#include <new>

using OrderID = uint64_t;
using Price = int32_t;
using Quantity = uint32_t;
using Index = uint32_t;

// Order: 32 bytes => Typically, two orders packed onto one cache line
struct Order {
    enum class Side: bool {
        Buy,
        Sell,
    };

	enum class Type: short {
		Add,
		Cancel,
		Trade,
	};
    
    static constexpr size_t NULL_INDEX = std::numeric_limits<Index>::max();
    OrderID order_id;
    Price price;
    Quantity quantity;
    Index previous = NULL_INDEX;
    Index next = NULL_INDEX;
    Side side;
	Type type;
};

template<std::size_t CAPACITY>
class OrderPool {
private:
    static constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;

    alignas (CACHE_LINE_SIZE) Order orders[CAPACITY];
    Index free_stack[CAPACITY];
    Index top_index = CAPACITY;

public:
    OrderPool() {
        std::ranges::iota(free_stack, 0);
    }

    Index allocate(Order order) {
        Index order_idx = free_stack[--top_index];
        orders[order_idx] = order;
        return order_idx;
    }

    void free(Index idx) {
        free_stack[top_index++] = idx;
    }

    Order& get(Index idx) {
        return orders[idx];
    }
};

#endif