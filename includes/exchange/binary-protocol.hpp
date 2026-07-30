#ifndef BINARY_PROTOCOL_HPP
#define BINARY_PROTOCOL_HPP

#include "order.hpp"
#include <span>
#include <array>

namespace exchange{
class BinaryProtocol {
public:
	static constexpr size_t BUFFER_SIZE = sizeof(MarketEvent::Type) 
		+ sizeof(Order::ID) + sizeof(Order::Price) + sizeof(Order::Quantity) + sizeof(Order::Side);

	BinaryProtocol() = delete;
	BinaryProtocol(const BinaryProtocol&) = delete;
	BinaryProtocol& operator=(const BinaryProtocol&) = delete;
	BinaryProtocol(BinaryProtocol&&) = delete;
	BinaryProtocol& operator=(BinaryProtocol&&) = delete;

	static std::array<std::byte, BUFFER_SIZE> encode(const MarketEvent& event);
	static MarketEvent decode(const std::span<const std::byte> buffer);
};
}

#endif