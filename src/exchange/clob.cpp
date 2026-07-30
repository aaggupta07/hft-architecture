#include "clob.hpp"
#include "order.hpp"
#include "book-state.hpp"

namespace exchange {
std::expected<Order::ID, Error> CentralLimitOrderBook::submit(const OrderRequest& request, std::stop_token stop_token) {
	switch(request.type) {
		case OrderRequest::Type::Buy:
			return buy_order(request, stop_token);
		case OrderRequest::Type::Sell:
			return sell_order(request, stop_token);
		case OrderRequest::Type::Cancel:
			auto result =  cancel_order(request, stop_token);
			if(!result) return std::unexpected(result.error());
			return Order::INVALID_ORDER_ID;
	}
}

bool CentralLimitOrderBook::execute_trade(Order& new_order, std::stop_token stop_token) {
	if(stop_token.stop_requested()) [[unlikely]] return false;
	
	const Order best_resting = state_.best_opposing_order(new_order.side);
	if(	(best_resting.order_id == Order::INVALID_ORDER_ID) ||
		(new_order.side == Order::Side::Buy && new_order.price < best_resting.price) || 
		(new_order.side == Order::Side::Sell && new_order.price > best_resting.price)) {
			return false;
		}
	
	Order::Quantity quantity = std::min(new_order.quantity, best_resting.quantity);
	
	MarketEvent* trade = buffer_.wait_get_head_ref(stop_token);
	if(trade == nullptr) [[unlikely]] return false;

	trade->order = {
		.order_id = best_resting.order_id,
		.price = best_resting.price,
		.quantity = quantity,
		.side = best_resting.side,
	};
	trade->type = MarketEvent::Type::Trade;
	buffer_.publish();

	state_.execute(*trade);
	new_order.quantity -= quantity;

	return true;
}

void CentralLimitOrderBook::add_order(Order& new_order, std::stop_token stop_token) {
	if(stop_token.stop_requested()) [[unlikely]] return;
	
	MarketEvent* add =  buffer_.wait_get_head_ref(stop_token);
	if(add == nullptr) [[unlikely]] return;

	add->order = new_order;
	add->type = MarketEvent::Type::Add;
	buffer_.publish();

	state_.execute(*add);
}

Order::ID CentralLimitOrderBook::buy_order(const OrderRequest& request, std::stop_token stop_token) {
	if(request.quantity == 0) return Order::INVALID_ORDER_ID;

	Order order {
		.price = request.price,
		.quantity = request.quantity,
		.side = Order::Side::Buy,
	};

	// Keep executing trades until the order is fully filled or no more trades can be executed
	while(order.quantity != 0 && execute_trade(order, stop_token));

	if(order.quantity != 0 && !stop_token.stop_requested()) {
		order.order_id = next_order_id_++;
		add_order(order, stop_token);
		return order.order_id;
	}

	return Order::INVALID_ORDER_ID;
}

Order::ID CentralLimitOrderBook::sell_order(const OrderRequest& request, std::stop_token stop_token) {
	if(request.quantity == 0) return Order::INVALID_ORDER_ID;

	Order order {
		.price = request.price,
		.quantity = request.quantity,
		.side = Order::Side::Sell,
	};

	// Keep executing trades until the order is fully filled or no more trades can be executed
	while(order.quantity != 0 && execute_trade(order, stop_token));

	if(order.quantity != 0 && !stop_token.stop_requested()) {
		order.order_id = next_order_id_++;
		add_order(order, stop_token);
		return order.order_id;
	}
	return Order::INVALID_ORDER_ID;
}

std::expected<void, Error> CentralLimitOrderBook::cancel_order(const OrderRequest& request, std::stop_token stop_token) {
	if(stop_token.stop_requested()) [[unlikely]] return {};
	if(!state_.order_exists(request.order_id)) return std::unexpected(Error::OrderDoesNotExist);

	state_.cancel(request.order_id);

	MarketEvent* canceled_order = buffer_.wait_get_head_ref(stop_token);
	if(canceled_order == nullptr) [[unlikely]] return {};

	canceled_order->order = Order { .order_id = request.order_id };
	canceled_order->type =  MarketEvent::Type::Cancel;
	buffer_.publish();

	return {};
}
}
