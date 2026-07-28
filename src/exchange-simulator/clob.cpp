#include "clob.hpp"
#include "order.hpp"
#include "book-state.hpp"

namespace exchange {
std::expected<void, Error> CentralLimitOrderBook::submit(const OrderRequest& request) {
	switch(request.type) {
		case OrderRequest::Type::Buy:
			buy_order(request);
			return {};
		case OrderRequest::Type::Sell:
			sell_order(request);
			return {};
		case OrderRequest::Type::Cancel:
			return cancel_order(request);
	}
	return {};
}

bool CentralLimitOrderBook::execute_trade(Order& new_order) {
	const Order best_resting = state_.best_opposing_order(Order::Side::Sell);
	if(	(best_resting.order_id == Order::INVALID_ORDER_ID) ||
		(new_order.side == Order::Side::Buy && new_order.price < best_resting.price) || 
		(new_order.side == Order::Side::Sell && new_order.price > best_resting.price)) {
			return false;
		}
	
	Order::Quantity quantity = std::min(new_order.quantity, best_resting.quantity);
	MarketEvent trade {
		.order = {
			.order_id = best_resting.order_id,
			.price = best_resting.price,
			.quantity = quantity,
			.side = best_resting.side,
		},
		.type = MarketEvent::Type::Trade,
	};

	state_.execute(trade);
	buffer_.wait_push(trade);
	new_order.quantity -= quantity;
	return true;
}

void CentralLimitOrderBook::add_order(Order& new_order) {
	MarketEvent add { .order = new_order, .type = MarketEvent::Type::Add };
	state_.execute(add);
	buffer_.wait_push(add);
}

void CentralLimitOrderBook::buy_order(const OrderRequest& request) {
	Order order {
		.order_id = request.order_id,
		.price = request.price,
		.quantity = request.quantity,
		.side = Order::Side::Buy,
	};

	// Keep executing trades until the order is fully filled or no more trades can be executed
	while(execute_trade(order));

	if(order.quantity != 0) {
		add_order(order);
	}
}

void CentralLimitOrderBook::sell_order(const OrderRequest& request) {
	Order order {
		.order_id = request.order_id,
		.price = request.price,
		.quantity = request.quantity,
		.side = Order::Side::Sell,
	};

	// Keep executing trades until the order is fully filled or no more trades can be executed
	while(execute_trade(order));

	if(order.quantity != 0) {
		add_order(order);
	}
}

std::expected<void, Error> CentralLimitOrderBook::cancel_order(const OrderRequest& request) {
	if(state_.order_exists(request.order_id)) {
		state_.cancel(request.order_id);

		Order new_order { .order_id = request.order_id };
		MarketEvent canceled_order {
			.order = new_order,
			.type = MarketEvent::Type::Cancel,
		};

		buffer_.wait_push(canceled_order);
	}
	
	else {
		return std::unexpected(Error::OrderDoesNotExist);
	}

	return {};
}
}
