#ifndef CLOB_HPP
#define CLOB_HPP

#include "protocol.hpp"
#include "exchange-errors.hpp"
#include "order.hpp"
#include "book-state.hpp"
#include "ring-buffer.hpp"
#include "config.hpp"

#include <expected>
#include <stop_token>

namespace exchange {
class CentralLimitOrderBook {
private:
	using MarketEventBuffer = SharedRingBuffer<MarketEvent, config::MARKET_EVENT_BUFFER_CAPACITY>;
	
	Order::ID next_order_id_ = 1;
	BookState state_;
	MarketEventBuffer& buffer_;

	std::expected<void, Error> cancel_order(const OrderRequest& request, std::stop_token stop_token);

	bool execute_trade(Order& new_order, std::stop_token stop_token);
	void add_order(Order& new_order, std::stop_token stop_token);
	Order::ID buy_order(const OrderRequest& request, std::stop_token stop_token);
	Order::ID sell_order(const OrderRequest& request, std::stop_token stop_token);

public:
	explicit constexpr CentralLimitOrderBook(MarketEventBuffer& buffer): buffer_(buffer) {}
	std::expected<Order::ID, Error> submit(const OrderRequest& request, std::stop_token stop_token = {});
	BookState::OrderSnapshot snapshot() const noexcept { return state_.snapshot(); }
	bool order_exists(Order::ID order_id) const noexcept { return state_.order_exists(order_id); }
};

}

#endif
