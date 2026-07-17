#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

#include "order-pool.hpp"
#include "closed-hash-map.hpp"
#include <set>
#include <limits>

class OrderBook {
private:
    static constexpr size_t MAX_PRICE_LEVELS = 1 << 10;
    static constexpr size_t MAX_CONCURRENT_ORDERS = 1 << 14;
    static constexpr Price NO_BID = std::numeric_limits<Price>::min();
    static constexpr Price NO_OFFER = std::numeric_limits<Price>::max();

    struct BestOrderInfo {
        Price price;
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
    BestOrderInfo best_bid {NO_BID, 0};
    BestOrderInfo best_offer {NO_OFFER, 0};

    void detach(const Order& order, PriceLevel& price_level);
    void purge_order(OrderID resting_order_id);
    void purge_order(Index order_index, Order& order);
    
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
    void cancel(Order order);
    void trade(Order order);

    void cancel(OrderID resting_order_id);
    void trade(OrderID resting_order_id, Quantity quantity);
};



#endif