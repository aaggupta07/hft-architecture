#include "order-generator.hpp"
#include <cassert>
#include <print>

namespace exchange{
std::random_device MarketRequestGenerator::rd;
std::mt19937_64 MarketRequestGenerator::generator(MarketRequestGenerator::rd());

void MarketRequestGenerator::log_error(const Error& error) {
	std::println("[Market Request Generator] Error: {}", error);
}

void MarketRequestGenerator::log(const OrderRequest& request) {
	std::string order_type;
	switch(request.type) {
		case OrderRequest::Type::Buy:
			order_type = "Buy";
			break;
		case OrderRequest::Type::Sell:
			order_type = "Sell";
			break;
		case OrderRequest::Type::Cancel:
			order_type = "Cancel";
			break;
	}

	std::println(
		"New Order Request Generated:\n"
		"\tOrder Type: {}\n"
		"\tOrder ID: {}\n"
		"\tOrder Price: {}\n"
		"\tOrder Quantity: {}\n",
		order_type, request.order_id, request.price, request.quantity
	);
}

Order::Price MarketRequestGenerator::generate_price(OrderRequest::Type type, bool aggressive) {
	assert(type == OrderRequest::Type::Buy || type == OrderRequest::Type::Sell);
	BookState::OrderSnapshot snapshot = clob_.snapshot();
	const Order::Price best_bid = snapshot.best_bid.price == BookState::NO_BID ? config::DEFAULT_BUY_PRICE : snapshot.best_bid.price;
	const Order::Price best_ask = snapshot.best_offer.price == BookState::NO_OFFER ? config::DEFAULT_SELL_PRICE : snapshot.best_offer.price;

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

void MarketRequestGenerator::generate_and_post_cancel_request() {
	OrderRequest request {
		.type = OrderRequest::Type::Cancel,
	};
	


	while(true) {
		if(active_order_ids.is_empty()) {
			populate_active_orders();
		}

		request.order_id = active_order_ids.remove_random(generator).value();

		// Lazily removes orders that may have been traded against
		if(clob_.order_exists(request.order_id)) {
			auto result = clob_.submit(request);
			if(!result && config::LOGGING) [[unlikely]] {
				log_error(result.error());
			}

			if constexpr(config::LOGGING) {
				log(request);
			}
			
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

void MarketRequestGenerator::generate_and_post_new_order_request(OrderRequest::Type type) {
	assert(type == OrderRequest::Type::Buy || type == OrderRequest::Type::Sell);
	
	if(active_order_ids.is_full()) {
		purge_active_orders();
	}

	double random_value = order_type_distribution(generator);
	OrderRequest new_request;
	if(random_value < config::AGGRESSIVE_ORDER_PROBABILITY) {
		new_request =  generate_aggressive_order_request(type);
	}
	else [[likely]] {
		new_request = generate_passive_order_request(type);
	}

	auto result = clob_.submit(new_request);
	if(!result && config::LOGGING) [[unlikely]] {
		log_error(result.error());
		return;
	}

	active_order_ids.add(*result);
	if constexpr(config::LOGGING) {
		log(new_request);
	}
}

void MarketRequestGenerator::populate_active_orders() {
	while(active_order_ids.size() < config::POPULATE_WHEN_EMPTY) {
		double random_value = order_type_distribution(generator);
		OrderRequest::Type type = random_value < (NORMALIZED_BUY_SELL_PROBABILITY) ? OrderRequest::Type::Buy : OrderRequest::Type::Sell;
		generate_and_post_new_order_request(type);
	}
}

void MarketRequestGenerator::purge_active_orders() {
	while(active_order_ids.size() > config::PURGE_UNTIL_WHEN_FULL) {
		generate_and_post_cancel_request();
	}
}

void MarketRequestGenerator::generate_and_post_random_order_request() {
	double random_value = order_type_distribution(generator);

	if(random_value < config::CANCEL_PROBABILITY) {
		generate_and_post_cancel_request();
	}
	else if(random_value < config::CANCEL_PROBABILITY + config::BUY_PROBABILITY) {
		generate_and_post_new_order_request(OrderRequest::Type::Buy);
	}
	else {
		generate_and_post_new_order_request(OrderRequest::Type::Sell);
	}
}

void MarketRequestGenerator::start() {
	populate_active_orders();
	while(true) {
		generate_and_post_random_order_request();
	}
}
}