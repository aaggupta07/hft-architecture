#include "boe-format.hpp"

#include <cstring>
#include <cassert>
#include <arpa/inet.h>

namespace exchange {
auto BinaryOrderExchangeFormat::encode(const MarketEvent& event) -> std::array<std::byte, BUFFER_SIZE> {
	std::array<std::byte, BUFFER_SIZE> buffer;
	size_t offset = 0;

	Order order_network {
		.order_id = htonll(event.order.order_id),
		.price = static_cast<Order::Price>(htonl(static_cast<uint32_t>(event.order.price))),
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

MarketEvent BinaryOrderExchangeFormat::decode(const std::span<const std::byte> buffer) {
	assert(buffer.size() == BUFFER_SIZE);
	MarketEvent event {};
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

	event.order.order_id = ntohll(event.order.order_id);
	event.order.price = static_cast<Order::Price>(ntohl(static_cast<uint32_t>(event.order.price)));
	event.order.quantity = ntohl(event.order.quantity);

	return event;
}
}