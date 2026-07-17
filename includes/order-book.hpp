#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include "order-pool.hpp"
#include "closed-hash-map.hpp"
#include <set>

using TotalQuantity = uint64_t;
struct PriceLevel {
    Price price;
    Quantity order_count;
    Index head;
    Index tail;
    TotalQuantity total_quantity;
};

class OrderBook {
private:
    static constexpr size_t MAX_PRICE_LEVELS = 1 << 13;
    static constexpr size_t MAX_CONCURRENT_ORDERS = 1 << 20;

    OrderPool<MAX_CONCURRENT_ORDERS> order_pool;
    ClosedHashMap<Price, PriceLevel, MAX_PRICE_LEVELS> price_levels;
    ClosedHashMap<OrderID, Index, MAX_CONCURRENT_ORDERS> order_map;
    std::set<Price> prices;

    

public:
    void add(Order&& order);
    void cancel(OrderID resting_order_id);
    void trade(OrderID resting_order_id, Quantity quantity);
};

#endif