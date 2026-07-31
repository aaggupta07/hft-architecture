#include "order-generator.hpp"
#include "log.hpp"
#include <cassert>

namespace exchange{
std::random_device MarketRequestGenerator::rd;
std::mt19937_64 MarketRequestGenerator::generator(MarketRequestGenerator::rd());

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

void MarketRequestGenerator::generate_and_post_cancel_request(std::stop_token stop_token) {
	OrderRequest request {
		.type = OrderRequest::Type::Cancel,
	};
	


	while(!stop_token.stop_requested()) {
		if(active_order_ids.is_empty()) {
			populate_active_orders(stop_token);
		}
		if(active_order_ids.is_empty()) return;

		request.order_id = active_order_ids.remove_random(generator).value();

		// Lazily removes orders that may have been traded against
		if(clob_.order_exists(request.order_id)) {
			auto result = clob_.submit(request, stop_token);
			if(!result) [[unlikely]] if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>) logging::write<config::LOGGING>("Market Request Generator", "Error: {}", result.error());

			if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) {
				logging::write<config::LOGGING>("Market Request Generator", "Generated cancel order #{}.", request.order_id);
			}
			else if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Minimal>) {
				static size_t cancel_count = 0;
				if(++cancel_count % logging::MINIMAL_INTERVAL == 0) logging::write<config::LOGGING>("Market Request Generator", "Generated {} cancel orders.", cancel_count);
			}
			return;
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

void MarketRequestGenerator::generate_and_post_new_order_request(OrderRequest::Type type, std::stop_token stop_token) {
	assert(type == OrderRequest::Type::Buy || type == OrderRequest::Type::Sell);
	
	if(active_order_ids.is_full()) {
		purge_active_orders(stop_token);
	}

	double random_value = order_type_distribution(generator);
	OrderRequest new_request;
	if(random_value < config::AGGRESSIVE_ORDER_PROBABILITY) {
		new_request =  generate_aggressive_order_request(type);
	}
	else [[likely]] {
		new_request = generate_passive_order_request(type);
	}

	auto result = clob_.submit(new_request, stop_token);
	if(!result) [[unlikely]] {
		if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Errors>) logging::write<config::LOGGING>("Market Request Generator", "Error: {}", result.error());
		return;
	}
	
	if(*result != Order::INVALID_ORDER_ID) {
		active_order_ids.add(*result);
	}
	if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Detailed>) {
		const char* type_name = type == OrderRequest::Type::Buy ? "Buy" : "Sell";
		logging::write<config::LOGGING>("Market Request Generator", "Generated {} order #{} at {} x {}.", type_name, new_request.order_id, new_request.price, new_request.quantity);
	}
	else if constexpr(logging::enabled<config::LOGGING, ::config::LogSetting::Minimal>) {
		static size_t new_order_count = 0;
		if(++new_order_count % logging::MINIMAL_INTERVAL == 0) logging::write<config::LOGGING>("Market Request Generator", "Generated {} new orders.", new_order_count);
	}
}

void MarketRequestGenerator::populate_active_orders(std::stop_token stop_token) {
	while(!stop_token.stop_requested() && active_order_ids.size() < config::POPULATE_WHEN_EMPTY) {
		double random_value = order_type_distribution(generator);
		OrderRequest::Type type = random_value < (NORMALIZED_BUY_SELL_PROBABILITY) ? OrderRequest::Type::Buy : OrderRequest::Type::Sell;
		generate_and_post_new_order_request(type, stop_token);
	}
}

void MarketRequestGenerator::purge_active_orders(std::stop_token stop_token) {
	while(!stop_token.stop_requested() && active_order_ids.size() > config::PURGE_UNTIL_WHEN_FULL) {
		generate_and_post_cancel_request(stop_token);
	}
}

void MarketRequestGenerator::generate_and_post_random_order_request(std::stop_token stop_token) {
	double random_value = order_type_distribution(generator);

	if(random_value < config::CANCEL_PROBABILITY) {
		generate_and_post_cancel_request(stop_token);
	}
	else if(random_value < config::CANCEL_PROBABILITY + config::BUY_PROBABILITY) {
		generate_and_post_new_order_request(OrderRequest::Type::Buy, stop_token);
	}
	else {
		generate_and_post_new_order_request(OrderRequest::Type::Sell, stop_token);
	}
}

void MarketRequestGenerator::start(std::stop_token stop_token) {
	populate_active_orders(stop_token);
	while(!stop_token.stop_requested()) {
		generate_and_post_random_order_request(stop_token);
	}
}
}
