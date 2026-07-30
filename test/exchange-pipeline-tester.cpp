#include "binary-protocol.hpp"
#include "circular-cache.hpp"
#include "clob.hpp"
#include "encoded-message.hpp"
#include "lazy-ring-buffer.hpp"
#include "sequencer.hpp"

#include <cassert>
#include <cstdio>
#include <print>

namespace {
using Buffer = LazyRingBuffer<MarketEvent, exchange::config::MARKET_EVENT_BUFFER_CAPACITY>;
using Cache = CircularCache<exchange::EncodedMessage, exchange::config::RETRANSMIT_CACHE_SIZE>;

bool same_event(const MarketEvent& left, const MarketEvent& right) {
	return left.type == right.type
		&& left.order.order_id == right.order.order_id
		&& left.order.price == right.order.price
		&& left.order.quantity == right.order.quantity
		&& left.order.side == right.order.side;
}

void verify_events(
	Buffer& buffer,
	Cache& cache,
	exchange::Sequencer<exchange::BinaryProtocol>& sequencer,
	exchange::SequenceID& next_sequence)
{
	while(auto event = buffer.try_get_tail_ref()) {
		auto message = sequencer.generate_message(*event);
		assert(message.header().sequence_number == next_sequence);
		assert(exchange::BinaryProtocol::decode(message.payload()).order.order_id == event->order.order_id);
		assert(same_event(exchange::BinaryProtocol::decode(message.payload()), *event));

		cache.put_item(message);
		auto cached = cache.try_get_item(static_cast<uint32_t>(next_sequence - 1));
		assert(cached);
		assert(cached->header().sequence_number == next_sequence);
		assert(same_event(exchange::BinaryProtocol::decode(cached->payload()), *event));
		++next_sequence;
		buffer.consume();
	}
}
}

int main() {
	Buffer buffer;
	Cache cache;
	exchange::CentralLimitOrderBook clob(buffer);
	exchange::Sequencer<exchange::BinaryProtocol> sequencer;
	exchange::SequenceID next_sequence = 1;

	const auto buy_id = clob.submit({.price = 100, .quantity = 100, .type = exchange::OrderRequest::Type::Buy});
	assert(buy_id && *buy_id != Order::INVALID_ORDER_ID);
	verify_events(buffer, cache, sequencer, next_sequence);

	const auto first_sell = clob.submit({.price = 90, .quantity = 40, .type = exchange::OrderRequest::Type::Sell});
	assert(first_sell && *first_sell == Order::INVALID_ORDER_ID);
	verify_events(buffer, cache, sequencer, next_sequence);

	const auto second_sell = clob.submit({.price = 90, .quantity = 60, .type = exchange::OrderRequest::Type::Sell});
	assert(second_sell && *second_sell == Order::INVALID_ORDER_ID);
	verify_events(buffer, cache, sequencer, next_sequence);

	const auto sell_id = clob.submit({.price = 105, .quantity = 100, .type = exchange::OrderRequest::Type::Sell});
	assert(sell_id && *sell_id != Order::INVALID_ORDER_ID);
	verify_events(buffer, cache, sequencer, next_sequence);

	const auto buy = clob.submit({.price = 110, .quantity = 50, .type = exchange::OrderRequest::Type::Buy});
	assert(buy && *buy == Order::INVALID_ORDER_ID);
	verify_events(buffer, cache, sequencer, next_sequence);

	const auto cancel = clob.submit({.order_id = *sell_id, .type = exchange::OrderRequest::Type::Cancel});
	assert(cancel);
	verify_events(buffer, cache, sequencer, next_sequence);

	std::println("Exchange pipeline test passed ({} messages).", next_sequence - 1);
	std::fflush(stdout);
}
