#include "order-generator.hpp"
#include <cassert>

namespace exchange{
Order::Price MarketRequestGenerator::generate_price(OrderRequest::Type type, bool aggressive) {
	assert(type == OrderRequest::Type::Buy || type == OrderRequest::Type::Sell);
	BookState::OrderSnapshot snapshot = clob_.snapshot();
	const Order::Price best_bid = snapshot.best_bid.price == Order::INVALID_ORDER_ID ? config::DEFAULT_BUY_PRICE : snapshot.best_bid.price;
	const Order::Price best_ask = snapshot.best_offer.price == Order::INVALID_ORDER_ID ? config::DEFAULT_SELL_PRICE : snapshot.best_offer.price;

	const Order::Price price_variance = aggressive ? config::LARGE_PRICE_VARIANCE : config::PRICE_VARIANCE;

	Order::Price min_price;
	Order::Price max_price;
	if(type == OrderRequest::Type::Buy && !aggressive) {
		min_price = best_bid - price_variance;
		max_price = best_bid + price_variance;
	}
	else if(type == OrderRequest::Type::Sell && !aggressive) {
		min_price = best_ask - price_variance;
		max_price = best_ask + price_variance;
	}
	else if(type == OrderRequest::Type::Buy && aggressive) {
		min_price = best_ask;
		max_price = best_ask + price_variance;
	}
	else if(type == OrderRequest::Type::Sell && aggressive) {
		min_price = best_bid - price_variance;
		max_price = best_bid;
	}

	min_price = std::max(min_price, config::MIN_PRICE);
	max_price = std::min(max_price, config::MAX_PRICE);
	std::uniform_int_distribution<Order::Price> price_distribution(min_price, max_price);

	return price_distribution(generator);
}

std::optional<OrderRequest> MarketRequestGenerator::generate_cancel_request() {
	OrderRequest request {
		.type = OrderRequest::Type::Cancel,
	};
	

	while(true) {
		auto result = active_order_ids.remove_random(generator);
		if(!result) return std::nullopt;

		// Lazily removes orders that may have been traded against
		if(clob_.order_exists(*result)) {
			request.order_id = *result;
			return request;
		}
	};
}


OrderRequest MarketRequestGenerator::generate_aggressive_order_request(OrderRequest::Type type) {
	assert(type == OrderRequest::Type::Buy || type == OrderRequest::Type::Sell);
	OrderRequest request {
		.price = generate_price(type, true),
		.quantity = large_quantity_distribution(generator),
		.type = type,
	};
	return request;
}

OrderRequest MarketRequestGenerator::generate_passive_order_request(OrderRequest::Type type) {
	assert(type == OrderRequest::Type::Buy || type == OrderRequest::Type::Sell);
	OrderRequest request {
		.price = generate_price(type, false),
		.quantity = quantity_distribution(generator),
		.type = type,
	};
	return request;
}

std::optional<OrderRequest> MarketRequestGenerator::generate_new_order_request(OrderRequest::Type type) {
	assert(type == OrderRequest::Type::Buy || type == OrderRequest::Type::Sell);
	
	double random_value = order_type_distribution(generator);
	OrderRequest new_request;
	if(random_value < config::AGGRESSIVE_ORDER_PROBABILITY) {
		new_request =  generate_aggressive_order_request(type);
	}
	else [[likely]] {
		new_request = generate_passive_order_request(type);
	}
	
	bool added = active_order_ids.add(new_request.order_id);
	if(!added) return std::nullopt;

	return new_request;
}

void MarketRequestGenerator::populate_active_orders() {
	while(active_order_ids.size() < config::POPULATE_WHEN_EMPTY) {
		double random_value = order_type_distribution(generator);
		OrderRequest::Type type = random_value < (NORMALIZED_BUY_SELL_PROBABILITY) ? OrderRequest::Type::Buy : OrderRequest::Type::Sell;
		auto result = generate_new_order_request(type);
		if(!result) [[unlikely]] break; // Active order list is full

		std::ignore = clob_.submit(*result);
	}
}

void MarketRequestGenerator::purge_active_orders() {
	while(active_order_ids.size() > config::PURGE_UNTIL_WHEN_FULL) {
		auto result = generate_cancel_request();
		if(!result) break; // No more active orders to cancel

		if(clob_.order_exists(result->order_id)) {
			std::ignore = clob_.submit(*result);
		}
	}
}

void MarketRequestGenerator::generate_and_post_random_order_request() {
	double random_value = order_type_distribution(generator);

	std::optional<OrderRequest> request;
	if(random_value < config::CANCEL_PROBABILITY) {
		request = generate_cancel_request();
		if(!request) [[unlikely]]{
			populate_active_orders();
			return;
		}
	}
	else if(random_value < config::CANCEL_PROBABILITY + config::BUY_PROBABILITY) {
		request = generate_new_order_request(OrderRequest::Type::Buy);
		if(!request) [[unlikely]] {
			purge_active_orders();
			return;
		}
	}
	else {
		request = generate_new_order_request(OrderRequest::Type::Sell);
		if(!request) [[unlikely]] {
			purge_active_orders();
			return;
		}
	}
	
	std::ignore = clob_.submit(*request);
}

void MarketRequestGenerator::launch() {
	populate_active_orders();
	while(true) {
		generate_and_post_random_order_request();
	}
}
}