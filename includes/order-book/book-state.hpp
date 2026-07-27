#ifndef BOOK_STATE_HPP
#define BOOK_STATE_HPP

#include "order-pool.hpp"
#include "closed-hash-map.hpp"
#include <set>
#include <limits>

class BookState {
public:
	enum class BookUpdate {
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

	static constexpr Order::Price NO_BID = std::numeric_limits<Order::Price>::min();
    static constexpr Order::Price NO_OFFER = std::numeric_limits<Order::Price>::max();

private:
    static constexpr size_t MAX_PRICE_LEVELS = 1 << 10;
    static constexpr size_t MAX_CONCURRENT_ORDERS = 1 << 14;

    using TotalQuantity = uint64_t;
    struct PriceLevel {
        Order::Index head;
        Order::Index tail;
        TotalQuantity total_quantity;
    };

    OrderPool<MAX_CONCURRENT_ORDERS> order_pool;
    ClosedHashMap<Order::Price, PriceLevel, MAX_PRICE_LEVELS> price_levels;
    ClosedHashMap<Order::ID, Order::Index, MAX_CONCURRENT_ORDERS> order_map;
    
    std::set<Order::Price, std::greater<>> bids;
    std::set<Order::Price> offers;

	OrderSnapshot bbo {
		.best_bid {NO_BID, 0},
		.best_offer {NO_OFFER, 0},
	};

    void detach(const Order& order, PriceLevel& price_level);
    BookUpdate purge_order(Order::ID resting_order_id);
    BookUpdate purge_order(Order::Index order_index, const Order& order);
    
    void add_bid_or_offer(const Order& order);
    void remove_bid_or_offer(const Order& order);

    BookUpdate update_bbo_add(const Order& order);
    BookUpdate update_best_bid_add(const Order& order);
    BookUpdate update_best_offer_add(const Order& order);

    BookUpdate update_bbo_reduce(const Order& order);
    BookUpdate update_best_bid_reduce(const Order& order);
    BookUpdate update_best_offer_reduce(const Order& order);

public:
    BookUpdate add(const Order& order);
    BookUpdate cancel(const Order& order);
    BookUpdate trade(const Order& order);

    BookUpdate cancel(Order::ID resting_order_id);
    BookUpdate trade(Order::ID resting_order_id, Order::Quantity quantity);

	BookUpdate execute(const Order& order);

	constexpr OrderSnapshot snapshot() const noexcept { return bbo; }
};



#endif