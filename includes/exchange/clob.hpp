#ifndef CLOB_HPP
#define CLOB_HPP

#include "protocol.hpp"
#include "exchange-errors.hpp"
#include "book-state.hpp"
#include "order.hpp"
#include "ring-buffer.hpp"
#include <expected>

namespace exchange {
class CentralLimitOrderBook {
private:
	static constexpr size_t MARKET_EVENT_BUFFER_CAPACITY = 1 << 10;
	using MarketEventBuffer = SharedRingBuffer<MarketEvent, MARKET_EVENT_BUFFER_CAPACITY>;
	
	BookState state_;
	MarketEventBuffer& buffer_;

	std::expected<void, Error> cancel_order(const OrderRequest& request);

	bool execute_trade(Order& new_order);
	void add_order(Order& new_order);
	void buy_order(const OrderRequest& request);
	void sell_order(const OrderRequest& request);

public:
	CentralLimitOrderBook(MarketEventBuffer& buffer): buffer_(buffer) {}
	std::expected<void, Error> submit(const OrderRequest& request);
	BookState::OrderSnapshot snapshot() const noexcept { return state_.snapshot(); }
};

}

#endif