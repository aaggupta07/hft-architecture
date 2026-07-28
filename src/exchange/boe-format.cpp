#include "boe-format.hpp"

#include <cstring>
#include <cassert>
#include <arpa/inet.h>

namespace exchange {
std::array<std::byte, sizeof(MarketEvent)> BinaryOrderExchangeFormat::encode(const MarketEvent& event) {
	// TODO: Implement encoding logic for MarketEvent to binary format
	std::array<std::byte, sizeof(MarketEvent)> buffer;
	size_t offset = 0;

	Order order_network {
		.order_id = htonll(event.order.order_id),
		.price = htonl(event.order.price),
		.quantity = htonl(event.order.quantity),
		.side = event.order.side,
	};

	std::memcpy(buffer.data() + offset, &event.type, sizeof(event.type));
	offset += sizeof(event.type);
	std::memcpy(buffer.data() + offset, &order_network.order_id, sizeof(order_network.order_id));
	offset += sizeof(order_network.order_id);
	std::memcpy(buffer.data() + offset, &order_network.price, sizeof(order_network.price));
	offset += sizeof(order_network.price);
	std::memcpy(buffer.data() + offset, &order_network.quantity, sizeof(order_network.quantity));
	offset += sizeof(order_network.quantity);
	std::memcpy(buffer.data() + offset, &order_network.side, sizeof(order_network.side));
	offset += sizeof(order_network.side);

	return buffer;
}

MarketEvent BinaryOrderExchangeFormat::decode(std::span<const std::byte> buffer) {
	assert(buffer.size() == sizeof(MarketEvent));
	MarketEvent event;
	size_t offset = 0;

	std::memcpy(&event.type, buffer.data() + offset, sizeof(event.type));
	offset += sizeof(event.type);
	std::memcpy(&event.order.order_id, buffer.data() + offset, sizeof(event.order.order_id));
	offset += sizeof(event.order.order_id);
	std::memcpy(&event.order.price, buffer.data() + offset, sizeof(event.order.price));
	offset += sizeof(event.order.price);
	std::memcpy(&event.order.quantity, buffer.data() + offset, sizeof(event.order.quantity));
	offset += sizeof(event.order.quantity);
	std::memcpy(&event.order.side, buffer.data() + offset, sizeof(event.order.side));
	offset += sizeof(event.order.side);

	return event;
}
}