#ifndef ORDER_GENERATOR_HPP
#define ORDER_GENERATOR_HPP

#include "protocol.hpp"
#include "clob.hpp"
#include "random-container.hpp"

#include <random>
#include <optional>

namespace exchange {
class MarketRequestGenerator {
private:
	static constexpr double BUY_PROBABILITY = 0.4;
	static constexpr double CANCEL_PROBABILITY = 0.2;
	static constexpr double SELL_PROBABILITY = 0.4;
	static constexpr double NORMALIZED_BUY_SELL_PROBABILITY = BUY_PROBABILITY / (BUY_PROBABILITY + SELL_PROBABILITY);

	static constexpr double AGGRESSIVE_ORDER_PROBABILITY = 0.02; // Probability of an order being highly aggressive, with LARGE_QUANTITY and crossing the spread likely across multiple resting orders

	static constexpr Order::Price MIN_PRICE = 1;
	static constexpr Order::Price MAX_PRICE = 1000;

	// Used as the midpoint for generating prices when the order book is empty
	static constexpr Order::Price DEFAULT_BUY_PRICE = 400;
	static constexpr Order::Price DEFAULT_SELL_PRICE = 600;

	static constexpr size_t PRICE_VARIANCE = 10; // On each side of the price, e.g. best price +/- PRICE_VARIANCE
	static constexpr size_t LARGE_PRICE_VARIANCE = 20; // On one side of the price for aggressive orders, e.g. best_ask + LARGE_PRICE_VARIANCE for buy orders, and best_bid - LARGE_PRICE_VARIANCE for sell orders

	static constexpr Order::Quantity MIN_QUANTITY = 1;
	static constexpr Order::Quantity MAX_QUANTITY = 100;

	// For aggressive orders
	static constexpr Order::Quantity MIN_LARGE_QUANTITY = 400;
	static constexpr Order::Quantity MAX_LARGE_QUANTITY = 600;

	// Note: this may be significantly higher than the number of resting orders in the book, since resting orders may have been traded against and removed from the book, but still present until they are lazily removed. The generator will clear a large number of orders (either stale or currently resting) when it reaches the MAX_ACTIVE_ORDERS limit, so that it can continue generating new orders.
	static constexpr size_t MAX_ACTIVE_ORDERS = 1 << 14;
	static constexpr size_t PURGE_UNTIL_WHEN_FULL = 1 << 12; // When the active order count reaches MAX_ACTIVE_ORDERS, clear until the active order count reaches PURGE_UNTIL_WHEN_FULL

	static constexpr size_t POPULATE_WHEN_EMPTY = 1 << 8; // Number of orders to generate when the active order count reaches 0

	static std::random_device rd;
	static std::mt19937_64 generator;
	std::uniform_real_distribution<double> order_type_distribution;
	std::uniform_int_distribution<Order::Quantity> quantity_distribution;
	std::uniform_int_distribution<Order::Quantity> large_quantity_distribution;
	
	RandomContainer<Order::ID, MAX_ACTIVE_ORDERS> active_order_ids;

	CentralLimitOrderBook& clob_; // Used for price generation

	Order::Price generate_price(OrderRequest::Type type, bool aggressive);
	std::optional<OrderRequest> generate_cancel_request();
	std::optional<OrderRequest> generate_new_order_request(OrderRequest::Type type);
	OrderRequest generate_passive_order_request(OrderRequest::Type type);
	OrderRequest generate_aggressive_order_request(OrderRequest::Type type);

	void populate_active_orders();
	void purge_active_orders();

	void generate_and_post_random_order_request();

public:
	constexpr MarketRequestGenerator(CentralLimitOrderBook& order_book)
		: order_type_distribution(0.0, 1.0),
		quantity_distribution(MIN_QUANTITY, MAX_QUANTITY),
		large_quantity_distribution(MIN_LARGE_QUANTITY, MAX_LARGE_QUANTITY),
		clob_(order_book) {}
	
	void launch();
};
}

#endif