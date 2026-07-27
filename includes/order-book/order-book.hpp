#ifndef ORDER_BOOK_HPP
#define ORDER_BOOK_HPP

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
        Price price;
        Quantity quantity = 0;
    };

	struct OrderSnapshot {
		BestOrderInfo best_bid;
		BestOrderInfo best_offer;
	};

	static constexpr Price NO_BID = std::numeric_limits<Price>::min();
    static constexpr Price NO_OFFER = std::numeric_limits<Price>::max();

private:
    static constexpr size_t MAX_PRICE_LEVELS = 1 << 10;
    static constexpr size_t MAX_CONCURRENT_ORDERS = 1 << 14;

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

	OrderSnapshot bbo {
		.best_bid {NO_BID, 0},
		.best_offer {NO_OFFER, 0},
	};

    void detach(const Order& order, PriceLevel& price_level);
    BookUpdate purge_order(OrderID resting_order_id);
    BookUpdate purge_order(Index order_index, const Order& order);
    
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

    BookUpdate cancel(OrderID resting_order_id);
    BookUpdate trade(OrderID resting_order_id, Quantity quantity);

	BookUpdate execute(const Order& order);

	constexpr OrderSnapshot snapshot() const noexcept { return bbo; }
};



#endif