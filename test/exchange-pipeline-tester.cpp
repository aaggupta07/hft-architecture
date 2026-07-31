#include "binary-protocol.hpp"
#include "circular-cache.hpp"
#include "clob.hpp"
#include "encoded-message.hpp"
#include "lazy-ring-buffer.hpp"
#include "sequencer.hpp"
#include "tcp-connection.hpp"

#include <cassert>
#include <cstdio>
#include <print>
#include <thread>

#include <sys/socket.h>
#include <unistd.h>

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

void verify_lazy_ring_buffer_spsc() {
	constexpr size_t MESSAGE_COUNT = 10'000;
	LazyRingBuffer<size_t, 64> buffer;

	std::jthread producer([&buffer] {
		for(size_t value = 0; value < MESSAGE_COUNT; ++value) {
			buffer.wait_get_head_ref() = value;
			buffer.publish();
		}
	});

	for(size_t expected = 0; expected < MESSAGE_COUNT; ++expected) {
		size_t& value = buffer.wait_get_tail_ref();
		assert(value == expected);
		buffer.consume();
	}
}

void verify_connection_move() {
	int sockets[2];
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

	exchange::Connection<16> connection;
	connection.set_socket(sockets[0]);
	exchange::Connection<16> moved_connection(std::move(connection));
	assert(connection.socket() == -1);
	assert(moved_connection.socket() == sockets[0]);

	const std::array<std::byte, 3> message {std::byte{1}, std::byte{2}, std::byte{3}};
	moved_connection.save(message);
	assert(moved_connection.send());

	std::array<std::byte, message.size()> received{};
	assert(recv(sockets[1], received.data(), received.size(), 0) == static_cast<ssize_t>(received.size()));
	assert(received == message);
	close(sockets[1]);
}
}

int main() {
	verify_lazy_ring_buffer_spsc();
	verify_connection_move();

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
