#ifndef ORDER_POOL_HPP
#define ORDER_POOL_HPP

#include <cstdint>
#include <limits>
#include <numeric>

using OrderID = uint64_t;
using Price = int32_t;
using Quantity = uint32_t;
using Side = char;
using Index = uint32_t;

struct Order {
    OrderID order_id;
    Price price;
    Quantity quantity;
    Side side;
    Index previous;
    Index next;
};

template<std::size_t CAPACITY>
class OrderPool {
private:
    static constexpr std::size_t NULL_IDX = std::numeric_limits<Index>::max();

    Order orders[CAPACITY];
    Index free_stack[CAPACITY];
    Index top_index = CAPACITY;

public:
    OrderPool() {
        std::ranges::iota(free_stack, 0);
    }

    Index allocate(Order&& order) {
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