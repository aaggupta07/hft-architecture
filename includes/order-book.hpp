#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include "order-pool.hpp"
#include "closed-hash-map.hpp"
#include <set>

class OrderBook {
private:
    static constexpr size_t MAX_PRICE_LEVELS = 1 << 13;
    static constexpr size_t MAX_CONCURRENT_ORDERS = 1 << 20;

    struct BestOrderInfo {
        Price price = 0;
        Quantity quantity = 0;
    };

    using TotalQuantity = uint64_t;
    struct PriceLevel {
        Index head;
        Index tail;
        TotalQuantity total_quantity;
    };

    OrderPool<MAX_CONCURRENT_ORDERS> order_pool;
    ClosedHashMap<Price, PriceLevel, MAX_PRICE_LEVELS> price_levels;
    ClosedHashMap<OrderID, Index, MAX_CONCURRENT_ORDERS> order_map;
    
    std::set<Price, std::greater<>> bids;
    std::set<Price> offers;
    BestOrderInfo best_bid;
    BestOrderInfo best_offer;

    void detach(const Order& order, PriceLevel& price_level);
    void purge_order(OrderID resting_order_id);
    
    void add_bid_or_offer(Order& order);
    void remove_bid_or_offer(Order& order);

    void update_bbo_add(Order& order);
    void update_best_bid_add(Order& order);
    void update_best_offer_add(Order& order);

    void update_bbo_reduce(Order& order);
    void update_best_bid_reduce(Order& order);
    void update_best_offer_reduce(Order& order);

    void publish_new_best_bid();
    void publish_new_best_offer();

public:
    void add(Order order);
    void cancel(OrderID resting_order_id);
    void trade(OrderID resting_order_id, Quantity quantity);
};



#endif