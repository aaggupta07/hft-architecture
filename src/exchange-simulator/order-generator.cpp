#include "order-generator.hpp"
#include <cassert>

namespace exchange{
Order::Price MarketRequestGenerator::generate_price(OrderRequest::Type type, bool aggressive) {
	assert(type == OrderRequest::Type::Buy || type == OrderRequest::Type::Sell);
	BookState::OrderSnapshot snapshot = clob_.snapshot();
	const Order::Price best_bid = snapshot.best_bid.price == Order::INVALID_ORDER_ID ? DEFAULT_BUY_PRICE : snapshot.best_bid.price;
	const Order::Price best_ask = snapshot.best_offer.price == Order::INVALID_ORDER_ID ? DEFAULT_SELL_PRICE : snapshot.best_offer.price;

	const Order::Price price_variance = aggressive ? LARGE_PRICE_VARIANCE : PRICE_VARIANCE;

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

	min_price = std::max(min_price, MIN_PRICE);
	max_price = std::min(max_price, MAX_PRICE);
	std::uniform_int_distribution<Order::Price> price_distribution(min_price, max_price);

	return price_distribution(generator);
}

OrderRequest MarketRequestGenerator::generate_cancel_request() const {
	std::uniform_int_distribution<size_t> active_order_distribution(0, active_order_count - 1);
	Order::ID new_order_id = active_order_ids[active_order_distribution(generator)];
	
	OrderRequest request {
		.order_id = new_order_id,
		.type = OrderRequest::Type::Cancel,
	};

	return request;
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

OrderRequest MarketRequestGenerator::generate_new_order_request(OrderRequest::Type type) {
	assert(type == OrderRequest::Type::Buy || type == OrderRequest::Type::Sell);
	
	double random_value = order_type_distribution(generator);
	if(random_value < AGGRESSIVE_ORDER_PROBABILITY) {
		return generate_aggressive_order_request(type);
	}
	else {
		return generate_passive_order_request(type);
	}
}

OrderRequest MarketRequestGenerator::generate_random_order_request() {
	double random_value = order_type_distribution(generator);
	if(random_value < CANCEL_PROBABILITY) {
		return generate_cancel_request();
	}
	else if(random_value < CANCEL_PROBABILITY + BUY_PROBABILITY) {
		return generate_new_order_request(OrderRequest::Type::Buy);
	}
	else {
		return generate_new_order_request(OrderRequest::Type::Sell);
	}
}
}