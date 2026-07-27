#ifndef ORDER_POOL_HPP
#define ORDER_POOL_HPP

#include "order.hpp"
#include <numeric>
#include <new>

template<std::size_t CAPACITY>
class OrderPool {
private:
    static constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;

    alignas (CACHE_LINE_SIZE) RestingOrder orders[CAPACITY];
    RestingOrder::Index free_stack[CAPACITY];
    RestingOrder::Index top_index = CAPACITY;

public:
    OrderPool() {
        std::ranges::iota(free_stack, 0);
    }

    RestingOrder::Index allocate(RestingOrder order) {
        RestingOrder::Index order_idx = free_stack[--top_index];
        orders[order_idx] = order;
        return order_idx;
    }

    void free(RestingOrder::Index idx) {
        free_stack[top_index++] = idx;
    }

    RestingOrder& get(RestingOrder::Index idx) {
        return orders[idx];
    }

	const RestingOrder& get(RestingOrder::Index idx) const {
		return orders[idx];
	}
};

#endif