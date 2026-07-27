#ifndef ORDER_POOL_HPP
#define ORDER_POOL_HPP

#include "order.hpp"
#include <numeric>
#include <new>



template<std::size_t CAPACITY>
class OrderPool {
private:
    static constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;

    alignas (CACHE_LINE_SIZE) Order orders[CAPACITY];
    Order::Index free_stack[CAPACITY];
    Order::Index top_index = CAPACITY;

public:
    OrderPool() {
        std::ranges::iota(free_stack, 0);
    }

    Order::Index allocate(Order order) {
        Order::Index order_idx = free_stack[--top_index];
        orders[order_idx] = order;
        return order_idx;
    }

    void free(Order::Index idx) {
        free_stack[top_index++] = idx;
    }

    Order& get(Order::Index idx) {
        return orders[idx];
    }
};

#endif