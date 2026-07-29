#ifndef ORDER_GENERATOR_HPP
#define ORDER_GENERATOR_HPP

#include "protocol.hpp"
#include "clob.hpp"
#include "random-container.hpp"
#include "config.hpp"
#include "order.hpp"
#include "exchange-errors.hpp"

#include <random>

namespace exchange {
class MarketRequestGenerator {
private:
	static constexpr double NORMALIZED_BUY_SELL_PROBABILITY = config::BUY_PROBABILITY / (config::BUY_PROBABILITY + config::SELL_PROBABILITY);

	static std::random_device rd;
	static std::mt19937_64 generator;
	std::uniform_real_distribution<double> order_type_distribution;
	std::uniform_int_distribution<Order::Quantity> quantity_distribution;
	std::uniform_int_distribution<Order::Quantity> large_quantity_distribution;
	
	RandomContainer<Order::ID, config::MAX_ACTIVE_ORDERS> active_order_ids;

	CentralLimitOrderBook& clob_; // Used for price generation

	Order::Price generate_price(OrderRequest::Type type, bool aggressive);
	void generate_and_post_cancel_request();
	void generate_and_post_new_order_request(OrderRequest::Type type);
	OrderRequest generate_passive_order_request(OrderRequest::Type type);
	OrderRequest generate_aggressive_order_request(OrderRequest::Type type);

	void populate_active_orders();
	void purge_active_orders();

	void generate_and_post_random_order_request();

	static void log(const OrderRequest& order_request); 
	static void log_error(const Error& error);

public:
	explicit constexpr MarketRequestGenerator(CentralLimitOrderBook& order_book)
		: order_type_distribution(0.0, 1.0),
		quantity_distribution(config::MIN_QUANTITY, config::MAX_QUANTITY),
		large_quantity_distribution(config::MIN_LARGE_QUANTITY, config::MAX_LARGE_QUANTITY),
		clob_(order_book) {}
	
	void start();
};
}

#endif