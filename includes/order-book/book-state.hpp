#ifndef BOOK_STATE_HPP
#define BOOK_STATE_HPP

#include "order-pool.hpp"
#include "closed-hash-map.hpp"
#include <set>
#include <limits>

class BookState {
public:
	static constexpr Order::Price NO_BID = std::numeric_limits<Order::Price>::min();
    static constexpr Order::Price NO_OFFER = std::numeric_limits<Order::Price>::max();

	enum class Update {
		NoChange,
		NewBestBid,
		NewBestOffer,
	};

    struct BestOrderInfo {
        Order::Price price;
        Order::Quantity quantity = 0;
    };

	struct OrderSnapshot {
		BestOrderInfo best_bid;
		BestOrderInfo best_offer;
	};



private:
    static constexpr size_t MAX_PRICE_LEVELS = 1 << 10;
    static constexpr size_t MAX_CONCURRENT_ORDERS = 1 << 14;

    using TotalQuantity = uint64_t;
    struct PriceLevel {
        RestingOrder::Index head;
        RestingOrder::Index tail;
        TotalQuantity total_quantity;
    };

    OrderPool<MAX_CONCURRENT_ORDERS> order_pool;
    ClosedHashMap<Order::Price, PriceLevel, MAX_PRICE_LEVELS> price_levels;
    ClosedHashMap<Order::ID, RestingOrder::Index, MAX_CONCURRENT_ORDERS> order_map;
    
    std::set<Order::Price, std::greater<>> bids;
    std::set<Order::Price> offers;

	OrderSnapshot bbo {
		.best_bid {NO_BID, 0},
		.best_offer {NO_OFFER, 0},
	};

    void detach(const RestingOrder& order, PriceLevel& price_level);
    Update purge_order(Order::ID resting_order_id);
    Update purge_order(RestingOrder::Index order_index, const RestingOrder& resting_order);
    
    void add_bid_or_offer(const Order& order);
    void remove_bid_or_offer(const Order& order);

    Update update_bbo_add(const Order& order);
    Update update_best_bid_add(const Order& order);
    Update update_best_offer_add(const Order& order);

    Update update_bbo_reduce(const Order& order);
    Update update_best_bid_reduce(const Order& order);
    Update update_best_offer_reduce(const Order& order);

public:
    Update add		(const MarketEvent& event);
    Update cancel	(const MarketEvent& event);
    Update trade	(const MarketEvent& event);

    Update cancel(Order::ID resting_order_id);
    Update trade(Order::ID resting_order_id, Order::Quantity quantity);

	Update execute(const MarketEvent& event);

	constexpr OrderSnapshot snapshot() const noexcept { return bbo; }
	Order best_opposing_order(Order::Side side) const noexcept;
	bool order_exists(Order::ID order_id) const noexcept;
};



#endif